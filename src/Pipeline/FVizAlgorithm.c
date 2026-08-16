#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Pipeline/FVizAlgorithm.h>
#include <FViz/Parallel/FVizExecutor.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Pipeline/FVizAlgorithmPrivate.h>
#include <FViz/Pipeline/FVizExecutivePrivate.h>

const FVizObjectClass g_fviz_algorithm_class = {FVIZ_TYPE_ALGORITHM, "FVizAlgorithm", &g_fviz_object_class, NULL, NULL};
static FVizAtomicU64 g_fviz_algorithm_id_counter = {0};

static void fviz_custom_algorithm_destroy(FVizObject* object);
static FVizMTime fviz_custom_algorithm_mtime(const FVizObject* object);
static void fviz_algorithm_unobserve_state_object(FVizAlgorithm* algorithm);
static const FVizObjectClass g_fviz_custom_algorithm_class = {FVIZ_TYPE_ALGORITHM, "FVizCustomAlgorithm",
                                                              &g_fviz_algorithm_class, fviz_custom_algorithm_destroy,
                                                              fviz_custom_algorithm_mtime};

static FVizBool fviz_algorithm_state_object_event(FVizObject* caller, FVizEventId event_id, void* call_data,
                                                  void* client_data)
{
    FVizAlgorithm* algorithm = (FVizAlgorithm*)client_data;
    (void)call_data;
    if (algorithm == NULL) return FVIZ_FALSE;
    if (event_id == FVIZ_EVENT_MODIFIED)
    {
        algorithm->updated = FVIZ_FALSE;
        fviz_object_modified((FVizObject*)algorithm);
    }
    else if (event_id == FVIZ_EVENT_DELETE && caller == algorithm->observed_state_object)
    {
        algorithm->observed_state_object = NULL;
        algorithm->observed_state_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
        algorithm->observed_state_delete_tag = FVIZ_OBSERVER_TAG_INVALID;
    }
    return FVIZ_FALSE;
}

