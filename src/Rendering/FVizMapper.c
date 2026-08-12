#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizMapper.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizMapperPrivate.h>

static void fviz_mapper_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_mapper_class = {
    FVIZ_TYPE_MAPPER,
    "FVizMapper",
    &g_fviz_object_class,
    fviz_mapper_destroy,
    NULL
};

static void fviz_mapper_destroy(FVizObject* object)
{
    FVizMapper* mapper = (FVizMapper*)object;
    fviz_release(mapper->input_algorithm);
    fviz_release(mapper->poly_data);
    fviz_release(mapper->lookup_table);
    mapper->input_algorithm = NULL;
    mapper->poly_data = NULL;
    mapper->lookup_table = NULL;
}

FVizResult fviz_mapper_create(FVizMapper** out_mapper)
{
    FVizMapper* mapper;
    if (out_mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_mapper = NULL;
    mapper = (FVizMapper*)fviz_internal_object_allocate(sizeof(FVizMapper), &g_fviz_mapper_class, NULL);
    if (mapper == NULL)
    {
        return fviz_last_error_code();
    }
    mapper->scalar_visibility = FVIZ_FALSE;
    mapper->scalar_range_valid = FVIZ_FALSE;
    if (fviz_lookup_table_create(256u, &mapper->lookup_table) != FVIZ_OK)
    {
        fviz_release(mapper);
        return fviz_last_error_code();
    }
    *out_mapper = mapper;
    return FVIZ_OK;
}

FVizResult fviz_mapper_set_poly_data(FVizMapper* mapper, FVizPolyData* poly_data)
{
    if (mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (poly_data != NULL && fviz_retain(poly_data) == NULL)
    {
        return fviz_last_error_code();
    }
    fviz_release(mapper->input_algorithm);
    mapper->input_algorithm = NULL;
    mapper->input_port = 0u;
    fviz_release(mapper->poly_data);
    mapper->poly_data = poly_data;
    return FVIZ_OK;
}

FVizResult fviz_mapper_set_algorithm_connection(
    FVizMapper* mapper,
    FVizAlgorithmOutput* output)
{
    FVizAlgorithm* producer = fviz_algorithm_output_producer(output);
    uint32_t output_port = fviz_algorithm_output_index(output);
    FVizAlgorithmPortInfo port_info;
    if (mapper == NULL || producer == NULL ||
        fviz_algorithm_output_port_info(producer, output_port, &port_info) != FVIZ_OK)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "mapper algorithm connection is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (port_info.data_type != FVIZ_TYPE_POLY_DATA)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "mapper input connection must produce poly data");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (mapper->input_algorithm == producer && mapper->input_port == output_port) return FVIZ_OK;
    if (fviz_retain(producer) == NULL) return fviz_last_error_code();
    fviz_release(mapper->input_algorithm);
    mapper->input_algorithm = producer;
    mapper->input_port = output_port;
    fviz_release(mapper->poly_data);
    mapper->poly_data = NULL;
    return FVIZ_OK;
}

FVizAlgorithmOutput* fviz_mapper_algorithm_connection(FVizMapper* mapper)
{
    return mapper != NULL && mapper->input_algorithm != NULL
        ? fviz_algorithm_output_port(mapper->input_algorithm, mapper->input_port)
        : NULL;
}

FVizResult fviz_mapper_set_input_connection(FVizMapper* mapper, FVizFilter* producer)
{
    if (producer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "producer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_mapper_set_algorithm_connection(mapper, fviz_filter_output_port(producer));
}

FVizFilter* fviz_mapper_input_connection(FVizMapper* mapper)
{
    return mapper != NULL && mapper->input_algorithm != NULL &&
        fviz_object_is_type((const FVizObject*)mapper->input_algorithm, FVIZ_TYPE_FILTER)
        ? (FVizFilter*)mapper->input_algorithm
        : NULL;
}

FVizResult fviz_mapper_update(FVizMapper* mapper)
{
    FVizPolyData* output;
    if (mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (mapper->input_algorithm == NULL) return FVIZ_OK;
    if (fviz_algorithm_update(mapper->input_algorithm) != FVIZ_OK) return fviz_last_error_code();
    output = (FVizPolyData*)fviz_algorithm_output_data(mapper->input_algorithm, mapper->input_port);
    if (output == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "mapper producer returned no poly data");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (output == mapper->poly_data) return FVIZ_OK;
    if (fviz_retain(output) == NULL) return fviz_last_error_code();
    fviz_release(mapper->poly_data);
    mapper->poly_data = output;
    return FVIZ_OK;
}

FVizPolyData* fviz_mapper_poly_data(FVizMapper* mapper) { return mapper != NULL ? mapper->poly_data : NULL; }
const FVizPolyData* fviz_mapper_const_poly_data(const FVizMapper* mapper) { return mapper != NULL ? mapper->poly_data : NULL; }

void fviz_mapper_set_lookup_table(FVizMapper* mapper, FVizLookupTable* table)
{
    if (mapper == NULL) return;
    if (table != NULL && fviz_retain(table) == NULL) return;
    fviz_release(mapper->lookup_table);
    mapper->lookup_table = table;
}

FVizLookupTable* fviz_mapper_lookup_table(FVizMapper* mapper) { return mapper != NULL ? mapper->lookup_table : NULL; }

void fviz_mapper_set_scalar_visibility(FVizMapper* mapper, FVizBool visible)
{
    if (mapper != NULL) mapper->scalar_visibility = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_mapper_scalar_visibility(const FVizMapper* mapper)
{
    return mapper != NULL ? mapper->scalar_visibility : FVIZ_FALSE;
}

void fviz_mapper_set_scalar_range(FVizMapper* mapper, float minimum, float maximum)
{
    if (mapper == NULL) return;
    if (maximum <= minimum)
    {
        maximum = minimum + 1.0f;
    }
    mapper->scalar_min = minimum;
    mapper->scalar_max = maximum;
    mapper->scalar_range_valid = FVIZ_TRUE;
    if (mapper->lookup_table != NULL)
    {
        fviz_lookup_table_set_range(mapper->lookup_table, minimum, maximum);
    }
}

void fviz_mapper_get_scalar_range(const FVizMapper* mapper, float* minimum, float* maximum)
{
    if (mapper == NULL) return;
    if (minimum != NULL) *minimum = mapper->scalar_min;
    if (maximum != NULL) *maximum = mapper->scalar_max;
}

FVizBool fviz_mapper_scalar_range_valid(const FVizMapper* mapper)
{
    return mapper != NULL ? mapper->scalar_range_valid : FVIZ_FALSE;
}
