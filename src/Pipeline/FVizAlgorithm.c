#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Pipeline/FVizAlgorithmPrivate.h>
#include <FViz/Pipeline/FVizExecutivePrivate.h>

const FVizObjectClass g_fviz_algorithm_class = {
    FVIZ_TYPE_ALGORITHM,
    "FVizAlgorithm",
    &g_fviz_object_class,
    NULL,
    NULL
};

static FVizBool fviz_algorithm_port_accepts(FVizTypeId accepted, FVizTypeId produced)
{
    return accepted == FVIZ_TYPE_DATA_OBJECT || accepted == produced ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_algorithm_validate_input_port(
    const FVizAlgorithm* algorithm,
    uint32_t port)
{
    if (algorithm == NULL || port >= algorithm->input_port_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm input port is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return FVIZ_OK;
}

static FVizResult fviz_algorithm_validate_output_port(
    const FVizAlgorithm* algorithm,
    uint32_t port)
{
    if (algorithm == NULL || port >= algorithm->output_port_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm output port is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return FVIZ_OK;
}

static void fviz_algorithm_clear_connections(FVizAlgorithmInputPort* input_port)
{
    FVizSize i;
    for (i = 0u; i < fviz_array_count(input_port->connections); ++i)
    {
        FVizAlgorithmConnection* connection =
            (FVizAlgorithmConnection*)fviz_array_at(input_port->connections, i);
        fviz_release(connection->producer);
    }
    fviz_array_clear(input_port->connections);
}

static FVizBool fviz_algorithm_reaches(
    const FVizAlgorithm* start,
    const FVizAlgorithm* target,
    uint32_t depth)
{
    uint32_t port;
    if (start == target) return FVIZ_TRUE;
    if (start == NULL || depth > 1024u) return FVIZ_FALSE;
    for (port = 0u; port < start->input_port_count; ++port)
    {
        const FVizAlgorithmInputPort* input_port = &start->input_ports[port];
        FVizSize i;
        for (i = 0u; i < fviz_array_count(input_port->connections); ++i)
        {
            const FVizAlgorithmConnection* connection =
                (const FVizAlgorithmConnection*)fviz_array_const_at(input_port->connections, i);
            if (fviz_algorithm_reaches(connection->producer, target, depth + 1u) == FVIZ_TRUE)
                return FVIZ_TRUE;
        }
    }
    return FVIZ_FALSE;
}

FVizResult fviz_internal_algorithm_initialize(
    FVizAlgorithm* algorithm,
    uint32_t input_port_count,
    uint32_t output_port_count,
    FVizAlgorithmExecuteFn execute)
{
    uint32_t i;
    if (algorithm == NULL || output_port_count == 0u || execute == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm initialization contract is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    algorithm->input_port_count = input_port_count;
    algorithm->output_port_count = output_port_count;
    algorithm->execute = execute;
    if (input_port_count > 0u)
    {
        algorithm->input_ports = (FVizAlgorithmInputPort*)fviz_alloc(
            (FVizSize)input_port_count * sizeof(FVizAlgorithmInputPort));
        if (algorithm->input_ports == NULL) goto failed;
        (void)memset(
            algorithm->input_ports,
            0,
            (size_t)input_port_count * sizeof(FVizAlgorithmInputPort));
        for (i = 0u; i < input_port_count; ++i)
        {
            algorithm->input_ports[i].info.data_type = FVIZ_TYPE_DATA_OBJECT;
            if (fviz_array_create(
                    sizeof(FVizAlgorithmConnection),
                    &algorithm->input_ports[i].connections) != FVIZ_OK)
                goto failed;
        }
    }
    algorithm->output_ports = (FVizAlgorithmOutputPort*)fviz_alloc(
        (FVizSize)output_port_count * sizeof(FVizAlgorithmOutputPort));
    algorithm->output_proxies = (FVizAlgorithmOutput*)fviz_alloc(
        (FVizSize)output_port_count * sizeof(FVizAlgorithmOutput));
    if (algorithm->output_ports == NULL || algorithm->output_proxies == NULL) goto failed;
    (void)memset(
        algorithm->output_ports,
        0,
        (size_t)output_port_count * sizeof(FVizAlgorithmOutputPort));
    for (i = 0u; i < output_port_count; ++i)
    {
        algorithm->output_ports[i].info.data_type = FVIZ_TYPE_DATA_OBJECT;
        algorithm->output_proxies[i].producer = algorithm;
        algorithm->output_proxies[i].port = i;
    }
    if (fviz_internal_executive_create(algorithm, &algorithm->executive) != FVIZ_OK)
        goto failed;
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
        fviz_release(algorithm->input_ports[i].direct_data);
        fviz_release(algorithm->input_ports[i].connections);
    }
    for (i = 0u; i < algorithm->output_port_count; ++i)
        fviz_release(algorithm->output_ports[i].data);
    fviz_free(algorithm->input_ports);
    fviz_free(algorithm->output_ports);
    fviz_free(algorithm->output_proxies);
    algorithm->input_ports = NULL;
    algorithm->output_ports = NULL;
    algorithm->output_proxies = NULL;
    algorithm->input_port_count = 0u;
    algorithm->output_port_count = 0u;
}

FVizResult fviz_internal_algorithm_configure_input_port(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizTypeId data_type,
    FVizBool optional,
    FVizBool repeatable)
{
    if (fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK || data_type == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    algorithm->input_ports[port].info.data_type = data_type;
    algorithm->input_ports[port].info.optional = optional != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    algorithm->input_ports[port].info.repeatable = repeatable != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    return FVIZ_OK;
}

FVizResult fviz_internal_algorithm_configure_output_port(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizTypeId data_type)
{
    if (fviz_algorithm_validate_output_port(algorithm, port) != FVIZ_OK || data_type == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    algorithm->output_ports[port].info.data_type = data_type;
    return FVIZ_OK;
}

uint32_t fviz_algorithm_input_port_count(const FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? algorithm->input_port_count : 0u;
}

uint32_t fviz_algorithm_output_port_count(const FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? algorithm->output_port_count : 0u;
}

FVizResult fviz_algorithm_input_port_info(
    const FVizAlgorithm* algorithm,
    uint32_t port,
    FVizAlgorithmPortInfo* out_info)
{
    if (out_info == NULL || fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK)
    {
        if (out_info == NULL)
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_info must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_info = algorithm->input_ports[port].info;
    return FVIZ_OK;
}

FVizResult fviz_algorithm_output_port_info(
    const FVizAlgorithm* algorithm,
    uint32_t port,
    FVizAlgorithmPortInfo* out_info)
{
    if (out_info == NULL || fviz_algorithm_validate_output_port(algorithm, port) != FVIZ_OK)
    {
        if (out_info == NULL)
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_info must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_info = algorithm->output_ports[port].info;
    return FVIZ_OK;
}

FVizAlgorithmOutput* fviz_algorithm_output_port(FVizAlgorithm* algorithm, uint32_t port)
{
    return algorithm != NULL && port < algorithm->output_port_count
        ? &algorithm->output_proxies[port]
        : NULL;
}

FVizAlgorithm* fviz_algorithm_output_producer(const FVizAlgorithmOutput* output)
{
    return output != NULL ? output->producer : NULL;
}

uint32_t fviz_algorithm_output_index(const FVizAlgorithmOutput* output)
{
    return output != NULL ? output->port : UINT32_MAX;
}

FVizResult fviz_algorithm_set_input_data(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizDataObject* data_object)
{
    FVizAlgorithmInputPort* input_port;
    if (fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK || data_object == NULL)
    {
        if (data_object == NULL)
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "input data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    input_port = &algorithm->input_ports[port];
    if (fviz_object_is_type(
            (const FVizObject*)data_object,
            input_port->info.data_type) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "input data does not satisfy the port type");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_retain(data_object) == NULL) return fviz_last_error_code();
    fviz_algorithm_clear_connections(input_port);
    fviz_release(input_port->direct_data);
    input_port->direct_data = data_object;
    algorithm->updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)algorithm);
    return FVIZ_OK;
}

static FVizResult fviz_algorithm_append_input_connection(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizAlgorithmOutput* output,
    FVizBool replace)
{
    FVizAlgorithmInputPort* input_port;
    FVizAlgorithmConnection connection;
    FVizTypeId output_type;
    if (fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK ||
        output == NULL || output->producer == NULL ||
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
    if (fviz_algorithm_reaches(output->producer, algorithm, 0u) == FVIZ_TRUE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "algorithm connection would create a cycle");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    connection.producer = output->producer;
    connection.output_port = output->port;
    if (fviz_retain(connection.producer) == NULL) return fviz_last_error_code();
    if (replace == FVIZ_TRUE) fviz_algorithm_clear_connections(input_port);
    if (fviz_array_push(input_port->connections, &connection) != FVIZ_OK)
    {
        fviz_release(connection.producer);
        return fviz_last_error_code();
    }
    fviz_release(input_port->direct_data);
    input_port->direct_data = NULL;
    algorithm->updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)algorithm);
    return FVIZ_OK;
}

FVizResult fviz_algorithm_set_input_connection(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizAlgorithmOutput* output)
{
    return fviz_algorithm_append_input_connection(algorithm, port, output, FVIZ_TRUE);
}

FVizResult fviz_algorithm_add_input_connection(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizAlgorithmOutput* output)
{
    return fviz_algorithm_append_input_connection(algorithm, port, output, FVIZ_FALSE);
}

FVizResult fviz_algorithm_remove_input_connection(
    FVizAlgorithm* algorithm,
    uint32_t port,
    uint32_t connection_index)
{
    FVizAlgorithmInputPort* input_port;
    FVizSize count;
    FVizAlgorithmConnection* connections;
    if (fviz_algorithm_validate_input_port(algorithm, port) != FVIZ_OK)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    input_port = &algorithm->input_ports[port];
    count = fviz_array_count(input_port->connections);
    if ((FVizSize)connection_index >= count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "input connection index is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    connections = (FVizAlgorithmConnection*)fviz_array_data(input_port->connections);
    fviz_release(connections[connection_index].producer);
    if ((FVizSize)connection_index + 1u < count)
    {
        (void)memmove(
            &connections[connection_index],
            &connections[connection_index + 1u],
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

uint32_t fviz_algorithm_input_connection_count(
    const FVizAlgorithm* algorithm,
    uint32_t port)
{
    return algorithm != NULL && port < algorithm->input_port_count
        ? (uint32_t)fviz_array_count(algorithm->input_ports[port].connections)
        : 0u;
}

FVizAlgorithmOutput* fviz_algorithm_input_connection(
    FVizAlgorithm* algorithm,
    uint32_t port,
    uint32_t connection_index)
{
    FVizAlgorithmConnection* connection;
    if (algorithm == NULL || port >= algorithm->input_port_count ||
        (FVizSize)connection_index >= fviz_array_count(algorithm->input_ports[port].connections))
        return NULL;
    connection = (FVizAlgorithmConnection*)fviz_array_at(
        algorithm->input_ports[port].connections, connection_index);
    return fviz_algorithm_output_port(connection->producer, connection->output_port);
}

FVizDataObject* fviz_internal_algorithm_resolved_input(
    FVizAlgorithm* algorithm,
    uint32_t port,
    uint32_t connection_index)
{
    FVizAlgorithmInputPort* input_port;
    FVizAlgorithmConnection* connection;
    if (algorithm == NULL || port >= algorithm->input_port_count) return NULL;
    input_port = &algorithm->input_ports[port];
    if (input_port->direct_data != NULL)
        return connection_index == 0u ? input_port->direct_data : NULL;
    connection = (FVizAlgorithmConnection*)fviz_array_at(input_port->connections, connection_index);
    return connection != NULL
        ? fviz_algorithm_output_data(connection->producer, connection->output_port)
        : NULL;
}

const FVizDataObject* fviz_algorithm_input_data(
    const FVizAlgorithm* algorithm,
    uint32_t port)
{
    return (const FVizDataObject*)fviz_internal_algorithm_resolved_input(
        (FVizAlgorithm*)algorithm, port, 0u);
}

FVizResult fviz_internal_algorithm_set_output_data(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizDataObject* data_object)
{
    FVizAlgorithmOutputPort* output_port;
    if (fviz_algorithm_validate_output_port(algorithm, port) != FVIZ_OK || data_object == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    output_port = &algorithm->output_ports[port];
    if (fviz_object_is_type(
            (const FVizObject*)data_object,
            output_port->info.data_type) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "algorithm produced the wrong data type");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_retain(data_object) == NULL) return fviz_last_error_code();
    fviz_release(output_port->data);
    output_port->data = data_object;
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
            FVizAlgorithmConnection* connection =
                (FVizAlgorithmConnection*)fviz_array_at(input_port->connections, i);
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
    if (algorithm->updated == FVIZ_TRUE &&
        algorithm->last_input_mtime == input_mtime &&
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
    return algorithm != NULL
        ? fviz_executive_update(algorithm->executive, 0u)
        : fviz_internal_algorithm_update_now(NULL);
}

FVizExecutive* fviz_algorithm_executive(FVizAlgorithm* algorithm)
{
    return algorithm != NULL ? algorithm->executive : NULL;
}

void fviz_algorithm_set_progress_callback(
    FVizAlgorithm* algorithm,
    FVizAlgorithmProgressFn callback,
    void* user_data)
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
    return algorithm != NULL && fviz_atomic_u32_load(&algorithm->abort_requested) != 0u
        ? FVIZ_TRUE
        : FVIZ_FALSE;
}

FVizDataObject* fviz_algorithm_output_data(FVizAlgorithm* algorithm, uint32_t port)
{
    return algorithm != NULL && port < algorithm->output_port_count
        ? algorithm->output_ports[port].data
        : NULL;
}

const FVizDataObject* fviz_algorithm_const_output_data(
    const FVizAlgorithm* algorithm,
    uint32_t port)
{
    return algorithm != NULL && port < algorithm->output_port_count
        ? algorithm->output_ports[port].data
        : NULL;
}
