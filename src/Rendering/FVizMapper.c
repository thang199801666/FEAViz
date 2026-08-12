#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizMapper.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizMapperPrivate.h>

static void fviz_mapper_destroy(FVizObject* object);
static FVizMTime fviz_mapper_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_mapper_class = {
    FVIZ_TYPE_MAPPER,
    "FVizMapper",
    &g_fviz_object_class,
    fviz_mapper_destroy,
    fviz_mapper_mtime
};

static FVizMTime fviz_mapper_mtime(const FVizObject* object)
{
    const FVizMapper* mapper = (const FVizMapper*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    FVizMTime child = fviz_object_mtime((const FVizObject*)mapper->input_algorithm);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)mapper->poly_data);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)mapper->lookup_table);
    if (child > mtime) mtime = child;
    return mtime;
}

void fviz_array_selection_initialize(FVizArraySelection* selection)
{
    if (selection == NULL) return;
    (void)memset(selection, 0, sizeof(*selection));
    selection->struct_size = (uint32_t)sizeof(*selection);
    selection->association = FVIZ_ASSOCIATION_POINTS;
    selection->component_mode = FVIZ_COMPONENT_DIRECT;
}

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
    mapper->association = FVIZ_ASSOCIATION_POINTS;
    mapper->component_mode = FVIZ_COMPONENT_DIRECT;
    mapper->scalar_interpolation = FVIZ_SCALAR_INTERPOLATION_DEFAULT;
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
    fviz_object_modified((FVizObject*)mapper);
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
    fviz_object_modified((FVizObject*)mapper);
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
    fviz_object_modified((FVizObject*)mapper);
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
    fviz_object_modified((FVizObject*)mapper);
}

FVizLookupTable* fviz_mapper_lookup_table(FVizMapper* mapper) { return mapper != NULL ? mapper->lookup_table : NULL; }

void fviz_mapper_set_scalar_visibility(FVizMapper* mapper, FVizBool visible)
{
    if (mapper != NULL)
    {
        mapper->scalar_visibility = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        fviz_object_modified((FVizObject*)mapper);
    }
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
    fviz_object_modified((FVizObject*)mapper);
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

void fviz_mapper_use_automatic_scalar_range(FVizMapper* mapper)
{
    if (mapper == NULL) return;
    mapper->scalar_range_valid = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)mapper);
}

FVizResult fviz_mapper_set_array_selection(
    FVizMapper* mapper,
    const FVizArraySelection* selection)
{
    FVizSize length;
    if (mapper == NULL || selection == NULL ||
        selection->struct_size < sizeof(FVizArraySelection) ||
        selection->name == NULL || selection->name[0] == '\0' ||
        selection->association < FVIZ_ASSOCIATION_POINTS ||
        selection->association > FVIZ_ASSOCIATION_FIELD ||
        selection->component_mode < FVIZ_COMPONENT_DIRECT ||
        selection->component_mode > FVIZ_COMPONENT_COLOR)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "mapper array selection is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    length = (FVizSize)strlen(selection->name);
    if (length >= sizeof(mapper->array_name))
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "mapper array name is too long");
        return FVIZ_ERROR_OVERFLOW;
    }
    (void)memcpy(mapper->array_name, selection->name, length + 1u);
    mapper->association = selection->association;
    mapper->component_mode = selection->component_mode;
    mapper->component = selection->component;
    fviz_object_modified((FVizObject*)mapper);
    return FVIZ_OK;
}

FVizResult fviz_mapper_get_array_selection(
    const FVizMapper* mapper,
    FVizArraySelection* out_selection)
{
    if (mapper == NULL || out_selection == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "mapper and output selection are required");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_array_selection_initialize(out_selection);
    out_selection->association = mapper->association;
    out_selection->name = mapper->array_name[0] != '\0' ? mapper->array_name : NULL;
    out_selection->component_mode = mapper->component_mode;
    out_selection->component = mapper->component;
    return FVIZ_OK;
}

const FVizDataArray* fviz_mapper_selected_array(const FVizMapper* mapper)
{
    const FVizAttributeSet* attributes;
    if (mapper == NULL || mapper->poly_data == NULL || mapper->array_name[0] == '\0') return NULL;
    if (mapper->association == FVIZ_ASSOCIATION_POINTS)
        attributes = fviz_poly_data_const_point_data(mapper->poly_data);
    else if (mapper->association == FVIZ_ASSOCIATION_CELLS)
        attributes = fviz_poly_data_const_cell_data(mapper->poly_data);
    else
        attributes = fviz_poly_data_const_field_data(mapper->poly_data);
    return fviz_attribute_set_const_get(attributes, mapper->array_name);
}

void fviz_mapper_set_scalar_interpolation(
    FVizMapper* mapper,
    FVizScalarInterpolation interpolation)
{
    if (mapper == NULL || interpolation < FVIZ_SCALAR_INTERPOLATION_DEFAULT ||
        interpolation > FVIZ_SCALAR_INTERPOLATION_POINT)
        return;
    mapper->scalar_interpolation = interpolation;
    fviz_object_modified((FVizObject*)mapper);
}

FVizScalarInterpolation fviz_mapper_scalar_interpolation(const FVizMapper* mapper)
{
    return mapper != NULL ? mapper->scalar_interpolation : FVIZ_SCALAR_INTERPOLATION_DEFAULT;
}

FVizResult fviz_mapper_set_opacity_array(FVizMapper* mapper, const char* name)
{
    FVizSize length;
    if (mapper == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (name == NULL || name[0] == '\0')
    {
        mapper->opacity_array_name[0] = '\0';
        fviz_object_modified((FVizObject*)mapper);
        return FVIZ_OK;
    }
    length = (FVizSize)strlen(name);
    if (length >= sizeof(mapper->opacity_array_name)) return FVIZ_ERROR_OVERFLOW;
    (void)memcpy(mapper->opacity_array_name, name, length + 1u);
    fviz_object_modified((FVizObject*)mapper);
    return FVIZ_OK;
}

const char* fviz_mapper_opacity_array(const FVizMapper* mapper)
{
    return mapper != NULL && mapper->opacity_array_name[0] != '\0'
        ? mapper->opacity_array_name
        : NULL;
}

FVizResult fviz_mapper_add_clipping_plane(FVizMapper* mapper, FVizPlane plane)
{
    if (mapper == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (mapper->clipping_plane_count >= 6u) return FVIZ_ERROR_OVERFLOW;
    if (fviz_vec3_length(plane.normal) == 0.0f) return FVIZ_ERROR_INVALID_ARGUMENT;
    plane.normal = fviz_vec3_normalize(plane.normal);
    mapper->clipping_planes[mapper->clipping_plane_count++] = plane;
    fviz_object_modified((FVizObject*)mapper);
    return FVIZ_OK;
}

void fviz_mapper_remove_all_clipping_planes(FVizMapper* mapper)
{
    if (mapper == NULL) return;
    mapper->clipping_plane_count = 0u;
    fviz_object_modified((FVizObject*)mapper);
}

FVizSize fviz_mapper_clipping_plane_count(const FVizMapper* mapper)
{
    return mapper != NULL ? mapper->clipping_plane_count : 0u;
}

FVizResult fviz_mapper_clipping_plane(
    const FVizMapper* mapper,
    FVizSize index,
    FVizPlane* out_plane)
{
    if (mapper == NULL || out_plane == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (index >= mapper->clipping_plane_count) return FVIZ_ERROR_NOT_FOUND;
    *out_plane = mapper->clipping_planes[index];
    return FVIZ_OK;
}