static void fviz_algorithm_unobserve_state_object(FVizAlgorithm* algorithm)
{
    FVizObject* state_object;
    if (algorithm == NULL) return;
    state_object = algorithm->observed_state_object;
    if (state_object != NULL && algorithm->observed_state_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer(state_object, algorithm->observed_state_modified_tag);
    if (state_object != NULL && algorithm->observed_state_delete_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer(state_object, algorithm->observed_state_delete_tag);
    algorithm->observed_state_object = NULL;
    algorithm->observed_state_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    algorithm->observed_state_delete_tag = FVIZ_OBSERVER_TAG_INVALID;
}

static FVizResult fviz_algorithm_observe_state_object(FVizAlgorithm* algorithm, FVizObject* state_object)
{
    FVizObserverTag modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag delete_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (algorithm == NULL || state_object == NULL) return FVIZ_OK;
    if (fviz_object_add_observer(state_object, FVIZ_EVENT_MODIFIED, 0.0f, fviz_algorithm_state_object_event, algorithm,
                                 &modified_tag) != FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_object_add_observer(state_object, FVIZ_EVENT_DELETE, 0.0f, fviz_algorithm_state_object_event, algorithm,
                                 &delete_tag) != FVIZ_OK)
    {
        (void)fviz_object_remove_observer(state_object, modified_tag);
        return fviz_last_error_code();
    }
    algorithm->observed_state_object = state_object;
    algorithm->observed_state_modified_tag = modified_tag;
    algorithm->observed_state_delete_tag = delete_tag;
    return FVIZ_OK;
}

static void fviz_custom_algorithm_destroy(FVizObject* object)
{
    FVizAlgorithm* algorithm = (FVizAlgorithm*)object;
    fviz_algorithm_unobserve_state_object(algorithm);
    if (algorithm->callbacks.destroy_state != NULL) algorithm->callbacks.destroy_state(algorithm->state);
    algorithm->state = NULL;
    fviz_internal_algorithm_deinitialize(algorithm);
}

static FVizMTime fviz_custom_algorithm_mtime(const FVizObject* object)
{
    const FVizAlgorithm* algorithm = (const FVizAlgorithm*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    if (algorithm->callbacks.get_state_mtime != NULL)
    {
        FVizMTime state_mtime = algorithm->callbacks.get_state_mtime(algorithm->state);
        if (state_mtime > mtime) mtime = state_mtime;
    }
    return mtime;
}

void fviz_algorithm_callbacks_initialize(FVizAlgorithmCallbacks* callbacks)
{
    if (callbacks == NULL) return;
    (void)memset(callbacks, 0, sizeof(*callbacks));
    callbacks->struct_size = (uint32_t)sizeof(*callbacks);
}

FVizResult fviz_algorithm_create(uint32_t input_port_count, uint32_t output_port_count,
                                 const FVizAlgorithmCallbacks* callbacks, void* state, FVizAlgorithm** out_algorithm)
{
    FVizAlgorithm* algorithm;
    if (out_algorithm == NULL || callbacks == NULL || callbacks->struct_size < FVIZ_ALGORITHM_CALLBACKS_V1_SIZE ||
        callbacks->process_request == NULL || output_port_count == 0u)
    {
        if (out_algorithm != NULL) *out_algorithm = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "custom algorithm contract is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_algorithm = NULL;
    algorithm =
        (FVizAlgorithm*)fviz_internal_object_allocate(sizeof(FVizAlgorithm), &g_fviz_custom_algorithm_class, NULL);
    if (algorithm == NULL) return fviz_last_error_code();
    (void)memset(&algorithm->callbacks, 0, sizeof(algorithm->callbacks));
    (void)memcpy(&algorithm->callbacks, callbacks,
                 callbacks->struct_size < sizeof(FVizAlgorithmCallbacks) ? callbacks->struct_size
                                                                         : sizeof(FVizAlgorithmCallbacks));
    algorithm->callbacks.struct_size = (uint32_t)sizeof(FVizAlgorithmCallbacks);
    algorithm->state = state;
    algorithm->custom = FVIZ_TRUE;
    if (fviz_internal_algorithm_initialize(algorithm, input_port_count, output_port_count, NULL) != FVIZ_OK ||
        fviz_algorithm_observe_state_object(algorithm, algorithm->callbacks.state_object) != FVIZ_OK)
    {
        fviz_release(algorithm);
        return fviz_last_error_code();
    }
    *out_algorithm = algorithm;
    return FVIZ_OK;
}

void* fviz_algorithm_state(FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? algorithm->state : NULL;
}

const void* fviz_algorithm_const_state(const FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? algorithm->state : NULL;
}

uint64_t fviz_algorithm_diagnostic_id(const FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? algorithm->diagnostic_id : 0u;
}

static FVizBool fviz_algorithm_port_accepts(FVizTypeId accepted, FVizTypeId produced)
{
    return accepted == FVIZ_TYPE_DATA_OBJECT || accepted == produced ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_algorithm_validate_input_port(const FVizAlgorithm* algorithm, uint32_t port)
{
    if (algorithm == NULL || port >= algorithm->input_port_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm input port is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return FVIZ_OK;
}

static FVizResult fviz_algorithm_validate_output_port(const FVizAlgorithm* algorithm, uint32_t port)
{
    if (algorithm == NULL || port >= algorithm->output_port_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm output port is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return FVIZ_OK;
}

static FVizBool fviz_algorithm_input_dependency_modified(FVizObject* caller, FVizEventId event_id, void* call_data,
                                                         void* client_data)
{
    FVizAlgorithm* algorithm = (FVizAlgorithm*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (algorithm != NULL)
    {
        algorithm->updated = FVIZ_FALSE;
        fviz_object_modified((FVizObject*)algorithm);
    }
    return FVIZ_FALSE;
}

static FVizResult fviz_algorithm_observe_input_dependency(FVizAlgorithm* algorithm, FVizObject* dependency,
                                                          FVizObserverTag* out_tag)
{
    if (algorithm == NULL || out_tag == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (dependency == NULL) return FVIZ_OK;
    return fviz_object_add_observer(dependency, FVIZ_EVENT_MODIFIED, 0.0f, fviz_algorithm_input_dependency_modified,
                                    algorithm, out_tag);
}

static void fviz_algorithm_remove_input_observer(FVizObject* dependency, FVizObserverTag* tag)
{
    if (dependency != NULL && tag != NULL && *tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer(dependency, *tag);
    if (tag != NULL) *tag = FVIZ_OBSERVER_TAG_INVALID;
}

static void fviz_algorithm_clear_connections(FVizAlgorithmInputPort* input_port)
{
    FVizSize i;
    for (i = 0u; i < fviz_array_count(input_port->connections); ++i)
    {
        FVizAlgorithmConnection* connection = (FVizAlgorithmConnection*)fviz_array_at(input_port->connections, i);
        fviz_algorithm_remove_input_observer((FVizObject*)connection->producer, &connection->producer_modified_tag);
        fviz_release(connection->producer);
    }
    fviz_array_clear(input_port->connections);
}

typedef struct FVizAlgorithmTraversal
{
    const FVizAlgorithm** stack;
    FVizSize stack_count;
    FVizSize stack_capacity;
    const FVizAlgorithm** visited;
    FVizSize visited_count;
    FVizSize visited_capacity;
} FVizAlgorithmTraversal;

static FVizSize fviz_algorithm_pointer_hash(const FVizAlgorithm* algorithm)
{
    uintptr_t value = (uintptr_t)algorithm;
    value >>= 4u;
#if UINTPTR_MAX > UINT32_MAX
    value ^= value >> 33u;
    value *= (uintptr_t)UINT64_C(0xff51afd7ed558ccd);
    value ^= value >> 33u;
    value *= (uintptr_t)UINT64_C(0xc4ceb9fe1a85ec53);
    value ^= value >> 33u;
#else
    value ^= value >> 16u;
    value *= (uintptr_t)UINT32_C(0x7feb352d);
    value ^= value >> 15u;
    value *= (uintptr_t)UINT32_C(0x846ca68b);
    value ^= value >> 16u;
#endif
    return (FVizSize)value;
}

static FVizResult fviz_algorithm_traversal_push(FVizAlgorithmTraversal* traversal, const FVizAlgorithm* algorithm)
{
    if (traversal->stack_count == traversal->stack_capacity)
    {
        FVizSize new_capacity = traversal->stack_capacity == 0u ? 32u : traversal->stack_capacity * 2u;
        FVizSize bytes;
        const FVizAlgorithm** resized;
        if (new_capacity < traversal->stack_capacity ||
            fviz_size_multiply(new_capacity, sizeof(*traversal->stack), &bytes) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        resized = (const FVizAlgorithm**)fviz_realloc(traversal->stack, bytes);
        if (resized == NULL) return fviz_last_error_code();
        traversal->stack = resized;
        traversal->stack_capacity = new_capacity;
    }
    traversal->stack[traversal->stack_count++] = algorithm;
    return FVIZ_OK;
}

static FVizResult fviz_algorithm_traversal_rehash(FVizAlgorithmTraversal* traversal, FVizSize new_capacity)
{
    const FVizAlgorithm** table;
    FVizSize bytes;
    FVizSize i;
    if (fviz_size_multiply(new_capacity, sizeof(*table), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    table = (const FVizAlgorithm**)fviz_alloc(bytes);
    if (table == NULL) return fviz_last_error_code();
    (void)memset(table, 0, (size_t)bytes);
    for (i = 0u; i < traversal->visited_capacity; ++i)
    {
        const FVizAlgorithm* algorithm = traversal->visited[i];
        if (algorithm != NULL)
        {
            FVizSize slot = fviz_algorithm_pointer_hash(algorithm) & (new_capacity - 1u);
            while (table[slot] != NULL)
                slot = (slot + 1u) & (new_capacity - 1u);
            table[slot] = algorithm;
        }
    }
    fviz_free(traversal->visited);
    traversal->visited = table;
    traversal->visited_capacity = new_capacity;
    return FVIZ_OK;
}

static FVizResult fviz_algorithm_traversal_visit(FVizAlgorithmTraversal* traversal, const FVizAlgorithm* algorithm,
                                                 FVizBool* out_new)
{
    FVizSize slot;
    if (traversal->visited_capacity == 0u || (traversal->visited_count + 1u) * 10u >= traversal->visited_capacity * 7u)
    {
        FVizSize new_capacity = traversal->visited_capacity == 0u ? 64u : traversal->visited_capacity * 2u;
        if (new_capacity < traversal->visited_capacity) return FVIZ_ERROR_OVERFLOW;
        if (fviz_algorithm_traversal_rehash(traversal, new_capacity) != FVIZ_OK) return fviz_last_error_code();
    }
    slot = fviz_algorithm_pointer_hash(algorithm) & (traversal->visited_capacity - 1u);
    while (traversal->visited[slot] != NULL)
    {
        if (traversal->visited[slot] == algorithm)
        {
            *out_new = FVIZ_FALSE;
            return FVIZ_OK;
        }
        slot = (slot + 1u) & (traversal->visited_capacity - 1u);
    }
    traversal->visited[slot] = algorithm;
    ++traversal->visited_count;
    *out_new = FVIZ_TRUE;
    return FVIZ_OK;
}

static FVizResult fviz_algorithm_reaches(const FVizAlgorithm* start, const FVizAlgorithm* target, FVizBool* out_reaches)
{
    FVizAlgorithmTraversal traversal = {0};
    FVizResult result = FVIZ_OK;
    if (out_reaches == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_reaches = FVIZ_FALSE;
    if (start == NULL) return FVIZ_OK;
    if (fviz_algorithm_traversal_push(&traversal, start) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto cleanup;
    }
    while (traversal.stack_count != 0u)
    {
        const FVizAlgorithm* current = traversal.stack[--traversal.stack_count];
        FVizBool is_new = FVIZ_FALSE;
        uint32_t port;
        if (current == target)
        {
            *out_reaches = FVIZ_TRUE;
            break;
        }
        result = fviz_algorithm_traversal_visit(&traversal, current, &is_new);
        if (result != FVIZ_OK) goto cleanup;
        if (is_new == FVIZ_FALSE) continue;
        for (port = 0u; port < current->input_port_count; ++port)
        {
            const FVizAlgorithmInputPort* input_port = &current->input_ports[port];
            FVizSize i;
            for (i = 0u; i < fviz_array_count(input_port->connections); ++i)
            {
                const FVizAlgorithmConnection* connection =
                    (const FVizAlgorithmConnection*)fviz_array_const_at(input_port->connections, i);
                result = fviz_algorithm_traversal_push(&traversal, connection->producer);
                if (result != FVIZ_OK) goto cleanup;
            }
        }
    }
cleanup:
    fviz_free(traversal.stack);
    fviz_free(traversal.visited);
    return result;
}

FVizResult fviz_internal_algorithm_initialize(FVizAlgorithm* algorithm, uint32_t input_port_count,
                                              uint32_t output_port_count, FVizAlgorithmExecuteFn execute)
{
    uint32_t i;
    if (algorithm == NULL || output_port_count == 0u || (execute == NULL && algorithm->custom == FVIZ_FALSE))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm initialization contract is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    algorithm->input_port_count = input_port_count;
    algorithm->output_port_count = output_port_count;
    algorithm->execute = execute;
    algorithm->diagnostic_id = fviz_atomic_u64_fetch_add(&g_fviz_algorithm_id_counter, 1u) + 1u;
    if (input_port_count > 0u)
    {
        algorithm->input_ports =
            (FVizAlgorithmInputPort*)fviz_alloc((FVizSize)input_port_count * sizeof(FVizAlgorithmInputPort));
        if (algorithm->input_ports == NULL) goto failed;
        (void)memset(algorithm->input_ports, 0, (size_t)input_port_count * sizeof(FVizAlgorithmInputPort));
        for (i = 0u; i < input_port_count; ++i)
        {
            algorithm->input_ports[i].info.data_type = FVIZ_TYPE_DATA_OBJECT;
            if (fviz_array_create(sizeof(FVizAlgorithmConnection), &algorithm->input_ports[i].connections) != FVIZ_OK)
                goto failed;
        }
    }
    algorithm->output_ports =
        (FVizAlgorithmOutputPort*)fviz_alloc((FVizSize)output_port_count * sizeof(FVizAlgorithmOutputPort));
    algorithm->output_proxies =
        (FVizAlgorithmOutput*)fviz_alloc((FVizSize)output_port_count * sizeof(FVizAlgorithmOutput));
    if (algorithm->output_ports == NULL || algorithm->output_proxies == NULL) goto failed;
    (void)memset(algorithm->output_ports, 0, (size_t)output_port_count * sizeof(FVizAlgorithmOutputPort));
    for (i = 0u; i < output_port_count; ++i)
    {
        static const int64_t empty_extent[6] = {0, -1, 0, -1, 0, -1};
        algorithm->output_ports[i].info.data_type = FVIZ_TYPE_DATA_OBJECT;
        (void)memcpy(algorithm->output_ports[i].whole_extent, empty_extent, sizeof(empty_extent));
        algorithm->output_ports[i].has_whole_extent = FVIZ_FALSE;
        algorithm->output_proxies[i].producer = algorithm;
        algorithm->output_proxies[i].port = i;
        if (fviz_array_create(sizeof(double), &algorithm->output_ports[i].time_steps) != FVIZ_OK) goto failed;
    }
    if (fviz_internal_executive_create(algorithm, &algorithm->executive) != FVIZ_OK) goto failed;
    return FVIZ_OK;

failed:
    fviz_internal_algorithm_deinitialize(algorithm);
    if (fviz_last_error_code() == FVIZ_OK)
        fviz_internal_set_error(FVIZ_ERROR_OUT_OF_MEMORY, "failed to allocate algorithm ports");
    return fviz_last_error_code();
}

void fviz_internal_algorithm_deinitialize(FVizAlgorithm* algorithm)
{
    uint32_t i;
    if (algorithm == NULL) return;
    fviz_release(algorithm->executive);
    algorithm->executive = NULL;
    for (i = 0u; i < algorithm->input_port_count; ++i)
    {
        fviz_algorithm_clear_connections(&algorithm->input_ports[i]);
        fviz_algorithm_remove_input_observer((FVizObject*)algorithm->input_ports[i].direct_data,
                                             &algorithm->input_ports[i].direct_data_modified_tag);
        fviz_release(algorithm->input_ports[i].direct_data);
        fviz_release(algorithm->input_ports[i].connections);
    }
    for (i = 0u; i < algorithm->output_port_count; ++i)
    {
        fviz_release(algorithm->output_ports[i].data);
        fviz_release(algorithm->output_ports[i].time_steps);
        algorithm->output_ports[i].time_steps = NULL;
    }
    fviz_free(algorithm->input_ports);
    fviz_free(algorithm->output_ports);
    fviz_free(algorithm->output_proxies);
    algorithm->input_ports = NULL;
    algorithm->output_ports = NULL;
    algorithm->output_proxies = NULL;
    algorithm->input_port_count = 0u;
    algorithm->output_port_count = 0u;
}

FVizResult fviz_internal_algorithm_configure_input_port(FVizAlgorithm* algorithm, uint32_t port, FVizTypeId data_type,
                                                        FVizBool optional, FVizBool repeatable)
{
    if (fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK || data_type == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    algorithm->input_ports[port].info.data_type = data_type;
    algorithm->input_ports[port].info.optional = optional != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    algorithm->input_ports[port].info.repeatable = repeatable != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    return FVIZ_OK;
}

FVizResult fviz_internal_algorithm_configure_output_port(FVizAlgorithm* algorithm, uint32_t port, FVizTypeId data_type)
{
    if (fviz_algorithm_validate_output_port(algorithm, port) != FVIZ_OK || data_type == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    algorithm->output_ports[port].info.data_type = data_type;
    return FVIZ_OK;
}

FVizResult fviz_algorithm_configure_input_port(FVizAlgorithm* algorithm, uint32_t port, FVizTypeId data_type,
                                               FVizBool optional, FVizBool repeatable)
{
    FVizResult result = fviz_internal_algorithm_configure_input_port(algorithm, port, data_type, optional, repeatable);
    if (result == FVIZ_OK) fviz_object_modified((FVizObject*)algorithm);
    return result;
}

FVizResult fviz_algorithm_configure_output_port(FVizAlgorithm* algorithm, uint32_t port, FVizTypeId data_type)
{
    FVizResult result = fviz_internal_algorithm_configure_output_port(algorithm, port, data_type);
    if (result == FVIZ_OK) fviz_object_modified((FVizObject*)algorithm);
    return result;
}

uint32_t fviz_algorithm_input_port_count(const FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? algorithm->input_port_count : 0u;
}

uint32_t fviz_algorithm_output_port_count(const FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? algorithm->output_port_count : 0u;
}

FVizResult fviz_algorithm_input_port_info(const FVizAlgorithm* algorithm, uint32_t port,
                                          FVizAlgorithmPortInfo* out_info)
{
    if (out_info == NULL || fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK)
    {
        if (out_info == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_info must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_info = algorithm->input_ports[port].info;
    return FVIZ_OK;
}

FVizResult fviz_algorithm_output_port_info(const FVizAlgorithm* algorithm, uint32_t port,
                                           FVizAlgorithmPortInfo* out_info)
{
    if (out_info == NULL || fviz_algorithm_validate_output_port(algorithm, port) != FVIZ_OK)
    {
        if (out_info == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_info must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_info = algorithm->output_ports[port].info;
    return FVIZ_OK;
}

FVizAlgorithmOutput* fviz_algorithm_output_port(FVizAlgorithm* algorithm, uint32_t port)
{
    return algorithm != NULL && port < algorithm->output_port_count ? &algorithm->output_proxies[port] : NULL;
}

FVizAlgorithm* fviz_algorithm_output_producer(const FVizAlgorithmOutput* output)
{
    return output != NULL ? output->producer : NULL;
}

uint32_t fviz_algorithm_output_index(const FVizAlgorithmOutput* output)
{
    return output != NULL ? output->port : UINT32_MAX;
}

FVizResult fviz_algorithm_set_input_data(FVizAlgorithm* algorithm, uint32_t port, FVizDataObject* data_object)
{
    FVizAlgorithmInputPort* input_port;
    FVizObserverTag new_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK || data_object == NULL)
    {
        if (data_object == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "input data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    input_port = &algorithm->input_ports[port];
    if (fviz_object_is_type((const FVizObject*)data_object, input_port->info.data_type) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "input data does not satisfy the port type");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (data_object == input_port->direct_data && fviz_array_count(input_port->connections) == 0u) return FVIZ_OK;
    if (fviz_retain(data_object) == NULL) return fviz_last_error_code();
    if (fviz_algorithm_observe_input_dependency(algorithm, (FVizObject*)data_object, &new_tag) != FVIZ_OK)
    {
        fviz_release(data_object);
        return fviz_last_error_code();
    }
    fviz_algorithm_clear_connections(input_port);
    fviz_algorithm_remove_input_observer((FVizObject*)input_port->direct_data, &input_port->direct_data_modified_tag);
    fviz_release(input_port->direct_data);
    input_port->direct_data = data_object;
    input_port->direct_data_modified_tag = new_tag;
    algorithm->updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)algorithm);
    return FVIZ_OK;
}

static FVizResult fviz_algorithm_append_input_connection(FVizAlgorithm* algorithm, uint32_t port,
                                                         FVizAlgorithmOutput* output, FVizBool replace)
{
    FVizAlgorithmInputPort* input_port;
    FVizAlgorithmConnection connection;
    FVizTypeId output_type;
    if (fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK || output == NULL || output->producer == NULL ||
        output->port >= output->producer->output_port_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "input connection is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    input_port = &algorithm->input_ports[port];
    if (replace == FVIZ_FALSE && input_port->info.repeatable == FVIZ_FALSE &&
        fviz_array_count(input_port->connections) > 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "input port is not repeatable");
        return FVIZ_ERROR_INVALID_STATE;
    }
    output_type = output->producer->output_ports[output->port].info.data_type;
    if (fviz_algorithm_port_accepts(input_port->info.data_type, output_type) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "connection output type does not satisfy the input port");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    {
        FVizBool reaches = FVIZ_FALSE;
        FVizResult traversal_result = fviz_algorithm_reaches(output->producer, algorithm, &reaches);
        if (traversal_result != FVIZ_OK) return traversal_result;
        if (reaches != FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm connection would create a cycle");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    connection.producer = output->producer;
    connection.output_port = output->port;
    connection.producer_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (fviz_retain(connection.producer) == NULL) return fviz_last_error_code();
    if (fviz_algorithm_observe_input_dependency(algorithm, (FVizObject*)connection.producer,
                                                &connection.producer_modified_tag) != FVIZ_OK)
    {
        fviz_release(connection.producer);
        return fviz_last_error_code();
    }
    if (replace == FVIZ_TRUE) fviz_algorithm_clear_connections(input_port);
    if (fviz_array_push(input_port->connections, &connection) != FVIZ_OK)
    {
        fviz_algorithm_remove_input_observer((FVizObject*)connection.producer, &connection.producer_modified_tag);
        fviz_release(connection.producer);
        return fviz_last_error_code();
    }
    fviz_algorithm_remove_input_observer((FVizObject*)input_port->direct_data, &input_port->direct_data_modified_tag);
    fviz_release(input_port->direct_data);
    input_port->direct_data = NULL;
    algorithm->updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)algorithm);
    return FVIZ_OK;
}

FVizResult fviz_algorithm_set_input_connection(FVizAlgorithm* algorithm, uint32_t port, FVizAlgorithmOutput* output)
{
    return fviz_algorithm_append_input_connection(algorithm, port, output, FVIZ_TRUE);
}

FVizResult fviz_algorithm_add_input_connection(FVizAlgorithm* algorithm, uint32_t port, FVizAlgorithmOutput* output)
{
    return fviz_algorithm_append_input_connection(algorithm, port, output, FVIZ_FALSE);
}

FVizResult fviz_algorithm_remove_input_connection(FVizAlgorithm* algorithm, uint32_t port, uint32_t connection_index)
{
    FVizAlgorithmInputPort* input_port;
    FVizSize count;
    FVizAlgorithmConnection* connections;
    if (fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK) return FVIZ_ERROR_INVALID_ARGUMENT;
    input_port = &algorithm->input_ports[port];
    count = fviz_array_count(input_port->connections);
    if ((FVizSize)connection_index >= count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "input connection index is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    connections = (FVizAlgorithmConnection*)fviz_array_data(input_port->connections);
    fviz_algorithm_remove_input_observer((FVizObject*)connections[connection_index].producer,
                                         &connections[connection_index].producer_modified_tag);
    fviz_release(connections[connection_index].producer);
    if ((FVizSize)connection_index + 1u < count)
    {
        (void)memmove(&connections[connection_index], &connections[connection_index + 1u],
                      (size_t)(count - (FVizSize)connection_index - 1u) * sizeof(FVizAlgorithmConnection));
    }
    (void)fviz_array_resize(input_port->connections, count - 1u);
    algorithm->updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)algorithm);
    return FVIZ_OK;
}

void fviz_algorithm_remove_all_input_connections(FVizAlgorithm* algorithm, uint32_t port)
{
    if (algorithm == NULL || port >= algorithm->input_port_count) return;
    if (fviz_array_count(algorithm->input_ports[port].connections) == 0u) return;
    fviz_algorithm_clear_connections(&algorithm->input_ports[port]);
    algorithm->updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)algorithm);
}

FVizResult fviz_algorithm_clear_input(FVizAlgorithm* algorithm, uint32_t port)
{
    FVizAlgorithmInputPort* input_port;
    if (fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK) return FVIZ_ERROR_INVALID_ARGUMENT;
    input_port = &algorithm->input_ports[port];
    fviz_algorithm_clear_connections(input_port);
    fviz_algorithm_remove_input_observer((FVizObject*)input_port->direct_data, &input_port->direct_data_modified_tag);
    fviz_release(input_port->direct_data);
    input_port->direct_data = NULL;
    algorithm->updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)algorithm);
    return FVIZ_OK;
}

uint32_t fviz_algorithm_input_connection_count(const FVizAlgorithm* algorithm, uint32_t port)
{
    return algorithm != NULL && port < algorithm->input_port_count
               ? (uint32_t)fviz_array_count(algorithm->input_ports[port].connections)
               : 0u;
}

FVizAlgorithmOutput* fviz_algorithm_input_connection(FVizAlgorithm* algorithm, uint32_t port, uint32_t connection_index)
{
    FVizAlgorithmConnection* connection;
    if (algorithm == NULL || port >= algorithm->input_port_count ||
        (FVizSize)connection_index >= fviz_array_count(algorithm->input_ports[port].connections))
        return NULL;
    connection = (FVizAlgorithmConnection*)fviz_array_at(algorithm->input_ports[port].connections, connection_index);
    return fviz_algorithm_output_port(connection->producer, connection->output_port);
}

FVizDataObject* fviz_internal_algorithm_resolved_input(FVizAlgorithm* algorithm, uint32_t port,
                                                       uint32_t connection_index)
{
    FVizAlgorithmInputPort* input_port;
    FVizAlgorithmConnection* connection;
    if (algorithm == NULL || port >= algorithm->input_port_count) return NULL;
    input_port = &algorithm->input_ports[port];
    if (input_port->direct_data != NULL) return connection_index == 0u ? input_port->direct_data : NULL;
    connection = (FVizAlgorithmConnection*)fviz_array_at(input_port->connections, connection_index);
    return connection != NULL ? fviz_algorithm_output_data(connection->producer, connection->output_port) : NULL;
}

FVizDataObject* fviz_algorithm_resolved_input(FVizAlgorithm* algorithm, uint32_t port, uint32_t connection)
{
    return fviz_internal_algorithm_resolved_input(algorithm, port, connection);
}

const FVizDataObject* fviz_algorithm_input_data(const FVizAlgorithm* algorithm, uint32_t port)
{
    return (const FVizDataObject*)fviz_internal_algorithm_resolved_input((FVizAlgorithm*)algorithm, port, 0u);
}

FVizResult fviz_internal_algorithm_set_output_data(FVizAlgorithm* algorithm, uint32_t port, FVizDataObject* data_object)
{
    FVizAlgorithmOutputPort* output_port;
    if (fviz_algorithm_validate_output_port(algorithm, port) != FVIZ_OK || data_object == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    output_port = &algorithm->output_ports[port];
    if (fviz_object_is_type((const FVizObject*)data_object, output_port->info.data_type) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "algorithm produced the wrong data type");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_retain(data_object) == NULL) return fviz_last_error_code();
    fviz_release(output_port->data);
    output_port->data = data_object;
    return FVIZ_OK;
}

FVizResult fviz_algorithm_set_output_data(FVizAlgorithm* algorithm, uint32_t port, FVizDataObject* data_object)
{
    return fviz_internal_algorithm_set_output_data(algorithm, port, data_object);
}

FVizResult fviz_algorithm_report_progress(FVizAlgorithm* algorithm, double progress)
{
    if (algorithm == NULL || progress < 0.0 || progress > 1.0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm progress must be in [0, 1]");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    algorithm->progress = progress;
    if (algorithm->progress_callback != NULL)
        algorithm->progress_callback(algorithm, progress, algorithm->progress_user_data);
    /* Forward monotonic progress to the future of a running context-based async
     * update. The executive reports 0.0 before and 1.0 after each algorithm, so
     * values are clamped to a non-decreasing sequence to satisfy the executor's
     * progress contract across multi-node pipeline walks. */
    if (algorithm->async_progress_context != NULL && progress >= algorithm->async_progress_last)
    {
        if (fviz_task_context_report_progress(algorithm->async_progress_context, progress) == FVIZ_OK)
            algorithm->async_progress_last = progress;
    }
    if (fviz_object_invoke_event((FVizObject*)algorithm, FVIZ_EVENT_PROGRESS, &algorithm->progress) != FVIZ_FALSE)
        fviz_algorithm_request_abort(algorithm);
    if (fviz_object_invoke_event((FVizObject*)algorithm, FVIZ_EVENT_ABORT_CHECK, &algorithm->progress) != FVIZ_FALSE)
        fviz_algorithm_request_abort(algorithm);
    return fviz_algorithm_abort_requested(algorithm) == FVIZ_TRUE ? FVIZ_ERROR_BUSY : FVIZ_OK;
}

FVizResult fviz_internal_algorithm_map_input_request(FVizAlgorithm* algorithm, uint32_t input_port, uint32_t connection,
                                                     const FVizPipelineRequestInfo* downstream_request,
                                                     FVizPipelineRequestInfo* upstream_request)
{
    if (algorithm == NULL || downstream_request == NULL || upstream_request == NULL ||
        input_port >= algorithm->input_port_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "input request mapping arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *upstream_request = *downstream_request;
    if (algorithm->custom != FVIZ_FALSE && algorithm->callbacks.map_input_request != NULL)
    {
        FVizResult result = algorithm->callbacks.map_input_request(
            algorithm, input_port, connection, downstream_request, upstream_request, algorithm->state);
        if (result != FVIZ_OK) return result;
    }
    if (upstream_request->number_of_pieces == 0u || upstream_request->piece >= upstream_request->number_of_pieces)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "mapped pipeline piece request is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (upstream_request->has_extent != FVIZ_FALSE)
    {
        uint32_t axis;
        for (axis = 0u; axis < 3u; ++axis)
        {
            if (upstream_request->extent[axis * 2u] > upstream_request->extent[axis * 2u + 1u])
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "mapped pipeline extent is invalid");
                return FVIZ_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    return FVIZ_OK;
}

FVizResult fviz_internal_algorithm_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                   FVizMTime input_mtime, uint64_t request_key, FVizBool* out_executed)
{
    FVizAlgorithmOutputPort* output;
    FVizMTime algorithm_mtime;
    FVizResult result;
    uint32_t port;
    if (algorithm == NULL || request == NULL || out_executed == NULL ||
        request->requested_output_port >= algorithm->output_port_count)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_executed = FVIZ_FALSE;
    algorithm->last_update_executed = FVIZ_FALSE;
    if (fviz_algorithm_abort_requested(algorithm) == FVIZ_TRUE)
    {
        fviz_internal_set_error(FVIZ_ERROR_BUSY, "algorithm execution was aborted");
        return FVIZ_ERROR_BUSY;
    }
    output = &algorithm->output_ports[request->requested_output_port];
    algorithm_mtime = fviz_object_mtime((const FVizObject*)algorithm);
    if (request->type == FVIZ_PIPELINE_REQUEST_DATA && output->updated == FVIZ_TRUE &&
        output->last_input_mtime == input_mtime && output->last_algorithm_mtime == algorithm_mtime &&
        output->last_request_key == request_key)
        return FVIZ_OK;
    if (algorithm->custom == FVIZ_TRUE)
        result = algorithm->callbacks.process_request(algorithm, request, algorithm->state);
    else if (request->type == FVIZ_PIPELINE_REQUEST_DATA)
        result = algorithm->execute(algorithm);
    else
        result = FVIZ_OK;
    if (result != FVIZ_OK) return result;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (output->data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "algorithm produced no requested output data");
        return FVIZ_ERROR_INVALID_STATE;
    }
    output->last_input_mtime = input_mtime;
    output->last_algorithm_mtime = algorithm_mtime;
    output->last_request_key = request_key;
    output->updated = FVIZ_TRUE;
    algorithm->last_input_mtime = input_mtime;
    algorithm->last_algorithm_mtime = algorithm_mtime;
    algorithm->updated = FVIZ_TRUE;
    algorithm->last_update_executed = FVIZ_TRUE;
    *out_executed = FVIZ_TRUE;
    if ((request->flags & FVIZ_PIPELINE_REQUEST_FLAG_RELEASE_DATA) != 0u)
    {
        for (port = 0u; port < algorithm->output_port_count; ++port)
        {
            FVizAlgorithmOutputPort* released;
            if (port == request->requested_output_port) continue;
            released = &algorithm->output_ports[port];
            fviz_release(released->data);
            released->data = NULL;
            released->updated = FVIZ_FALSE;
            released->last_input_mtime = 0u;
            released->last_algorithm_mtime = 0u;
            released->last_request_key = 0u;
        }
    }
    return FVIZ_OK;
}

FVizResult fviz_internal_algorithm_update_now(FVizAlgorithm* algorithm)
{
    uint32_t port;
    FVizMTime input_mtime = 0u;
    FVizMTime algorithm_mtime;
    FVizResult result = FVIZ_OK;
    if (algorithm == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    algorithm->last_update_executed = FVIZ_FALSE;
    if (fviz_algorithm_abort_requested(algorithm) == FVIZ_TRUE)
    {
        fviz_internal_set_error(FVIZ_ERROR_BUSY, "algorithm execution was aborted");
        return FVIZ_ERROR_BUSY;
    }
    if (algorithm->updating == FVIZ_TRUE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cycle detected while updating algorithm pipeline");
        return FVIZ_ERROR_INVALID_STATE;
    }
    algorithm->updating = FVIZ_TRUE;
    for (port = 0u; port < algorithm->input_port_count; ++port)
    {
        FVizAlgorithmInputPort* input_port = &algorithm->input_ports[port];
        FVizSize connection_count = fviz_array_count(input_port->connections);
        FVizSize i;
        if (input_port->direct_data == NULL && connection_count == 0u)
        {
            if (input_port->info.optional == FVIZ_FALSE)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "required algorithm input port is not connected");
                result = FVIZ_ERROR_INVALID_STATE;
                goto done;
            }
            continue;
        }
        for (i = 0u; i < connection_count; ++i)
        {
            FVizAlgorithmConnection* connection = (FVizAlgorithmConnection*)fviz_array_at(input_port->connections, i);
            result = fviz_algorithm_update(connection->producer);
            if (result != FVIZ_OK) goto done;
        }
        if (input_port->direct_data != NULL)
        {
            FVizMTime mtime = fviz_object_mtime((const FVizObject*)input_port->direct_data);
            if (mtime > input_mtime) input_mtime = mtime;
        }
        for (i = 0u; i < connection_count; ++i)
        {
            FVizDataObject* data = fviz_internal_algorithm_resolved_input(algorithm, port, (uint32_t)i);
            FVizMTime mtime;
            if (data == NULL)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "upstream algorithm produced no output data");
                result = FVIZ_ERROR_INVALID_STATE;
                goto done;
            }
            mtime = fviz_object_mtime((const FVizObject*)data);
            if (mtime > input_mtime) input_mtime = mtime;
        }
    }
    algorithm_mtime = fviz_object_mtime((const FVizObject*)algorithm);
    if (algorithm->updated == FVIZ_TRUE && algorithm->last_input_mtime == input_mtime &&
        algorithm->last_algorithm_mtime == algorithm_mtime)
        goto done;
    algorithm->progress = 0.0;
    if (algorithm->progress_callback != NULL)
        algorithm->progress_callback(algorithm, 0.0, algorithm->progress_user_data);
    if (fviz_algorithm_abort_requested(algorithm) == FVIZ_TRUE)
    {
        fviz_internal_set_error(FVIZ_ERROR_BUSY, "algorithm execution was aborted");
        result = FVIZ_ERROR_BUSY;
        goto done;
    }
    result = algorithm->execute(algorithm);
    if (result != FVIZ_OK) goto done;
    algorithm->last_update_executed = FVIZ_TRUE;
    for (port = 0u; port < algorithm->output_port_count; ++port)
    {
        if (algorithm->output_ports[port].data == NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "algorithm produced no data for a required output port");
            result = FVIZ_ERROR_INVALID_STATE;
            goto done;
        }
    }
    algorithm->last_input_mtime = input_mtime;
    algorithm->last_algorithm_mtime = algorithm_mtime;
    algorithm->updated = FVIZ_TRUE;
    algorithm->progress = 1.0;
    if (algorithm->progress_callback != NULL)
        algorithm->progress_callback(algorithm, 1.0, algorithm->progress_user_data);

done:
    algorithm->updating = FVIZ_FALSE;
    return result;
}

FVizResult fviz_algorithm_update(FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? fviz_executive_update(algorithm->executive, 0u)
                             : fviz_internal_algorithm_update_now(NULL);
}

typedef struct FVizAlgorithmAsyncUpdate
{
    FVizAlgorithm* algorithm;
} FVizAlgorithmAsyncUpdate;

static FVizResult fviz_algorithm_async_update_run(FVizAlgorithm* algorithm, FVizCancellationToken* cancellation,
                                                  FVizTaskContext* context)
{
    FVizPipelineRequestInfo request;
    FVizResult result;
    fviz_pipeline_request_initialize(&request);
    request.type = FVIZ_PIPELINE_REQUEST_DATA;
    request.requested_output_port = 0u;
    request.cancellation = cancellation;
    /* Bridge algorithm progress to the future through the task context. The
     * algorithm is only mutated by this worker while the future is running
     * (pipeline mutation is externally synchronized). */
    if (context != NULL)
    {
        algorithm->async_progress_context = context;
        algorithm->async_progress_last = 0.0;
    }
    result = fviz_executive_update_request(algorithm->executive, &request);
    if (context != NULL)
    {
        algorithm->async_progress_context = NULL;
        algorithm->async_progress_last = 0.0;
    }
    return result;
}

static FVizResult fviz_algorithm_async_update_context_task(FVizTaskContext* context, void* user_data, void** out_value)
{
    FVizAlgorithmAsyncUpdate* update = (FVizAlgorithmAsyncUpdate*)user_data;
    FVIZ_UNUSED(out_value);
    return fviz_algorithm_async_update_run(update->algorithm, fviz_task_context_cancellation(context), context);
}

static void fviz_algorithm_async_update_destroy(void* user_data)
{
    FVizAlgorithmAsyncUpdate* update = (FVizAlgorithmAsyncUpdate*)user_data;
    if (update == NULL) return;
    fviz_release(update->algorithm);
    fviz_free(update);
}

FVizResult fviz_algorithm_update_async(FVizAlgorithm* algorithm, FVizExecutor* executor, int priority,
                                       FVizFuture** out_future)
{
    FVizAlgorithmAsyncUpdate* update;
    if (algorithm == NULL || executor == NULL || out_future == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_future = NULL;
    update = (FVizAlgorithmAsyncUpdate*)fviz_alloc(sizeof(*update));
    if (update == NULL) return fviz_last_error_code();
    update->algorithm = (FVizAlgorithm*)fviz_retain(algorithm);
    if (update->algorithm == NULL ||
        fviz_executor_submit_context(executor, priority, fviz_algorithm_async_update_context_task, update,
                                     fviz_algorithm_async_update_destroy, NULL, out_future) != FVIZ_OK)
    {
        if (*out_future == NULL) fviz_algorithm_async_update_destroy(update);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

/* ---------------------------------------------------------------------------
 * Ordered async pipeline chain. Runs an array of algorithm updates as a
 * dependent continuation chain on one executor: stage i+1 only becomes runnable
 * after stage i completes, so large pipelines are drained through the shared
 * worker pool without one-thread-per-request or caller-side waits. A failing or
 * cancelled stage short-circuits the remaining stages.
 * ------------------------------------------------------------------------- */

typedef struct FVizAlgorithmAsyncChain FVizAlgorithmAsyncChain;

typedef struct FVizAlgorithmChainStage
{
    FVizAlgorithm* algorithm;
    FVizSize index;
    FVizBool terminal;
    FVizAlgorithmAsyncChain* chain;
} FVizAlgorithmChainStage;

struct FVizAlgorithmAsyncChain
{
    FVizSize count;
    /* Internal non-terminal futures owned by the chain. They are freed only by
     * the terminal stage's destroy callback, by which point every upstream
     * continuation has released its dependency, so destruction is non-blocking. */
    FVizFuture** links;
    FVizSize link_count;
};

static FVizResult fviz_algorithm_chain_stage_run(FVizAlgorithm* algorithm, FVizCancellationToken* cancellation)
{
    FVizPipelineRequestInfo request;
    fviz_pipeline_request_initialize(&request);
    request.type = FVIZ_PIPELINE_REQUEST_DATA;
    request.requested_output_port = 0u;
    request.cancellation = cancellation;
    return fviz_executive_update_request(algorithm->executive, &request);
}

static FVizResult fviz_algorithm_chain_first_task(FVizCancellationToken* cancellation, void* user_data,
                                                  void** out_value)
{
    FVizAlgorithmChainStage* stage = (FVizAlgorithmChainStage*)user_data;
    FVIZ_UNUSED(out_value);
    return fviz_algorithm_chain_stage_run(stage->algorithm, cancellation);
}

static FVizResult fviz_algorithm_chain_continuation(FVizResult antecedent_result, FVizCancellationToken* cancellation,
                                                    void* user_data, void** out_value)
{
    FVizAlgorithmChainStage* stage = (FVizAlgorithmChainStage*)user_data;
    FVIZ_UNUSED(out_value);
    /* Short-circuit: a failed or cancelled predecessor aborts the chain. */
    if (antecedent_result != FVIZ_OK) return antecedent_result;
    return fviz_algorithm_chain_stage_run(stage->algorithm, cancellation);
}

static void fviz_algorithm_chain_stage_destroy(void* user_data)
{
    FVizAlgorithmChainStage* stage = (FVizAlgorithmChainStage*)user_data;
    if (stage == NULL) return;
    fviz_release(stage->algorithm);
    if (stage->terminal != FVIZ_FALSE && stage->chain != NULL)
    {
        FVizSize i;
        for (i = 0u; i < stage->chain->link_count; ++i)
            fviz_future_destroy(stage->chain->links[i]);
        fviz_free(stage->chain->links);
        fviz_free(stage->chain);
    }
    fviz_free(stage);
}

FVizResult fviz_algorithm_update_async_chain(FVizAlgorithm** algorithms, FVizSize count, FVizExecutor* executor,
                                             int priority, FVizFuture** out_future)
{
    FVizAlgorithmAsyncChain* chain = NULL;
    FVizFuture* current = NULL;
    FVizFuture* next = NULL;
    FVizSize i;
    if (algorithms == NULL || count == 0u || executor == NULL || out_future == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_future = NULL;
    for (i = 0u; i < count; ++i)
        if (algorithms[i] == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    chain = (FVizAlgorithmAsyncChain*)fviz_alloc(sizeof(*chain));
    if (chain == NULL) return fviz_last_error_code();
    (void)memset(chain, 0, sizeof(*chain));
    chain->count = count;
    chain->links = (FVizFuture**)fviz_alloc((count - 1u) * sizeof(*chain->links));
    if (chain->links == NULL)
    {
        fviz_free(chain);
        return fviz_last_error_code();
    }
    for (i = 0u; i < count; ++i)
    {
        FVizAlgorithmChainStage* stage = (FVizAlgorithmChainStage*)fviz_alloc(sizeof(*stage));
        if (stage == NULL)
        {
            if (current != NULL) fviz_future_destroy(current);
            fviz_free(chain->links);
            fviz_free(chain);
            return fviz_last_error_code();
        }
        stage->algorithm = (FVizAlgorithm*)fviz_retain(algorithms[i]);
        stage->index = i;
        stage->terminal = (i + 1u == count) ? FVIZ_TRUE : FVIZ_FALSE;
        stage->chain = chain;
        if (stage->algorithm == NULL)
        {
            fviz_algorithm_chain_stage_destroy(stage);
            if (current != NULL) fviz_future_destroy(current);
            fviz_free(chain->links);
            fviz_free(chain);
            return fviz_last_error_code();
        }
        if (i == 0u)
        {
            if (fviz_executor_submit(executor, priority, fviz_algorithm_chain_first_task, stage,
                                     fviz_algorithm_chain_stage_destroy, NULL, &current) != FVIZ_OK)
            {
                fviz_algorithm_chain_stage_destroy(stage);
                if (current != NULL) fviz_future_destroy(current);
                fviz_free(chain->links);
                fviz_free(chain);
                return fviz_last_error_code();
            }
        }
        else
        {
            next = NULL;
            if (fviz_future_then(current, executor, priority, fviz_algorithm_chain_continuation, stage,
                                 fviz_algorithm_chain_stage_destroy, NULL, &next) != FVIZ_OK)
            {
                fviz_algorithm_chain_stage_destroy(stage);
                fviz_future_destroy(current);
                fviz_free(chain->links);
                fviz_free(chain);
                return fviz_last_error_code();
            }
            /* Keep the intermediate link alive in the chain; it is released by
             * the terminal stage after the whole chain drains. */
            chain->links[chain->link_count++] = current;
            current = next;
        }
    }
    *out_future = current;
    return FVIZ_OK;
}

FVizExecutive* fviz_algorithm_executive(FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? algorithm->executive : NULL;
}

void fviz_algorithm_set_progress_callback(FVizAlgorithm* algorithm, FVizAlgorithmProgressFn callback, void* user_data)
{
    if (algorithm == NULL) return;
    algorithm->progress_callback = callback;
    algorithm->progress_user_data = user_data;
}

double fviz_algorithm_progress(const FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? algorithm->progress : 0.0;
}

void fviz_algorithm_request_abort(FVizAlgorithm* algorithm)
{
    if (algorithm != NULL) (void)fviz_atomic_u32_exchange(&algorithm->abort_requested, 1u);
}

void fviz_algorithm_clear_abort(FVizAlgorithm* algorithm)
{
    if (algorithm != NULL) (void)fviz_atomic_u32_exchange(&algorithm->abort_requested, 0u);
}

FVizBool fviz_algorithm_abort_requested(const FVizAlgorithm* algorithm)
{
    return algorithm != NULL && fviz_atomic_u32_load(&algorithm->abort_requested) != 0u ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizDataObject* fviz_algorithm_output_data(FVizAlgorithm* algorithm, uint32_t port)
{
    return algorithm != NULL && port < algorithm->output_port_count ? algorithm->output_ports[port].data : NULL;
}

const FVizDataObject* fviz_algorithm_const_output_data(const FVizAlgorithm* algorithm, uint32_t port)
{
    return algorithm != NULL && port < algorithm->output_port_count ? algorithm->output_ports[port].data : NULL;
}

FVizResult fviz_algorithm_release_output_data(FVizAlgorithm* algorithm, uint32_t port)
{
    FVizAlgorithmOutputPort* output;
    if (algorithm == NULL || port >= algorithm->output_port_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm output port is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    output = &algorithm->output_ports[port];
    fviz_release(output->data);
    output->data = NULL;
    output->updated = FVIZ_FALSE;
    output->last_input_mtime = 0u;
    output->last_algorithm_mtime = 0u;
    output->last_request_key = 0u;
    algorithm->updated = FVIZ_FALSE;
    return FVIZ_OK;
}

void fviz_algorithm_release_all_output_data(FVizAlgorithm* algorithm)
{
    uint32_t port;
    if (algorithm == NULL) return;
    for (port = 0u; port < algorithm->output_port_count; ++port)
    {
        FVizAlgorithmOutputPort* output = &algorithm->output_ports[port];
        fviz_release(output->data);
        output->data = NULL;
        output->updated = FVIZ_FALSE;
        output->last_input_mtime = 0u;
        output->last_algorithm_mtime = 0u;
        output->last_request_key = 0u;
    }
    algorithm->updated = FVIZ_FALSE;
}

FVizResult fviz_algorithm_set_output_time_steps(FVizAlgorithm* algorithm, uint32_t port, const double* time_steps,
                                                FVizSize count)
{
    FVizSize i;
    FVizAlgorithmOutputPort* output;
    if (fviz_algorithm_validate_output_port(algorithm, port) != FVIZ_OK || (count > 0u && time_steps == NULL))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "output time-step metadata is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < count; ++i)
    {
        if (!isfinite(time_steps[i]) || (i > 0u && time_steps[i] <= time_steps[i - 1u]))
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "output time steps must be finite and strictly increasing");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    output = &algorithm->output_ports[port];
    if (fviz_array_resize(output->time_steps, count) != FVIZ_OK) return fviz_last_error_code();
    if (count > 0u) (void)memcpy(fviz_array_data(output->time_steps), time_steps, (size_t)count * sizeof(double));
    return FVIZ_OK;
}

const double* fviz_algorithm_output_time_steps(const FVizAlgorithm* algorithm, uint32_t port, FVizSize* out_count)
{
    if (out_count != NULL) *out_count = 0u;
    if (algorithm == NULL || port >= algorithm->output_port_count) return NULL;
    if (out_count != NULL) *out_count = fviz_array_count(algorithm->output_ports[port].time_steps);
    return (const double*)fviz_array_const_data(algorithm->output_ports[port].time_steps);
}

FVizResult fviz_algorithm_output_time_range(const FVizAlgorithm* algorithm, uint32_t port, double* out_minimum,
                                            double* out_maximum)
{
    const double* steps;
    FVizSize count = 0u;
    if (out_minimum != NULL) *out_minimum = 0.0;
    if (out_maximum != NULL) *out_maximum = 0.0;
    if (algorithm == NULL || port >= algorithm->output_port_count || out_minimum == NULL || out_maximum == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "output time-range query is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    steps = fviz_algorithm_output_time_steps(algorithm, port, &count);
    if (steps == NULL || count == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "algorithm output has no time-step metadata");
        return FVIZ_ERROR_NOT_FOUND;
    }
    *out_minimum = steps[0];
    *out_maximum = steps[count - 1u];
    return FVIZ_OK;
}

FVizResult fviz_algorithm_set_output_whole_extent(FVizAlgorithm* algorithm, uint32_t port, const int64_t extent[6])
{
    FVizAlgorithmOutputPort* output;
    uint32_t axis;
    if (fviz_algorithm_validate_output_port(algorithm, port) != FVIZ_OK || extent == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "output whole extent is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (axis = 0u; axis < 3u; ++axis)
    {
        if (extent[axis * 2u + 1u] < extent[axis * 2u])
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "output whole extent must be non-empty and ordered");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    output = &algorithm->output_ports[port];
    if (output->has_whole_extent != FVIZ_FALSE &&
        memcmp(output->whole_extent, extent, sizeof(output->whole_extent)) == 0)
        return FVIZ_OK;
    (void)memcpy(output->whole_extent, extent, sizeof(output->whole_extent));
    output->has_whole_extent = FVIZ_TRUE;
    return FVIZ_OK;
}

void fviz_algorithm_clear_output_whole_extent(FVizAlgorithm* algorithm, uint32_t port)
{
    static const int64_t empty_extent[6] = {0, -1, 0, -1, 0, -1};
    FVizAlgorithmOutputPort* output;
    if (algorithm == NULL || port >= algorithm->output_port_count) return;
    output = &algorithm->output_ports[port];
    if (output->has_whole_extent == FVIZ_FALSE) return;
    output->has_whole_extent = FVIZ_FALSE;
    (void)memcpy(output->whole_extent, empty_extent, sizeof(empty_extent));
}

FVizBool fviz_algorithm_output_whole_extent(const FVizAlgorithm* algorithm, uint32_t port, int64_t out_extent[6])
{
    const FVizAlgorithmOutputPort* output;
    if (out_extent != NULL)
    {
        static const int64_t empty_extent[6] = {0, -1, 0, -1, 0, -1};
        (void)memcpy(out_extent, empty_extent, sizeof(empty_extent));
    }
    if (algorithm == NULL || port >= algorithm->output_port_count || out_extent == NULL) return FVIZ_FALSE;
    output = &algorithm->output_ports[port];
    if (output->has_whole_extent == FVIZ_FALSE) return FVIZ_FALSE;
    (void)memcpy(out_extent, output->whole_extent, sizeof(output->whole_extent));
    return FVIZ_TRUE;
}
