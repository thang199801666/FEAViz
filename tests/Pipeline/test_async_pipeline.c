#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

typedef struct AsyncState
{
    uint32_t calls;
    FVizBool saw_cancellation;
    FVizBool slow;
} AsyncState;

static FVizResult process_request(
    FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request, void* user_data)
{
    AsyncState* state = (AsyncState*)user_data;
    FVIZ_UNUSED(algorithm);
    ++state->calls;
    if (request->cancellation != NULL) state->saw_cancellation = FVIZ_TRUE;
    if (state->slow != FVIZ_FALSE && request->type == FVIZ_PIPELINE_REQUEST_DATA)
    {
        uint32_t i;
        for (i = 0u; i < 100u; ++i)
        {
            if (fviz_cancellation_token_is_cancelled(request->cancellation) != FVIZ_FALSE)
                return FVIZ_ERROR_CANCELLED;
#if defined(_WIN32)
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
    }
    if (request->type == FVIZ_PIPELINE_REQUEST_DATA)
    {
        FVizPolyData* output = NULL;
        FVizResult result;
        if (fviz_poly_data_create(&output) != FVIZ_OK) return fviz_last_error_code();
        result = fviz_algorithm_set_output_data(
            algorithm, request->requested_output_port, (FVizDataObject*)output);
        fviz_release(output);
        return result;
    }
    return FVIZ_OK;
}

int main(void)
{
    FVizAlgorithmCallbacks callbacks;
    FVizAlgorithm* algorithm = NULL;
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* future = NULL;
    AsyncState state = {0u, FVIZ_FALSE, FVIZ_FALSE};
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = process_request;
    CHECK(fviz_algorithm_create(0u, 1u, &callbacks, &state, &algorithm) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_output_port(algorithm, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    fviz_executor_options_initialize(&options);
    options.thread_count = 1u;
    CHECK(fviz_executor_create(&options, &executor) == FVIZ_OK);
    CHECK(fviz_algorithm_update_async(algorithm, executor, 5, &future) == FVIZ_OK);
    CHECK(fviz_future_wait(future) == FVIZ_OK);
    CHECK(state.calls > 0u && state.saw_cancellation == FVIZ_TRUE);
    fviz_future_destroy(future);
    future = NULL;
    state.slow = FVIZ_TRUE;
    fviz_object_modified((FVizObject*)algorithm);
    CHECK(fviz_algorithm_update_async(algorithm, executor, 0, &future) == FVIZ_OK);
#if defined(_WIN32)
    Sleep(5u);
#else
    usleep(5000u);
#endif
    fviz_future_cancel(future);
    CHECK(fviz_future_wait(future) == FVIZ_ERROR_CANCELLED);
    fviz_future_destroy(future);
    fviz_executor_destroy(executor);
    fviz_release(algorithm);
    return 0;
}
