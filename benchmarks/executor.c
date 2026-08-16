#include <FViz/FViz.h>

#include <stdio.h>
#include <time.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

static double benchmark_wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

/* A near-no-op task that touches its counter through an atomic increment so the
 * optimizer cannot hoist or eliminate the work across benchmark iterations. */
static volatile long g_executor_counter = 0L;

static FVizResult noop_task(FVizCancellationToken* cancellation, void* user_data, void** out_value)
{
    (void)user_data;
    (void)out_value;
    if (fviz_cancellation_token_is_cancelled(cancellation) == FVIZ_FALSE) ++g_executor_counter;
    return FVIZ_OK;
}

static FVizResult continuation_task(
    FVizResult antecedent_result, FVizCancellationToken* cancellation,
    void* user_data, void** out_value)
{
    (void)user_data;
    (void)out_value;
    if (antecedent_result == FVIZ_OK &&
        fviz_cancellation_token_is_cancelled(cancellation) == FVIZ_FALSE) ++g_executor_counter;
    return antecedent_result;
}

/* Submits task_count independent tasks and waits for each. Reports tasks/sec. */
static int run_batch_throughput(uint32_t thread_count, FVizSize task_count)
{
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture** futures = NULL;
    long counter;
    FVizSize i;
    double started;
    double finished;
    double seconds;

    fviz_executor_options_initialize(&options);
    options.thread_count = thread_count;
    options.queue_capacity = task_count;
    if (fviz_executor_create(&options, &executor) != FVIZ_OK) return 2;

    futures = (FVizFuture**)fviz_alloc(task_count * sizeof(*futures));
    if (futures == NULL) { fviz_executor_destroy(executor); return 3; }
    for (i = 0u; i < task_count; ++i) futures[i] = NULL;

    g_executor_counter = 0L;
    counter = 0L;
    started = benchmark_wall_seconds();
    for (i = 0u; i < task_count; ++i)
    {
        if (fviz_executor_submit(executor, 0, noop_task, NULL, NULL, NULL, &futures[i]) != FVIZ_OK)
        { for (; i > 0u; --i) fviz_future_destroy(futures[i - 1u]); fviz_free(futures); fviz_executor_destroy(executor); return 4; }
    }
    for (i = 0u; i < task_count; ++i)
    {
        if (fviz_future_wait(futures[i]) != FVIZ_OK)
        { for (; i < task_count; ++i) if (futures[i]) fviz_future_destroy(futures[i]); fviz_free(futures); fviz_executor_destroy(executor); return 5; }
    }
    finished = benchmark_wall_seconds();
    seconds = finished - started;

    for (i = 0u; i < task_count; ++i) fviz_future_destroy(futures[i]);
    fviz_free(futures);
    fviz_executor_destroy(executor);

    printf("%u,%llu,%.6f,%.0f,%ld\n",
        thread_count,
        (unsigned long long)task_count,
        seconds,
        seconds > 0.0 ? (double)task_count / seconds : 0.0,
        counter);
    return 0;
}

/* A chain of dependent continuations is the worst case for the old pop scan:
 * every chain node must wait for its predecessor. Reports chains/s. */
