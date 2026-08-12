#include <string.h>
#include <stdio.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Parallel/FVizParallel.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Pipeline/FVizAlgorithmPrivate.h>
#include <FViz/Pipeline/FVizExecutivePrivate.h>

static const FVizObjectClass g_fviz_executive_class = {
    FVIZ_TYPE_EXECUTIVE,
    "FVizExecutive",
    &g_fviz_object_class,
    NULL,
    NULL
};

static FVizAtomicU64 g_fviz_transaction_counter = {0};

typedef struct FVizDotWriter
{
    char* text;
    FVizSize capacity;
    FVizSize length;
    FVizAlgorithm* visited[1024];
    uint32_t visited_count;
} FVizDotWriter;

static void fviz_dot_append(FVizDotWriter* writer, const char* value)
{
    FVizSize length = (FVizSize)strlen(value);
    if (writer->text != NULL && writer->capacity > writer->length)
    {
        FVizSize available = writer->capacity - writer->length - 1u;
        FVizSize copied = length < available ? length : available;
        if (copied > 0u) (void)memcpy(writer->text + writer->length, value, copied);
        writer->text[writer->length + copied] = '\0';
    }
    writer->length += length;
}

static FVizBool fviz_dot_was_visited(FVizDotWriter* writer, FVizAlgorithm* algorithm)
{
    uint32_t i;
    for (i = 0u; i < writer->visited_count; ++i)
        if (writer->visited[i] == algorithm) return FVIZ_TRUE;
    if (writer->visited_count < 1024u)
        writer->visited[writer->visited_count++] = algorithm;
    return FVIZ_FALSE;
}

static void fviz_dot_visit(FVizDotWriter* writer, FVizAlgorithm* algorithm)
{
    char line[320];
    uint32_t port;
    if (fviz_dot_was_visited(writer, algorithm) == FVIZ_TRUE) return;
    (void)snprintf(
        line,
        sizeof(line),
        "  n%llu [label=\"%s\\nid=%llu in=%u out=%u\\nexec=%llu cache=%llu\"];\n",
        (unsigned long long)algorithm->diagnostic_id,
        algorithm->base.object_class->type_name,
        (unsigned long long)algorithm->diagnostic_id,
        algorithm->input_port_count,
        algorithm->output_port_count,
        (unsigned long long)algorithm->executive->execution_count,
        (unsigned long long)algorithm->executive->cache_hit_count);
    fviz_dot_append(writer, line);
    for (port = 0u; port < algorithm->input_port_count; ++port)
    {
        FVizSize i;
        for (i = 0u; i < fviz_array_count(algorithm->input_ports[port].connections); ++i)
        {
            FVizAlgorithmConnection* connection = (FVizAlgorithmConnection*)fviz_array_at(
                algorithm->input_ports[port].connections, i);
            fviz_dot_visit(writer, connection->producer);
            (void)snprintf(
                line,
                sizeof(line),
                "  n%llu -> n%llu [label=\"out%u -> in%u\"];\n",
                (unsigned long long)connection->producer->diagnostic_id,
                (unsigned long long)algorithm->diagnostic_id,
                connection->output_port,
                port);
            fviz_dot_append(writer, line);
        }
    }
}

static uint64_t fviz_request_hash(const FVizPipelineRequestInfo* request)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    uint64_t time_bits = 0u;
    uint32_t i;
#define FVIZ_HASH_VALUE(value) \
    do { hash ^= (uint64_t)(value); hash *= UINT64_C(1099511628211); } while (0)
    FVIZ_HASH_VALUE(request->requested_output_port);
    FVIZ_HASH_VALUE(request->piece);
    FVIZ_HASH_VALUE(request->number_of_pieces);
    FVIZ_HASH_VALUE(request->ghost_levels);
    FVIZ_HASH_VALUE(request->has_extent);
    for (i = 0u; i < 6u; ++i) FVIZ_HASH_VALUE((uint64_t)request->extent[i]);
    FVIZ_HASH_VALUE(request->has_time);
    (void)memcpy(&time_bits, &request->time, sizeof(time_bits));
    FVIZ_HASH_VALUE(time_bits);
    FVIZ_HASH_VALUE(request->flags);
#undef FVIZ_HASH_VALUE
    return hash;
}

void fviz_pipeline_request_initialize(FVizPipelineRequestInfo* request)
{
    if (request == NULL) return;
    (void)memset(request, 0, sizeof(*request));
    request->struct_size = (uint32_t)sizeof(*request);
    request->type = FVIZ_PIPELINE_REQUEST_DATA;
    request->number_of_pieces = 1u;
}

