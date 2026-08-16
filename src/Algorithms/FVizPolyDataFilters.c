#include <math.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Algorithms/FVizPolyDataFilters.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizTransformPolyDataFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizTransform* transform;
};

struct FVizElevationFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizVec3 low_point;
    FVizVec3 high_point;
    double scalar_low;
    double scalar_high;
    char array_name[64];
};

struct FVizAppendPolyDataFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
};

static FVizMTime fviz_filter_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static void fviz_transform_poly_data_filter_destroy(FVizObject* object)
{
    FVizTransformPolyDataFilter* filter = (FVizTransformPolyDataFilter*)object;
    fviz_release(filter->algorithm);
    fviz_release(filter->transform);
    filter->algorithm = NULL;
    filter->transform = NULL;
}

static FVizMTime fviz_transform_poly_data_filter_mtime(const FVizObject* object)
{
    const FVizTransformPolyDataFilter* filter = (const FVizTransformPolyDataFilter*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    const FVizMTime transform_mtime = fviz_object_mtime((const FVizObject*)filter->transform);
    if (transform_mtime > mtime) mtime = transform_mtime;
    return mtime;
}

static const FVizObjectClass g_fviz_transform_poly_data_filter_class = {
    FVIZ_TYPE_TRANSFORM_POLY_DATA_FILTER, "FVizTransformPolyDataFilter", &g_fviz_object_class,
    fviz_transform_poly_data_filter_destroy, fviz_transform_poly_data_filter_mtime};

static FVizResult fviz_transform_poly_data_process_request(FVizAlgorithm* algorithm,
                                                           const FVizPipelineRequestInfo* request, void* state)
{
    FVizTransformPolyDataFilter* filter = (FVizTransformPolyDataFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    FVizSize i;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "transform poly data filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_poly_data_deep_copy(input, &output) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < fviz_poly_data_point_count(output); ++i)
    {
        FVizVec3 point;
        if (fviz_poly_data_get_point(output, i, &point) != FVIZ_OK ||
            fviz_poly_data_set_point(output, i, fviz_transform_point(filter->transform, point)) != FVIZ_OK)
            goto fail;
    }
    if ((fviz_poly_data_poly_cell_count(output) != 0u || fviz_poly_data_strip_cell_count(output) != 0u) &&
        fviz_poly_data_compute_normals(output) != FVIZ_OK)
        goto fail;
    if (fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_transform_poly_data_filter_create(FVizTransform* transform, FVizTransformPolyDataFilter** out_filter)
{
    FVizTransformPolyDataFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (transform == NULL || out_filter == NULL)
    {
        if (out_filter != NULL) *out_filter = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "transform poly data filter requires transform and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizTransformPolyDataFilter*)fviz_internal_object_allocate(
        sizeof(*filter), &g_fviz_transform_poly_data_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->transform = (FVizTransform*)fviz_retain(transform);
    if (filter->transform == NULL)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_transform_poly_data_process_request;
    callbacks.get_state_mtime = fviz_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) !=
            FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_transform_poly_data_filter_set_transform(FVizTransformPolyDataFilter* filter, FVizTransform* transform)
{
    if (filter == NULL || transform == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "filter and transform must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_retain(transform) == NULL) return fviz_last_error_code();
    fviz_release(filter->transform);
    filter->transform = transform;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizTransform* fviz_transform_poly_data_filter_transform(FVizTransformPolyDataFilter* filter)
{
    return filter != NULL ? filter->transform : NULL;
}

FVizResult fviz_transform_poly_data_filter_set_input_data(FVizTransformPolyDataFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_transform_poly_data_filter_set_input_connection(FVizTransformPolyDataFilter* filter,
                                                                FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_transform_poly_data_filter_algorithm(FVizTransformPolyDataFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_transform_poly_data_filter_output_port(FVizTransformPolyDataFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_transform_poly_data_filter_output(FVizTransformPolyDataFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_transform_poly_data_filter_update(FVizTransformPolyDataFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}

static void fviz_elevation_filter_destroy(FVizObject* object)
{
    FVizElevationFilter* filter = (FVizElevationFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_elevation_filter_class = {
    FVIZ_TYPE_ELEVATION_FILTER, "FVizElevationFilter", &g_fviz_object_class, fviz_elevation_filter_destroy, NULL};

static FVizResult fviz_elevation_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                 void* state)
{
    FVizElevationFilter* filter = (FVizElevationFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    FVizDataArray* scalars = NULL;
    FVizVec3 direction;
    double length2;
    FVizSize i;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "elevation filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    direction = fviz_vec3_sub(filter->high_point, filter->low_point);
    length2 = (double)fviz_vec3_dot(direction, direction);
    if (length2 <= 1.0e-30)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "elevation low/high points must be distinct");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_poly_data_deep_copy(input, &output) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalars) != FVIZ_OK ||
        fviz_data_array_resize(scalars, fviz_poly_data_point_count(input)) != FVIZ_OK)
        goto fail;
    for (i = 0u; i < fviz_poly_data_point_count(input); ++i)
    {
        const FVizVec3 point = fviz_poly_data_points(input)[i];
        const FVizVec3 relative = fviz_vec3_sub(point, filter->low_point);
        double t = (double)fviz_vec3_dot(relative, direction) / length2;
        float value;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        value = (float)(filter->scalar_low + t * (filter->scalar_high - filter->scalar_low));
        if (fviz_data_array_set_tuple(scalars, i, &value) != FVIZ_OK) goto fail;
    }
    if (fviz_attribute_set_add(fviz_poly_data_point_data(output), filter->array_name, scalars) != FVIZ_OK ||
        fviz_attribute_set_set_active(fviz_poly_data_point_data(output), FVIZ_ATTRIBUTE_SCALARS, filter->array_name) !=
            FVIZ_OK ||
        fviz_poly_data_set_scalars(output, scalars) != FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_release(scalars);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_release(scalars);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_elevation_filter_create(FVizElevationFilter** out_filter)
{
    FVizElevationFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizElevationFilter*)fviz_internal_object_allocate(sizeof(*filter), &g_fviz_elevation_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->low_point = fviz_vec3(0.0f, 0.0f, 0.0f);
    filter->high_point = fviz_vec3(0.0f, 0.0f, 1.0f);
    filter->scalar_low = 0.0;
    filter->scalar_high = 1.0;
    (void)strcpy(filter->array_name, "Elevation");
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_elevation_process_request;
    callbacks.get_state_mtime = fviz_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) !=
            FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

void fviz_elevation_filter_set_low_point(FVizElevationFilter* filter, FVizVec3 low_point)
{
    if (filter != NULL)
    {
        filter->low_point = low_point;
        fviz_object_modified((FVizObject*)filter);
    }
}

void fviz_elevation_filter_set_high_point(FVizElevationFilter* filter, FVizVec3 high_point)
{
    if (filter != NULL)
    {
        filter->high_point = high_point;
        fviz_object_modified((FVizObject*)filter);
    }
}

void fviz_elevation_filter_set_scalar_range(FVizElevationFilter* filter, double low, double high)
{
    if (filter != NULL)
    {
        filter->scalar_low = low;
        filter->scalar_high = high;
        fviz_object_modified((FVizObject*)filter);
    }
}

FVizResult fviz_elevation_filter_set_array_name(FVizElevationFilter* filter, const char* name)
{
    size_t length;
    if (filter == NULL || name == NULL || name[0] == '\0')
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "elevation array name must not be empty");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    length = strlen(name);
    if (length >= sizeof(filter->array_name))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "elevation array name is too long");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memcpy(filter->array_name, name, length + 1u);
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizResult fviz_elevation_filter_set_input_data(FVizElevationFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_elevation_filter_set_input_connection(FVizElevationFilter* filter, FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_elevation_filter_algorithm(FVizElevationFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_elevation_filter_output_port(FVizElevationFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_elevation_filter_output(FVizElevationFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_elevation_filter_update(FVizElevationFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}

static void fviz_append_poly_data_filter_destroy(FVizObject* object)
{
    FVizAppendPolyDataFilter* filter = (FVizAppendPolyDataFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_append_poly_data_filter_class = {FVIZ_TYPE_APPEND_POLY_DATA_FILTER,
                                                                     "FVizAppendPolyDataFilter", &g_fviz_object_class,
                                                                     fviz_append_poly_data_filter_destroy, NULL};

static uint32_t fviz_append_input_count_internal(FVizAlgorithm* algorithm)
{
    if (fviz_algorithm_input_data(algorithm, 0u) != NULL && fviz_algorithm_input_connection_count(algorithm, 0u) == 0u)
        return 1u;
    return fviz_algorithm_input_connection_count(algorithm, 0u);
}

static FVizPolyData* fviz_append_input(FVizAlgorithm* algorithm, uint32_t index)
{
    return (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, index);
}

static FVizBool fviz_append_array_is_common(FVizAlgorithm* algorithm, const char* name, const FVizDataArray* prototype,
                                            uint32_t input_count)
{
    uint32_t i;
    for (i = 0u; i < input_count; ++i)
    {
        const FVizPolyData* input = fviz_append_input(algorithm, i);
        const FVizDataArray* array =
            input != NULL ? fviz_attribute_set_const_get(fviz_poly_data_const_point_data(input), name) : NULL;
        if (array == NULL || fviz_data_array_type(array) != fviz_data_array_type(prototype) ||
            fviz_data_array_components(array) != fviz_data_array_components(prototype) ||
            fviz_data_array_tuple_count(array) != fviz_poly_data_point_count(input))
            return FVIZ_FALSE;
    }
    return FVIZ_TRUE;
}

static FVizResult fviz_append_common_point_arrays(FVizAlgorithm* algorithm, FVizPolyData* output, uint32_t input_count)
{
    const FVizPolyData* first = fviz_append_input(algorithm, 0u);
    const FVizAttributeSet* first_attributes = fviz_poly_data_const_point_data(first);
    FVizSize array_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(first_attributes); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(first_attributes, array_index);
        const FVizDataArray* prototype = fviz_attribute_set_const_array_at(first_attributes, array_index);
        FVizDataArray* appended = NULL;
        uint32_t input_index;
        if (fviz_data_array_tuple_count(prototype) != fviz_poly_data_point_count(first) ||
            fviz_append_array_is_common(algorithm, name, prototype, input_count) == FVIZ_FALSE)
            continue;
        if (fviz_data_array_create(fviz_data_array_type(prototype), fviz_data_array_components(prototype), &appended) !=
                FVIZ_OK ||
            fviz_data_array_reserve(appended, fviz_poly_data_point_count(output)) != FVIZ_OK)
        {
            fviz_release(appended);
            return fviz_last_error_code();
        }
        for (input_index = 0u; input_index < input_count; ++input_index)
        {
            const FVizDataArray* source = fviz_attribute_set_const_get(
                fviz_poly_data_const_point_data(fviz_append_input(algorithm, input_index)), name);
            FVizSize tuple;
            for (tuple = 0u; tuple < fviz_data_array_tuple_count(source); ++tuple)
                if (fviz_data_array_append_tuple(appended, fviz_data_array_const_tuple(source, tuple)) != FVIZ_OK)
                    goto fail_array;
        }
        if (fviz_attribute_set_add(fviz_poly_data_point_data(output), name, appended) != FVIZ_OK) goto fail_array;
        {
            FVizAttributeRole role;
            for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
            {
                const char* active = fviz_attribute_set_active_name(first_attributes, role);
                if (active != NULL && strcmp(active, name) == 0)
                    (void)fviz_attribute_set_set_active(fviz_poly_data_point_data(output), role, name);
            }
        }
        fviz_release(appended);
        continue;
    fail_array:
        fviz_release(appended);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizBool fviz_append_cell_array_is_common(FVizAlgorithm* algorithm, const char* name,
                                                 const FVizDataArray* prototype, uint32_t input_count)
{
    uint32_t i;
    for (i = 0u; i < input_count; ++i)
    {
        const FVizPolyData* input = fviz_append_input(algorithm, i);
        const FVizDataArray* array =
            input != NULL ? fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(input), name) : NULL;
        if (array == NULL || fviz_data_array_type(array) != fviz_data_array_type(prototype) ||
            fviz_data_array_components(array) != fviz_data_array_components(prototype) ||
            fviz_data_array_tuple_count(array) != fviz_poly_data_cell_count(input))
            return FVIZ_FALSE;
    }
    return FVIZ_TRUE;
}

static FVizResult fviz_append_common_cell_arrays(FVizAlgorithm* algorithm, FVizPolyData* output, uint32_t input_count)
{
    const FVizPolyData* first = fviz_append_input(algorithm, 0u);
    const FVizAttributeSet* first_attributes = fviz_poly_data_const_cell_data(first);
    FVizSize array_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(first_attributes); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(first_attributes, array_index);
        const FVizDataArray* prototype = fviz_attribute_set_const_array_at(first_attributes, array_index);
        FVizDataArray* appended = NULL;
        uint32_t input_index;
        if (fviz_data_array_tuple_count(prototype) != fviz_poly_data_cell_count(first) ||
            fviz_append_cell_array_is_common(algorithm, name, prototype, input_count) == FVIZ_FALSE)
            continue;
        if (fviz_data_array_create(fviz_data_array_type(prototype), fviz_data_array_components(prototype), &appended) !=
                FVIZ_OK ||
            fviz_data_array_reserve(appended, fviz_poly_data_cell_count(output)) != FVIZ_OK)
        {
            fviz_release(appended);
            return fviz_last_error_code();
        }
        for (input_index = 0u; input_index < input_count; ++input_index)
        {
            const FVizDataArray* source = fviz_attribute_set_const_get(
                fviz_poly_data_const_cell_data(fviz_append_input(algorithm, input_index)), name);
            FVizSize tuple;
            for (tuple = 0u; tuple < fviz_data_array_tuple_count(source); ++tuple)
                if (fviz_data_array_append_tuple(appended, fviz_data_array_const_tuple(source, tuple)) != FVIZ_OK)
                    goto fail_array;
        }
        if (fviz_attribute_set_add(fviz_poly_data_cell_data(output), name, appended) != FVIZ_OK) goto fail_array;
        {
            FVizAttributeRole role;
            for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
            {
                const char* active = fviz_attribute_set_active_name(first_attributes, role);
                if (active != NULL && strcmp(active, name) == 0)
                    (void)fviz_attribute_set_set_active(fviz_poly_data_cell_data(output), role, name);
            }
        }
        fviz_release(appended);
        continue;
    fail_array:
        fviz_release(appended);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_append_remapped_cell(FVizPolyData* output, const FVizCellArray* cells, FVizSize cell_id,
                                            FVizId point_offset)
{
    FVizId stack_ids[16];
    FVizId* remapped = stack_ids;
    FVizCellView view;
    FVizSize i;
    FVizResult result;
    if (fviz_cell_array_cell_view(cells, cell_id, &view) != FVIZ_OK) return fviz_last_error_code();
    if (view.point_count > (FVizSize)(sizeof(stack_ids) / sizeof(stack_ids[0])))
    {
        FVizSize bytes = 0u;
        if (fviz_size_multiply(view.point_count, sizeof(*remapped), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
        remapped = (FVizId*)fviz_alloc(bytes);
        if (remapped == NULL) return fviz_last_error_code();
    }
    for (i = 0u; i < view.point_count; ++i)
    {
        const FVizId source_id = fviz_cell_view_point_id(&view, i);
        if (source_id > UINT64_MAX - point_offset)
        {
            if (remapped != stack_ids) fviz_free(remapped);
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "append poly data connectivity ID overflow");
            return FVIZ_ERROR_OVERFLOW;
        }
        remapped[i] = point_offset + source_id;
    }
    result = fviz_poly_data_add_cell_ids(output, view.type, view.point_count, remapped);
    if (remapped != stack_ids) fviz_free(remapped);
    return result;
}

static FVizResult fviz_append_poly_data_process_request(FVizAlgorithm* algorithm,
                                                        const FVizPipelineRequestInfo* request, void* state)
{
    const uint32_t input_count = fviz_append_input_count_internal(algorithm);
    FVizPolyData* output = NULL;
    FVizSize total_points = 0u;
    FVizSize total_triangles = 0u;
    FVizSize total_lines = 0u;
    uint32_t input_index;
    FVizId point_offset = 0u;
    (void)state;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (input_count == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "append poly data filter has no inputs");
        return FVIZ_ERROR_INVALID_STATE;
    }
    for (input_index = 0u; input_index < input_count; ++input_index)
    {
        const FVizPolyData* input = fviz_append_input(algorithm, input_index);
        if (input == NULL || fviz_poly_data_validate(input) != FVIZ_OK) return fviz_last_error_code();
        if (total_points > (FVizSize)-1 - fviz_poly_data_point_count(input) ||
            total_triangles > (FVizSize)-1 - fviz_poly_data_triangle_count(input) ||
            total_lines > (FVizSize)-1 - fviz_poly_data_line_count(input))
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "append poly data size overflow");
            return FVIZ_ERROR_OVERFLOW;
        }
        total_points += fviz_poly_data_point_count(input);
        total_triangles += fviz_poly_data_triangle_count(input);
        total_lines += fviz_poly_data_line_count(input);
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_reserve(output, total_points, total_triangles) != FVIZ_OK)
        goto fail;
    for (input_index = 0u; input_index < input_count; ++input_index)
    {
        const FVizPolyData* input = fviz_append_input(algorithm, input_index);
        const FVizVec3* points = fviz_poly_data_points(input);
        const FVizCellArray* categories[4] = {fviz_poly_data_verts(input), fviz_poly_data_lines(input),
                                              fviz_poly_data_polys(input), fviz_poly_data_strips(input)};
        FVizSize i;
        uint32_t category;
        if (fviz_poly_data_add_points_ids(output, points, fviz_poly_data_point_count(input), NULL) != FVIZ_OK)
            goto fail;
        for (category = 0u; category < 4u; ++category)
            for (i = 0u; i < fviz_cell_array_count(categories[category]); ++i)
                if (fviz_append_remapped_cell(output, categories[category], i, point_offset) != FVIZ_OK) goto fail;
        if (fviz_poly_data_point_count(input) > UINT64_MAX - point_offset)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "append point offset overflow");
            goto fail;
        }
        point_offset += (FVizId)fviz_poly_data_point_count(input);
    }
    if (fviz_append_common_point_arrays(algorithm, output, input_count) != FVIZ_OK ||
        fviz_append_common_cell_arrays(algorithm, output, input_count) != FVIZ_OK)
        goto fail;
    if ((fviz_poly_data_poly_cell_count(output) != 0u || fviz_poly_data_strip_cell_count(output) != 0u) &&
        fviz_poly_data_compute_normals(output) != FVIZ_OK)
        goto fail;
    if (fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_append_poly_data_filter_create(FVizAppendPolyDataFilter** out_filter)
{
    FVizAppendPolyDataFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizAppendPolyDataFilter*)fviz_internal_object_allocate(sizeof(*filter),
                                                                      &g_fviz_append_poly_data_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_append_poly_data_process_request;
    callbacks.get_state_mtime = fviz_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_TRUE) !=
            FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_append_poly_data_filter_set_input_data(FVizAppendPolyDataFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_append_poly_data_filter_set_input_connection(FVizAppendPolyDataFilter* filter,
                                                             FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_append_poly_data_filter_add_input_connection(FVizAppendPolyDataFilter* filter,
                                                             FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_add_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

void fviz_append_poly_data_filter_remove_all_inputs(FVizAppendPolyDataFilter* filter)
{
    if (filter != NULL) (void)fviz_algorithm_clear_input(filter->algorithm, 0u);
}

uint32_t fviz_append_poly_data_filter_input_count(const FVizAppendPolyDataFilter* filter)
{
    return filter != NULL ? fviz_append_input_count_internal(filter->algorithm) : 0u;
}

FVizAlgorithm* fviz_append_poly_data_filter_algorithm(FVizAppendPolyDataFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_append_poly_data_filter_output_port(FVizAppendPolyDataFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_append_poly_data_filter_output(FVizAppendPolyDataFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_append_poly_data_filter_update(FVizAppendPolyDataFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}

struct FVizCleanPolyDataFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    double tolerance;
    FVizBool remove_degenerate;
};

typedef struct FVizCleanNode
{
    int64_t qx;
    int64_t qy;
    int64_t qz;
    uint32_t new_id;
    uint32_t source_id;
    uint32_t next;
} FVizCleanNode;

static void fviz_clean_poly_data_filter_destroy(FVizObject* object)
{
    FVizCleanPolyDataFilter* filter = (FVizCleanPolyDataFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_clean_poly_data_filter_class = {FVIZ_TYPE_CLEAN_POLY_DATA_FILTER,
                                                                    "FVizCleanPolyDataFilter", &g_fviz_object_class,
                                                                    fviz_clean_poly_data_filter_destroy, NULL};

static uint64_t fviz_clean_mix64(uint64_t value)
{
    value ^= value >> 30u;
    value *= UINT64_C(0xBF58476D1CE4E5B9);
    value ^= value >> 27u;
    value *= UINT64_C(0x94D049BB133111EB);
    value ^= value >> 31u;
    return value;
}

static uint64_t fviz_clean_hash_cell(int64_t x, int64_t y, int64_t z)
{
    uint64_t h = fviz_clean_mix64((uint64_t)x + UINT64_C(0x9E3779B97F4A7C15));
    h ^= fviz_clean_mix64((uint64_t)y + UINT64_C(0xD1B54A32D192ED03));
    h ^= fviz_clean_mix64((uint64_t)z + UINT64_C(0x94D049BB133111EB));
    return fviz_clean_mix64(h);
}

static int64_t fviz_clean_quantize(float value, double tolerance)
{
    double q;
    if (tolerance <= 0.0)
    {
        union
        {
            float f;
            uint32_t u;
        } bits;

        bits.f = value == 0.0f ? 0.0f : value;
        return (int64_t)(uint64_t)bits.u;
    }
    q = floor((double)value / tolerance);
    if (q <= (double)INT64_MIN) return INT64_MIN;
    if (q >= (double)INT64_MAX) return INT64_MAX;
    return (int64_t)q;
}

static FVizBool fviz_clean_points_match(FVizVec3 a, FVizVec3 b, double tolerance)
{
    if (tolerance <= 0.0) return a.x == b.x && a.y == b.y && a.z == b.z ? FVIZ_TRUE : FVIZ_FALSE;
    {
        const double dx = (double)a.x - b.x;
        const double dy = (double)a.y - b.y;
        const double dz = (double)a.z - b.z;
        return dx * dx + dy * dy + dz * dz <= tolerance * tolerance ? FVIZ_TRUE : FVIZ_FALSE;
    }
}

static FVizSize fviz_clean_bucket_count(FVizSize point_count)
{
    FVizSize buckets = 16u;
    const FVizSize target = point_count > ((FVizSize)-1) / 2u ? (FVizSize)-1 : point_count * 2u;
    while (buckets < target && buckets <= ((FVizSize)-1) / 2u)
        buckets *= 2u;
    return buckets;
}

static void* fviz_clean_alloc_count(FVizSize count, FVizSize element_size)
{
    FVizSize bytes = 0u;
    if (count == 0u) return NULL;
    if (fviz_size_multiply(count, element_size, &bytes) != FVIZ_OK) return NULL;
    return fviz_alloc(bytes);
}

static int64_t fviz_clean_neighbor(int64_t value, int delta)
{
    if (delta < 0 && value == INT64_MIN) return INT64_MIN;
    if (delta > 0 && value == INT64_MAX) return INT64_MAX;
    return value + delta;
}

static FVizResult fviz_copy_attribute_set_contents(const FVizAttributeSet* source_set,
                                                   FVizAttributeSet* destination_set)
{
    FVizSize array_index;
    if (source_set == NULL || destination_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "attribute sets must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (array_index = 0u; array_index < fviz_attribute_set_count(source_set); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source_set, array_index);
        const FVizDataArray* source = fviz_attribute_set_const_array_at(source_set, array_index);
        FVizDataArray* copy = NULL;
        if (fviz_data_array_deep_copy(source, &copy) != FVIZ_OK) return fviz_last_error_code();
        if (fviz_attribute_set_add(destination_set, name, copy) != FVIZ_OK)
        {
            fviz_release(copy);
            return fviz_last_error_code();
        }
        fviz_release(copy);
    }
    {
        FVizAttributeRole role;
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source_set, role);
            if (active != NULL && fviz_attribute_set_set_active(destination_set, role, active) != FVIZ_OK)
                return fviz_last_error_code();
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_clean_copy_point_attributes(const FVizPolyData* input, FVizPolyData* output,
                                                   const FVizCleanNode* nodes, FVizSize unique_count)
{
    const FVizAttributeSet* source_set = fviz_poly_data_const_point_data(input);
    FVizSize array_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source_set); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source_set, array_index);
        const FVizDataArray* source = fviz_attribute_set_const_array_at(source_set, array_index);
        FVizDataArray* destination = NULL;
        FVizSize i;
        if (fviz_data_array_tuple_count(source) != fviz_poly_data_point_count(input)) continue;
        if (fviz_data_array_create(fviz_data_array_type(source), fviz_data_array_components(source), &destination) !=
                FVIZ_OK ||
            fviz_data_array_resize(destination, unique_count) != FVIZ_OK)
        {
            fviz_release(destination);
            return fviz_last_error_code();
        }
        for (i = 0u; i < unique_count; ++i)
            if (fviz_data_array_set_tuple(destination, i, fviz_data_array_const_tuple(source, nodes[i].source_id)) !=
                FVIZ_OK)
                goto fail_point_array;
        if (fviz_attribute_set_add(fviz_poly_data_point_data(output), name, destination) != FVIZ_OK)
            goto fail_point_array;
        {
            FVizAttributeRole role;
            for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
            {
                const char* active = fviz_attribute_set_active_name(source_set, role);
                if (active != NULL && strcmp(active, name) == 0 &&
                    fviz_attribute_set_set_active(fviz_poly_data_point_data(output), role, name) != FVIZ_OK)
                    goto fail_point_array;
            }
        }
        fviz_release(destination);
        continue;
    fail_point_array:
        fviz_release(destination);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_clean_copy_cell_attributes(const FVizPolyData* input, FVizPolyData* output,
                                                  const uint64_t* source_cell_ids, FVizSize output_cell_count,
                                                  const uint32_t* source_triangle_ids, FVizSize output_triangle_count,
                                                  FVizBool legacy_triangle_mapping_valid)
{
    const FVizAttributeSet* source_set = fviz_poly_data_const_cell_data(input);
    const FVizSize source_cell_count = fviz_poly_data_cell_count(input);
    const FVizSize source_triangle_count = fviz_poly_data_triangle_count(input);
    FVizSize array_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source_set); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source_set, array_index);
        const FVizDataArray* source = fviz_attribute_set_const_array_at(source_set, array_index);
        const FVizSize source_tuple_count = fviz_data_array_tuple_count(source);
        const uint64_t* remap = source_cell_ids;
        FVizSize destination_count = output_cell_count;
        FVizBool triangle_legacy = FVIZ_FALSE;
        FVizDataArray* destination = NULL;
        FVizSize i;
        FVizAttributeRole role;

        if (source_tuple_count == source_cell_count)
        {
            /* Preferred VTK-style contract: one tuple for each logical PolyData cell. */
        }
        else if (source_tuple_count == source_triangle_count && source_triangle_count != source_cell_count &&
                 legacy_triangle_mapping_valid != FVIZ_FALSE)
        {
            /* Compatibility with the pre-general-topology FEAViz contract. */
            remap = NULL;
            destination_count = output_triangle_count;
            triangle_legacy = FVIZ_TRUE;
        }
        else
        {
            continue;
        }

        if (fviz_data_array_create(fviz_data_array_type(source), fviz_data_array_components(source), &destination) !=
                FVIZ_OK ||
            fviz_data_array_resize(destination, destination_count) != FVIZ_OK)
        {
            fviz_release(destination);
            return fviz_last_error_code();
        }
        for (i = 0u; i < destination_count; ++i)
        {
            const FVizSize source_id =
                triangle_legacy != FVIZ_FALSE ? (FVizSize)source_triangle_ids[i] : (FVizSize)remap[i];
            if (source_id >= source_tuple_count ||
                fviz_data_array_set_tuple(destination, i, fviz_data_array_const_tuple(source, source_id)) != FVIZ_OK)
                goto fail_cell_array;
        }
        if (fviz_attribute_set_add(fviz_poly_data_cell_data(output), name, destination) != FVIZ_OK)
            goto fail_cell_array;
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source_set, role);
            if (active != NULL && strcmp(active, name) == 0 &&
                fviz_attribute_set_set_active(fviz_poly_data_cell_data(output), role, name) != FVIZ_OK)
                goto fail_cell_array;
        }
        fviz_release(destination);
        continue;
    fail_cell_array:
        fviz_release(destination);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizBool fviz_clean_id_exists(const uint32_t* ids, FVizSize count, uint32_t id)
{
    FVizSize i;
    for (i = 0u; i < count; ++i)
        if (ids[i] == id) return FVIZ_TRUE;
    return FVIZ_FALSE;
}

static FVizResult fviz_clean_append_remapped_cell(FVizPolyData* output, const FVizCellArray* cells, FVizSize cell_id,
                                                  const uint32_t* point_map, FVizBool remove_degenerate,
                                                  FVizBool* out_added, FVizBool* out_created_triangle)
{
    uint32_t stack_ids[32];
    uint32_t* mapped = stack_ids;
    const uint32_t* source_ids = fviz_cell_array_point_ids(cells, cell_id);
    const FVizSize source_count = fviz_cell_array_point_count(cells, cell_id);
    const FVizCellType type = fviz_cell_array_type(cells, cell_id);
    FVizSize mapped_count = 0u;
    FVizSize i;
    FVizResult result = FVIZ_OK;

    if (out_added != NULL) *out_added = FVIZ_FALSE;
    if (out_created_triangle != NULL) *out_created_triangle = FVIZ_FALSE;
    if (source_count > (FVizSize)(sizeof(stack_ids) / sizeof(stack_ids[0])))
    {
        FVizSize bytes = 0u;
        if (fviz_size_multiply(source_count, sizeof(*mapped), &bytes) != FVIZ_OK) return fviz_last_error_code();
        mapped = (uint32_t*)fviz_alloc(bytes);
        if (mapped == NULL) return fviz_last_error_code();
    }

    for (i = 0u; i < source_count; ++i)
    {
        const uint32_t id = point_map[source_ids[i]];
        FVizBool keep = FVIZ_TRUE;
        if (remove_degenerate != FVIZ_FALSE)
        {
            if (type == FVIZ_CELL_POLY_VERTEX || type == FVIZ_CELL_POLYGON || type == FVIZ_CELL_QUAD)
                keep = fviz_clean_id_exists(mapped, mapped_count, id) == FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
            else if ((type == FVIZ_CELL_POLY_LINE || type == FVIZ_CELL_TRIANGLE_STRIP) && mapped_count != 0u &&
                     mapped[mapped_count - 1u] == id)
                keep = FVIZ_FALSE;
        }
        if (keep != FVIZ_FALSE) mapped[mapped_count++] = id;
    }

    if (remove_degenerate != FVIZ_FALSE && mapped_count > 1u && (type == FVIZ_CELL_POLYGON || type == FVIZ_CELL_QUAD) &&
        mapped[0] == mapped[mapped_count - 1u])
        --mapped_count;

    switch (type)
    {
        case FVIZ_CELL_VERTEX:
            result = fviz_poly_data_add_vertex(output, mapped[0]);
            break;
        case FVIZ_CELL_POLY_VERTEX:
            if (mapped_count == 0u) goto done;
            result = fviz_poly_data_add_poly_vertex(output, mapped_count, mapped);
            break;
        case FVIZ_CELL_LINE:
            if (remove_degenerate != FVIZ_FALSE && mapped[0] == mapped[1]) goto done;
            result = fviz_poly_data_add_line(output, mapped[0], mapped[1]);
            break;
        case FVIZ_CELL_POLY_LINE:
            if (mapped_count < 2u) goto done;
            result = fviz_poly_data_add_poly_line(output, mapped_count, mapped);
            break;
        case FVIZ_CELL_TRIANGLE:
            if (remove_degenerate != FVIZ_FALSE &&
                (mapped[0] == mapped[1] || mapped[1] == mapped[2] || mapped[2] == mapped[0]))
                goto done;
            result = fviz_poly_data_add_triangle(output, mapped[0], mapped[1], mapped[2]);
            if (result == FVIZ_OK && out_created_triangle != NULL) *out_created_triangle = FVIZ_TRUE;
            break;
        case FVIZ_CELL_QUAD:
        case FVIZ_CELL_POLYGON:
            if (mapped_count < 3u) goto done;
            result = fviz_poly_data_add_polygon(output, mapped_count, mapped);
            if (result == FVIZ_OK && mapped_count == 3u && out_created_triangle != NULL)
                *out_created_triangle = FVIZ_TRUE;
            break;
        case FVIZ_CELL_TRIANGLE_STRIP:
            if (mapped_count < 3u) goto done;
            result = fviz_poly_data_add_triangle_strip(output, mapped_count, mapped);
            break;
        default:
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                    "clean poly data encountered unsupported PolyData cell type");
            result = FVIZ_ERROR_INVALID_STATE;
            break;
    }
    if (result == FVIZ_OK && out_added != NULL) *out_added = FVIZ_TRUE;
done:
    if (mapped != stack_ids) fviz_free(mapped);
    return result;
}

static FVizResult fviz_clean_poly_data_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                       void* state)
{
    FVizCleanPolyDataFilter* filter = (FVizCleanPolyDataFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    FVizSize point_count;
    FVizSize source_cell_count;
    FVizSize bucket_count;
    uint32_t* heads = NULL;
    FVizCleanNode* nodes = NULL;
    uint32_t* point_map = NULL;
    uint64_t* source_cell_ids = NULL;
    uint32_t* source_triangle_ids = NULL;
    FVizSize unique_count = 0u;
    FVizSize output_cell_count = 0u;
    FVizSize output_triangle_count = 0u;
    FVizSize old_id;
    const FVizVec3* points;
    FVizBool legacy_triangle_mapping_valid = FVIZ_TRUE;

    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "clean poly data filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_poly_data_validate(input) != FVIZ_OK) return fviz_last_error_code();
    point_count = fviz_poly_data_point_count(input);
    source_cell_count = fviz_poly_data_cell_count(input);
    points = fviz_poly_data_points(input);
    bucket_count = fviz_clean_bucket_count(point_count);
    heads = (uint32_t*)fviz_clean_alloc_count(bucket_count, sizeof(*heads));
    nodes = (FVizCleanNode*)fviz_clean_alloc_count(point_count, sizeof(*nodes));
    point_map = (uint32_t*)fviz_clean_alloc_count(point_count, sizeof(*point_map));
    source_cell_ids = (uint64_t*)fviz_clean_alloc_count(source_cell_count, sizeof(*source_cell_ids));
    source_triangle_ids =
        (uint32_t*)fviz_clean_alloc_count(fviz_poly_data_triangle_count(input), sizeof(*source_triangle_ids));
    if (heads == NULL || (point_count != 0u && (nodes == NULL || point_map == NULL)) ||
        (source_cell_count != 0u && source_cell_ids == NULL) ||
        (fviz_poly_data_triangle_count(input) != 0u && source_triangle_ids == NULL))
        goto fail;
    (void)memset(heads, 0xFF, bucket_count * sizeof(*heads));
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_reserve(output, point_count, fviz_poly_data_triangle_count(input)) != FVIZ_OK)
        goto fail;

    for (old_id = 0u; old_id < point_count; ++old_id)
    {
        const FVizVec3 point = points[old_id];
        const int64_t qx = fviz_clean_quantize(point.x, filter->tolerance);
        const int64_t qy = fviz_clean_quantize(point.y, filter->tolerance);
        const int64_t qz = fviz_clean_quantize(point.z, filter->tolerance);
        uint32_t match = UINT32_MAX;
        int dz;
        for (dz = filter->tolerance > 0.0 ? -1 : 0; dz <= (filter->tolerance > 0.0 ? 1 : 0) && match == UINT32_MAX;
             ++dz)
        {
            int dy;
            for (dy = filter->tolerance > 0.0 ? -1 : 0; dy <= (filter->tolerance > 0.0 ? 1 : 0) && match == UINT32_MAX;
                 ++dy)
            {
                int dx;
                for (dx = filter->tolerance > 0.0 ? -1 : 0;
                     dx <= (filter->tolerance > 0.0 ? 1 : 0) && match == UINT32_MAX; ++dx)
                {
                    const int64_t nx = fviz_clean_neighbor(qx, dx);
                    const int64_t ny = fviz_clean_neighbor(qy, dy);
                    const int64_t nz = fviz_clean_neighbor(qz, dz);
                    const FVizSize bucket =
                        (FVizSize)(fviz_clean_hash_cell(nx, ny, nz) & (uint64_t)(bucket_count - 1u));
                    uint32_t node_id = heads[bucket];
                    while (node_id != UINT32_MAX)
                    {
                        const FVizCleanNode* node = &nodes[node_id];
                        if (node->qx == nx && node->qy == ny && node->qz == nz &&
                            fviz_clean_points_match(point, fviz_poly_data_points(output)[node->new_id],
                                                    filter->tolerance) != FVIZ_FALSE)
                        {
                            match = node->new_id;
                            break;
                        }
                        node_id = node->next;
                    }
                }
            }
        }
        if (match == UINT32_MAX)
        {
            const FVizSize bucket = (FVizSize)(fviz_clean_hash_cell(qx, qy, qz) & (uint64_t)(bucket_count - 1u));
            uint32_t new_id;
            if (unique_count > UINT32_MAX || fviz_poly_data_add_point(output, point, &new_id) != FVIZ_OK) goto fail;
            nodes[unique_count].qx = qx;
            nodes[unique_count].qy = qy;
            nodes[unique_count].qz = qz;
            nodes[unique_count].new_id = new_id;
            nodes[unique_count].source_id = (uint32_t)old_id;
            nodes[unique_count].next = heads[bucket];
            heads[bucket] = (uint32_t)unique_count;
            match = new_id;
            ++unique_count;
        }
        point_map[old_id] = match;
    }

    {
        const FVizCellArray* categories[4] = {fviz_poly_data_verts(input), fviz_poly_data_lines(input),
                                              fviz_poly_data_polys(input), fviz_poly_data_strips(input)};
        FVizSize global_cell = 0u;
        FVizSize source_triangle_ordinal = 0u;
        uint32_t category;
        for (category = 0u; category < 4u; ++category)
        {
            FVizSize cell_id;
            for (cell_id = 0u; cell_id < fviz_cell_array_count(categories[category]); ++cell_id, ++global_cell)
            {
                const FVizCellType type = fviz_cell_array_type(categories[category], cell_id);
                FVizBool added = FVIZ_FALSE;
                FVizBool created_triangle = FVIZ_FALSE;
                const FVizSize triangle_ordinal_before = source_triangle_ordinal;
                if (type == FVIZ_CELL_TRIANGLE) ++source_triangle_ordinal;
                if (fviz_clean_append_remapped_cell(output, categories[category], cell_id, point_map,
                                                    filter->remove_degenerate, &added, &created_triangle) != FVIZ_OK)
                    goto fail;
                if (added != FVIZ_FALSE)
                {
                    source_cell_ids[output_cell_count++] = (uint64_t)global_cell;
                    if (created_triangle != FVIZ_FALSE)
                    {
                        if (type == FVIZ_CELL_TRIANGLE)
                            source_triangle_ids[output_triangle_count++] = (uint32_t)triangle_ordinal_before;
                        else
                        {
                            legacy_triangle_mapping_valid = FVIZ_FALSE;
                            ++output_triangle_count;
                        }
                    }
                }
            }
        }
    }

    if (fviz_clean_copy_point_attributes(input, output, nodes, unique_count) != FVIZ_OK ||
        fviz_clean_copy_cell_attributes(input, output, source_cell_ids, output_cell_count, source_triangle_ids,
                                        output_triangle_count, legacy_triangle_mapping_valid) != FVIZ_OK ||
        fviz_copy_attribute_set_contents(fviz_poly_data_const_field_data(input), fviz_poly_data_field_data(output)) !=
            FVIZ_OK)
        goto fail;
    {
        const FVizDataArray* legacy = fviz_poly_data_const_scalars(input);
        if (legacy != NULL && fviz_data_array_tuple_count(legacy) == point_count)
        {
            FVizDataArray* cleaned = NULL;
            FVizSize i;
            if (fviz_data_array_create(fviz_data_array_type(legacy), fviz_data_array_components(legacy), &cleaned) !=
                    FVIZ_OK ||
                fviz_data_array_resize(cleaned, unique_count) != FVIZ_OK)
            {
                fviz_release(cleaned);
                goto fail;
            }
            for (i = 0u; i < unique_count; ++i)
                if (fviz_data_array_set_tuple(cleaned, i, fviz_data_array_const_tuple(legacy, nodes[i].source_id)) !=
                    FVIZ_OK)
                {
                    fviz_release(cleaned);
                    goto fail;
                }
            if (fviz_poly_data_set_scalars(output, cleaned) != FVIZ_OK)
            {
                fviz_release(cleaned);
                goto fail;
            }
            fviz_release(cleaned);
        }
    }
    if ((fviz_poly_data_poly_cell_count(output) != 0u || fviz_poly_data_strip_cell_count(output) != 0u) &&
        fviz_poly_data_compute_normals(output) != FVIZ_OK)
        goto fail;
    if (fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_free(source_triangle_ids);
    fviz_free(source_cell_ids);
    fviz_free(point_map);
    fviz_free(nodes);
    fviz_free(heads);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_free(source_triangle_ids);
    fviz_free(source_cell_ids);
    fviz_free(point_map);
    fviz_free(nodes);
    fviz_free(heads);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_clean_poly_data_filter_create(FVizCleanPolyDataFilter** out_filter)
{
    FVizCleanPolyDataFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizCleanPolyDataFilter*)fviz_internal_object_allocate(sizeof(*filter),
                                                                     &g_fviz_clean_poly_data_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->tolerance = 0.0;
    filter->remove_degenerate = FVIZ_TRUE;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_clean_poly_data_process_request;
    callbacks.get_state_mtime = fviz_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) !=
            FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_clean_poly_data_filter_set_tolerance(FVizCleanPolyDataFilter* filter, double tolerance)
{
    if (filter == NULL || tolerance < 0.0 || !isfinite(tolerance))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "clean tolerance must be finite and non-negative");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    filter->tolerance = tolerance;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

double fviz_clean_poly_data_filter_tolerance(const FVizCleanPolyDataFilter* filter)
{
    return filter != NULL ? filter->tolerance : 0.0;
}

void fviz_clean_poly_data_filter_set_remove_degenerate(FVizCleanPolyDataFilter* filter, FVizBool remove_degenerate)
{
    if (filter != NULL)
    {
        filter->remove_degenerate = remove_degenerate != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        fviz_object_modified((FVizObject*)filter);
    }
}

FVizBool fviz_clean_poly_data_filter_remove_degenerate(const FVizCleanPolyDataFilter* filter)
{
    return filter != NULL ? filter->remove_degenerate : FVIZ_FALSE;
}

FVizResult fviz_clean_poly_data_filter_set_input_data(FVizCleanPolyDataFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_clean_poly_data_filter_set_input_connection(FVizCleanPolyDataFilter* filter, FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_clean_poly_data_filter_algorithm(FVizCleanPolyDataFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_clean_poly_data_filter_output_port(FVizCleanPolyDataFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_clean_poly_data_filter_output(FVizCleanPolyDataFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_clean_poly_data_filter_update(FVizCleanPolyDataFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
