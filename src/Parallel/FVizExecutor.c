#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#endif

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Parallel/FVizExecutor.h>

#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_EXECUTOR_MAX_THREADS 64u
#define FVIZ_EXECUTOR_DEFAULT_QUEUE_CAPACITY ((FVizSize)1024u)

typedef struct FVizExecutorWorker
{
    struct FVizExecutor* executor;
    FVizFuture* current;
} FVizExecutorWorker;

struct FVizFuture
{
#if defined(_WIN32)
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE condition;
#else
    pthread_mutex_t mutex;
    pthread_cond_t condition;
#endif
    FVizCancellationToken* cancellation;
    FVizTaskFn function;
    FVizTaskContextFn context_function;
    FVizContinuationFn continuation_function;
    struct FVizExecutor* executor;
    struct FVizFuture* dependency;
    uint32_t dependent_count;
    void* user_data;
    FVizTaskDataDestroyFn user_data_destroy;
    void* value;
    FVizTaskValueDestroyFn value_destroy;
    FVizResult result;
    int priority;
    uint64_t sequence;
    double progress;
    FVizFutureProgressFn progress_callback;
    void* progress_user_data;
    FVizAtomicU32 ready;
    uint64_t enqueue_ns;
    /* Futures that are parked waiting for this future to complete form a
     * singly-linked chain through waiter_next. */
    struct FVizFuture* waiting_head;
    struct FVizFuture* waiter_next;
    /* Executor-owned registry of parked (not yet runnable) futures. */
    struct FVizFuture* pending_prev;
    struct FVizFuture* pending_next;
};

struct FVizTaskContext
{
    FVizFuture* future;
};

struct FVizExecutor
{
#if defined(_WIN32)
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE work_condition;
    HANDLE threads[FVIZ_EXECUTOR_MAX_THREADS];
#else
    pthread_mutex_t mutex;
    pthread_cond_t work_condition;
    pthread_t threads[FVIZ_EXECUTOR_MAX_THREADS];
#endif
    FVizExecutorWorker workers[FVIZ_EXECUTOR_MAX_THREADS];
    FVizFuture** queue;
    FVizSize queue_count;
    FVizSize queue_capacity;
    /* Number of submitted-but-not-running futures (ready heap + parked). */
    FVizSize pending_count;
    /* Registry of parked futures so executor destruction can complete them. */
    FVizFuture* pending_head;
    FVizFuture* pending_tail;
    uint32_t thread_count;
    uint32_t running;
    uint64_t next_sequence;
    uint64_t submitted;
    uint64_t completed;
    uint64_t cancelled;
    uint64_t failed;
    uint64_t queue_wait_ns;
    uint64_t execution_ns;
    FVizBool stopping;
};

static uint64_t fviz_executor_now_ns(void)
{
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return frequency.QuadPart > 0
        ? (uint64_t)((counter.QuadPart * UINT64_C(1000000000)) / frequency.QuadPart) : 0u;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0u;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
#endif
}

static void fviz_executor_lock(FVizExecutor* executor)
{
#if defined(_WIN32)
    EnterCriticalSection(&executor->mutex);
#else
    (void)pthread_mutex_lock(&executor->mutex);
#endif
}
static void fviz_executor_unlock(FVizExecutor* executor)
{
#if defined(_WIN32)
    LeaveCriticalSection(&executor->mutex);
#else
    (void)pthread_mutex_unlock(&executor->mutex);
#endif
}
static void fviz_future_lock(FVizFuture* future)
{
#if defined(_WIN32)
    EnterCriticalSection(&future->mutex);
#else
    (void)pthread_mutex_lock(&future->mutex);
#endif
}
static void fviz_future_unlock(FVizFuture* future)
{
#if defined(_WIN32)
    LeaveCriticalSection(&future->mutex);
#else
    (void)pthread_mutex_unlock(&future->mutex);
#endif
}

static void fviz_executor_wake_one(FVizExecutor* executor)
{
#if defined(_WIN32)
    WakeConditionVariable(&executor->work_condition);
#else
    (void)pthread_cond_signal(&executor->work_condition);
#endif
}

static void fviz_executor_wake_all(FVizExecutor* executor)
{
#if defined(_WIN32)
    WakeAllConditionVariable(&executor->work_condition);
#else
    (void)pthread_cond_broadcast(&executor->work_condition);
#endif
}

