#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Parallel/FVizParallel.h>

#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_PARALLEL_MAX_THREADS 64u
#define FVIZ_PARALLEL_DEFAULT_GRAIN 4096u
#define FVIZ_PARALLEL_SUM_GRAIN 4096u
#define FVIZ_PARALLEL_SCRATCH_CAPACITY ((FVizSize)65536u)

typedef struct FVizParallelThreadArg
{
    struct FVizParallelContext* context;
    uint32_t index;
} FVizParallelThreadArg;

typedef struct FVizQueuedTask
{
    FVizSize begin;
    FVizSize end;
    FVizSize grain_size;
    FVizParallelRangeResultFn function;
    void* user_data;
} FVizQueuedTask;

struct FVizCancellationToken
{
    FVizAtomicU32 cancelled;
};

struct FVizTaskGroup
{
    FVizParallelContext* context;
    FVizCancellationToken* cancellation;
    FVizQueuedTask* tasks;
    FVizSize task_count;
    FVizSize task_capacity;
};

struct FVizParallelContext
{
#if defined(_WIN32)
    CRITICAL_SECTION dispatch_mutex;
    CRITICAL_SECTION state_mutex;
    CONDITION_VARIABLE work_condition;
    CONDITION_VARIABLE done_condition;
    HANDLE threads[FVIZ_PARALLEL_MAX_THREADS - 1u];
#else
    pthread_mutex_t dispatch_mutex;
    pthread_mutex_t state_mutex;
    pthread_cond_t work_condition;
    pthread_cond_t done_condition;
    pthread_t threads[FVIZ_PARALLEL_MAX_THREADS - 1u];
#endif
    FVizParallelThreadArg thread_args[FVIZ_PARALLEL_MAX_THREADS - 1u];
    uint32_t worker_count;
    uint32_t configured_thread_count;
    uint32_t participating_workers;
    uint32_t is_default;
    FVizParallelAffinityPolicy affinity_policy;
    uint64_t generation;
    FVizParallelRangeFn void_function;
    FVizParallelRangeResultFn result_function;
    void* user_data;
    FVizCancellationToken* cancellation;
    FVizSize end;
    FVizSize grain_size;
    FVizSize first_failure_begin;
    FVizResult first_failure;
    FVizAtomicU64 next;
    FVizAtomicU32 remaining;
    FVizAtomicU32 stopping;
    FVizAtomicU64 dispatch_count;
    FVizAtomicU64 chunk_count;
    FVizAtomicU64 completed_chunk_count;
    FVizAtomicU64 cancelled_dispatch_count;
    FVizAtomicU64 failed_dispatch_count;
};

static FVizParallelContext g_fviz_parallel_context;
static FVizAtomicU32 g_fviz_parallel_thread_limit = {0};

#if defined(_MSC_VER)
static __declspec(thread) int g_fviz_parallel_inside_worker = 0;
__declspec(align(64)) static __declspec(thread) unsigned char
    g_fviz_parallel_scratch[FVIZ_PARALLEL_SCRATCH_CAPACITY];
static __declspec(thread) FVizSize g_fviz_parallel_scratch_offset = 0u;
#else
static _Thread_local int g_fviz_parallel_inside_worker = 0;
static _Alignas(64) _Thread_local unsigned char
    g_fviz_parallel_scratch[FVIZ_PARALLEL_SCRATCH_CAPACITY];
static _Thread_local FVizSize g_fviz_parallel_scratch_offset = 0u;
#endif

#if defined(_WIN32)
static INIT_ONCE g_fviz_parallel_once = INIT_ONCE_STATIC_INIT;
#else
static pthread_once_t g_fviz_parallel_once = PTHREAD_ONCE_INIT;
#endif

static void fviz_parallel_shutdown_default(void);

uint32_t fviz_parallel_hardware_thread_count(void)
{
    uint32_t count = 1u;
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    if (info.dwNumberOfProcessors > 0u) count = (uint32_t)info.dwNumberOfProcessors;
#else
    const long detected = sysconf(_SC_NPROCESSORS_ONLN);
    if (detected > 0) count = (uint32_t)detected;
#endif
    return count > FVIZ_PARALLEL_MAX_THREADS ? FVIZ_PARALLEL_MAX_THREADS : count;
}

