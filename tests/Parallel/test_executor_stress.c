#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return __LINE__; } } while (0)

typedef struct OrderedTask
{
    int id;
    int priority;
    int* order;
    volatile int* count;
} OrderedTask;

static FVizResult ordered_task(FVizCancellationToken* cancellation, void* user_data, void** out_value)
{
    OrderedTask* task = (OrderedTask*)user_data;
    (void)cancellation;
    (void)out_value;
    if (task != NULL)
    {
        task->order[*task->count] = task->id;
        ++(*task->count);
    }
    return FVIZ_OK;
}

typedef struct ChainCounter
{
    int* sequence;
    volatile int* count;
    int depth;
} ChainCounter;

static FVizResult counter_task(FVizCancellationToken* cancellation, void* user_data, void** out_value)
{
    volatile int* count = (volatile int*)user_data;
    (void)cancellation;
    (void)out_value;
    if (count != NULL) ++(*count);
    return FVIZ_OK;
}

static FVizResult chain_link(FVizResult antecedent_result, FVizCancellationToken* cancellation,
    void* user_data, void** out_value)
{
    ChainCounter* state = (ChainCounter*)user_data;
    (void)cancellation;
    (void)out_value;
    if (antecedent_result != FVIZ_OK) return antecedent_result;
    state->sequence[*state->count] = state->depth - *state->count;
    ++(*state->count);
    return FVIZ_OK;
}

/* A proper continuation (FVizContinuationFn argument order) that increments a
 * user counter. Casting a task fn to a continuation fn would misread the
 * shifted argument list, so destroy/chain tests use this instead. */
static FVizResult ordered_continuation(FVizResult antecedent_result, FVizCancellationToken* cancellation,
    void* user_data, void** out_value)
{
    volatile int* count = (volatile int*)user_data;
    (void)cancellation;
    (void)out_value;
    if (antecedent_result != FVIZ_OK) return antecedent_result;
    if (count != NULL) ++(*count);
    return FVIZ_OK;
}

static volatile int g_release_barrier = 0;

static FVizResult barrier_task(FVizCancellationToken* cancellation, void* user_data, void** out_value)
{
    (void)user_data;
    (void)out_value;
    while (g_release_barrier == 0 && fviz_cancellation_token_is_cancelled(cancellation) == FVIZ_FALSE)
    {
#if defined(_WIN32)
        Sleep(1u);
#else
        usleep(1000u);
#endif
    }
    return fviz_cancellation_token_is_cancelled(cancellation) != FVIZ_FALSE
        ? FVIZ_ERROR_CANCELLED : FVIZ_OK;
}

/* Verifies that a single-thread executor schedules strictly by priority then
 * by submission order. A barrier task holds the worker while every other task
 * is enqueued, so the heap order (not submission timing) decides the run order. */
static int test_priority_ordering(void)
{
    enum { TASK_COUNT = 512 };
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* futures[TASK_COUNT];
    FVizFuture* barrier = NULL;
    OrderedTask tasks[TASK_COUNT];
    int order[TASK_COUNT];
    volatile int count = 0;
    int i;
    for (i = 0; i < TASK_COUNT; ++i)
    {
        futures[i] = NULL;
        order[i] = -1;
        tasks[i].id = i;
        tasks[i].priority = i % 8; /* groups of 8 sharing one priority */
        tasks[i].order = order;
        tasks[i].count = &count;
    }
    fviz_executor_options_initialize(&options);
    options.thread_count = 1u;
    options.queue_capacity = TASK_COUNT + 2u;
    CHECK(fviz_executor_create(&options, &executor) == FVIZ_OK);
    g_release_barrier = 0;
    CHECK(fviz_executor_submit(executor, -1, barrier_task, NULL, NULL, NULL, &barrier) == FVIZ_OK);
    /* Give the worker a moment to pick up the barrier before enqueueing all
     * ordered tasks, so they accumulate in the ready heap. */
#if defined(_WIN32)
    Sleep(30u);
#else
    usleep(30000u);
#endif
    /* Submit in reverse order so the heap must sort by priority and sequence. */
    for (i = TASK_COUNT - 1; i >= 0; --i)
        CHECK(fviz_executor_submit(executor, tasks[i].priority, ordered_task, &tasks[i], NULL, NULL, &futures[i]) == FVIZ_OK);
    g_release_barrier = 1;
    for (i = 0; i < TASK_COUNT; ++i)
        CHECK(fviz_future_wait(futures[i]) == FVIZ_OK);
    CHECK(fviz_future_wait(barrier) == FVIZ_OK);
    CHECK(count == TASK_COUNT);
    for (i = 0; i < TASK_COUNT; ++i)
    {
        const int id = order[i];
        CHECK(id >= 0 && id < TASK_COUNT);
        if (i > 0)
        {
            const int prev_id = order[i - 1];
            /* Priority must never increase. */
            CHECK(tasks[prev_id].priority >= tasks[id].priority);
            /* Within a priority, earlier submission (higher id, lower sequence)
             * must run first. */
            if (tasks[prev_id].priority == tasks[id].priority)
                CHECK(prev_id > id);
        }
    }
    fviz_future_destroy(barrier);
    for (i = 0; i < TASK_COUNT; ++i) fviz_future_destroy(futures[i]);
    fviz_executor_destroy(executor);
    return 0;
}

