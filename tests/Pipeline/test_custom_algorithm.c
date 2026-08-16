#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) \
    do { if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } } while (0)

typedef struct TestState
{
    uint32_t stage_calls[5];
    uint32_t data_calls;
    uint32_t last_output;
    uint32_t last_piece;
    uint32_t last_number_of_pieces;
    uint32_t last_ghost_levels;
    FVizBool last_has_extent;
    int64_t last_extent[6];
    double last_time;
} TestState;

static FVizResult source_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* user_state)
{
    TestState* state = (TestState*)user_state;
    ++state->stage_calls[request->type];
    state->last_output = request->requested_output_port;
    state->last_piece = request->piece;
    state->last_number_of_pieces = request->number_of_pieces;
    state->last_ghost_levels = request->ghost_levels;
    state->last_has_extent = request->has_extent;
    (void)memcpy(state->last_extent, request->extent, sizeof(state->last_extent));
    state->last_time = request->time;
    if (request->type == FVIZ_PIPELINE_REQUEST_DATA)
    {
        FVizPolyData* output = NULL;
        FVizResult result;
        ++state->data_calls;
        result = fviz_poly_data_create(&output);
        if (result == FVIZ_OK)
            result = fviz_algorithm_set_output_data(
                algorithm, request->requested_output_port, (FVizDataObject*)output);
        fviz_release(output);
        return result;
    }
    return FVIZ_OK;
}

static FVizResult multi_output_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* user_state)
{
    FVizPolyData* first = NULL;
    FVizPolyData* second = NULL;
    FVizResult result = FVIZ_OK;
    uint32_t* calls = (uint32_t*)user_state;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (calls != NULL) ++(*calls);
    result = fviz_poly_data_create(&first);
    if (result == FVIZ_OK) result = fviz_poly_data_create(&second);
    if (result == FVIZ_OK) result = fviz_algorithm_set_output_data(algorithm, 0u, (FVizDataObject*)first);
    if (result == FVIZ_OK) result = fviz_algorithm_set_output_data(algorithm, 1u, (FVizDataObject*)second);
    fviz_release(second);
    fviz_release(first);
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

static FVizResult merge_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* user_state)
{
    FVizDataObject* first;
    FVizDataObject* second;
    (void)user_state;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    first = fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    second = fviz_algorithm_resolved_input(algorithm, 0u, 1u);
    if (first == NULL || second == NULL) return FVIZ_ERROR_INVALID_STATE;
    return fviz_algorithm_set_output_data(algorithm, request->requested_output_port, first);
}

static FVizResult streaming_map_request(
    FVizAlgorithm* algorithm,
    uint32_t input_port,
    uint32_t connection,
    const FVizPipelineRequestInfo* downstream,
    FVizPipelineRequestInfo* upstream,
    void* user_state)
{
    int64_t extent[6];
    (void)algorithm;
    (void)connection;
    (void)user_state;
    if (input_port != 0u) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_pipeline_request_set_piece(
            upstream, downstream->piece * 2u, downstream->number_of_pieces * 2u,
            downstream->ghost_levels + 1u) != FVIZ_OK)
        return fviz_last_error_code();
    if (downstream->has_extent != FVIZ_FALSE)
    {
        (void)memcpy(extent, downstream->extent, sizeof(extent));
        --extent[0]; ++extent[1];
        return fviz_pipeline_request_set_extent(upstream, extent);
    }
    return FVIZ_OK;
}

static FVizResult make_algorithm(
    uint32_t inputs,
    uint32_t outputs,
    FVizAlgorithmProcessRequestFn callback,
    void* state,
    FVizAlgorithm** out_algorithm)
{
    FVizAlgorithmCallbacks callbacks;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = callback;
    return fviz_algorithm_create(inputs, outputs, &callbacks, state, out_algorithm);
}

static FVizBool count_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    uint32_t* count = (uint32_t*)client_data;
    (void)caller;
    (void)call_data;
    if (event_id == FVIZ_EVENT_MODIFIED && count != NULL) ++(*count);
    return FVIZ_FALSE;
}