static void fviz_parallel_lock_dispatch(FVizParallelContext* context)
{
#if defined(_WIN32)
    EnterCriticalSection(&context->dispatch_mutex);
#else
    (void)pthread_mutex_lock(&context->dispatch_mutex);
#endif
}

static void fviz_parallel_unlock_dispatch(FVizParallelContext* context)
{
#if defined(_WIN32)
    LeaveCriticalSection(&context->dispatch_mutex);
#else
    (void)pthread_mutex_unlock(&context->dispatch_mutex);
#endif
}

static void fviz_parallel_lock_state(FVizParallelContext* context)
{
#if defined(_WIN32)
    EnterCriticalSection(&context->state_mutex);
#else
    (void)pthread_mutex_lock(&context->state_mutex);
#endif
}

static void fviz_parallel_unlock_state(FVizParallelContext* context)
{
#if defined(_WIN32)
    LeaveCriticalSection(&context->state_mutex);
#else
    (void)pthread_mutex_unlock(&context->state_mutex);
#endif
}

static void fviz_parallel_broadcast_work(FVizParallelContext* context)
{
#if defined(_WIN32)
    WakeAllConditionVariable(&context->work_condition);
#else
    (void)pthread_cond_broadcast(&context->work_condition);
#endif
}

static void fviz_parallel_signal_done(FVizParallelContext* context)
{
    fviz_parallel_lock_state(context);
#if defined(_WIN32)
    WakeConditionVariable(&context->done_condition);
#else
    (void)pthread_cond_signal(&context->done_condition);
#endif
    fviz_parallel_unlock_state(context);
}

static FVizBool fviz_parallel_cancelled(const FVizParallelContext* context)
{
    return context->cancellation != NULL &&
            fviz_atomic_u32_load(&context->cancellation->cancelled) != 0u
        ? FVIZ_TRUE
        : FVIZ_FALSE;
}

static void fviz_parallel_capture_failure(
    FVizParallelContext* context,
    FVizSize begin,
    FVizResult result)
{
    fviz_parallel_lock_state(context);
    if (context->first_failure == FVIZ_OK || begin < context->first_failure_begin)
    {
        context->first_failure = result;
        context->first_failure_begin = begin;
    }
    fviz_parallel_unlock_state(context);
}

static void fviz_parallel_execute_chunks(FVizParallelContext* context)
{
    for (;;)
    {
        FVizSize begin;
        FVizSize end;
        FVizResult result = FVIZ_OK;
        if (fviz_parallel_cancelled(context) != FVIZ_FALSE) break;
        begin = (FVizSize)fviz_atomic_u64_fetch_add(
            &context->next, (uint64_t)context->grain_size);
        if (begin >= context->end) break;
        end = begin + context->grain_size;
        if (end < begin || end > context->end) end = context->end;
        (void)fviz_atomic_u64_fetch_add(&context->chunk_count, 1u);
        g_fviz_parallel_scratch_offset = 0u;
        if (context->result_function != NULL)
            result = context->result_function(begin, end, context->user_data);
        else
            context->void_function(begin, end, context->user_data);
        if (result != FVIZ_OK)
            fviz_parallel_capture_failure(context, begin, result);
        else
            (void)fviz_atomic_u64_fetch_add(&context->completed_chunk_count, 1u);
    }
}