static FVizResult fviz_executive_execute_algorithm(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* root_request,
    FVizBool* out_root_executed)
{
    FVizPipelineRequestInfo request = *root_request;
    FVizMTime input_mtime = 0u;
    FVizResult result = FVIZ_OK;
    FVizBool executed = FVIZ_FALSE;
    FVizBool will_execute;
    uint64_t request_key;
    uint32_t port;
    static const FVizPipelineRequest stages[] = {
        FVIZ_PIPELINE_REQUEST_INFORMATION,
        FVIZ_PIPELINE_REQUEST_DATA_OBJECT,
        FVIZ_PIPELINE_REQUEST_UPDATE_EXTENT,
        FVIZ_PIPELINE_REQUEST_DATA
    };
    uint32_t stage;

    if (root_request->cancellation != NULL &&
        fviz_cancellation_token_is_cancelled(root_request->cancellation) != FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_CANCELLED, "pipeline request was cancelled");
        return FVIZ_ERROR_CANCELLED;
    }
    if (algorithm->updating == FVIZ_TRUE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "re-entrant pipeline update detected");
        return FVIZ_ERROR_INVALID_STATE;
    }
    algorithm->updating = FVIZ_TRUE;
    for (port = 0u; port < algorithm->input_port_count; ++port)
    {
        FVizAlgorithmInputPort* input = &algorithm->input_ports[port];
        FVizSize connection_count = fviz_array_count(input->connections);
        FVizSize connection_index;
        if (input->direct_data == NULL && connection_count == 0u && input->info.optional == FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "required algorithm input port is not connected");
            result = FVIZ_ERROR_INVALID_STATE;
            goto done;
        }
        if (input->direct_data != NULL)
        {
            FVizMTime mtime = fviz_object_mtime((const FVizObject*)input->direct_data);
            if (mtime > input_mtime) input_mtime = mtime;
        }
        for (connection_index = 0u; connection_index < connection_count; ++connection_index)
        {
            FVizAlgorithmConnection* connection =
                (FVizAlgorithmConnection*)fviz_array_at(input->connections, connection_index);
            FVizPipelineRequestInfo upstream_request = *root_request;
            FVizDataObject* data;
            FVizMTime mtime;
            upstream_request.requested_output_port = connection->output_port;
            result = fviz_executive_execute_algorithm(
                connection->producer, &upstream_request, NULL);
            if (result != FVIZ_OK) goto done;
            data = fviz_internal_algorithm_resolved_input(
                algorithm, port, (uint32_t)connection_index);
            if (data == NULL)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "upstream algorithm produced no requested data");
                result = FVIZ_ERROR_INVALID_STATE;
                goto done;
            }
            mtime = fviz_object_mtime((const FVizObject*)data);
            if (mtime > input_mtime) input_mtime = mtime;
        }
    }
    request_key = fviz_request_hash(&request);
    will_execute = algorithm->output_ports[request.requested_output_port].updated == FVIZ_TRUE &&
        algorithm->output_ports[request.requested_output_port].last_input_mtime == input_mtime &&
        algorithm->output_ports[request.requested_output_port].last_algorithm_mtime ==
            fviz_object_mtime((const FVizObject*)algorithm) &&
        algorithm->output_ports[request.requested_output_port].last_request_key == request_key
        ? FVIZ_FALSE : FVIZ_TRUE;
    if (will_execute == FVIZ_TRUE && fviz_algorithm_report_progress(algorithm, 0.0) != FVIZ_OK)
    {
        fviz_internal_set_error(FVIZ_ERROR_BUSY, "pipeline update cancelled before execution");
        result = FVIZ_ERROR_BUSY;
        goto done;
    }
    for (stage = 0u; stage < (uint32_t)(sizeof(stages) / sizeof(stages[0])); ++stage)
    {
        if (request.cancellation != NULL &&
            fviz_cancellation_token_is_cancelled(request.cancellation) != FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_CANCELLED, "pipeline request was cancelled during execution");
            result = FVIZ_ERROR_CANCELLED;
            goto done;
        }
        request.type = stages[stage];
        result = fviz_internal_algorithm_process_request(
            algorithm, &request, input_mtime, request_key, &executed);
        if (result != FVIZ_OK) goto done;
    }
    if (executed == FVIZ_TRUE)
    {
        if (fviz_algorithm_report_progress(algorithm, 1.0) != FVIZ_OK)
        {
            fviz_internal_set_error(FVIZ_ERROR_BUSY, "pipeline update cancelled during publication");
            result = FVIZ_ERROR_BUSY;
            goto done;
        }
    }
    if (out_root_executed != NULL) *out_root_executed = executed;

