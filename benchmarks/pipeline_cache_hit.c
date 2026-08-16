#include <FViz/FViz.h>

#include <stdio.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

static FVizResult source_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* user_state)
{
    FVizPolyData* output = NULL;
    FVizResult result;
    (void)user_state;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    result = fviz_poly_data_create(&output);
    if (result == FVIZ_OK)
        result = fviz_algorithm_set_output_data(
            algorithm, request->requested_output_port, (FVizDataObject*)output);
    fviz_release(output);
    return result;
}

static FVizResult pass_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* user_state)
{
    FVizDataObject* input;
    (void)user_state;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    return input != NULL
        ? fviz_algorithm_set_output_data(algorithm, request->requested_output_port, input)
        : FVIZ_ERROR_INVALID_STATE;
}

static FVizResult create_algorithm(
    uint32_t input_count,
    FVizAlgorithmProcessRequestFn process_request,
    FVizAlgorithm** out_algorithm)
{
    FVizAlgorithmCallbacks callbacks;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = process_request;
    return fviz_algorithm_create(input_count, 1u, &callbacks, NULL, out_algorithm);
}

int main(void)
{
    enum { PIPELINE_DEPTH = 4096, CACHE_HIT_COUNT = 1000 };
    FVizAlgorithm** algorithms = NULL;
    uint32_t i;
    double start;
    double finish;

    algorithms = (FVizAlgorithm**)fviz_alloc(
        (FVizSize)PIPELINE_DEPTH * sizeof(*algorithms));
    if (algorithms == NULL) return 1;
    for (i = 0u; i < PIPELINE_DEPTH; ++i) algorithms[i] = NULL;

    if (create_algorithm(0u, source_request, &algorithms[0]) != FVIZ_OK) goto fail;
    for (i = 1u; i < PIPELINE_DEPTH; ++i)
    {
        if (create_algorithm(1u, pass_request, &algorithms[i]) != FVIZ_OK ||
            fviz_algorithm_set_input_connection(
                algorithms[i], 0u, fviz_algorithm_output_port(algorithms[i - 1u], 0u)) != FVIZ_OK)
            goto fail;
    }

    if (fviz_algorithm_update(algorithms[PIPELINE_DEPTH - 1u]) != FVIZ_OK) goto fail;
    start = wall_seconds();
    for (i = 0u; i < CACHE_HIT_COUNT; ++i)
        if (fviz_algorithm_update(algorithms[PIPELINE_DEPTH - 1u]) != FVIZ_OK) goto fail;
    finish = wall_seconds();

    puts("depth,cache_hits,seconds,microseconds_per_update");
    printf("%u,%u,%.9f,%.6f\n",
        (unsigned)PIPELINE_DEPTH,
        (unsigned)CACHE_HIT_COUNT,
        finish - start,
        (finish - start) * 1.0e6 / (double)CACHE_HIT_COUNT);

    for (i = PIPELINE_DEPTH; i > 0u; --i) fviz_release(algorithms[i - 1u]);
    fviz_free(algorithms);
    return 0;

fail:
    for (i = PIPELINE_DEPTH; i > 0u; --i) fviz_release(algorithms[i - 1u]);
    fviz_free(algorithms);
    return 1;
}