#if defined(_WIN32)
static DWORD WINAPI fviz_parallel_worker_entry(LPVOID user_data)
#else
static void* fviz_parallel_worker_entry(void* user_data)
#endif
{
    FVizParallelThreadArg* argument = (FVizParallelThreadArg*)user_data;
    FVizParallelContext* context = argument->context;
    uint64_t observed_generation = 0u;
    g_fviz_parallel_inside_worker = 1;
#if defined(_WIN32)
    if (context->affinity_policy != FVIZ_PARALLEL_AFFINITY_NONE)
    {
        const uint32_t hardware_threads = fviz_parallel_hardware_thread_count();
        const uint32_t processor = context->affinity_policy == FVIZ_PARALLEL_AFFINITY_SPREAD
            ? (argument->index * hardware_threads) / context->configured_thread_count
            : argument->index;
        (void)SetThreadIdealProcessor(GetCurrentThread(), processor);
    }
#endif
    for (;;)
    {
        FVizBool participate;
        fviz_parallel_lock_state(context);
        while (observed_generation == context->generation &&
               fviz_atomic_u32_load(&context->stopping) == 0u)
        {
#if defined(_WIN32)
            (void)SleepConditionVariableCS(
                &context->work_condition, &context->state_mutex, INFINITE);
#else
            (void)pthread_cond_wait(&context->work_condition, &context->state_mutex);
#endif
        }
        if (fviz_atomic_u32_load(&context->stopping) != 0u)
        {
            fviz_parallel_unlock_state(context);
            break;
        }
        observed_generation = context->generation;
        participate = argument->index < context->participating_workers ? FVIZ_TRUE : FVIZ_FALSE;
        fviz_parallel_unlock_state(context);
        if (participate == FVIZ_FALSE) continue;
        fviz_parallel_execute_chunks(context);
        if (fviz_atomic_u32_fetch_sub(&context->remaining, 1u) == 1u)
            fviz_parallel_signal_done(context);
    }
#if defined(_WIN32)
    return 0u;
#else
    return NULL;
#endif
}

static FVizResult fviz_parallel_context_initialize(
    FVizParallelContext* context,
    uint32_t thread_count,
    uint32_t is_default,
    FVizParallelAffinityPolicy affinity_policy)
{
    uint32_t desired_workers;
    uint32_t i;
    memset(context, 0, sizeof(*context));
    if (thread_count == 0u) thread_count = fviz_parallel_hardware_thread_count();
    if (thread_count > FVIZ_PARALLEL_MAX_THREADS) thread_count = FVIZ_PARALLEL_MAX_THREADS;
    if (thread_count == 0u) thread_count = 1u;
    context->configured_thread_count = thread_count;
    context->is_default = is_default;
    context->affinity_policy = affinity_policy;
#if defined(_WIN32)
    InitializeCriticalSection(&context->dispatch_mutex);
    InitializeCriticalSection(&context->state_mutex);
    InitializeConditionVariable(&context->work_condition);
    InitializeConditionVariable(&context->done_condition);
#else
    if (pthread_mutex_init(&context->dispatch_mutex, NULL) != 0 ||
        pthread_mutex_init(&context->state_mutex, NULL) != 0 ||
        pthread_cond_init(&context->work_condition, NULL) != 0 ||
        pthread_cond_init(&context->done_condition, NULL) != 0)
        return FVIZ_ERROR_INTERNAL;
#endif
    desired_workers = thread_count - 1u;
    for (i = 0u; i < desired_workers; ++i)
    {
        context->thread_args[i].context = context;
        context->thread_args[i].index = i;
#if defined(_WIN32)
        context->threads[i] = CreateThread(
            NULL, 0u, fviz_parallel_worker_entry, &context->thread_args[i], 0u, NULL);
        if (context->threads[i] == NULL) break;
#else
        if (pthread_create(
                &context->threads[i], NULL, fviz_parallel_worker_entry, &context->thread_args[i]) != 0)
            break;
#endif
        ++context->worker_count;
    }
    context->configured_thread_count = context->worker_count + 1u;
    return FVIZ_OK;
}