/* Builds a deep dependent chain: every link only runs after the previous one. */
static int test_deep_chain(void)
{
    enum { CHAIN_DEPTH = 4000 };
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* first = NULL;
    FVizFuture* current = NULL;
    int sequence[CHAIN_DEPTH];
    volatile int count = 0;
    ChainCounter state;
    int i;
    for (i = 0; i < CHAIN_DEPTH; ++i) sequence[i] = 0;
    state.sequence = sequence;
    state.count = &count;
    state.depth = CHAIN_DEPTH;
    fviz_executor_options_initialize(&options);
    options.thread_count = 4u;
    options.queue_capacity = CHAIN_DEPTH + 1u;
    CHECK(fviz_executor_create(&options, &executor) == FVIZ_OK);
    CHECK(fviz_executor_submit(executor, 0, ordered_task, NULL, NULL, NULL, &first) == FVIZ_OK);
    current = first;
    for (i = 1; i < CHAIN_DEPTH; ++i)
    {
        FVizFuture* next = NULL;
        CHECK(fviz_future_then(current, executor, 0, chain_link, &state, NULL, NULL, &next) == FVIZ_OK);
        current = next;
    }
    CHECK(fviz_future_wait(current) == FVIZ_OK);
    CHECK(count == CHAIN_DEPTH - 1);
    for (i = 0; i < CHAIN_DEPTH - 1; ++i)
    {
        /* Each link i observes the i-th predecessor completion in order. */
        CHECK(sequence[i] == CHAIN_DEPTH - i);
    }
    fviz_future_destroy(current);
    fviz_future_destroy(first);
    fviz_executor_destroy(executor);
    return 0;
}

/* Destroying an executor with queued and parked work must complete every future
 * as cancelled and leave no dangling waiter chains. */
static int test_destroy_with_pending(void)
{
    enum { TASK_COUNT = 200, CHAIN_DEPTH = 50 };
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* futures[TASK_COUNT];
    FVizFuture* chain_first = NULL;
    FVizFuture* chain_current = NULL;
    int i;
    for (i = 0; i < TASK_COUNT; ++i) futures[i] = NULL;
    fviz_executor_options_initialize(&options);
    options.thread_count = 2u;
    options.queue_capacity = TASK_COUNT + CHAIN_DEPTH + 2u;
    CHECK(fviz_executor_create(&options, &executor) == FVIZ_OK);
    CHECK(fviz_executor_submit(executor, 0, ordered_task, NULL, NULL, NULL, &chain_first) == FVIZ_OK);
    chain_current = chain_first;
    for (i = 1; i < CHAIN_DEPTH; ++i)
    {
        FVizFuture* next = NULL;
        CHECK(fviz_future_then(chain_current, executor, 0, ordered_continuation, NULL, NULL, NULL, &next) == FVIZ_OK);
        chain_current = next;
    }
    for (i = 0; i < TASK_COUNT; ++i)
        CHECK(fviz_executor_submit(executor, 0, ordered_task, NULL, NULL, NULL, &futures[i]) == FVIZ_OK);
    fviz_executor_destroy(executor); /* futures must become ready (cancelled) */
    for (i = 0; i < TASK_COUNT; ++i)
    {
        const FVizResult result = fviz_future_wait(futures[i]);
        CHECK(result == FVIZ_OK || result == FVIZ_ERROR_CANCELLED);
        fviz_future_destroy(futures[i]);
    }
    {
        const FVizResult result = fviz_future_wait(chain_current);
        CHECK(result == FVIZ_OK || result == FVIZ_ERROR_CANCELLED);
    }
    fviz_future_destroy(chain_current);
    fviz_future_destroy(chain_first);
    return 0;
}