done:
    algorithm->updating = FVIZ_FALSE;
    return result;
}

FVizResult fviz_internal_executive_create(
    FVizAlgorithm* algorithm,
    FVizExecutive** out_executive)
{
    FVizExecutive* executive;
    if (algorithm == NULL || out_executive == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "executive requires an algorithm and output pointer");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_executive = NULL;
    executive = (FVizExecutive*)fviz_internal_object_allocate(
        sizeof(FVizExecutive), &g_fviz_executive_class, NULL);
    if (executive == NULL) return fviz_last_error_code();
    executive->algorithm = algorithm;
    executive->last_result = FVIZ_OK;
    *out_executive = executive;
    return FVIZ_OK;
}

FVizAlgorithm* fviz_executive_algorithm(FVizExecutive* executive)
{
    return executive != NULL ? executive->algorithm : NULL;
}

FVizResult fviz_executive_update(FVizExecutive* executive, uint32_t output_port)
{
    FVizPipelineRequestInfo request;
    fviz_pipeline_request_initialize(&request);
    request.requested_output_port = output_port;
    return fviz_executive_update_request(executive, &request);
}

FVizResult fviz_executive_update_request(
    FVizExecutive* executive,
    const FVizPipelineRequestInfo* requested)
{
    FVizPipelineRequestInfo request;
    FVizResult result;
    FVizBool executed = FVIZ_FALSE;
    if (executive == NULL || executive->algorithm == NULL ||
        requested == NULL || requested->struct_size < sizeof(FVizPipelineRequestInfo) ||
        requested->requested_output_port >= executive->algorithm->output_port_count ||
        requested->number_of_pieces == 0u || requested->piece >= requested->number_of_pieces)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "pipeline request is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    request = *requested;
    request.transaction_id = fviz_atomic_u64_fetch_add(&g_fviz_transaction_counter, 1u) + 1u;
    executive->last_transaction_id = request.transaction_id;
    executive->last_request = FVIZ_PIPELINE_REQUEST_DATA;
    result = fviz_executive_execute_algorithm(executive->algorithm, &request, &executed);
    executive->last_result = result;
    if (result == FVIZ_OK)
    {
        if (executed == FVIZ_TRUE)
            ++executive->execution_count;
        else
            ++executive->cache_hit_count;
    }
    return result;
}

uint64_t fviz_executive_last_transaction_id(const FVizExecutive* executive)
{
    return executive != NULL ? executive->last_transaction_id : 0u;
}

FVizResult fviz_executive_write_dot(
    const FVizExecutive* executive,
    char* text,
    FVizSize capacity,
    FVizSize* out_required_size)
{
    FVizDotWriter writer;
    if (executive == NULL || executive->algorithm == NULL || out_required_size == NULL ||
        (text == NULL && capacity != 0u))
    {
        if (out_required_size != NULL) *out_required_size = 0u;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "DOT diagnostic output contract is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(&writer, 0, sizeof(writer));
    writer.text = text;
    writer.capacity = capacity;
    if (text != NULL && capacity > 0u) text[0] = '\0';
    fviz_dot_append(&writer, "digraph FEAVizPipeline {\n  rankdir=LR;\n");
    fviz_dot_visit(&writer, executive->algorithm);
    fviz_dot_append(&writer, "}\n");
    *out_required_size = writer.length + 1u;
    if (text != NULL && capacity < *out_required_size)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "DOT diagnostic buffer is too small");
        return FVIZ_ERROR_OVERFLOW;
    }
    return FVIZ_OK;
}

FVizPipelineRequest fviz_executive_last_request(const FVizExecutive* executive)
{
    return executive != NULL ? executive->last_request : FVIZ_PIPELINE_REQUEST_NONE;
}

uint64_t fviz_executive_execution_count(const FVizExecutive* executive)
{
    return executive != NULL ? executive->execution_count : 0u;
}

uint64_t fviz_executive_cache_hit_count(const FVizExecutive* executive)
{
    return executive != NULL ? executive->cache_hit_count : 0u;
}

FVizResult fviz_executive_last_result(const FVizExecutive* executive)
{
    return executive != NULL ? executive->last_result : FVIZ_ERROR_INVALID_ARGUMENT;
}

void fviz_executive_reset_statistics(FVizExecutive* executive)
{
    if (executive == NULL) return;
    executive->execution_count = 0u;
    executive->cache_hit_count = 0u;
}