static void fviz_parallel_context_shutdown(FVizParallelContext* context)
{
    uint32_t i;
    fviz_parallel_lock_dispatch(context);
    (void)fviz_atomic_u32_exchange(&context->stopping, 1u);
    fviz_parallel_lock_state(context);
    ++context->generation;
    fviz_parallel_broadcast_work(context);
    fviz_parallel_unlock_state(context);
    for (i = 0u; i < context->worker_count; ++i)
    {
#if defined(_WIN32)
        (void)WaitForSingleObject(context->threads[i], INFINITE);
        (void)CloseHandle(context->threads[i]);
#else
        (void)pthread_join(context->threads[i], NULL);
#endif
    }
    fviz_parallel_unlock_dispatch(context);
#if defined(_WIN32)
    DeleteCriticalSection(&context->state_mutex);
    DeleteCriticalSection(&context->dispatch_mutex);
#else
    (void)pthread_cond_destroy(&context->done_condition);
    (void)pthread_cond_destroy(&context->work_condition);
    (void)pthread_mutex_destroy(&context->state_mutex);
    (void)pthread_mutex_destroy(&context->dispatch_mutex);
#endif
}

static void fviz_parallel_shutdown_default(void)
{
    if (g_fviz_parallel_context.configured_thread_count != 0u &&
        fviz_atomic_u32_load(&g_fviz_parallel_context.stopping) == 0u)
        fviz_parallel_context_shutdown(&g_fviz_parallel_context);
}

#if defined(_WIN32)
static BOOL CALLBACK fviz_parallel_initialize_once(PINIT_ONCE once, PVOID parameter, PVOID* state)
{
    (void)once;
    (void)parameter;
    (void)state;
    (void)fviz_parallel_context_initialize(
        &g_fviz_parallel_context, fviz_parallel_hardware_thread_count(), 1u,
        FVIZ_PARALLEL_AFFINITY_NONE);
    (void)atexit(fviz_parallel_shutdown_default);
    return TRUE;
}
#else
static void fviz_parallel_initialize_once(void)
{
    (void)fviz_parallel_context_initialize(
        &g_fviz_parallel_context, fviz_parallel_hardware_thread_count(), 1u,
        FVIZ_PARALLEL_AFFINITY_NONE);
    (void)atexit(fviz_parallel_shutdown_default);
}
#endif

FVizParallelContext* fviz_parallel_default_context(void)
{
#if defined(_WIN32)
    (void)InitOnceExecuteOnce(&g_fviz_parallel_once, fviz_parallel_initialize_once, NULL, NULL);
#else
    (void)pthread_once(&g_fviz_parallel_once, fviz_parallel_initialize_once);
#endif
    return &g_fviz_parallel_context;
}

void fviz_parallel_context_options_initialize(FVizParallelContextOptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->version = FVIZ_PARALLEL_CONTEXT_OPTIONS_VERSION;
}

FVizResult fviz_parallel_context_create(
    const FVizParallelContextOptions* options,
    FVizParallelContext** out_context)
{
    FVizParallelContext* context;
    uint32_t thread_count = 0u;
    FVizParallelAffinityPolicy affinity_policy = FVIZ_PARALLEL_AFFINITY_NONE;
    FVizResult result;
    if (out_context == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "parallel context output is required");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_context = NULL;
    if (options != NULL)
    {
        if (options->struct_size < sizeof(FVizParallelContextOptions) ||
            options->version != FVIZ_PARALLEL_CONTEXT_OPTIONS_VERSION)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "parallel context options are incompatible");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        thread_count = options->thread_count;
        affinity_policy = options->affinity_policy;
        if (affinity_policy < FVIZ_PARALLEL_AFFINITY_NONE ||
            affinity_policy > FVIZ_PARALLEL_AFFINITY_SPREAD)
            return FVIZ_ERROR_INVALID_ARGUMENT;
#if !defined(_WIN32)
        if (affinity_policy != FVIZ_PARALLEL_AFFINITY_NONE)
            return FVIZ_ERROR_NOT_SUPPORTED;
#endif
    }
    context = (FVizParallelContext*)fviz_alloc(sizeof(*context));
    if (context == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
    result = fviz_parallel_context_initialize(context, thread_count, 0u, affinity_policy);
    if (result != FVIZ_OK)
    {
        fviz_free(context);
        return result;
    }
    *out_context = context;
    return FVIZ_OK;
}

void fviz_parallel_context_destroy(FVizParallelContext* context)
{
    if (context == NULL || context->is_default != 0u) return;
    fviz_parallel_context_shutdown(context);
    fviz_free(context);
}

