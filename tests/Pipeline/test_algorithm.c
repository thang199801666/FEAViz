#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) \
    do \
    { \
        if (!(expr)) \
        { \
            fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
            return 1; \
        } \
    } while (0)

typedef struct ProgressState
{
    uint32_t calls;
    double last;
} ProgressState;

typedef struct AlgorithmEventState
{
    uint32_t starts;
    uint32_t progress_events;
    uint32_t abort_checks;
    uint32_t ends;
    double last_progress;
    FVizResult last_result;
} AlgorithmEventState;

static FVizBool record_algorithm_event(
    FVizObject* caller,
    FVizEventId event_id,
    void* call_data,
    void* client_data)
{
    AlgorithmEventState* state = (AlgorithmEventState*)client_data;
    (void)caller;
    if (state == NULL) return FVIZ_FALSE;
    switch (event_id)
    {
        case FVIZ_EVENT_START:
            ++state->starts;
            break;
        case FVIZ_EVENT_PROGRESS:
            ++state->progress_events;
            if (call_data != NULL) state->last_progress = *(const double*)call_data;
            break;
        case FVIZ_EVENT_ABORT_CHECK:
            ++state->abort_checks;
            break;
        case FVIZ_EVENT_END:
            ++state->ends;
            if (call_data != NULL) state->last_result = *(const FVizResult*)call_data;
            break;
        default:
            break;
    }
    return FVIZ_FALSE;
}

static void record_progress(FVizAlgorithm* algorithm, double progress, void* user_data)
{
    ProgressState* state = (ProgressState*)user_data;
    (void)algorithm;
    ++state->calls;
    state->last = progress;
}

static FVizBool count_modified_event(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    uint32_t* count = (uint32_t*)client_data;
    (void)caller;
    (void)call_data;
    if (event_id == FVIZ_EVENT_MODIFIED && count != NULL) ++(*count);
    return FVIZ_FALSE;
}

static int add_tetra(FVizUnstructuredGrid* grid)
{
    FVizVec3 points[4] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };
    uint32_t ids[4] = {0u, 1u, 2u, 3u};
    uint32_t i;
    for (i = 0u; i < 4u; ++i)
        CHECK(fviz_unstructured_grid_add_point(grid, points[i], NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_TETRA, 4u, ids) == FVIZ_OK);
    return 0;
}