int main(void)
{
    TestState source_state = {0};
    TestState split_state = {0};
    FVizAlgorithm* source = NULL;
    FVizAlgorithm* left = NULL;
    FVizAlgorithm* right = NULL;
    FVizAlgorithm* merge = NULL;
    FVizAlgorithm* split = NULL;
    FVizPipelineRequestInfo request;
    FVizCancellationToken* cancellation = NULL;
    uint64_t first_transaction;
    FVizSize dot_size = 0u;
    char dot[4096];
    FVizObject* observable_state = NULL;
    FVizAlgorithm* state_algorithm = NULL;
    FVizAlgorithm* v1_algorithm = NULL;
    FVizObserverTag state_algorithm_tag = FVIZ_OBSERVER_TAG_INVALID;
    uint32_t state_algorithm_modified = 0u;

    CHECK(make_algorithm(0u, 1u, source_request, &source_state, &source) == FVIZ_OK);
    CHECK(make_algorithm(1u, 1u, pass_request, NULL, &left) == FVIZ_OK);
    CHECK(make_algorithm(1u, 1u, pass_request, NULL, &right) == FVIZ_OK);
    CHECK(make_algorithm(1u, 1u, merge_request, NULL, &merge) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_output_port(source, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_input_port(
        left, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_output_port(left, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_input_port(
        right, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_output_port(right, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_input_port(
        merge, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_TRUE) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_output_port(merge, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    CHECK(fviz_algorithm_set_input_connection(left, 0u, fviz_algorithm_output_port(source, 0u)) == FVIZ_OK);
    CHECK(fviz_algorithm_set_input_connection(right, 0u, fviz_algorithm_output_port(source, 0u)) == FVIZ_OK);
    CHECK(fviz_algorithm_add_input_connection(merge, 0u, fviz_algorithm_output_port(left, 0u)) == FVIZ_OK);
    CHECK(fviz_algorithm_add_input_connection(merge, 0u, fviz_algorithm_output_port(right, 0u)) == FVIZ_OK);

    CHECK(fviz_algorithm_update(merge) == FVIZ_OK);
    CHECK(source_state.data_calls == 1u);
    CHECK(source_state.stage_calls[FVIZ_PIPELINE_REQUEST_INFORMATION] >= 1u);
    first_transaction = fviz_executive_last_transaction_id(fviz_algorithm_executive(merge));
    CHECK(first_transaction != 0u);
    {
        const uint32_t info_calls_before_cache_hit =
            source_state.stage_calls[FVIZ_PIPELINE_REQUEST_INFORMATION];
        const uint32_t object_calls_before_cache_hit =
            source_state.stage_calls[FVIZ_PIPELINE_REQUEST_DATA_OBJECT];
        const uint32_t extent_calls_before_cache_hit =
            source_state.stage_calls[FVIZ_PIPELINE_REQUEST_UPDATE_EXTENT];
        CHECK(fviz_algorithm_update(merge) == FVIZ_OK);
        CHECK(source_state.data_calls == 1u);
        CHECK(source_state.stage_calls[FVIZ_PIPELINE_REQUEST_INFORMATION] == info_calls_before_cache_hit);
        CHECK(source_state.stage_calls[FVIZ_PIPELINE_REQUEST_DATA_OBJECT] == object_calls_before_cache_hit);
        CHECK(source_state.stage_calls[FVIZ_PIPELINE_REQUEST_UPDATE_EXTENT] == extent_calls_before_cache_hit);
    }
    CHECK(fviz_executive_cache_hit_count(fviz_algorithm_executive(merge)) == 1u);
    /* The O(1) root cache gate is valid only because dependency ModifiedEvent
       propagation invalidates downstream algorithms.  Verify an upstream change
       breaks that gate and re-executes the shared source exactly once. */
    fviz_object_modified((FVizObject*)source);
    CHECK(fviz_algorithm_update(merge) == FVIZ_OK);
    CHECK(source_state.data_calls == 2u);

    fviz_pipeline_request_initialize(&request);
    request.requested_output_port = 0u;
    request.piece = 2u;
    request.number_of_pieces = 4u;
    request.has_time = FVIZ_TRUE;
    request.time = 1.25;
    CHECK(fviz_executive_update_request(fviz_algorithm_executive(merge), &request) == FVIZ_OK);
    CHECK(source_state.data_calls == 3u);
    CHECK(source_state.last_piece == 2u && source_state.last_time == 1.25);
    CHECK(fviz_executive_last_transaction_id(fviz_algorithm_executive(merge)) > first_transaction);
    CHECK(fviz_algorithm_diagnostic_id(source) != fviz_algorithm_diagnostic_id(merge));
    CHECK(fviz_executive_write_dot(
        fviz_algorithm_executive(merge), NULL, 0u, &dot_size) == FVIZ_OK);
    CHECK(dot_size > 1u && dot_size <= sizeof(dot));
    CHECK(fviz_executive_write_dot(
        fviz_algorithm_executive(merge), dot, sizeof(dot), &dot_size) == FVIZ_OK);
    CHECK(strstr(dot, "digraph FEAVizPipeline") != NULL);
    CHECK(strstr(dot, "out0 -> in0") != NULL);
    CHECK(strstr(dot, "request=4 result=0") != NULL);
    CHECK(strstr(dot, "out0=") != NULL);

    {
        FVizAlgorithm* stream_source = NULL;
        FVizAlgorithm* stream_filter = NULL;
        FVizAlgorithmCallbacks callbacks;
        TestState stream_state = {0};
        const int64_t extent[6] = {10, 19, 20, 29, 0, 0};
        CHECK(make_algorithm(0u, 1u, source_request, &stream_state, &stream_source) == FVIZ_OK);
        fviz_algorithm_callbacks_initialize(&callbacks);
        callbacks.process_request = pass_request;
        callbacks.map_input_request = streaming_map_request;
        CHECK(fviz_algorithm_create(1u, 1u, &callbacks, NULL, &stream_filter) == FVIZ_OK);
        CHECK(fviz_algorithm_configure_output_port(stream_source, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
        {
            const int64_t whole_extent[6] = {0, 99, 0, 49, 0, 9};
            int64_t queried_extent[6];
            CHECK(fviz_algorithm_output_whole_extent(stream_source, 0u, queried_extent) == FVIZ_FALSE);
            CHECK(fviz_algorithm_set_output_whole_extent(stream_source, 0u, whole_extent) == FVIZ_OK);
            CHECK(fviz_algorithm_output_whole_extent(stream_source, 0u, queried_extent) == FVIZ_TRUE);
            CHECK(memcmp(whole_extent, queried_extent, sizeof(whole_extent)) == 0);
            fviz_algorithm_clear_output_whole_extent(stream_source, 0u);
            CHECK(fviz_algorithm_output_whole_extent(stream_source, 0u, queried_extent) == FVIZ_FALSE);
            CHECK(fviz_algorithm_set_output_whole_extent(stream_source, 0u, whole_extent) == FVIZ_OK);
        }
        CHECK(fviz_algorithm_configure_input_port(
            stream_filter, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) == FVIZ_OK);
        CHECK(fviz_algorithm_configure_output_port(stream_filter, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
        CHECK(fviz_algorithm_set_input_connection(
            stream_filter, 0u, fviz_algorithm_output_port(stream_source, 0u)) == FVIZ_OK);
        fviz_pipeline_request_initialize(&request);
        request.requested_output_port = 0u;
        CHECK(fviz_pipeline_request_set_piece(&request, 2u, 4u, 2u) == FVIZ_OK);
        CHECK(fviz_pipeline_request_set_extent(&request, extent) == FVIZ_OK);
        CHECK(fviz_executive_update_request(fviz_algorithm_executive(stream_filter), &request) == FVIZ_OK);
        CHECK(stream_state.last_piece == 4u);
        CHECK(stream_state.last_number_of_pieces == 8u);
        CHECK(stream_state.last_ghost_levels == 3u);
        CHECK(stream_state.last_has_extent == FVIZ_TRUE);
        CHECK(stream_state.last_extent[0] == 9 && stream_state.last_extent[1] == 20);
        CHECK(stream_state.last_extent[2] == 20 && stream_state.last_extent[3] == 29);
        fviz_pipeline_request_clear_extent(&request);
        CHECK(request.has_extent == FVIZ_FALSE);
        CHECK(fviz_executive_update_piece(
            fviz_algorithm_executive(stream_filter), 0u, 1u, 2u, 0u) == FVIZ_OK);
        CHECK(stream_state.last_piece == 2u && stream_state.last_number_of_pieces == 4u);
        CHECK(stream_state.last_ghost_levels == 1u);
        {
            const int64_t small_extent[6] = {2, 5, 4, 7, 0, 0};
            CHECK(fviz_executive_update_extent(
                fviz_algorithm_executive(stream_filter), 0u, small_extent, 1u) == FVIZ_OK);
            CHECK(stream_state.last_has_extent == FVIZ_TRUE);
            CHECK(stream_state.last_extent[0] == 1 && stream_state.last_extent[1] == 6);
        }
        CHECK(fviz_executive_update_time(
            fviz_algorithm_executive(stream_filter), 0u, 3.5) == FVIZ_OK);
        CHECK(stream_state.last_time == 3.5);
        fviz_release(stream_filter);
        fviz_release(stream_source);
    }

    CHECK(fviz_cancellation_token_create(&cancellation) == FVIZ_OK);
    request.cancellation = cancellation;
    fviz_cancellation_token_cancel(cancellation);
    CHECK(fviz_executive_update_request(
        fviz_algorithm_executive(merge), &request) == FVIZ_ERROR_CANCELLED);
    fviz_cancellation_token_reset(cancellation);
    CHECK(fviz_executive_update_request(fviz_algorithm_executive(merge), &request) == FVIZ_OK);

    CHECK(make_algorithm(0u, 2u, source_request, &split_state, &split) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_output_port(split, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    CHECK(fviz_algorithm_configure_output_port(split, 1u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
    CHECK(fviz_executive_update(fviz_algorithm_executive(split), 1u) == FVIZ_OK);
    CHECK(split_state.data_calls == 1u && split_state.last_output == 1u);
    CHECK(fviz_algorithm_output_data(split, 0u) == NULL);
    CHECK(fviz_algorithm_output_data(split, 1u) != NULL);

    {
        FVizAlgorithmCallbacks callbacks;
        TestState state = {0};
        fviz_algorithm_callbacks_initialize(&callbacks);
        callbacks.process_request = source_request;
        CHECK(fviz_object_create(&observable_state) == FVIZ_OK);
        callbacks.state_object = observable_state;
        CHECK(fviz_algorithm_create(0u, 1u, &callbacks, &state, &state_algorithm) == FVIZ_OK);
        CHECK(fviz_object_add_observer(
            (FVizObject*)state_algorithm, FVIZ_EVENT_MODIFIED, 0.0f,
            count_modified, &state_algorithm_modified, &state_algorithm_tag) == FVIZ_OK);
        fviz_object_modified(observable_state);
        CHECK(state_algorithm_modified == 1u);
        /* DeleteEvent must detach the borrowed state-object bridge before the
           algorithm itself is destroyed. */
        fviz_release(observable_state);
        observable_state = NULL;
        fviz_object_modified((FVizObject*)state_algorithm);
        CHECK(state_algorithm_modified == 2u);
        CHECK(fviz_object_remove_observer(
            (FVizObject*)state_algorithm, state_algorithm_tag) == FVIZ_OK);

        /* A caller compiled against the pre-state-object callback prefix remains
           accepted because struct_size gates the new trailing field. */
        fviz_algorithm_callbacks_initialize(&callbacks);
        callbacks.struct_size = FVIZ_ALGORITHM_CALLBACKS_V1_SIZE;
        callbacks.process_request = source_request;
        CHECK(fviz_algorithm_create(0u, 1u, &callbacks, &state, &v1_algorithm) == FVIZ_OK);
    }

    {
        enum { DEEP_EXECUTION_COUNT = 4096 };
        FVizAlgorithm** deep = (FVizAlgorithm**)fviz_alloc(
            (FVizSize)DEEP_EXECUTION_COUNT * sizeof(*deep));
        TestState deep_source_state = {0};
        uint32_t i;
        CHECK(deep != NULL);
        (void)memset(deep, 0, (FVizSize)DEEP_EXECUTION_COUNT * sizeof(*deep));
        CHECK(make_algorithm(0u, 1u, source_request, &deep_source_state, &deep[0]) == FVIZ_OK);
        for (i = 1u; i < DEEP_EXECUTION_COUNT; ++i)
        {
            CHECK(make_algorithm(1u, 1u, pass_request, NULL, &deep[i]) == FVIZ_OK);
            CHECK(fviz_algorithm_set_input_connection(
                deep[i], 0u, fviz_algorithm_output_port(deep[i - 1u], 0u)) == FVIZ_OK);
        }
        /* The executive uses an explicit frame stack: deep demand-driven updates
           must not consume one native C stack frame per upstream algorithm. */
        CHECK(fviz_algorithm_update(deep[DEEP_EXECUTION_COUNT - 1u]) == FVIZ_OK);
        CHECK(deep_source_state.data_calls == 1u);
        for (i = DEEP_EXECUTION_COUNT; i > 0u; --i) fviz_release(deep[i - 1u]);
        fviz_free(deep);
    }

    {
        enum { DEEP_PIPELINE_COUNT = 1100 };
        FVizAlgorithm* deep[DEEP_PIPELINE_COUNT] = {0};
        uint32_t i;
        for (i = 0u; i < DEEP_PIPELINE_COUNT; ++i)
            CHECK(make_algorithm(1u, 1u, pass_request, NULL, &deep[i]) == FVIZ_OK);
        for (i = 1u; i < DEEP_PIPELINE_COUNT; ++i)
            CHECK(fviz_algorithm_set_input_connection(
                deep[i], 0u, fviz_algorithm_output_port(deep[i - 1u], 0u)) == FVIZ_OK);
        /* Regression: the old recursive detector stopped after depth 1024 and
           could accept this cycle.  The iterative visited-set traversal must
           reject arbitrarily deep graphs without consuming the C call stack. */
        CHECK(fviz_algorithm_set_input_connection(
            deep[0], 0u, fviz_algorithm_output_port(deep[DEEP_PIPELINE_COUNT - 1u], 0u))
            == FVIZ_ERROR_INVALID_ARGUMENT);
        for (i = DEEP_PIPELINE_COUNT; i > 0u; --i) fviz_release(deep[i - 1u]);
    }

    {
        FVizAlgorithm* multi = NULL;
        FVizPipelineRequestInfo release_request;
        uint32_t multi_calls = 0u;
        CHECK(make_algorithm(0u, 2u, multi_output_request, &multi_calls, &multi) == FVIZ_OK);
        CHECK(fviz_algorithm_configure_output_port(multi, 0u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
        CHECK(fviz_algorithm_configure_output_port(multi, 1u, FVIZ_TYPE_POLY_DATA) == FVIZ_OK);
        fviz_pipeline_request_initialize(&release_request);
        release_request.requested_output_port = 0u;
        release_request.flags |= FVIZ_PIPELINE_REQUEST_FLAG_RELEASE_DATA;
        CHECK(fviz_executive_update_request(fviz_algorithm_executive(multi), &release_request) == FVIZ_OK);
        CHECK(multi_calls == 1u);
        CHECK(fviz_algorithm_output_data(multi, 0u) != NULL);
        CHECK(fviz_algorithm_output_data(multi, 1u) == NULL);
        fviz_pipeline_request_initialize(&release_request);
        release_request.requested_output_port = 1u;
        CHECK(fviz_executive_update_request(fviz_algorithm_executive(multi), &release_request) == FVIZ_OK);
        CHECK(multi_calls == 2u);
        CHECK(fviz_algorithm_output_data(multi, 0u) != NULL);
        CHECK(fviz_algorithm_output_data(multi, 1u) != NULL);
        CHECK(fviz_algorithm_release_output_data(multi, 0u) == FVIZ_OK);
        CHECK(fviz_algorithm_output_data(multi, 0u) == NULL);
        CHECK(fviz_algorithm_output_data(multi, 1u) != NULL);
        CHECK(fviz_algorithm_release_output_data(multi, 2u) == FVIZ_ERROR_INVALID_ARGUMENT);
        fviz_algorithm_release_all_output_data(multi);
        CHECK(fviz_algorithm_output_data(multi, 0u) == NULL);
        CHECK(fviz_algorithm_output_data(multi, 1u) == NULL);
        fviz_release(multi);
    }

    fviz_release(v1_algorithm);
    fviz_release(state_algorithm);
    fviz_release(observable_state);
    fviz_release(split);
    fviz_release(merge);
    fviz_release(right);
    fviz_release(left);
    fviz_release(source);
    fviz_cancellation_token_destroy(cancellation);
    puts("custom algorithm executive tests passed");
    return 0;
}