static void fviz_future_complete(FVizFuture* future, FVizResult result, void* value,
    FVizFuture** out_waiters)
{
    FVizFuture* waiters = NULL;
    fviz_future_lock(future);
    future->result = result;
    future->value = value;
    /* Detach the waiter chain under the same lock that publishes ready, so a
     * concurrent submit either observes the completed dependency and enqueues
     * directly, or links into this chain before the detach. The detached chain
     * is scheduled by the caller after releasing the future lock. */
    waiters = future->waiting_head;
    future->waiting_head = NULL;
    (void)fviz_atomic_u32_exchange(&future->ready, 1u);
#if defined(_WIN32)
    WakeAllConditionVariable(&future->condition);
#else
    (void)pthread_cond_broadcast(&future->condition);
#endif
    fviz_future_unlock(future);
    if (out_waiters != NULL) *out_waiters = waiters;
    if (future->executor != NULL) fviz_executor_wake_all(future->executor);
}

/* Appends a detached waiter chain to a collected list, keeping the tail cached
 * so repeated appends are O(1) per chain plus O(1) tail advance. */
static void fviz_executor_collect_waiters(
    FVizFuture** head, FVizFuture** tail, FVizFuture* waiters)
{
    FVizFuture* last;
    if (waiters == NULL) return;
    if (*tail != NULL) (*tail)->waiter_next = waiters;
    else *head = waiters;
    last = waiters;
    while (last->waiter_next != NULL) last = last->waiter_next;
    *tail = last;
}

static void fviz_future_release_dependency(FVizFuture* future)
{
    FVizFuture* dependency = future->dependency;
    if (dependency == NULL) return;
    future->dependency = NULL;
    fviz_future_lock(dependency);
    if (dependency->dependent_count != 0u) --dependency->dependent_count;
#if defined(_WIN32)
    WakeAllConditionVariable(&dependency->condition);
#else
    (void)pthread_cond_broadcast(&dependency->condition);
#endif
    fviz_future_unlock(dependency);
}

/* ---------------------------------------------------------------------------
 * Ready max-heap keyed on (priority desc, sequence asc). The heap contains
 * only runnable futures, so scheduling is O(log n) and never scans for a
 * dependency that is not ready yet.
 * ------------------------------------------------------------------------- */

static int fviz_future_schedules_first(FVizFuture* left, FVizFuture* right)
{
    if (left->priority != right->priority) return left->priority > right->priority ? 1 : 0;
    return left->sequence < right->sequence ? 1 : 0;
}

static void fviz_heap_swap(FVizFuture** heap, FVizSize left, FVizSize right)
{
    FVizFuture* temporary = heap[left];
    heap[left] = heap[right];
    heap[right] = temporary;
}

static void fviz_heap_sift_up(FVizFuture** heap, FVizSize index)
{
    while (index > 0u)
    {
        const FVizSize parent = (index - 1u) / 2u;
        if (!fviz_future_schedules_first(heap[index], heap[parent])) break;
        fviz_heap_swap(heap, index, parent);
        index = parent;
    }
}

static void fviz_heap_sift_down(FVizFuture** heap, FVizSize count, FVizSize index)
{
    for (;;)
    {
        const FVizSize left = index * 2u + 1u;
        const FVizSize right = left + 1u;
        FVizSize largest = index;
        if (left < count && fviz_future_schedules_first(heap[left], heap[largest])) largest = left;
        if (right < count && fviz_future_schedules_first(heap[right], heap[largest])) largest = right;
        if (largest == index) break;
        fviz_heap_swap(heap, index, largest);
        index = largest;
    }
}

static void fviz_heap_push(FVizExecutor* executor, FVizFuture* future)
{
    executor->queue[executor->queue_count] = future;
    fviz_heap_sift_up(executor->queue, executor->queue_count);
    ++executor->queue_count;
}

static FVizFuture* fviz_heap_pop(FVizExecutor* executor)
{
    FVizFuture* top;
    if (executor->queue_count == 0u) return NULL;
    top = executor->queue[0];
    --executor->queue_count;
    if (executor->queue_count > 0u)
    {
        executor->queue[0] = executor->queue[executor->queue_count];
        fviz_heap_sift_down(executor->queue, executor->queue_count, 0u);
    }
    executor->queue[executor->queue_count] = NULL;
    return top;
}

