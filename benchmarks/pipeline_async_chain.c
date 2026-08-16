#include <FViz/FViz.h>

#include <stdio.h>
#include <time.h>

static double benchmark_wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

typedef struct StageCounter
{
    long* calls;
    int id;
} StageCounter;

static FVizResult stage_process(
    FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request, void* user_data)
{
    StageCounter* counter = (StageCounter*)user_data;
    FVizPolyData* output = NULL;
    FVizResult result;
    (void)algorithm;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    ++(*counter->calls);
    if (fviz_poly_data_create(&output) != FVIZ_OK) return fviz_last_error_code();
    result = fviz_algorithm_set_output_data(
        algorithm, request->requested_output_port, (FVizDataObject*)output);
    fviz_release(output);
    return result;
}

/* Runs a pipeline of independent source stages as one continuation chain,
 * repeating to measure average drain time through the executor pool. */
static int run_chain_benchmark(uint32_t thread_count, uint32_t stage_count, uint32_t iterations)
{
    FVizAlgorithmCallbacks callbacks;
    FVizAlgorithm** algorithms = NULL;
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* future = NULL;
    StageCounter* counters = NULL;
    long calls = 0L;
    uint32_t i;
    uint32_t iteration;
    double started;
    double finished;
    double seconds;

    algorithms = (FVizAlgorithm**)fviz_alloc(stage_count * sizeof(*algorithms));
    counters = (StageCounter*)fviz_alloc(stage_count * sizeof(*counters));
    if (algorithms == NULL || counters == NULL) return 2;
    for (i = 0u; i < stage_count; ++i) algorithms[i] = NULL;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = stage_process;
    for (i = 0u; i < stage_count; ++i)
    {
        counters[i].calls = &calls;
        counters[i].id = (int)i;
        if (fviz_algorithm_create(0u, 1u, &callbacks, &counters[i], &algorithms[i]) != FVIZ_OK)
        { for (; i > 0u; --i) fviz_release(algorithms[i - 1u]); fviz_free(counters); fviz_free(algorithms); return 3; }
        if (fviz_algorithm_configure_output_port(algorithms[i], 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
        { for (; i > 0u; --i) fviz_release(algorithms[i - 1u]); fviz_free(counters); fviz_free(algorithms); return 4; }
    }
    fviz_executor_options_initialize(&options);
    options.thread_count = thread_count;
    options.queue_capacity = stage_count + 1u;
    if (fviz_executor_create(&options, &executor) != FVIZ_OK)
    { for (i = 0u; i < stage_count; ++i) fviz_release(algorithms[i]); fviz_free(counters); fviz_free(algorithms); return 5; }

    started = benchmark_wall_seconds();
    for (iteration = 0u; iteration < iterations; ++iteration)
    {
        uint32_t s;
        /* Re-execute every stage: mark each algorithm modified so the update is
         * a real drain through the continuation chain rather than a cache hit. */
        for (s = 0u; s < stage_count; ++s)
            fviz_object_modified((FVizObject*)algorithms[s]);
        if (fviz_algorithm_update_async_chain(algorithms, stage_count, executor, 0, &future) != FVIZ_OK)
        { fviz_executor_destroy(executor); for (i = 0u; i < stage_count; ++i) fviz_release(algorithms[i]); fviz_free(counters); fviz_free(algorithms); return 6; }
        if (fviz_future_wait(future) != FVIZ_OK)
        { fviz_future_destroy(future); fviz_executor_destroy(executor); for (i = 0u; i < stage_count; ++i) fviz_release(algorithms[i]); fviz_free(counters); fviz_free(algorithms); return 7; }
        fviz_future_destroy(future);
        future = NULL;
    }
    finished = benchmark_wall_seconds();
    seconds = finished - started;

    fviz_executor_destroy(executor);
    for (i = 0u; i < stage_count; ++i) fviz_release(algorithms[i]);
    fviz_free(counters);
    fviz_free(algorithms);

    printf("%u,%u,%u,%.6f,%.2f,%ld\n",
        thread_count, stage_count, iterations, seconds,
        seconds > 0.0 ? (double)(iterations * stage_count) / seconds : 0.0,
        calls);
    return 0;
}

int main(void)
{
    const uint32_t thread_counts[3] = {1u, 4u, 0u};
    uint32_t i;
    puts("threads,stages,iterations,seconds,stages_per_second,calls");
    for (i = 0u; i < 3u; ++i)
    {
        const uint32_t threads = thread_counts[i] != 0u
            ? thread_counts[i] : fviz_parallel_hardware_thread_count();
        if (run_chain_benchmark(threads, 32u, 100u) != 0) return 1;
    }
    return 0;
}
