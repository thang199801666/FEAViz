#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

static FVizResult process(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request, void* state)
{
    FVizPolyData* poly = (FVizPolyData*)state;
    if (request->type == FVIZ_PIPELINE_REQUEST_INFORMATION)
    {
        const double times[3] = {0.0, 0.5, 1.0};
        return fviz_algorithm_set_output_time_steps(algorithm, 0u, times, 3u);
    }
    if (request->type == FVIZ_PIPELINE_REQUEST_DATA)
        return fviz_algorithm_set_output_data(algorithm, 0u, (FVizDataObject*)poly);
    return FVIZ_OK;
}

int main(void)
{
    FVizAlgorithm* algorithm = NULL;
    FVizAlgorithmCallbacks cb;
    FVizPolyData* poly = NULL;
    FVizPipelineRequestInfo request;
    FVizElevationFilter* transform = NULL;
    const double* times;
    FVizSize count = 0u;
    double tmin = 0.0, tmax = 0.0;

    CHECK(fviz_poly_data_create(&poly) == FVIZ_OK);
    fviz_algorithm_callbacks_initialize(&cb);
    cb.process_request = process;
    CHECK(fviz_algorithm_create(0u, 1u, &cb, poly, &algorithm) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_output_port(algorithm, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);

    fviz_pipeline_request_initialize(&request);
    request.type = FVIZ_PIPELINE_REQUEST_INFORMATION;
    CHECK(fviz_executive_update_request(fviz_algorithm_executive(algorithm), &request) == FVIZ_OK);
    times = fviz_algorithm_output_time_steps(algorithm, 0u, &count);
    CHECK(times != NULL && count == 3u);
    CHECK(times[1] == 0.5);
    CHECK(fviz_algorithm_output_time_range(algorithm, 0u, &tmin, &tmax) == FVIZ_OK);
    CHECK(tmin == 0.0 && tmax == 1.0);

    fviz_pipeline_request_initialize(&request);
    CHECK(fviz_pipeline_request_set_time(&request, 0.75) == FVIZ_OK);
    CHECK(request.has_time == FVIZ_TRUE && fabs(request.time - 0.75) < 1e-12);
    CHECK(fviz_executive_update_request(fviz_algorithm_executive(algorithm), &request) == FVIZ_OK);
    fviz_pipeline_request_clear_time(&request);
    CHECK(request.has_time == FVIZ_FALSE);

    /* Single-upstream filters inherit the producer time domain automatically. */
    CHECK(fviz_elevation_filter_create(&transform) == FVIZ_OK);
    CHECK(fviz_elevation_filter_set_input_connection(transform, fviz_algorithm_output_port(algorithm, 0u)) == FVIZ_OK);
    fviz_pipeline_request_initialize(&request);
    CHECK(fviz_pipeline_request_set_time(&request, 0.5) == FVIZ_OK);
    CHECK(fviz_executive_update_request(fviz_algorithm_executive(fviz_elevation_filter_algorithm(transform)), &request) == FVIZ_OK);
    times = fviz_algorithm_output_time_steps(fviz_elevation_filter_algorithm(transform), 0u, &count);
    CHECK(times != NULL && count == 3u && times[1] == 0.5);

    fviz_release(transform);
    fviz_release(algorithm);
    fviz_release(poly);
    return 0;
}
