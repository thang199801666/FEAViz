#include <math.h>
#include <string.h>
#include <stdio.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizArena.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Parallel/FVizParallel.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Pipeline/FVizAlgorithmPrivate.h>
#include <FViz/Pipeline/FVizExecutivePrivate.h>

static void fviz_executive_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_executive_class = {
    FVIZ_TYPE_EXECUTIVE,
    "FVizExecutive",
    &g_fviz_object_class,
    fviz_executive_destroy,
    NULL
};

static FVizAtomicU64 g_fviz_transaction_counter = {0};

typedef struct FVizDotWriter
{
    char* text;
    FVizSize capacity;
    FVizSize length;
} FVizDotWriter;

typedef struct FVizExecutionVisit
{
    FVizAlgorithm* algorithm;
    uint32_t output_port;
    FVizMTime input_mtime;
    FVizMTime algorithm_mtime;
    uint64_t request_key;
} FVizExecutionVisit;

typedef struct FVizExecutionContext
{
    FVizArena* arena;
    FVizExecutionVisit* slots;
    uint8_t* states;
    FVizSize capacity;
    FVizSize count;
} FVizExecutionContext;

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

static uint64_t fviz_execution_visit_hash(
    FVizAlgorithm* algorithm,
    uint32_t output_port,
    FVizMTime input_mtime,
    FVizMTime algorithm_mtime,
    uint64_t request_key)
{
    uint64_t hash = (uint64_t)(uintptr_t)algorithm + UINT64_C(0x9E3779B97F4A7C15);
#define FVIZ_MIX_VISIT(value) do { \
    hash ^= (uint64_t)(value) + UINT64_C(0x9E3779B97F4A7C15) + (hash << 6) + (hash >> 2); \
    hash ^= hash >> 30; hash *= UINT64_C(0xBF58476D1CE4E5B9); \
    hash ^= hash >> 27; hash *= UINT64_C(0x94D049BB133111EB); hash ^= hash >> 31; \
} while (0)
    FVIZ_MIX_VISIT(output_port);
    FVIZ_MIX_VISIT(input_mtime);
    FVIZ_MIX_VISIT(algorithm_mtime);
    FVIZ_MIX_VISIT(request_key);
#undef FVIZ_MIX_VISIT
    return hash;
}