static void fviz_pending_link(FVizExecutor* executor, FVizFuture* future)
{
    future->pending_prev = executor->pending_tail;
    future->pending_next = NULL;
    if (executor->pending_tail != NULL) executor->pending_tail->pending_next = future;
    else executor->pending_head = future;
    executor->pending_tail = future;
}

static void fviz_pending_unlink(FVizExecutor* executor, FVizFuture* future)
{
    if (future->pending_prev != NULL) future->pending_prev->pending_next = future->pending_next;
    else executor->pending_head = future->pending_next;
    if (future->pending_next != NULL) future->pending_next->pending_prev = future->pending_prev;
    else executor->pending_tail = future->pending_prev;
    future->pending_prev = NULL;
    future->pending_next = NULL;
}

/* Schedules a chain of waiters detached from a completed future into their own
 * executors' ready heaps. The completed future is not touched, so a waiter
 * thread may destroy the completed future as soon as it observes ready. */
static void fviz_executor_promote_waiters(FVizFuture* waiters)
{
    while (waiters != NULL)
    {
        FVizFuture* next = waiters->waiter_next;
        waiters->waiter_next = NULL;
        if (waiters->executor != NULL)
        {
            fviz_executor_lock(waiters->executor);
            if (waiters->executor->stopping == FVIZ_FALSE)
            {
                fviz_pending_unlink(waiters->executor, waiters);
                fviz_heap_push(waiters->executor, waiters);
                fviz_executor_wake_one(waiters->executor);
            }
            /* If the waiter's executor is shutting down it stays in the parked
             * registry, which the destroy path completes as cancelled. */
            fviz_executor_unlock(waiters->executor);
        }
        waiters = next;
    }
}

static FVizFuture* fviz_executor_pop(FVizExecutor* executor)
{
    /* The ready heap only ever contains runnable tasks; dependency-blocked
     * continuations live in the parked registry until their dependency
     * completes, so scheduling does not rescan the queue. */
    return fviz_heap_pop(executor);
}

#if defined(_WIN32)
static DWORD WINAPI fviz_executor_worker_entry(LPVOID user_data)
#else
static void* fviz_executor_worker_entry(void* user_data)
#endif
{
    FVizExecutorWorker* worker = (FVizExecutorWorker*)user_data;
    FVizExecutor* executor = worker->executor;
    for (;;)
    {
        FVizFuture* future;
        FVizFuture* waiters = NULL;
        FVizResult result;
        void* value = NULL;
        fviz_executor_lock(executor);
        future = fviz_executor_pop(executor);
        while (future == NULL && executor->stopping == FVIZ_FALSE)
        {
#if defined(_WIN32)
            (void)SleepConditionVariableCS(&executor->work_condition, &executor->mutex, INFINITE);
#else
            (void)pthread_cond_wait(&executor->work_condition, &executor->mutex);
#endif
            future = fviz_executor_pop(executor);
        }
        if (future == NULL && executor->stopping != FVIZ_FALSE)
        {
            fviz_executor_unlock(executor);
            break;
        }
        worker->current = future;
        ++executor->running;
        if (executor->pending_count != 0u) --executor->pending_count;
        if (future->enqueue_ns != 0u)
        {
            const uint64_t now = fviz_executor_now_ns();
            if (now >= future->enqueue_ns) executor->queue_wait_ns += now - future->enqueue_ns;
        }
        fviz_executor_unlock(executor);
        {
            const uint64_t execution_begin_ns = fviz_executor_now_ns();
            uint64_t execution_end_ns = execution_begin_ns;
        if (fviz_cancellation_token_is_cancelled(future->cancellation) != FVIZ_FALSE)
            result = FVIZ_ERROR_CANCELLED;
        else if (future->continuation_function != NULL)
        {
            FVizResult antecedent_result;
            fviz_future_lock(future->dependency);
            antecedent_result = future->dependency->result;
            fviz_future_unlock(future->dependency);
            fviz_future_release_dependency(future);
            result = future->continuation_function(
                antecedent_result, future->cancellation, future->user_data, &value);
        }
        else if (future->context_function != NULL)
        {
            FVizTaskContext context;
            context.future = future;
            result = future->context_function(&context, future->user_data, &value);
        }
        else
            result = future->function(future->cancellation, future->user_data, &value);
        /* Release the dependency before invoking the user-data destroy callback.
         * A cancelled continuation skips the in-branch release above, so without
         * this ordering the antecedent would keep its dependent_count and a
         * destroy callback that frees antecedent futures (e.g. a pipeline chain
         * freeing its intermediate links) would block forever in
         * fviz_future_destroy. */
        fviz_future_release_dependency(future);
        if (future->user_data_destroy != NULL) future->user_data_destroy(future->user_data);
        future->user_data = NULL;
        if (result != FVIZ_OK && value != NULL && future->value_destroy != NULL)
        {
            future->value_destroy(value);
            value = NULL;
        }
        execution_end_ns = fviz_executor_now_ns();
        /* Publish completion and clear worker->current before publishing ready,
         * so a waiter thread that destroys the future cannot race with an
         * executor_destroy reading worker->current. */
        fviz_executor_lock(executor);
        worker->current = NULL;
        --executor->running;
        ++executor->completed;
        if (result == FVIZ_ERROR_CANCELLED) ++executor->cancelled;
        else if (result != FVIZ_OK) ++executor->failed;
        if (execution_end_ns >= execution_begin_ns)
            executor->execution_ns += execution_end_ns - execution_begin_ns;
        fviz_executor_unlock(executor);
        fviz_future_complete(future, result, value, &waiters);
        /* The completed future may now unlock dependents parked on other
         * executors; schedule them. The completed future must not be touched
         * here: a waiter thread may destroy it as soon as it observes ready. */
        fviz_executor_promote_waiters(waiters);
        }
    }
#if defined(_WIN32)
    return 0u;
#else
    return NULL;
#endif
}