static int run_chain_throughput(uint32_t thread_count, FVizSize chain_length)
{
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* first = NULL;
    FVizFuture* current = NULL;
    long counter;
    FVizSize i;
    double started;
    double finished;
    double seconds;

    fviz_executor_options_initialize(&options);
    options.thread_count = thread_count;
    options.queue_capacity = chain_length + 1u;
    if (fviz_executor_create(&options, &executor) != FVIZ_OK) return 2;

    g_executor_counter = 0L;
    counter = 0L;
    started = benchmark_wall_seconds();
    if (fviz_executor_submit(executor, 0, noop_task, NULL, NULL, NULL, &first) != FVIZ_OK)
    { fviz_executor_destroy(executor); return 3; }
    current = first;
    for (i = 1u; i < chain_length; ++i)
    {
        FVizFuture* next = NULL;
        if (fviz_future_then(current, executor, 0, continuation_task, NULL, NULL, NULL, &next) != FVIZ_OK)
        { fviz_future_destroy(first); fviz_executor_destroy(executor); return 4; }
        current = next;
    }
    if (fviz_future_wait(current) != FVIZ_OK)
    { fviz_future_destroy(first); fviz_executor_destroy(executor); return 5; }
    finished = benchmark_wall_seconds();
    seconds = finished - started;

    fviz_future_destroy(current);
    fviz_future_destroy(first);
    fviz_executor_destroy(executor);

    printf("%u,%llu,%.6f,%.0f,%ld\n",
        thread_count,
        (unsigned long long)chain_length,
        seconds,
        seconds > 0.0 ? (double)chain_length / seconds : 0.0,
        counter);
    return 0;
}

/* Submits task_count tasks, cancels every other one, then drains. */
static int run_cancellation_throughput(uint32_t thread_count, FVizSize task_count)
{
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture** futures = NULL;
    long counter;
    FVizSize i;
    double started;
    double finished;
    double seconds;
    FVizSize cancelled = 0u;

    fviz_executor_options_initialize(&options);
    options.thread_count = thread_count;
    options.queue_capacity = task_count;
    if (fviz_executor_create(&options, &executor) != FVIZ_OK) return 2;

    futures = (FVizFuture**)fviz_alloc(task_count * sizeof(*futures));
    if (futures == NULL) { fviz_executor_destroy(executor); return 3; }
    for (i = 0u; i < task_count; ++i) futures[i] = NULL;

    g_executor_counter = 0L;
    counter = 0L;
    started = benchmark_wall_seconds();
    for (i = 0u; i < task_count; ++i)
    {
        if (fviz_executor_submit(executor, 0, noop_task, NULL, NULL, NULL, &futures[i]) != FVIZ_OK)
        { for (; i > 0u; --i) fviz_future_destroy(futures[i - 1u]); fviz_free(futures); fviz_executor_destroy(executor); return 4; }
        if ((i & 1u) == 0u) fviz_future_cancel(futures[i]);
    }
    for (i = 0u; i < task_count; ++i)
    {
        if (fviz_future_wait(futures[i]) == FVIZ_ERROR_CANCELLED) ++cancelled;
    }
    finished = benchmark_wall_seconds();
    seconds = finished - started;

    for (i = 0u; i < task_count; ++i) fviz_future_destroy(futures[i]);
    fviz_free(futures);
    fviz_executor_destroy(executor);

    printf("%u,%llu,%.6f,%.0f,%llu\n",
        thread_count,
        (unsigned long long)task_count,
        seconds,
        seconds > 0.0 ? (double)task_count / seconds : 0.0,
        (unsigned long long)cancelled);
    return 0;
}

int main(void)
{
    const uint32_t thread_counts[3] = {1u, 4u, 0u};
    const FVizSize task_count = 200000u;
    const FVizSize chain_length = 50000u;
    uint32_t i;
    puts("mode,batch_or_threads,count,seconds,tasks_per_second,counter");
    for (i = 0u; i < 3u; ++i)
    {
        const uint32_t threads = thread_counts[i] != 0u
            ? thread_counts[i] : fviz_parallel_hardware_thread_count();
        printf("batch,%u,%llu,0,0,0\n", threads, (unsigned long long)task_count);
        if (run_batch_throughput(threads, task_count) != 0) return 1;
        printf("chain,%u,%llu,0,0,0\n", threads, (unsigned long long)chain_length);
        if (run_chain_throughput(threads, chain_length) != 0) return 2;
        printf("cancel,%u,%llu,0,0,0\n", threads, (unsigned long long)task_count);
        if (run_cancellation_throughput(threads, task_count) != 0) return 3;
    }
    return 0;
}