static FVizBool fviz_execution_visit_matches(
    const FVizExecutionVisit* visit,
    FVizAlgorithm* algorithm,
    uint32_t output_port,
    FVizMTime input_mtime,
    FVizMTime algorithm_mtime,
    uint64_t request_key)
{
    return visit->algorithm == algorithm && visit->output_port == output_port &&
        visit->input_mtime == input_mtime && visit->algorithm_mtime == algorithm_mtime &&
        visit->request_key == request_key ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_execution_context_rehash(FVizExecutionContext* context, FVizSize new_capacity)
{
    FVizExecutionVisit* slots;
    uint8_t* states;
    FVizSize slot_bytes;
    FVizSize i;
    if (fviz_size_multiply(new_capacity, sizeof(*slots), &slot_bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    if (context->arena != NULL)
    {
        slots = (FVizExecutionVisit*)fviz_arena_allocate(
            context->arena, slot_bytes, (FVizSize)_Alignof(FVizExecutionVisit));
        states = (uint8_t*)fviz_arena_allocate(context->arena, new_capacity, 1u);
    }
    else
    {
        slots = (FVizExecutionVisit*)fviz_alloc(slot_bytes);
        states = (uint8_t*)fviz_alloc(new_capacity);
    }
    if (slots == NULL || states == NULL)
    {
        if (context->arena == NULL)
        {
            fviz_free(slots);
            fviz_free(states);
        }
        return FVIZ_ERROR_OUT_OF_MEMORY;
    }
    (void)memset(states, 0, new_capacity);
    for (i = 0u; i < context->capacity; ++i)
    {
        FVizExecutionVisit visit;
        FVizSize slot;
        FVizSize mask;
        if (context->states == NULL || context->states[i] == 0u) continue;
        visit = context->slots[i];
        mask = new_capacity - 1u;
        slot = (FVizSize)fviz_execution_visit_hash(
            visit.algorithm, visit.output_port, visit.input_mtime,
            visit.algorithm_mtime, visit.request_key) & mask;
        while (states[slot] != 0u) slot = (slot + 1u) & mask;
        slots[slot] = visit;
        states[slot] = 1u;
    }
    if (context->arena == NULL)
    {
        fviz_free(context->slots);
        fviz_free(context->states);
    }
    context->slots = slots;
    context->states = states;
    context->capacity = new_capacity;
    return FVIZ_OK;
}

static void fviz_execution_context_destroy(FVizExecutionContext* context)
{
    if (context == NULL) return;
    if (context->arena == NULL)
    {
        fviz_free(context->slots);
        fviz_free(context->states);
    }
    (void)memset(context, 0, sizeof(*context));
}

static FVizBool fviz_execution_context_contains(
    const FVizExecutionContext* context,
    FVizAlgorithm* algorithm,
    uint32_t output_port,
    FVizMTime input_mtime,
    FVizMTime algorithm_mtime,
    uint64_t request_key)
{
    FVizSize slot;
    FVizSize mask;
    if (context == NULL || context->capacity == 0u) return FVIZ_FALSE;
    mask = context->capacity - 1u;
    slot = (FVizSize)fviz_execution_visit_hash(
        algorithm, output_port, input_mtime, algorithm_mtime, request_key) & mask;
    while (context->states[slot] != 0u)
    {
        if (fviz_execution_visit_matches(
                &context->slots[slot], algorithm, output_port,
                input_mtime, algorithm_mtime, request_key) != FVIZ_FALSE)
            return FVIZ_TRUE;
        slot = (slot + 1u) & mask;
    }
    return FVIZ_FALSE;
}

static FVizResult fviz_execution_context_add(
    FVizExecutionContext* context,
    FVizAlgorithm* algorithm,
    uint32_t output_port,
    FVizMTime input_mtime,
    FVizMTime algorithm_mtime,
    uint64_t request_key)
{
    FVizSize slot;
    FVizSize mask;
    FVizExecutionVisit* visit;
    if (context == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (context->capacity == 0u || (context->count + 1u) * 10u >= context->capacity * 7u)
    {
        FVizSize new_capacity = context->capacity == 0u ? 64u : context->capacity * 2u;
        if (new_capacity < context->capacity) return FVIZ_ERROR_OVERFLOW;
        if (fviz_execution_context_rehash(context, new_capacity) != FVIZ_OK)
            return fviz_last_error_code();
    }
    mask = context->capacity - 1u;
    slot = (FVizSize)fviz_execution_visit_hash(
        algorithm, output_port, input_mtime, algorithm_mtime, request_key) & mask;
    while (context->states[slot] != 0u)
    {
        if (fviz_execution_visit_matches(
                &context->slots[slot], algorithm, output_port,
                input_mtime, algorithm_mtime, request_key) != FVIZ_FALSE)
            return FVIZ_OK;
        slot = (slot + 1u) & mask;
    }
    visit = &context->slots[slot];
    visit->algorithm = algorithm;
    visit->output_port = output_port;
    visit->input_mtime = input_mtime;
    visit->algorithm_mtime = algorithm_mtime;
    visit->request_key = request_key;
    context->states[slot] = 1u;
    ++context->count;
    return FVIZ_OK;
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


typedef struct FVizDotTraversal
{
    FVizAlgorithm** stack;
    FVizSize stack_count;
    FVizSize stack_capacity;
    FVizAlgorithm** visited;
    uint8_t* states;
    FVizSize visited_count;
    FVizSize visited_capacity;
} FVizDotTraversal;

static uint64_t fviz_dot_pointer_hash(const FVizAlgorithm* algorithm)
{
    uint64_t x = (uint64_t)(uintptr_t)algorithm + UINT64_C(0x9E3779B97F4A7C15);
    x = (x ^ (x >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94D049BB133111EB);
    return x ^ (x >> 31);
}

static void fviz_dot_traversal_destroy(FVizDotTraversal* traversal)
{
    if (traversal == NULL) return;
    fviz_free(traversal->stack);
    fviz_free(traversal->visited);
    fviz_free(traversal->states);
    (void)memset(traversal, 0, sizeof(*traversal));
}

static FVizResult fviz_dot_stack_push(FVizDotTraversal* traversal, FVizAlgorithm* algorithm)
{
    if (traversal->stack_count == traversal->stack_capacity)
    {
        FVizSize capacity = traversal->stack_capacity == 0u ? 64u : traversal->stack_capacity * 2u;
        FVizSize bytes;
        FVizAlgorithm** stack;
        if (capacity < traversal->stack_capacity ||
            fviz_size_multiply(capacity, sizeof(*stack), &bytes) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        stack = (FVizAlgorithm**)fviz_realloc(traversal->stack, bytes);
        if (stack == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
        traversal->stack = stack;
        traversal->stack_capacity = capacity;
    }
    traversal->stack[traversal->stack_count++] = algorithm;
    return FVIZ_OK;
}

static FVizResult fviz_dot_visited_rehash(FVizDotTraversal* traversal, FVizSize capacity)
{
    FVizAlgorithm** visited;
    uint8_t* states;
    FVizSize bytes;
    FVizSize i;
    if (fviz_size_multiply(capacity, sizeof(*visited), &bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    visited = (FVizAlgorithm**)fviz_alloc(bytes);
    states = (uint8_t*)fviz_alloc(capacity);
    if (visited == NULL || states == NULL)
    {
        fviz_free(visited);
        fviz_free(states);
        return FVIZ_ERROR_OUT_OF_MEMORY;
    }
    (void)memset(states, 0, capacity);
    for (i = 0u; i < traversal->visited_capacity; ++i)
    {
        FVizSize slot;
        const FVizSize mask = capacity - 1u;
        if (traversal->states == NULL || traversal->states[i] == 0u) continue;
        slot = (FVizSize)fviz_dot_pointer_hash(traversal->visited[i]) & mask;
        while (states[slot] != 0u) slot = (slot + 1u) & mask;
        visited[slot] = traversal->visited[i];
        states[slot] = 1u;
    }
    fviz_free(traversal->visited);
    fviz_free(traversal->states);
    traversal->visited = visited;
    traversal->states = states;
    traversal->visited_capacity = capacity;
    return FVIZ_OK;
}

static FVizResult fviz_dot_mark_visited(
    FVizDotTraversal* traversal, FVizAlgorithm* algorithm, FVizBool* out_new)
{
    FVizSize slot;
    FVizSize mask;
    if (out_new == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_new = FVIZ_FALSE;
    if (traversal->visited_capacity == 0u ||
        (traversal->visited_count + 1u) * 10u >= traversal->visited_capacity * 7u)
    {
        const FVizSize capacity = traversal->visited_capacity == 0u
            ? 64u : traversal->visited_capacity * 2u;
        if (capacity < traversal->visited_capacity) return FVIZ_ERROR_OVERFLOW;
        if (fviz_dot_visited_rehash(traversal, capacity) != FVIZ_OK)
            return fviz_last_error_code();
    }
    mask = traversal->visited_capacity - 1u;
    slot = (FVizSize)fviz_dot_pointer_hash(algorithm) & mask;
    while (traversal->states[slot] != 0u)
    {
        if (traversal->visited[slot] == algorithm) return FVIZ_OK;
        slot = (slot + 1u) & mask;
    }
    traversal->visited[slot] = algorithm;
    traversal->states[slot] = 1u;
    ++traversal->visited_count;
    *out_new = FVIZ_TRUE;
    return FVIZ_OK;
}

static FVizResult fviz_dot_visit(FVizDotWriter* writer, FVizAlgorithm* root)
{
    FVizDotTraversal traversal;
    FVizResult result = FVIZ_OK;
    (void)memset(&traversal, 0, sizeof(traversal));
    if (fviz_dot_stack_push(&traversal, root) != FVIZ_OK)
    {
        fviz_dot_traversal_destroy(&traversal);
        return fviz_last_error_code();
    }
    while (traversal.stack_count != 0u)
    {
        FVizAlgorithm* algorithm = traversal.stack[--traversal.stack_count];
        FVizBool is_new = FVIZ_FALSE;
        char line[768];
        uint32_t port;
        result = fviz_dot_mark_visited(&traversal, algorithm, &is_new);
        if (result != FVIZ_OK) break;
        if (is_new == FVIZ_FALSE) continue;
        (void)snprintf(
            line, sizeof(line),
            "  n%llu [label=\"%s\\nid=%llu in=%u out=%u\\nrequest=%u result=%d\\nexec=%llu cache=%llu",
            (unsigned long long)algorithm->diagnostic_id,
            algorithm->base.object_class->type_name,
            (unsigned long long)algorithm->diagnostic_id,
            algorithm->input_port_count,
            algorithm->output_port_count,
            (unsigned int)algorithm->executive->last_request,
            (int)algorithm->executive->last_result,
            (unsigned long long)algorithm->executive->execution_count,
            (unsigned long long)algorithm->executive->cache_hit_count);
        for (port = 0u; port < algorithm->output_port_count; ++port)
        {
            FVizDataObject* data = algorithm->output_ports[port].data;
            (void)snprintf(
                line + strlen(line), sizeof(line) - strlen(line),
                "\\nout%u=%llu", port,
                (unsigned long long)(data != NULL
                    ? fviz_object_mtime((const FVizObject*)data) : 0u));
        }
        (void)snprintf(line + strlen(line), sizeof(line) - strlen(line), "\"];\n");
        fviz_dot_append(writer, line);
        for (port = 0u; port < algorithm->input_port_count; ++port)
        {
            FVizSize i;
            FVizAlgorithmInputPort* input = &algorithm->input_ports[port];
            for (i = 0u; i < fviz_array_count(input->connections); ++i)
            {
                FVizAlgorithmConnection* connection =
                    (FVizAlgorithmConnection*)fviz_array_at(input->connections, i);
                (void)snprintf(
                    line, sizeof(line),
                    "  n%llu -> n%llu [label=\"out%u -> in%u\"];\n",
                    (unsigned long long)connection->producer->diagnostic_id,
                    (unsigned long long)algorithm->diagnostic_id,
                    connection->output_port, port);
                fviz_dot_append(writer, line);
                result = fviz_dot_stack_push(&traversal, connection->producer);
                if (result != FVIZ_OK) break;
            }
            if (result != FVIZ_OK) break;
        }
        if (result != FVIZ_OK) break;
    }
    fviz_dot_traversal_destroy(&traversal);
    return result;
}

void fviz_pipeline_request_initialize(FVizPipelineRequestInfo* request)
{
    if (request == NULL) return;
    (void)memset(request, 0, sizeof(*request));
    request->struct_size = (uint32_t)sizeof(*request);
    request->type = FVIZ_PIPELINE_REQUEST_DATA;
    request->number_of_pieces = 1u;
}


FVizResult fviz_pipeline_request_set_time(FVizPipelineRequestInfo* request, double time)
{
    if (request == NULL || !isfinite(time))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "pipeline time request must be finite");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    request->has_time = FVIZ_TRUE;
    request->time = time;
    return FVIZ_OK;
}

void fviz_pipeline_request_clear_time(FVizPipelineRequestInfo* request)
{
    if (request == NULL) return;
    request->has_time = FVIZ_FALSE;
    request->time = 0.0;
}

FVizResult fviz_pipeline_request_set_piece(
    FVizPipelineRequestInfo* request, uint32_t piece, uint32_t number_of_pieces, uint32_t ghost_levels)
{
    if (request == NULL || number_of_pieces == 0u || piece >= number_of_pieces)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "pipeline piece request is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    request->piece = piece;
    request->number_of_pieces = number_of_pieces;
    request->ghost_levels = ghost_levels;
    return FVIZ_OK;
}

FVizResult fviz_pipeline_request_set_extent(
    FVizPipelineRequestInfo* request, const int64_t extent[6])
{
    uint32_t axis;
    if (request == NULL || extent == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "pipeline extent request is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (axis = 0u; axis < 3u; ++axis)
    {
        if (extent[axis * 2u] > extent[axis * 2u + 1u])
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "pipeline extent minimum exceeds maximum");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    (void)memcpy(request->extent, extent, 6u * sizeof(int64_t));
    request->has_extent = FVIZ_TRUE;
    return FVIZ_OK;
}

void fviz_pipeline_request_clear_extent(FVizPipelineRequestInfo* request)
{
    if (request == NULL) return;
    request->has_extent = FVIZ_FALSE;
    (void)memset(request->extent, 0, sizeof(request->extent));
}

static FVizResult fviz_executive_inherit_time_metadata(FVizAlgorithm* algorithm,uint32_t output_port)
{
    FVizAlgorithmConnection* inherited = NULL;
    uint32_t port;
    for (port=0u;port<algorithm->input_port_count;++port)
    {
        FVizAlgorithmInputPort* input=&algorithm->input_ports[port];
        FVizSize i;
        for (i=0u;i<fviz_array_count(input->connections);++i)
        {
            FVizAlgorithmConnection* connection=(FVizAlgorithmConnection*)fviz_array_at(input->connections,i);
            if (inherited!=NULL) return FVIZ_OK; /* Ambiguous multi-source time domain: do not guess. */
            inherited=connection;
        }
    }
    if (inherited!=NULL)
    {
        FVizSize count=0u;
        const double* steps=fviz_algorithm_output_time_steps(inherited->producer,inherited->output_port,&count);
        return fviz_algorithm_set_output_time_steps(algorithm,output_port,steps,count);
    }
    return FVIZ_OK;
}

typedef enum FVizExecutiveFramePhase
{
    FVIZ_EXEC_FRAME_ENTER = 0,
    FVIZ_EXEC_FRAME_INPUTS,
    FVIZ_EXEC_FRAME_AFTER_CHILD,
    FVIZ_EXEC_FRAME_PROCESS
} FVizExecutiveFramePhase;

typedef struct FVizExecutiveFrame
{
    FVizAlgorithm* algorithm;
    FVizPipelineRequestInfo request;
    FVizMTime input_mtime;
    FVizMTime algorithm_mtime;
    uint64_t request_key;
    uint32_t input_port;
    FVizSize connection_index;
    FVizBool port_initialized;
    FVizBool executed;
    FVizBool start_event_emitted;
    FVizExecutiveFramePhase phase;
} FVizExecutiveFrame;

typedef struct FVizExecutiveFrameStack
{
    FVizArena* arena;
    FVizExecutiveFrame* frames;
    FVizSize count;
    FVizSize capacity;
} FVizExecutiveFrameStack;

static FVizResult fviz_executive_frame_stack_reserve(
    FVizExecutiveFrameStack* stack, FVizSize minimum_capacity)
{
    FVizExecutiveFrame* frames;
    FVizSize capacity;
    FVizSize bytes;
    if (stack->capacity >= minimum_capacity) return FVIZ_OK;
    capacity = stack->capacity == 0u ? 64u : stack->capacity;
    while (capacity < minimum_capacity)
    {
        if (capacity > (FVizSize)-1 / 2u)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "pipeline execution stack capacity overflow");
            return FVIZ_ERROR_OVERFLOW;
        }
        capacity *= 2u;
    }
    if (fviz_size_multiply(capacity, sizeof(*frames), &bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    if (stack->arena != NULL)
        frames = (FVizExecutiveFrame*)fviz_arena_allocate(
            stack->arena, bytes, (FVizSize)_Alignof(FVizExecutiveFrame));
    else
        frames = (FVizExecutiveFrame*)fviz_alloc(bytes);
    if (frames == NULL) return fviz_last_error_code();
    if (stack->frames != NULL && stack->count != 0u)
        (void)memcpy(frames, stack->frames, stack->count * sizeof(*frames));
    if (stack->arena == NULL) fviz_free(stack->frames);
    stack->frames = frames;
    stack->capacity = capacity;
    return FVIZ_OK;
}

static FVizResult fviz_executive_frame_stack_push(
    FVizExecutiveFrameStack* stack,
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request)
{
    FVizExecutiveFrame* frame;
    FVizResult result;
    result = fviz_executive_frame_stack_reserve(stack, stack->count + 1u);
    if (result != FVIZ_OK) return result;
    frame = &stack->frames[stack->count++];
    (void)memset(frame, 0, sizeof(*frame));
    frame->algorithm = algorithm;
    frame->request = *request;
    frame->phase = FVIZ_EXEC_FRAME_ENTER;
    return FVIZ_OK;
}

static void fviz_executive_frame_stack_destroy(FVizExecutiveFrameStack* stack)
{
    if (stack == NULL) return;
    if (stack->arena == NULL) fviz_free(stack->frames);
    (void)memset(stack, 0, sizeof(*stack));
}

static void fviz_executive_unwind_frames(
    FVizExecutiveFrameStack* stack, FVizResult result)
{
    while (stack != NULL && stack->count != 0u)
    {
        FVizExecutiveFrame* frame = &stack->frames[stack->count - 1u];
        if (frame->start_event_emitted != FVIZ_FALSE)
            (void)fviz_object_invoke_event((FVizObject*)frame->algorithm, FVIZ_EVENT_END, &result);
        frame->algorithm->updating = FVIZ_FALSE;
        --stack->count;
    }
}

static FVizResult fviz_executive_execute_algorithm(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* root_request,
    FVizExecutionContext* context,
    FVizBool* out_root_executed)
{
    static const FVizPipelineRequest stages[] = {
        FVIZ_PIPELINE_REQUEST_INFORMATION,
        FVIZ_PIPELINE_REQUEST_DATA_OBJECT,
        FVIZ_PIPELINE_REQUEST_UPDATE_EXTENT,
        FVIZ_PIPELINE_REQUEST_DATA
    };
    FVizExecutiveFrameStack stack;
    FVizResult result;
    FVizBool root_executed = FVIZ_FALSE;
    (void)memset(&stack, 0, sizeof(stack));
    stack.arena = context != NULL ? context->arena : NULL;
    if (out_root_executed != NULL) *out_root_executed = FVIZ_FALSE;
    result = fviz_executive_frame_stack_push(&stack, algorithm, root_request);
    if (result != FVIZ_OK) goto failed;

    while (stack.count != 0u)
    {
        FVizExecutiveFrame* frame = &stack.frames[stack.count - 1u];
        FVizAlgorithm* current = frame->algorithm;

        if (frame->phase == FVIZ_EXEC_FRAME_ENTER)
        {
            if (frame->request.cancellation != NULL &&
                fviz_cancellation_token_is_cancelled(frame->request.cancellation) != FVIZ_FALSE)
            {
                fviz_internal_set_error(FVIZ_ERROR_CANCELLED, "pipeline request was cancelled");
                result = FVIZ_ERROR_CANCELLED;
                goto failed;
            }
            if (fviz_algorithm_abort_requested(current) != FVIZ_FALSE)
            {
                fviz_internal_set_error(FVIZ_ERROR_BUSY, "algorithm execution was aborted");
                result = FVIZ_ERROR_BUSY;
                goto failed;
            }
            if (current->updating == FVIZ_TRUE)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "re-entrant pipeline update detected");
                result = FVIZ_ERROR_INVALID_STATE;
                goto failed;
            }
            /* Dependency objects and upstream producers bridge ModifiedEvent into
             * the owning algorithm.  Therefore an unchanged algorithm MTime plus
             * an identical request key proves that the previously published output
             * remains valid without walking the upstream graph again. */
            frame->request_key = fviz_request_hash(&frame->request);
            frame->algorithm_mtime = fviz_object_mtime((const FVizObject*)current);
            if (current->output_ports[frame->request.requested_output_port].updated == FVIZ_TRUE &&
                current->output_ports[frame->request.requested_output_port].last_algorithm_mtime == frame->algorithm_mtime &&
                current->output_ports[frame->request.requested_output_port].last_request_key == frame->request_key)
            {
                frame->input_mtime =
                    current->output_ports[frame->request.requested_output_port].last_input_mtime;
                if (fviz_execution_context_contains(
                        context, current, frame->request.requested_output_port,
                        frame->input_mtime, frame->algorithm_mtime, frame->request_key) == FVIZ_FALSE)
                {
                    result = fviz_execution_context_add(
                        context, current, frame->request.requested_output_port,
                        frame->input_mtime, frame->algorithm_mtime, frame->request_key);
                    if (result != FVIZ_OK) goto failed;
                }
                if (stack.count == 1u) root_executed = FVIZ_FALSE;
                --stack.count;
                continue;
            }
            current->updating = FVIZ_TRUE;
            frame->input_port = 0u;
            frame->connection_index = 0u;
            frame->port_initialized = FVIZ_FALSE;
            frame->input_mtime = 0u;
            frame->phase = FVIZ_EXEC_FRAME_INPUTS;
            continue;
        }

        if (frame->phase == FVIZ_EXEC_FRAME_INPUTS)
        {
            FVizBool pushed_child = FVIZ_FALSE;
            while (frame->input_port < current->input_port_count)
            {
                FVizAlgorithmInputPort* input = &current->input_ports[frame->input_port];
                const FVizSize connection_count = fviz_array_count(input->connections);
                if (frame->port_initialized == FVIZ_FALSE)
                {
                    if (input->direct_data == NULL && connection_count == 0u &&
                        input->info.optional == FVIZ_FALSE)
                    {
                        fviz_internal_set_error(
                            FVIZ_ERROR_INVALID_STATE, "required algorithm input port is not connected");
                        result = FVIZ_ERROR_INVALID_STATE;
                        goto failed;
                    }
                    if (input->direct_data != NULL)
                    {
                        const FVizMTime mtime = fviz_object_mtime((const FVizObject*)input->direct_data);
                        if (mtime > frame->input_mtime) frame->input_mtime = mtime;
                    }
                    frame->connection_index = 0u;
                    frame->port_initialized = FVIZ_TRUE;
                }
                if (frame->connection_index < connection_count)
                {
                    FVizAlgorithmConnection* connection = (FVizAlgorithmConnection*)fviz_array_at(
                        input->connections, frame->connection_index);
                    FVizPipelineRequestInfo upstream_request;
                    result = fviz_internal_algorithm_map_input_request(
                        current, frame->input_port, (uint32_t)frame->connection_index,
                        &frame->request, &upstream_request);
                    if (result != FVIZ_OK) goto failed;
                    upstream_request.requested_output_port = connection->output_port;
                    upstream_request.transaction_id = frame->request.transaction_id;
                    frame->phase = FVIZ_EXEC_FRAME_AFTER_CHILD;
                    result = fviz_executive_frame_stack_push(
                        &stack, connection->producer, &upstream_request);
                    if (result != FVIZ_OK) goto failed;
                    pushed_child = FVIZ_TRUE;
                    break;
                }
                ++frame->input_port;
                frame->port_initialized = FVIZ_FALSE;
            }
            if (pushed_child != FVIZ_FALSE) continue;
            frame->phase = FVIZ_EXEC_FRAME_PROCESS;
            continue;
        }

        if (frame->phase == FVIZ_EXEC_FRAME_AFTER_CHILD)
        {
            FVizAlgorithmInputPort* input = &current->input_ports[frame->input_port];
            FVizDataObject* data = fviz_internal_algorithm_resolved_input(
                current, frame->input_port, (uint32_t)frame->connection_index);
            (void)input;
            if (data == NULL)
            {
                fviz_internal_set_error(
                    FVIZ_ERROR_INVALID_STATE, "upstream algorithm produced no requested data");
                result = FVIZ_ERROR_INVALID_STATE;
                goto failed;
            }
            {
                const FVizMTime mtime = fviz_object_mtime((const FVizObject*)data);
                if (mtime > frame->input_mtime) frame->input_mtime = mtime;
            }
            ++frame->connection_index;
            frame->phase = FVIZ_EXEC_FRAME_INPUTS;
            continue;
        }

        if (frame->phase == FVIZ_EXEC_FRAME_PROCESS)
        {
            FVizBool will_execute;
            uint32_t stage;
            will_execute = current->output_ports[frame->request.requested_output_port].updated == FVIZ_TRUE &&
                current->output_ports[frame->request.requested_output_port].last_input_mtime == frame->input_mtime &&
                current->output_ports[frame->request.requested_output_port].last_algorithm_mtime == frame->algorithm_mtime &&
                current->output_ports[frame->request.requested_output_port].last_request_key == frame->request_key
                ? FVIZ_FALSE : FVIZ_TRUE;
            if (fviz_execution_context_contains(
                    context, current, frame->request.requested_output_port,
                    frame->input_mtime, frame->algorithm_mtime, frame->request_key) != FVIZ_FALSE)
            {
                frame->executed = FVIZ_FALSE;
            }
            else if (will_execute == FVIZ_FALSE)
            {
                frame->executed = FVIZ_FALSE;
                result = fviz_execution_context_add(
                    context, current, frame->request.requested_output_port,
                    frame->input_mtime, frame->algorithm_mtime, frame->request_key);
                if (result != FVIZ_OK) goto failed;
            }
            else
            {
                frame->start_event_emitted = FVIZ_TRUE;
                if (fviz_object_invoke_event(
                        (FVizObject*)current, FVIZ_EVENT_START, &frame->request) != FVIZ_FALSE)
                    fviz_algorithm_request_abort(current);
                if (fviz_algorithm_report_progress(current, 0.0) != FVIZ_OK)
                {
                    fviz_internal_set_error(
                        FVIZ_ERROR_BUSY, "pipeline update cancelled before execution");
                    result = FVIZ_ERROR_BUSY;
                    goto failed;
                }
                frame->executed = FVIZ_FALSE;
                for (stage = 0u; stage < (uint32_t)(sizeof(stages) / sizeof(stages[0])); ++stage)
                {
                    if (frame->request.cancellation != NULL &&
                        fviz_cancellation_token_is_cancelled(frame->request.cancellation) != FVIZ_FALSE)
                    {
                        fviz_internal_set_error(
                            FVIZ_ERROR_CANCELLED, "pipeline request was cancelled during execution");
                        result = FVIZ_ERROR_CANCELLED;
                        goto failed;
                    }
                    frame->request.type = stages[stage];
                    if (frame->request.type == FVIZ_PIPELINE_REQUEST_INFORMATION &&
                        fviz_executive_inherit_time_metadata(
                            current, frame->request.requested_output_port) != FVIZ_OK)
                    {
                        result = fviz_last_error_code();
                        goto failed;
                    }
                    result = fviz_internal_algorithm_process_request(
                        current, &frame->request, frame->input_mtime,
                        frame->request_key, &frame->executed);
                    if (result != FVIZ_OK) goto failed;
                }
                if (frame->executed != FVIZ_FALSE &&
                    fviz_algorithm_report_progress(current, 1.0) != FVIZ_OK)
                {
                    fviz_internal_set_error(
                        FVIZ_ERROR_BUSY, "pipeline update cancelled during publication");
                    result = FVIZ_ERROR_BUSY;
                    goto failed;
                }
                result = fviz_execution_context_add(
                    context, current, frame->request.requested_output_port,
                    frame->input_mtime, frame->algorithm_mtime, frame->request_key);
                if (result != FVIZ_OK) goto failed;
            }

            if (stack.count == 1u) root_executed = frame->executed;
            if (frame->start_event_emitted != FVIZ_FALSE)
                (void)fviz_object_invoke_event((FVizObject*)current, FVIZ_EVENT_END, &result);
            current->updating = FVIZ_FALSE;
            --stack.count;
            continue;
        }
    }

    if (out_root_executed != NULL) *out_root_executed = root_executed;
    fviz_executive_frame_stack_destroy(&stack);
    return FVIZ_OK;

failed:
    fviz_executive_unwind_frames(&stack, result);
    fviz_executive_frame_stack_destroy(&stack);
    return result;
}
static void fviz_executive_destroy(FVizObject* object)
{
    FVizExecutive* executive = (FVizExecutive*)object;
    fviz_arena_destroy(executive->scratch_arena);
    executive->scratch_arena = NULL;
    executive->algorithm = NULL;
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
    if (fviz_arena_create(32768u, &executive->scratch_arena) != FVIZ_OK)
    {
        fviz_release(executive);
        return fviz_last_error_code();
    }
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
    FVizExecutionContext context;
    if (executive == NULL || executive->algorithm == NULL ||
        requested == NULL || requested->struct_size < sizeof(FVizPipelineRequestInfo) ||
        requested->requested_output_port >= executive->algorithm->output_port_count ||
        requested->number_of_pieces == 0u || requested->piece >= requested->number_of_pieces)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "pipeline request is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    request = *requested;
    fviz_arena_reset(executive->scratch_arena);
    (void)memset(&context, 0, sizeof(context));
    context.arena = executive->scratch_arena;
    request.transaction_id = fviz_atomic_u64_fetch_add(&g_fviz_transaction_counter, 1u) + 1u;
    executive->last_transaction_id = request.transaction_id;
    executive->last_request = request.type;
    result = fviz_executive_execute_algorithm(executive->algorithm, &request, &context, &executed);
    fviz_execution_context_destroy(&context);
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
    {
        FVizResult traversal_result = fviz_dot_visit(&writer, executive->algorithm);
        if (traversal_result != FVIZ_OK) return traversal_result;
    }
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

FVizResult fviz_executive_update_piece(
    FVizExecutive* executive, uint32_t output_port,
    uint32_t piece, uint32_t number_of_pieces, uint32_t ghost_levels)
{
    FVizPipelineRequestInfo request;
    fviz_pipeline_request_initialize(&request);
    request.requested_output_port = output_port;
    if (fviz_pipeline_request_set_piece(&request, piece, number_of_pieces, ghost_levels) != FVIZ_OK)
        return fviz_last_error_code();
    return fviz_executive_update_request(executive, &request);
}

FVizResult fviz_executive_update_extent(
    FVizExecutive* executive, uint32_t output_port,
    const int64_t extent[6], uint32_t ghost_levels)
{
    FVizPipelineRequestInfo request;
    fviz_pipeline_request_initialize(&request);
    request.requested_output_port = output_port;
    request.ghost_levels = ghost_levels;
    if (fviz_pipeline_request_set_extent(&request, extent) != FVIZ_OK)
        return fviz_last_error_code();
    return fviz_executive_update_request(executive, &request);
}

FVizResult fviz_executive_update_time(
    FVizExecutive* executive, uint32_t output_port, double time)
{
    FVizPipelineRequestInfo request;
    fviz_pipeline_request_initialize(&request);
    request.requested_output_port = output_port;
    if (fviz_pipeline_request_set_time(&request, time) != FVIZ_OK)
        return fviz_last_error_code();
    return fviz_executive_update_request(executive, &request);
}

