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

int main(void)
{
    TestState source_state = {{0u}, 0u, 0u, 0u, 0.0};
    TestState split_state = {{0u}, 0u, 0u, 0u, 0.0};
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
    CHECK(fviz_algorithm_update(merge) == FVIZ_OK);
    CHECK(source_state.data_calls == 1u);
    CHECK(fviz_executive_cache_hit_count(fviz_algorithm_executive(merge)) == 1u);

    fviz_pipeline_request_initialize(&request);
    request.requested_output_port = 0u;
    request.piece = 2u;
    request.number_of_pieces = 4u;
    request.has_time = FVIZ_TRUE;
    request.time = 1.25;
    CHECK(fviz_executive_update_request(fviz_algorithm_executive(merge), &request) == FVIZ_OK);
    CHECK(source_state.data_calls == 2u);
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

    fviz_release(split);
    fviz_release(merge);
    fviz_release(right);
    fviz_release(left);
    fviz_release(source);
    fviz_cancellation_token_destroy(cancellation);
    puts("custom algorithm executive tests passed");
    return 0;
}