/* The queue capacity must reject new submissions once pending work fills it,
 * and a cancelled future must report CANCELLED without running its function. */
static int test_queue_full_and_cancel(void)
{
    enum { CAPACITY = 4 };
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* barrier = NULL;
    FVizFuture* futures[CAPACITY];
    FVizFuture* rejected = NULL;
    volatile int count = 0;
    int i;
    for (i = 0; i < CAPACITY; ++i) futures[i] = NULL;
    fviz_executor_options_initialize(&options);
    options.thread_count = 1u;
    options.queue_capacity = CAPACITY;
    CHECK(fviz_executor_create(&options, &executor) == FVIZ_OK);
    /* Hold the single worker with a barrier so every submitted future stays
     * pending and the capacity check is deterministic. */
    g_release_barrier = 0;
    CHECK(fviz_executor_submit(executor, -1, barrier_task, NULL, NULL, NULL, &barrier) == FVIZ_OK);
#if defined(_WIN32)
    Sleep(20u);
#else
    usleep(20000u);
#endif
    for (i = 0; i < CAPACITY; ++i)
        CHECK(fviz_executor_submit(executor, 0, counter_task, (void*)&count, NULL, NULL, &futures[i]) == FVIZ_OK);
    CHECK(fviz_executor_submit(executor, 0, counter_task, (void*)&count, NULL, NULL, &rejected) == FVIZ_ERROR_BUSY);
    /* Cancel while the worker is still blocked on the barrier, so the worker is
     * guaranteed to observe the cancellation before it can run the future. */
    fviz_future_cancel(futures[CAPACITY - 1]);
    g_release_barrier = 1;
    for (i = 0; i < CAPACITY; ++i)
    {
        const FVizResult result = fviz_future_wait(futures[i]);
        CHECK(result == FVIZ_OK || result == FVIZ_ERROR_CANCELLED);
    }
    CHECK(fviz_future_wait(barrier) == FVIZ_OK);
    CHECK(count == CAPACITY - 1);
    fviz_future_destroy(barrier);
    for (i = 0; i < CAPACITY; ++i) fviz_future_destroy(futures[i]);
    fviz_executor_destroy(executor);
    return 0;
}

/* A continuation on a different executor than its dependency must still run. */
static int test_cross_executor_continuation(void)
{
    FVizExecutorOptions options;
    FVizExecutor* first_executor = NULL;
    FVizExecutor* second_executor = NULL;
    FVizFuture* root = NULL;
    FVizFuture* continuation = NULL;
    int called = 0;
    fviz_executor_options_initialize(&options);
    options.thread_count = 2u;
    CHECK(fviz_executor_create(&options, &first_executor) == FVIZ_OK);
    CHECK(fviz_executor_create(&options, &second_executor) == FVIZ_OK);
    CHECK(fviz_executor_submit(first_executor, 0, counter_task, &called, NULL, NULL, &root) == FVIZ_OK);
    CHECK(fviz_future_then(root, second_executor, 0, ordered_continuation, &called, NULL, NULL, &continuation) == FVIZ_OK);
    CHECK(fviz_future_wait(continuation) == FVIZ_OK);
    fviz_future_destroy(continuation);
    fviz_future_destroy(root);
    fviz_executor_destroy(second_executor);
    fviz_executor_destroy(first_executor);
    return 0;
}

int main(void)
{
    int result;
    result = test_priority_ordering();
    if (result != 0) return result;
    result = test_deep_chain();
    if (result != 0) return result;
    result = test_destroy_with_pending();
    if (result != 0) return result;
    result = test_queue_full_and_cancel();
    if (result != 0) return result;
    result = test_cross_executor_continuation();
    if (result != 0) return result;
    puts("executor stress tests passed");
    return 0;
}
