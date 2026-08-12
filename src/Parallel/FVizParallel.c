#include <stdint.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

#include <FViz/Core/FVizError.h>
#include <FViz/Parallel/FVizParallel.h>

#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_PARALLEL_MAX_THREADS 64u
#define FVIZ_PARALLEL_DEFAULT_GRAIN 4096u

typedef struct FVizParallelRuntime FVizParallelRuntime;

typedef struct FVizParallelThreadArg
{
    FVizParallelRuntime* runtime;
    uint32_t index;
} FVizParallelThreadArg;

struct FVizParallelRuntime
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
    uint32_t participating_workers;
    uint64_t generation;
    FVizParallelRangeFn function;
    void* user_data;
    FVizSize end;
    FVizSize grain_size;
    FVizAtomicU64 next;
    FVizAtomicU32 remaining;
    FVizAtomicU32 stopping;
    FVizAtomicU64 dispatch_count;
};

static FVizParallelRuntime g_fviz_parallel_runtime;
static FVizAtomicU32 g_fviz_parallel_thread_limit = {0};

#if defined(_MSC_VER)
static __declspec(thread) int g_fviz_parallel_inside_worker = 0;
#else
static _Thread_local int g_fviz_parallel_inside_worker = 0;
#endif

#if defined(_WIN32)
static INIT_ONCE g_fviz_parallel_once = INIT_ONCE_STATIC_INIT;
#else
static pthread_once_t g_fviz_parallel_once = PTHREAD_ONCE_INIT;
#endif

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

static void fviz_parallel_lock_dispatch(FVizParallelRuntime* runtime)
{
#if defined(_WIN32)
    EnterCriticalSection(&runtime->dispatch_mutex);
#else
    (void)pthread_mutex_lock(&runtime->dispatch_mutex);
#endif
}

static void fviz_parallel_unlock_dispatch(FVizParallelRuntime* runtime)
{
#if defined(_WIN32)
    LeaveCriticalSection(&runtime->dispatch_mutex);
#else
    (void)pthread_mutex_unlock(&runtime->dispatch_mutex);
#endif
}

static void fviz_parallel_lock_state(FVizParallelRuntime* runtime)
{
#if defined(_WIN32)
    EnterCriticalSection(&runtime->state_mutex);
#else
    (void)pthread_mutex_lock(&runtime->state_mutex);
#endif
}

static void fviz_parallel_unlock_state(FVizParallelRuntime* runtime)
{
#if defined(_WIN32)
    LeaveCriticalSection(&runtime->state_mutex);
#else
    (void)pthread_mutex_unlock(&runtime->state_mutex);
#endif
}

static void fviz_parallel_broadcast_work(FVizParallelRuntime* runtime)
{
#if defined(_WIN32)
    WakeAllConditionVariable(&runtime->work_condition);
#else
    (void)pthread_cond_broadcast(&runtime->work_condition);
#endif
}

static void fviz_parallel_signal_done(FVizParallelRuntime* runtime)
{
    fviz_parallel_lock_state(runtime);
#if defined(_WIN32)
    WakeConditionVariable(&runtime->done_condition);
#else
    (void)pthread_cond_signal(&runtime->done_condition);
#endif
    fviz_parallel_unlock_state(runtime);
}

static void fviz_parallel_execute_chunks(FVizParallelRuntime* runtime)
{
    for (;;)
    {
        const FVizSize begin = (FVizSize)fviz_atomic_u64_fetch_add(
            &runtime->next, (uint64_t)runtime->grain_size);
        FVizSize end;
        if (begin >= runtime->end) break;
        end = begin + runtime->grain_size;
        if (end < begin || end > runtime->end) end = runtime->end;
        runtime->function(begin, end, runtime->user_data);
    }
}

#if defined(_WIN32)
static DWORD WINAPI fviz_parallel_worker_entry(LPVOID user_data)
#else
static void* fviz_parallel_worker_entry(void* user_data)
#endif
{
    FVizParallelThreadArg* argument = (FVizParallelThreadArg*)user_data;
    FVizParallelRuntime* runtime = argument->runtime;
    uint64_t observed_generation = 0u;
    g_fviz_parallel_inside_worker = 1;
    while (fviz_atomic_u32_load(&runtime->stopping) == 0u)
    {
        FVizBool participate;
        fviz_parallel_lock_state(runtime);
        while (observed_generation == runtime->generation)
        {
#if defined(_WIN32)
            (void)SleepConditionVariableCS(
                &runtime->work_condition, &runtime->state_mutex, INFINITE);
#else
            (void)pthread_cond_wait(&runtime->work_condition, &runtime->state_mutex);
#endif
        }
        observed_generation = runtime->generation;
        participate = argument->index < runtime->participating_workers ? FVIZ_TRUE : FVIZ_FALSE;
        fviz_parallel_unlock_state(runtime);
        if (participate == FVIZ_FALSE) continue;
        fviz_parallel_execute_chunks(runtime);
        if (fviz_atomic_u32_fetch_sub(&runtime->remaining, 1u) == 1u)
            fviz_parallel_signal_done(runtime);
    }
#if defined(_WIN32)
    return 0u;
#else
    return NULL;
#endif
}