void fviz_executor_options_initialize(FVizExecutorOptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
}

FVizResult fviz_executor_create(
    const FVizExecutorOptions* options, FVizExecutor** out_executor)
{
    FVizExecutorOptions defaults;
    FVizExecutor* executor;
    uint32_t thread_count;
    uint32_t i;
    if (out_executor == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_executor = NULL;
    if (options == NULL)
    {
        fviz_executor_options_initialize(&defaults);
        options = &defaults;
    }
    if (options->struct_size < sizeof(*options)) return FVIZ_ERROR_INVALID_ARGUMENT;
    thread_count = options->thread_count != 0u
        ? options->thread_count : fviz_parallel_thread_limit();
    if (thread_count == 0u) thread_count = fviz_parallel_hardware_thread_count();
    if (thread_count > FVIZ_EXECUTOR_MAX_THREADS) thread_count = FVIZ_EXECUTOR_MAX_THREADS;
    executor = (FVizExecutor*)fviz_alloc(sizeof(*executor));
    if (executor == NULL) return fviz_last_error_code();
    memset(executor, 0, sizeof(*executor));
    executor->queue_capacity = options->queue_capacity != 0u
        ? options->queue_capacity : FVIZ_EXECUTOR_DEFAULT_QUEUE_CAPACITY;
    if (executor->queue_capacity > (FVizSize)-1 / sizeof(*executor->queue))
    {
        fviz_free(executor);
        return FVIZ_ERROR_OVERFLOW;
    }
    executor->queue = (FVizFuture**)fviz_alloc(
        executor->queue_capacity * sizeof(*executor->queue));
    if (executor->queue == NULL)
    {
        fviz_free(executor);
        return fviz_last_error_code();
    }
#if defined(_WIN32)
    InitializeCriticalSection(&executor->mutex);
    InitializeConditionVariable(&executor->work_condition);
#else
    if (pthread_mutex_init(&executor->mutex, NULL) != 0 ||
        pthread_cond_init(&executor->work_condition, NULL) != 0)
    {
        fviz_free(executor->queue); fviz_free(executor);
        return FVIZ_ERROR_INTERNAL;
    }
#endif
    for (i = 0u; i < thread_count; ++i)
    {
        executor->workers[i].executor = executor;
#if defined(_WIN32)
        executor->threads[i] = CreateThread(NULL, 0u, fviz_executor_worker_entry,
            &executor->workers[i], 0u, NULL);
        if (executor->threads[i] == NULL) break;
#else
        if (pthread_create(&executor->threads[i], NULL, fviz_executor_worker_entry,
                &executor->workers[i]) != 0) break;
#endif
        ++executor->thread_count;
    }
    if (executor->thread_count == 0u)
    {
        fviz_executor_destroy(executor);
        return FVIZ_ERROR_INTERNAL;
    }
    *out_executor = executor;
    return FVIZ_OK;
}

void fviz_executor_destroy(FVizExecutor* executor)
{
    FVizFuture* waiters_head = NULL;
    FVizFuture* waiters_tail = NULL;
    uint32_t i;
    if (executor == NULL) return;
    /* Phase 1: signal shutdown, cancel running work, and drain the ready heap.
     * Running futures are completed by their own workers before the join. */
    fviz_executor_lock(executor);
    executor->stopping = FVIZ_TRUE;
    for (i = 0u; i < executor->thread_count; ++i)
        if (executor->workers[i].current != NULL)
            fviz_future_cancel(executor->workers[i].current);
    while (executor->queue_count != 0u)
    {
        FVizFuture* waiters = NULL;
        FVizFuture* future = fviz_heap_pop(executor);
        if (future->user_data_destroy != NULL) future->user_data_destroy(future->user_data);
        future->user_data = NULL;
        fviz_future_release_dependency(future);
        fviz_future_complete(future, FVIZ_ERROR_CANCELLED, NULL, &waiters);
        fviz_executor_collect_waiters(&waiters_head, &waiters_tail, waiters);
        ++executor->completed;
        ++executor->cancelled;
    }
    fviz_executor_wake_all(executor);
    fviz_executor_unlock(executor);
    for (i = 0u; i < executor->thread_count; ++i)
    {
#if defined(_WIN32)
        (void)WaitForSingleObject(executor->threads[i], INFINITE);
        (void)CloseHandle(executor->threads[i]);
#else
        (void)pthread_join(executor->threads[i], NULL);
#endif
    }
    /* Phase 2: complete every remaining parked future (cancelled). A parked
     * future is still linked into its dependency waiter chain; remove it so
     * a later dependency completion cannot schedule a destroyed future. */
    fviz_executor_lock(executor);
    {
        FVizFuture* future = executor->pending_head;
        while (future != NULL)
        {
            FVizFuture* next = future->pending_next;
            if (future->dependency != NULL)
            {
                fviz_future_lock(future->dependency);
                if (future->dependency->waiting_head == future)
                    future->dependency->waiting_head = future->waiter_next;
                else
                {
                    FVizFuture* cursor = future->dependency->waiting_head;
                    while (cursor != NULL && cursor->waiter_next != future)
                        cursor = cursor->waiter_next;
                    if (cursor != NULL) cursor->waiter_next = future->waiter_next;
                }
                fviz_future_unlock(future->dependency);
            }
            future->waiter_next = NULL;
            fviz_future_cancel(future);
            if (future->user_data_destroy != NULL) future->user_data_destroy(future->user_data);
            future->user_data = NULL;
            fviz_future_release_dependency(future);
            {
                FVizFuture* waiters = NULL;
                fviz_future_complete(future, FVIZ_ERROR_CANCELLED, NULL, &waiters);
                fviz_executor_collect_waiters(&waiters_head, &waiters_tail, waiters);
            }
            ++executor->completed;
            ++executor->cancelled;
            future = next;
        }
        executor->pending_head = NULL;
        executor->pending_tail = NULL;
    }
    fviz_executor_unlock(executor);
    /* Promote dependents detached from every future cancelled here. Dependents
     * parked on other executors observe the cancelled antecedent instead of
     * remaining parked forever; dependents parked on this executor were already
     * completed by the registry walk above, so promotion skips them. */
    fviz_executor_promote_waiters(waiters_head);
#if defined(_WIN32)
    DeleteCriticalSection(&executor->mutex);
#else
    (void)pthread_cond_destroy(&executor->work_condition);
    (void)pthread_mutex_destroy(&executor->mutex);
#endif
    fviz_free(executor->queue);
    fviz_free(executor);
}

static FVizResult fviz_executor_submit_internal(
    FVizExecutor* executor, int priority, FVizTaskFn function,
    FVizTaskContextFn context_function, FVizContinuationFn continuation_function,
    FVizFuture* dependency, void* user_data,
    FVizTaskDataDestroyFn user_data_destroy, FVizTaskValueDestroyFn value_destroy,
    FVizFuture** out_future)
{
    FVizFuture* future;
    FVizBool parked = FVIZ_FALSE;
    if (executor == NULL ||
        (function == NULL && context_function == NULL && continuation_function == NULL) ||
        out_future == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_future = NULL;
    future = (FVizFuture*)fviz_alloc(sizeof(*future));
    if (future == NULL) return fviz_last_error_code();
    memset(future, 0, sizeof(*future));
#if defined(_WIN32)
    InitializeCriticalSection(&future->mutex);
    InitializeConditionVariable(&future->condition);
#else
    if (pthread_mutex_init(&future->mutex, NULL) != 0 ||
        pthread_cond_init(&future->condition, NULL) != 0)
    { fviz_free(future); return FVIZ_ERROR_INTERNAL; }
#endif
    future->function = function;
    future->context_function = context_function;
    future->continuation_function = continuation_function;
    future->executor = executor;
    future->dependency = dependency;
    future->user_data = user_data;
    future->user_data_destroy = user_data_destroy;
    future->value_destroy = value_destroy;
    future->priority = priority;
    future->result = FVIZ_ERROR_BUSY;
    if (fviz_cancellation_token_create(&future->cancellation) != FVIZ_OK)
    { fviz_future_destroy(future); return fviz_last_error_code(); }
    fviz_executor_lock(executor);
    if (executor->stopping != FVIZ_FALSE ||
        executor->pending_count >= executor->queue_capacity)
    {
        fviz_executor_unlock(executor);
        future->user_data = NULL;
        future->user_data_destroy = NULL;
        fviz_future_release_dependency(future);
        fviz_future_complete(future, FVIZ_ERROR_CANCELLED, NULL, NULL);
        fviz_future_destroy(future);
        fviz_internal_set_error(FVIZ_ERROR_BUSY, "executor queue is full or shutting down");
        return FVIZ_ERROR_BUSY;
    }
    future->sequence = executor->next_sequence++;
    future->enqueue_ns = fviz_executor_now_ns();
    ++executor->submitted;
    ++executor->pending_count;
    if (dependency != NULL)
    {
        /* The park/push decision is taken while holding the dependency lock so
         * it is atomic with fviz_future_complete + promote_dependents: a
         * continuation is either linked into the waiter chain before the
         * dependency completes, or observes the completed dependency and goes
         * straight to the ready heap. Nested lock order is executor ->
         * dependency, matching executor destruction. */
        fviz_future_lock(dependency);
        ++dependency->dependent_count;
        if (fviz_atomic_u32_load(&dependency->ready) != 0u)
        {
            fviz_future_unlock(dependency);
            fviz_heap_push(executor, future);
        }
        else
        {
            future->waiter_next = dependency->waiting_head;
            dependency->waiting_head = future;
            parked = FVIZ_TRUE;
            fviz_future_unlock(dependency);
            fviz_pending_link(executor, future);
        }
    }
    else
    {
        fviz_heap_push(executor, future);
    }
    fviz_executor_wake_one(executor);
    fviz_executor_unlock(executor);
    *out_future = future;
    return FVIZ_OK;
}

FVizResult fviz_executor_submit(
    FVizExecutor* executor, int priority, FVizTaskFn function, void* user_data,
    FVizTaskDataDestroyFn user_data_destroy, FVizTaskValueDestroyFn value_destroy,
    FVizFuture** out_future)
{
    return fviz_executor_submit_internal(executor, priority, function, NULL, NULL,
        NULL, user_data, user_data_destroy, value_destroy, out_future);
}

FVizResult fviz_executor_submit_context(
    FVizExecutor* executor, int priority, FVizTaskContextFn function, void* user_data,
    FVizTaskDataDestroyFn user_data_destroy, FVizTaskValueDestroyFn value_destroy,
    FVizFuture** out_future)
{
    return fviz_executor_submit_internal(executor, priority, NULL, function, NULL,
        NULL, user_data, user_data_destroy, value_destroy, out_future);
}

FVizResult fviz_future_then(
    FVizFuture* antecedent, FVizExecutor* executor, int priority,
    FVizContinuationFn function, void* user_data,
    FVizTaskDataDestroyFn user_data_destroy, FVizTaskValueDestroyFn value_destroy,
    FVizFuture** out_future)
{
    if (antecedent == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_executor_submit_internal(executor, priority, NULL, NULL, function,
        antecedent, user_data, user_data_destroy, value_destroy, out_future);
}

void fviz_executor_get_statistics(
    const FVizExecutor* executor, FVizExecutorStatistics* out_statistics)
{
    FVizExecutor* mutable_executor = (FVizExecutor*)executor;
    if (out_statistics == NULL) return;
    memset(out_statistics, 0, sizeof(*out_statistics));
    out_statistics->struct_size = (uint32_t)sizeof(*out_statistics);
    if (mutable_executor == NULL) return;
    fviz_executor_lock(mutable_executor);
    out_statistics->submitted = executor->submitted;
    out_statistics->completed = executor->completed;
    out_statistics->cancelled = executor->cancelled;
    out_statistics->failed = executor->failed;
    out_statistics->queued = executor->pending_count;
    out_statistics->running = executor->running;
    out_statistics->queue_wait_ns = executor->queue_wait_ns;
    out_statistics->execution_ns = executor->execution_ns;
    fviz_executor_unlock(mutable_executor);
}

void fviz_future_cancel(FVizFuture* future)
{
    if (future != NULL) fviz_cancellation_token_cancel(future->cancellation);
}

FVizCancellationToken* fviz_task_context_cancellation(FVizTaskContext* context)
{
    return context != NULL && context->future != NULL
        ? context->future->cancellation : NULL;
}

FVizResult fviz_task_context_report_progress(FVizTaskContext* context, double progress)
{
    FVizFuture* future;
    FVizFutureProgressFn callback;
    void* user_data;
    if (context == NULL || context->future == NULL || !isfinite(progress) ||
        progress < 0.0 || progress > 1.0) return FVIZ_ERROR_INVALID_ARGUMENT;
    future = context->future;
    fviz_future_lock(future);
    if (progress < future->progress)
    {
        fviz_future_unlock(future);
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "future progress must be monotonic");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    future->progress = progress;
    callback = future->progress_callback;
    user_data = future->progress_user_data;
    fviz_future_unlock(future);
    if (callback != NULL) callback(future, progress, user_data);
    return FVIZ_OK;
}

double fviz_future_progress(const FVizFuture* future)
{
    double progress;
    FVizFuture* mutable_future = (FVizFuture*)future;
    if (mutable_future == NULL) return 0.0;
    fviz_future_lock(mutable_future);
    progress = future->progress;
    fviz_future_unlock(mutable_future);
    return progress;
}

void fviz_future_set_progress_callback(
    FVizFuture* future, FVizFutureProgressFn callback, void* user_data)
{
    if (future == NULL) return;
    fviz_future_lock(future);
    future->progress_callback = callback;
    future->progress_user_data = user_data;
    fviz_future_unlock(future);
}

FVizBool fviz_future_ready(const FVizFuture* future)
{
    return future != NULL && fviz_atomic_u32_load(&future->ready) != 0u
        ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizResult fviz_future_wait(FVizFuture* future)
{
    if (future == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_future_lock(future);
    while (fviz_atomic_u32_load(&future->ready) == 0u)
    {
#if defined(_WIN32)
        (void)SleepConditionVariableCS(&future->condition, &future->mutex, INFINITE);
#else
        (void)pthread_cond_wait(&future->condition, &future->mutex);
#endif
    }
    {
        const FVizResult result = future->result;
        fviz_future_unlock(future);
        return result;
    }
}

FVizResult fviz_future_take_value(FVizFuture* future, void** out_value)
{
    FVizResult result;
    if (future == NULL || out_value == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_value = NULL;
    result = fviz_future_wait(future);
    if (result != FVIZ_OK) return result;
    fviz_future_lock(future);
    if (future->value == NULL)
    {
        fviz_future_unlock(future);
        return FVIZ_ERROR_NOT_FOUND;
    }
    *out_value = future->value;
    future->value = NULL;
    fviz_future_unlock(future);
    return FVIZ_OK;
}

void fviz_future_destroy(FVizFuture* future)
{
    if (future == NULL) return;
    if (future->cancellation != NULL)
    {
        fviz_future_cancel(future);
        if (fviz_future_ready(future) == FVIZ_FALSE) (void)fviz_future_wait(future);
    }
    fviz_future_lock(future);
    while (future->dependent_count != 0u)
    {
#if defined(_WIN32)
        (void)SleepConditionVariableCS(&future->condition, &future->mutex, INFINITE);
#else
        (void)pthread_cond_wait(&future->condition, &future->mutex);
#endif
    }
    fviz_future_unlock(future);
    if (future->value != NULL && future->value_destroy != NULL)
        future->value_destroy(future->value);
    fviz_cancellation_token_destroy(future->cancellation);
#if defined(_WIN32)
    DeleteCriticalSection(&future->mutex);
#else
    (void)pthread_cond_destroy(&future->condition);
    (void)pthread_mutex_destroy(&future->mutex);
#endif
    fviz_free(future);
}