uint32_t fviz_parallel_context_thread_count(const FVizParallelContext* context)
{
    return context != NULL ? context->configured_thread_count : 0u;
}

uint32_t fviz_parallel_context_worker_count(const FVizParallelContext* context)
{
    return context != NULL ? context->worker_count : 0u;
}

void fviz_parallel_context_get_statistics(
    const FVizParallelContext* context,
    FVizParallelStatistics* out_statistics)
{
    if (out_statistics == NULL) return;
    memset(out_statistics, 0, sizeof(*out_statistics));
    if (context == NULL) return;
    out_statistics->dispatch_count = fviz_atomic_u64_load(&context->dispatch_count);
    out_statistics->chunk_count = fviz_atomic_u64_load(&context->chunk_count);
    out_statistics->completed_chunk_count = fviz_atomic_u64_load(&context->completed_chunk_count);
    out_statistics->cancelled_dispatch_count = fviz_atomic_u64_load(&context->cancelled_dispatch_count);
    out_statistics->failed_dispatch_count = fviz_atomic_u64_load(&context->failed_dispatch_count);
}

FVizResult fviz_cancellation_token_create(FVizCancellationToken** out_token)
{
    FVizCancellationToken* token;
    if (out_token == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_token = NULL;
    token = (FVizCancellationToken*)fviz_alloc(sizeof(*token));
    if (token == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
    memset(token, 0, sizeof(*token));
    *out_token = token;
    return FVIZ_OK;
}

void fviz_cancellation_token_destroy(FVizCancellationToken* token)
{
    fviz_free(token);
}

void fviz_cancellation_token_cancel(FVizCancellationToken* token)
{
    if (token != NULL) (void)fviz_atomic_u32_exchange(&token->cancelled, 1u);
}

void fviz_cancellation_token_reset(FVizCancellationToken* token)
{
    if (token != NULL) (void)fviz_atomic_u32_exchange(&token->cancelled, 0u);
}

FVizBool fviz_cancellation_token_is_cancelled(const FVizCancellationToken* token)
{
    return token != NULL && fviz_atomic_u32_load(&token->cancelled) != 0u
        ? FVIZ_TRUE
        : FVIZ_FALSE;
}

static FVizResult fviz_parallel_dispatch(
    FVizParallelContext* context,
    FVizSize begin,
    FVizSize end,
    FVizSize grain_size,
    FVizParallelRangeFn void_function,
    FVizParallelRangeResultFn result_function,
    void* user_data,
    FVizCancellationToken* cancellation)
{
    FVizSize item_count;
    FVizSize task_count;
    uint32_t thread_count;
    int previous_inside;
    FVizResult result;
    if (context == NULL || (void_function == NULL && result_function == NULL) || end < begin)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "parallel range, context, and function must be valid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (cancellation != NULL && fviz_cancellation_token_is_cancelled(cancellation) != FVIZ_FALSE)
        return FVIZ_ERROR_CANCELLED;
    item_count = end - begin;
    if (item_count == 0u) return FVIZ_OK;
    if (grain_size == 0u) grain_size = FVIZ_PARALLEL_DEFAULT_GRAIN;
    if (g_fviz_parallel_inside_worker != 0)
    {
        if (result_function != NULL) return result_function(begin, end, user_data);
        void_function(begin, end, user_data);
        return FVIZ_OK;
    }
    task_count = item_count / grain_size + (item_count % grain_size != 0u ? 1u : 0u);
    thread_count = context->configured_thread_count;
    if (context->is_default != 0u) thread_count = fviz_parallel_thread_limit();
    if ((FVizSize)thread_count > task_count) thread_count = (uint32_t)task_count;
    if (thread_count > context->worker_count + 1u) thread_count = context->worker_count + 1u;
    if (thread_count <= 1u)
    {
        previous_inside = g_fviz_parallel_inside_worker;
        g_fviz_parallel_inside_worker = 1;
        g_fviz_parallel_scratch_offset = 0u;
        if (result_function != NULL) result = result_function(begin, end, user_data);
        else
        {
            void_function(begin, end, user_data);
            result = FVIZ_OK;
        }
        g_fviz_parallel_inside_worker = previous_inside;
        if (result != FVIZ_OK) return result;
        return cancellation != NULL && fviz_cancellation_token_is_cancelled(cancellation) != FVIZ_FALSE
            ? FVIZ_ERROR_CANCELLED
            : FVIZ_OK;
    }

    fviz_parallel_lock_dispatch(context);
    context->void_function = void_function;
    context->result_function = result_function;
    context->user_data = user_data;
    context->cancellation = cancellation;
    context->end = end;
    context->grain_size = grain_size;
    context->first_failure = FVIZ_OK;
    context->first_failure_begin = (FVizSize)-1;
    (void)fviz_atomic_u64_exchange(&context->next, (uint64_t)begin);
    context->participating_workers = thread_count - 1u;
    (void)fviz_atomic_u32_exchange(&context->remaining, context->participating_workers);
    (void)fviz_atomic_u64_fetch_add(&context->dispatch_count, 1u);
    fviz_parallel_lock_state(context);
    ++context->generation;
    fviz_parallel_broadcast_work(context);
    fviz_parallel_unlock_state(context);

    previous_inside = g_fviz_parallel_inside_worker;
    g_fviz_parallel_inside_worker = 1;
    fviz_parallel_execute_chunks(context);
    g_fviz_parallel_inside_worker = previous_inside;

    fviz_parallel_lock_state(context);
    while (fviz_atomic_u32_load(&context->remaining) != 0u)
    {
#if defined(_WIN32)
        (void)SleepConditionVariableCS(&context->done_condition, &context->state_mutex, INFINITE);
#else
        (void)pthread_cond_wait(&context->done_condition, &context->state_mutex);
#endif
    }
    result = context->first_failure;
    fviz_parallel_unlock_state(context);
    if (result != FVIZ_OK)
        (void)fviz_atomic_u64_fetch_add(&context->failed_dispatch_count, 1u);
    else if (cancellation != NULL && fviz_cancellation_token_is_cancelled(cancellation) != FVIZ_FALSE)
    {
        result = FVIZ_ERROR_CANCELLED;
        (void)fviz_atomic_u64_fetch_add(&context->cancelled_dispatch_count, 1u);
    }
    context->cancellation = NULL;
    fviz_parallel_unlock_dispatch(context);
    return result;
}

FVizResult fviz_parallel_context_for(
    FVizParallelContext* context,
    FVizSize begin,
    FVizSize end,
    FVizSize grain_size,
    FVizParallelRangeResultFn function,
    void* user_data,
    FVizCancellationToken* cancellation)
{
    return fviz_parallel_dispatch(
        context, begin, end, grain_size, NULL, function, user_data, cancellation);
}

void fviz_parallel_set_thread_limit(uint32_t thread_limit)
{
    if (thread_limit > FVIZ_PARALLEL_MAX_THREADS) thread_limit = FVIZ_PARALLEL_MAX_THREADS;
    (void)fviz_atomic_u32_exchange(&g_fviz_parallel_thread_limit, thread_limit);
}

uint32_t fviz_parallel_thread_limit(void)
{
    const uint32_t limit = fviz_atomic_u32_load(&g_fviz_parallel_thread_limit);
    return limit != 0u ? limit : fviz_parallel_hardware_thread_count();
}

uint32_t fviz_parallel_worker_count(void)
{
    return fviz_parallel_default_context()->worker_count;
}

uint64_t fviz_parallel_dispatch_count(void)
{
    return fviz_atomic_u64_load(&fviz_parallel_default_context()->dispatch_count);
}

FVizResult fviz_parallel_for(
    FVizSize begin,
    FVizSize end,
    FVizSize grain_size,
    FVizParallelRangeFn function,
    void* user_data)
{
    return fviz_parallel_dispatch(
        fviz_parallel_default_context(), begin, end, grain_size, function, NULL, user_data, NULL);
}

FVizResult fviz_task_group_create(
    FVizParallelContext* context,
    FVizCancellationToken* cancellation,
    FVizTaskGroup** out_group)
{
    FVizTaskGroup* group;
    if (context == NULL || out_group == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_group = NULL;
    group = (FVizTaskGroup*)fviz_alloc(sizeof(*group));
    if (group == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
    memset(group, 0, sizeof(*group));
    group->context = context;
    group->cancellation = cancellation;
    *out_group = group;
    return FVIZ_OK;
}

void fviz_task_group_destroy(FVizTaskGroup* group)
{
    if (group == NULL) return;
    fviz_free(group->tasks);
    fviz_free(group);
}

FVizResult fviz_task_group_run(
    FVizTaskGroup* group,
    FVizSize begin,
    FVizSize end,
    FVizSize grain_size,
    FVizParallelRangeResultFn function,
    void* user_data)
{
    FVizQueuedTask* tasks;
    FVizSize new_capacity;
    FVizSize allocation_size;
    if (group == NULL || function == NULL || end < begin) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (group->task_count == group->task_capacity)
    {
        new_capacity = group->task_capacity == 0u ? 4u : group->task_capacity * 2u;
        if (new_capacity < group->task_capacity) return FVIZ_ERROR_OVERFLOW;
        if (fviz_size_multiply(new_capacity, sizeof(*tasks), &allocation_size) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        tasks = (FVizQueuedTask*)fviz_realloc(group->tasks, allocation_size);
        if (tasks == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
        group->tasks = tasks;
        group->task_capacity = new_capacity;
    }
    group->tasks[group->task_count].begin = begin;
    group->tasks[group->task_count].end = end;
    group->tasks[group->task_count].grain_size = grain_size;
    group->tasks[group->task_count].function = function;
    group->tasks[group->task_count].user_data = user_data;
    ++group->task_count;
    return FVIZ_OK;
}

FVizResult fviz_task_group_wait(FVizTaskGroup* group)
{
    FVizResult result = FVIZ_OK;
    FVizSize i;
    if (group == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < group->task_count; ++i)
    {
        const FVizQueuedTask* task = &group->tasks[i];
        result = fviz_parallel_context_for(
            group->context,
            task->begin,
            task->end,
            task->grain_size,
            task->function,
            task->user_data,
            group->cancellation);
        if (result != FVIZ_OK) break;
    }
    group->task_count = 0u;
    return result;
}

typedef struct FVizSumContext
{
    const double* values;
    double* partials;
    FVizSize grain_size;
} FVizSumContext;

static FVizResult fviz_parallel_sum_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizSumContext* context = (FVizSumContext*)user_data;
    double sum = 0.0;
    FVizSize i;
    for (i = begin; i < end; ++i) sum += context->values[i];
    context->partials[begin / context->grain_size] = sum;
    return FVIZ_OK;
}

FVizResult fviz_parallel_sum_f64(
    FVizParallelContext* context,
    const double* values,
    FVizSize count,
    double* out_sum,
    FVizCancellationToken* cancellation)
{
    FVizSumContext sum_context;
    FVizSize chunk_count;
    FVizSize allocation_size;
    FVizSize i;
    FVizResult result;
    if (context == NULL || (values == NULL && count != 0u) || out_sum == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_sum = 0.0;
    if (count == 0u) return FVIZ_OK;
    chunk_count = count / FVIZ_PARALLEL_SUM_GRAIN +
        (count % FVIZ_PARALLEL_SUM_GRAIN != 0u ? 1u : 0u);
    if (fviz_size_multiply(chunk_count, sizeof(double), &allocation_size) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    sum_context.partials = (double*)fviz_alloc(allocation_size);
    if (sum_context.partials == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
    sum_context.values = values;
    sum_context.grain_size = FVIZ_PARALLEL_SUM_GRAIN;
    result = fviz_parallel_context_for(
        context, 0u, count, FVIZ_PARALLEL_SUM_GRAIN,
        fviz_parallel_sum_range, &sum_context, cancellation);
    if (result == FVIZ_OK)
        for (i = 0u; i < chunk_count; ++i) *out_sum += sum_context.partials[i];
    fviz_free(sum_context.partials);
    return result;
}

FVizResult fviz_parallel_exclusive_scan_u64(
    FVizParallelContext* context,
    const uint64_t* input,
    uint64_t* output,
    FVizSize count,
    uint64_t initial_value,
    uint64_t* out_total)
{
    uint64_t sum = initial_value;
    FVizSize i;
    if (context == NULL || (count != 0u && (input == NULL || output == NULL)))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < count; ++i)
    {
        const uint64_t value = input[i];
        output[i] = sum;
        if (UINT64_MAX - sum < value) return FVIZ_ERROR_OVERFLOW;
        sum += value;
    }
    if (out_total != NULL) *out_total = sum;
    return FVIZ_OK;
}

FVizResult fviz_parallel_inclusive_scan_u64(
    FVizParallelContext* context,
    const uint64_t* input,
    uint64_t* output,
    FVizSize count,
    uint64_t* out_total)
{
    uint64_t sum = 0u;
    FVizSize i;
    if (context == NULL || (count != 0u && (input == NULL || output == NULL)))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < count; ++i)
    {
        if (UINT64_MAX - sum < input[i]) return FVIZ_ERROR_OVERFLOW;
        sum += input[i];
        output[i] = sum;
    }
    if (out_total != NULL) *out_total = sum;
    return FVIZ_OK;
}

FVizResult fviz_parallel_stable_sort_u64_indices(
    FVizParallelContext* context,
    const uint64_t* keys,
    FVizSize* indices,
    FVizSize count)
{
    FVizSize* temporary;
    FVizSize width;
    FVizSize i;
    FVizSize allocation_size;
    if (context == NULL || (count != 0u && (keys == NULL || indices == NULL)))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (count < 2u) return FVIZ_OK;
    if (fviz_size_multiply(count, sizeof(*temporary), &allocation_size) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    for (i = 0u; i < count; ++i)
        if (indices[i] >= count) return FVIZ_ERROR_INVALID_ARGUMENT;
    temporary = (FVizSize*)fviz_alloc(allocation_size);
    if (temporary == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
    for (width = 1u; width < count; width = width > count / 2u ? count : width * 2u)
    {
        for (i = 0u; i < count; i += width > (FVizSize)-1 / 2u ? count : width * 2u)
        {
            FVizSize left = i;
            FVizSize middle = i + width < count ? i + width : count;
            FVizSize right = middle + width < count ? middle + width : count;
            FVizSize a = left;
            FVizSize b = middle;
            FVizSize out = left;
            while (a < middle && b < right)
            {
                const FVizSize ia = indices[a];
                const FVizSize ib = indices[b];
                if (keys[ia] <= keys[ib]) temporary[out++] = indices[a++];
                else temporary[out++] = indices[b++];
            }
            while (a < middle) temporary[out++] = indices[a++];
            while (b < right) temporary[out++] = indices[b++];
        }
        memcpy(indices, temporary, count * sizeof(*indices));
    }
    fviz_free(temporary);
    return FVIZ_OK;
}

void* fviz_parallel_scratch_allocate(FVizSize size, FVizSize alignment)
{
    FVizSize aligned_offset;
    if (g_fviz_parallel_inside_worker == 0 || size == 0u) return NULL;
    if (alignment == 0u) alignment = sizeof(void*);
    if (alignment > 64u || (alignment & (alignment - 1u)) != 0u) return NULL;
    aligned_offset = (g_fviz_parallel_scratch_offset + alignment - 1u) & ~(alignment - 1u);
    if (aligned_offset > FVIZ_PARALLEL_SCRATCH_CAPACITY ||
        size > FVIZ_PARALLEL_SCRATCH_CAPACITY - aligned_offset)
        return NULL;
    g_fviz_parallel_scratch_offset = aligned_offset + size;
    return &g_fviz_parallel_scratch[aligned_offset];
}

FVizSize fviz_parallel_scratch_capacity(void)
{
    return FVIZ_PARALLEL_SCRATCH_CAPACITY;
}