int main(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* displacement = NULL;
    FVizFilter* warp = NULL;
    FVizFilter* surface = NULL;
    FVizMapper* mapper = NULL;
    FVizAlgorithm* warp_algorithm;
    FVizAlgorithm* surface_algorithm;
    FVizAlgorithmOutput* warp_output;
    FVizAlgorithmOutput* surface_output;
    FVizAlgorithmPortInfo info;
    FVizExecutive* executive;
    ProgressState progress = {0u, 0.0};
    AlgorithmEventState events = {0u, 0u, 0u, 0u, 0.0, FVIZ_OK};
    FVizObserverTag start_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag progress_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag abort_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag end_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    uint32_t downstream_modified = 0u;
    uint32_t modified_before;

    CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
    CHECK(add_tetra(grid) == 0);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &displacement) == FVIZ_OK);
    CHECK(fviz_data_array_resize(displacement, 4u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(
        fviz_unstructured_grid_point_data(grid), "displacement", displacement) == FVIZ_OK);
    CHECK(fviz_data_object_is_data_object((const FVizDataObject*)grid) == FVIZ_TRUE);
    CHECK(fviz_object_is_type((const FVizObject*)grid, FVIZ_TYPE_DATA_OBJECT) == FVIZ_TRUE);

    CHECK(fviz_warp_filter_create("displacement", 1.0, &warp) == FVIZ_OK);
    CHECK(fviz_surface_filter_create(FVIZ_TRUE, &surface) == FVIZ_OK);
    warp_algorithm = fviz_filter_algorithm(warp);
    surface_algorithm = fviz_filter_algorithm(surface);
    CHECK(warp_algorithm != NULL && surface_algorithm != NULL);
    CHECK(fviz_object_is_type((const FVizObject*)warp_algorithm, FVIZ_TYPE_ALGORITHM) == FVIZ_TRUE);
    CHECK(fviz_algorithm_input_port_count(warp_algorithm) == 1u);
    CHECK(fviz_algorithm_output_port_count(warp_algorithm) == 1u);
    CHECK(fviz_algorithm_input_port_info(warp_algorithm, 0u, &info) == FVIZ_OK);
    CHECK(info.data_type == FVIZ_TYPE_UNSTRUCTURED_GRID);
    CHECK(info.optional == FVIZ_FALSE && info.repeatable == FVIZ_FALSE);
    CHECK(fviz_algorithm_output_port_info(surface_algorithm, 0u, &info) == FVIZ_OK);
    CHECK(info.data_type == FVIZ_TYPE_POLY_DATA);

    warp_output = fviz_filter_output_port(warp);
    surface_output = fviz_filter_output_port(surface);
    CHECK(warp_output != NULL && surface_output != NULL);
    CHECK(fviz_algorithm_output_producer(warp_output) == warp_algorithm);
    CHECK(fviz_algorithm_output_index(warp_output) == 0u);
    CHECK(fviz_algorithm_output_index(NULL) == UINT32_MAX);

    CHECK(fviz_algorithm_set_input_data(
        warp_algorithm, 0u, (FVizDataObject*)grid) == FVIZ_OK);
    CHECK(fviz_algorithm_set_input_data(
        warp_algorithm, 0u, (FVizDataObject*)displacement) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_algorithm_input_data(warp_algorithm, 0u) == (FVizDataObject*)grid);
    CHECK(fviz_algorithm_set_input_connection(surface_algorithm, 0u, warp_output) == FVIZ_OK);
    CHECK(fviz_algorithm_input_connection_count(surface_algorithm, 0u) == 1u);
    CHECK(fviz_algorithm_input_connection(surface_algorithm, 0u, 0u) == warp_output);
    CHECK(fviz_algorithm_set_input_connection(warp_algorithm, 0u, surface_output) ==
        FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_algorithm_set_input_connection(warp_algorithm, 1u, warp_output) ==
        FVIZ_ERROR_INVALID_ARGUMENT);

    CHECK(fviz_mapper_create(&mapper) == FVIZ_OK);
    CHECK(fviz_mapper_set_algorithm_connection(mapper, warp_output) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_mapper_set_algorithm_connection(mapper, surface_output) == FVIZ_OK);
    CHECK(fviz_mapper_algorithm_connection(mapper) == surface_output);
    CHECK(fviz_object_add_observer(
        (FVizObject*)surface_algorithm, FVIZ_EVENT_MODIFIED, 0.0f,
        count_modified_event, &downstream_modified, &modified_tag) == FVIZ_OK);
    modified_before = downstream_modified;
    CHECK(fviz_warp_filter_set_scale(warp, 1.25) == FVIZ_OK);
    CHECK(downstream_modified > modified_before);
    modified_before = downstream_modified;
    fviz_object_modified((FVizObject*)grid);
    CHECK(downstream_modified > modified_before);
    executive = fviz_algorithm_executive(surface_algorithm);
    CHECK(executive != NULL);
    CHECK(fviz_executive_algorithm(executive) == surface_algorithm);
    CHECK(fviz_object_add_observer(
        (FVizObject*)surface_algorithm, FVIZ_EVENT_START, 0.0f,
        record_algorithm_event, &events, &start_tag) == FVIZ_OK);
    CHECK(fviz_object_add_observer(
        (FVizObject*)surface_algorithm, FVIZ_EVENT_PROGRESS, 0.0f,
        record_algorithm_event, &events, &progress_tag) == FVIZ_OK);
    CHECK(fviz_object_add_observer(
        (FVizObject*)surface_algorithm, FVIZ_EVENT_ABORT_CHECK, 0.0f,
        record_algorithm_event, &events, &abort_tag) == FVIZ_OK);
    CHECK(fviz_object_add_observer(
        (FVizObject*)surface_algorithm, FVIZ_EVENT_END, 0.0f,
        record_algorithm_event, &events, &end_tag) == FVIZ_OK);
    fviz_algorithm_set_progress_callback(surface_algorithm, record_progress, &progress);
    CHECK(fviz_mapper_update(mapper) == FVIZ_OK);
    CHECK(fviz_mapper_poly_data(mapper) != NULL);
    CHECK(fviz_poly_data_triangle_count(fviz_mapper_poly_data(mapper)) == 4u);
    CHECK(progress.calls == 2u && progress.last == 1.0);
    CHECK(events.starts == 1u);
    CHECK(events.progress_events == 2u && events.last_progress == 1.0);
    CHECK(events.abort_checks == 2u);
    CHECK(events.ends == 1u && events.last_result == FVIZ_OK);
    CHECK(fviz_executive_last_request(executive) == FVIZ_PIPELINE_REQUEST_DATA);
    CHECK(fviz_executive_execution_count(executive) == 1u);
    CHECK(fviz_executive_cache_hit_count(executive) == 0u);
    CHECK(fviz_mapper_update(mapper) == FVIZ_OK);
    CHECK(fviz_executive_cache_hit_count(executive) == 1u);
    CHECK(progress.calls == 2u);
    CHECK(events.starts == 1u && events.progress_events == 2u);
    CHECK(events.abort_checks == 2u && events.ends == 1u);
    fviz_algorithm_request_abort(surface_algorithm);
    CHECK(fviz_algorithm_abort_requested(surface_algorithm) == FVIZ_TRUE);
    CHECK(fviz_mapper_update(mapper) == FVIZ_ERROR_BUSY);
    CHECK(events.starts == 1u && events.ends == 1u);
    fviz_algorithm_clear_abort(surface_algorithm);
    CHECK(fviz_mapper_update(mapper) == FVIZ_OK);
    fviz_executive_reset_statistics(executive);
    CHECK(fviz_executive_execution_count(executive) == 0u);
    CHECK(fviz_executive_cache_hit_count(executive) == 0u);

    CHECK(fviz_warp_filter_set_vector_name(warp, "unused") == FVIZ_OK);
    CHECK(fviz_algorithm_remove_input_connection(surface_algorithm, 0u, 0u) == FVIZ_OK);
    CHECK(fviz_algorithm_input_connection_count(surface_algorithm, 0u) == 0u);
    CHECK(fviz_algorithm_update(surface_algorithm) == FVIZ_ERROR_INVALID_STATE);

    CHECK(fviz_object_remove_observer((FVizObject*)surface_algorithm, start_tag) == FVIZ_OK);
    CHECK(fviz_object_remove_observer((FVizObject*)surface_algorithm, progress_tag) == FVIZ_OK);
    CHECK(fviz_object_remove_observer((FVizObject*)surface_algorithm, abort_tag) == FVIZ_OK);
    CHECK(fviz_object_remove_observer((FVizObject*)surface_algorithm, end_tag) == FVIZ_OK);
    CHECK(fviz_object_remove_observer((FVizObject*)surface_algorithm, modified_tag) == FVIZ_OK);

    fviz_release(mapper);
    fviz_release(surface);
    fviz_release(warp);
    fviz_release(displacement);
    fviz_release(grid);
    puts("algorithm port tests passed");
    return 0;
}