#if defined(_WIN32)
static BOOL CALLBACK fviz_parallel_initialize_once(
    PINIT_ONCE once,
    PVOID parameter,
    PVOID* context)
#else
static void fviz_parallel_initialize_once(void)
#endif
{
    FVizParallelRuntime* runtime = &g_fviz_parallel_runtime;
    uint32_t desired_workers = fviz_parallel_hardware_thread_count();
    uint32_t i;
#if defined(_WIN32)
    (void)once;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&runtime->dispatch_mutex);
    InitializeCriticalSection(&runtime->state_mutex);
    InitializeConditionVariable(&runtime->work_condition);
    InitializeConditionVariable(&runtime->done_condition);
#else
    (void)pthread_mutex_init(&runtime->dispatch_mutex, NULL);
    (void)pthread_mutex_init(&runtime->state_mutex, NULL);
    (void)pthread_cond_init(&runtime->work_condition, NULL);
    (void)pthread_cond_init(&runtime->done_condition, NULL);
#endif
    if (desired_workers > 0u) --desired_workers;
    for (i = 0u; i < desired_workers; ++i)
    {
        runtime->thread_args[i].runtime = runtime;
        runtime->thread_args[i].index = i;
#if defined(_WIN32)
        runtime->threads[i] = CreateThread(
            NULL, 0u, fviz_parallel_worker_entry, &runtime->thread_args[i], 0u, NULL);
        if (runtime->threads[i] == NULL) break;
#else
        if (pthread_create(
                &runtime->threads[i], NULL, fviz_parallel_worker_entry, &runtime->thread_args[i]) != 0)
            break;
#endif
        ++runtime->worker_count;
    }
#if defined(_WIN32)
    return TRUE;
#endif
}

static FVizParallelRuntime* fviz_parallel_runtime(void)
{
#if defined(_WIN32)
    (void)InitOnceExecuteOnce(
        &g_fviz_parallel_once, fviz_parallel_initialize_once, NULL, NULL);
#else
    (void)pthread_once(&g_fviz_parallel_once, fviz_parallel_initialize_once);
#endif
    return &g_fviz_parallel_runtime;
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
    return fviz_parallel_runtime()->worker_count;
}

uint64_t fviz_parallel_dispatch_count(void)
{
    return fviz_atomic_u64_load(&fviz_parallel_runtime()->dispatch_count);
}

FVizResult fviz_parallel_for(
    FVizSize begin,
    FVizSize end,
    FVizSize grain_size,
    FVizParallelRangeFn function,
    void* user_data)
{
    FVizParallelRuntime* runtime;
    FVizSize item_count;
    FVizSize task_count;
    uint32_t thread_count;
    int previous_inside;
    if (function == NULL || end < begin)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "parallel range and function must be valid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    item_count = end - begin;
    if (item_count == 0u) return FVIZ_OK;
    if (grain_size == 0u) grain_size = FVIZ_PARALLEL_DEFAULT_GRAIN;
    if (g_fviz_parallel_inside_worker != 0)
    {
        function(begin, end, user_data);
        return FVIZ_OK;
    }
    runtime = fviz_parallel_runtime();
    task_count = item_count / grain_size + (item_count % grain_size != 0u ? 1u : 0u);
    thread_count = fviz_parallel_thread_limit();
    if ((FVizSize)thread_count > task_count) thread_count = (uint32_t)task_count;
    if (thread_count > runtime->worker_count + 1u) thread_count = runtime->worker_count + 1u;
    if (thread_count <= 1u)
    {
        function(begin, end, user_data);
        return FVIZ_OK;
    }

    fviz_parallel_lock_dispatch(runtime);
    runtime->function = function;
    runtime->user_data = user_data;
    runtime->end = end;
    runtime->grain_size = grain_size;
    (void)fviz_atomic_u64_exchange(&runtime->next, (uint64_t)begin);
    runtime->participating_workers = thread_count - 1u;
    (void)fviz_atomic_u32_exchange(&runtime->remaining, runtime->participating_workers);
    (void)fviz_atomic_u64_fetch_add(&runtime->dispatch_count, 1u);
    fviz_parallel_lock_state(runtime);
    ++runtime->generation;
    fviz_parallel_broadcast_work(runtime);
    fviz_parallel_unlock_state(runtime);

    previous_inside = g_fviz_parallel_inside_worker;
    g_fviz_parallel_inside_worker = 1;
    fviz_parallel_execute_chunks(runtime);
    g_fviz_parallel_inside_worker = previous_inside;

    fviz_parallel_lock_state(runtime);
    while (fviz_atomic_u32_load(&runtime->remaining) != 0u)
    {
#if defined(_WIN32)
        (void)SleepConditionVariableCS(
            &runtime->done_condition, &runtime->state_mutex, INFINITE);
#else
        (void)pthread_cond_wait(&runtime->done_condition, &runtime->state_mutex);
#endif
    }
    fviz_parallel_unlock_state(runtime);
    fviz_parallel_unlock_dispatch(runtime);
    return FVIZ_OK;
}
