#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return __LINE__; } } while (0)

typedef struct ChainState
{
    int* order;
    volatile int* count;
    int* fail_at;
    double* last_progress;
    double* max_progress;
} ChainState;

static FVizResult chain_process(
    FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request, void* user_data)
{
    ChainState* state = (ChainState*)user_data;
    FVizPolyData* output = NULL;
    FVizResult result;
    (void)algorithm;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (state->fail_at != NULL && *state->fail_at == *state->count)
        return FVIZ_ERROR_INVALID_STATE;
    if (state->last_progress != NULL)
    {
        int stage;
        for (stage = 0; stage < 10; ++stage)
        {
            const double progress = (double)(*state->count * 10 + stage) / 40.0;
            if (progress > *state->last_progress && progress > 0.0 && progress < 1.0)
            {
                result = fviz_algorithm_report_progress(algorithm, progress);
                if (result != FVIZ_OK) return result;
            }
        }
    }
    state->order[*state->count] = *state->count;
    ++(*state->count);
    if (fviz_poly_data_create(&output) != FVIZ_OK) return fviz_last_error_code();
    result = fviz_algorithm_set_output_data(
        algorithm, request->requested_output_port, (FVizDataObject*)output);
    fviz_release(output);
    return result;
}

/* A chain of N algorithms: every stage must run in order exactly once. */
static int test_chain_ordering(void)
{
    enum { STAGES = 8 };
    FVizAlgorithmCallbacks callbacks;
    FVizAlgorithm* algorithms[STAGES];
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* future = NULL;
    ChainState state;
    int order[STAGES];
    volatile int count = 0;
    int i;
    for (i = 0; i < STAGES; ++i)
    {
        algorithms[i] = NULL;
        order[i] = -1;
    }
    state.order = order;
    state.count = &count;
    state.fail_at = NULL;
    state.last_progress = NULL;
    state.max_progress = NULL;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = chain_process;
    for (i = 0; i < STAGES; ++i)
    {
        CHECK(fviz_algorithm_create(0u, 1u, &callbacks, &state, &algorithms[i]) == FVIZ_OK);
        CHECK(fviz_algorithm_configure_output_port(algorithms[i], 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    }
    fviz_executor_options_initialize(&options);
    options.thread_count = 2u;
    options.queue_capacity = STAGES + 1u;
    CHECK(fviz_executor_create(&options, &executor) == FVIZ_OK);
    CHECK(fviz_algorithm_update_async_chain(algorithms, STAGES, executor, 0, &future) == FVIZ_OK);
    CHECK(fviz_future_wait(future) == FVIZ_OK);
    CHECK(count == STAGES);
    for (i = 0; i < STAGES; ++i) CHECK(order[i] == i);
    fviz_future_destroy(future);
    fviz_executor_destroy(executor);
    for (i = 0; i < STAGES; ++i) fviz_release(algorithms[i]);
    return 0;
}

/* A failing stage must short-circuit every later stage. */
static int test_chain_short_circuit(void)
{
    enum { STAGES = 6, FAIL_AT = 3 };
    FVizAlgorithmCallbacks callbacks;
    FVizAlgorithm* algorithms[STAGES];
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* future = NULL;
    ChainState state;
    int order[STAGES];
    volatile int count = 0;
    int i;
    for (i = 0; i < STAGES; ++i)
    {
        algorithms[i] = NULL;
        order[i] = -1;
    }
    state.order = order;
    state.count = &count;
    state.fail_at = &(int){FAIL_AT};
    state.last_progress = NULL;
    state.max_progress = NULL;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = chain_process;
    for (i = 0; i < STAGES; ++i)
    {
        CHECK(fviz_algorithm_create(0u, 1u, &callbacks, &state, &algorithms[i]) == FVIZ_OK);
        CHECK(fviz_algorithm_configure_output_port(algorithms[i], 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    }
    fviz_executor_options_initialize(&options);
    options.thread_count = 2u;
    options.queue_capacity = STAGES + 1u;
    CHECK(fviz_executor_create(&options, &executor) == FVIZ_OK);
    CHECK(fviz_algorithm_update_async_chain(algorithms, STAGES, executor, 0, &future) == FVIZ_OK);
    CHECK(fviz_future_wait(future) == FVIZ_ERROR_INVALID_STATE);
    /* Exactly FAIL_AT stages ran; the rest never executed. */
    CHECK(count == FAIL_AT);
    for (i = 0; i < FAIL_AT; ++i) CHECK(order[i] == i);
    fviz_future_destroy(future);
    fviz_executor_destroy(executor);
    for (i = 0; i < STAGES; ++i) fviz_release(algorithms[i]);
    return 0;
}

/* Cancelling the terminal future must stop later stages from running. */
static int test_chain_cancellation(void)
{
    enum { STAGES = 6 };
    FVizAlgorithmCallbacks callbacks;
    FVizAlgorithm* algorithms[STAGES];
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* future = NULL;
    ChainState state;
    int order[STAGES];
    volatile int count = 0;
    int i;
    for (i = 0; i < STAGES; ++i)
    {
        algorithms[i] = NULL;
        order[i] = -1;
    }
    state.order = order;
    state.count = &count;
    state.fail_at = NULL;
    state.last_progress = NULL;
    state.max_progress = NULL;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = chain_process;
    for (i = 0; i < STAGES; ++i)
    {
        CHECK(fviz_algorithm_create(0u, 1u, &callbacks, &state, &algorithms[i]) == FVIZ_OK);
        CHECK(fviz_algorithm_configure_output_port(algorithms[i], 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    }
    fviz_executor_options_initialize(&options);
    options.thread_count = 1u;
    options.queue_capacity = STAGES + 1u;
    CHECK(fviz_executor_create(&options, &executor) == FVIZ_OK);
    CHECK(fviz_algorithm_update_async_chain(algorithms, STAGES, executor, 0, &future) == FVIZ_OK);
    fviz_future_cancel(future);
    {
        const FVizResult result = fviz_future_wait(future);
        CHECK(result == FVIZ_OK || result == FVIZ_ERROR_CANCELLED);
    }
    fviz_future_destroy(future);
    fviz_executor_destroy(executor);
    for (i = 0; i < STAGES; ++i) fviz_release(algorithms[i]);
    return 0;
}

/* The async single-algorithm update must expose monotonic live progress through
 * the future while the pipeline runs. */
static int test_async_progress(void)
{
    FVizAlgorithmCallbacks callbacks;
    FVizAlgorithm* algorithm = NULL;
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* future = NULL;
    ChainState state;
    double last_progress = 0.0;
    double max_progress = 0.0;
    int order[1];
    volatile int count = 0;
    int i;
    order[0] = -1;
    state.order = order;
    state.count = &count;
    state.fail_at = NULL;
    state.last_progress = &last_progress;
    state.max_progress = &max_progress;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = chain_process;
    CHECK(fviz_algorithm_create(0u, 1u, &callbacks, &state, &algorithm) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_output_port(algorithm, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    fviz_executor_options_initialize(&options);
    options.thread_count = 1u;
    CHECK(fviz_executor_create(&options, &executor) == FVIZ_OK);
    CHECK(fviz_algorithm_update_async(algorithm, executor, 0, &future) == FVIZ_OK);
    /* Poll progress until the future completes; it must be monotonic and reach 1. */
    {
        double seen = 0.0;
        int attempts = 0;
        while (fviz_future_ready(future) == FVIZ_FALSE && attempts < 2000)
        {
            const double progress = fviz_future_progress(future);
            CHECK(progress >= seen);
            seen = progress;
#if defined(_WIN32)
            Sleep(1u);
#else
            usleep(1000u);
#endif
            ++attempts;
        }
        CHECK(fviz_future_wait(future) == FVIZ_OK);
        CHECK(fviz_future_progress(future) == 1.0);
        (void)i;
    }
    fviz_future_destroy(future);
    fviz_executor_destroy(executor);
    fviz_release(algorithm);
    return 0;
}

int main(void)
{
    int result;
    result = test_chain_ordering();
    if (result != 0) return result;
    result = test_chain_short_circuit();
    if (result != 0) return result;
    result = test_chain_cancellation();
    if (result != 0) return result;
    result = test_async_progress();
    if (result != 0) return result;
    puts("async pipeline chain tests passed");
    return 0;
}
