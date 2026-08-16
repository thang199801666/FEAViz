#include <stdint.h>
#include <string.h>

#include <FViz/Algorithms/FVizProbeFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Spatial/FVizPointLocator.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizProbeFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
};

typedef struct FVizProbeArray
{
    const char* name;
    const FVizDataArray* source;
    FVizDataArray* destination;
} FVizProbeArray;


static FVizMTime fviz_probe_filter_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static void fviz_probe_filter_destroy(FVizObject* object)
{
    FVizProbeFilter* filter = (FVizProbeFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_probe_filter_class = {
    FVIZ_TYPE_PROBE_FILTER,
    "FVizProbeFilter",
    &g_fviz_object_class,
    fviz_probe_filter_destroy,
    NULL
};

static void fviz_probe_write_value(unsigned char* destination, FVizDataType type, double value)
{
    switch (type)
    {
        case FVIZ_DATA_INT8: { int8_t v = (int8_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_UINT8: { uint8_t v = (uint8_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_INT16: { int16_t v = (int16_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_UINT16: { uint16_t v = (uint16_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_INT32: { int32_t v = (int32_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_UINT32: { uint32_t v = (uint32_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_INT64: { int64_t v = (int64_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_UINT64: { uint64_t v = (uint64_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_FLOAT32: { float v = (float)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_FLOAT64: memcpy(destination, &value, sizeof(value)); break;
        default: break;
    }
}

static FVizResult fviz_probe_location_weights(
    const FVizUnstructuredGrid* source,
    const FVizLocatedCell* location,
    FVizId out_ids[20],
    double weights[20])
{
    const FVizCellArray* cells = fviz_unstructured_grid_cells((FVizUnstructuredGrid*)source);
    const FVizCellType type = fviz_cell_array_type(cells, location->cell_index);
    FVizCellView view;
    FVizSize i;
    if (fviz_cell_array_cell_view(cells, location->cell_index, &view) != FVIZ_OK ||
        view.point_count != location->point_count || view.point_count > 20u)
        return fviz_last_error_code();
    for (i = 0u; i < view.point_count; ++i) out_ids[i] = fviz_cell_view_point_id(&view, i);
    for (i = 0u; i < 20u; ++i) weights[i] = 0.0;
    {
        FVizSize weight_count = 0u;
        if (fviz_cell_type_shape_weights(type, location->barycentric, weights, 20u, &weight_count) != FVIZ_OK ||
            weight_count != location->point_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "probe filter has no compatible interpolation weights");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_probe_interpolate_tuple(
    const FVizDataArray* source,
    const FVizId* ids,
    const double* weights,
    FVizSize point_count,
    unsigned char* scratch)
{
    const uint32_t components = fviz_data_array_components(source);
    const FVizDataType type = fviz_data_array_type(source);
    const FVizSize type_size = fviz_data_type_size(type);
    uint32_t component;
    for (component = 0u; component < components; ++component)
    {
        double value = 0.0;
        FVizSize i;
        for (i = 0u; i < point_count; ++i)
        {
            double sample = 0.0;
            if (fviz_data_array_get_component(source, (FVizSize)ids[i], component, &sample) != FVIZ_OK)
                return fviz_last_error_code();
            value += weights[i] * sample;
        }
        fviz_probe_write_value(scratch + (FVizSize)component * type_size, type, value);
    }
    return FVIZ_OK;
}

static FVizResult fviz_probe_filter_process_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* state)
{
    FVizPolyData* input;
    FVizUnstructuredGrid* source;
    FVizPolyData* output = NULL;
    FVizPointLocator* locator = NULL;
    FVizProbeArray* arrays = NULL;
    FVizSize array_count = 0u;
    FVizSize max_stride = 1u;
    unsigned char* scratch = NULL;
    FVizDataArray* valid_mask = NULL;
    FVizSize point_count;
    FVizSize source_points;
    FVizDataArray* probed_active_scalars = NULL;
    FVizSize i;
    FVizSize bytes;
    (void)state;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    source = (FVizUnstructuredGrid*)fviz_algorithm_resolved_input(algorithm, 1u, 0u);
    if (input == NULL || source == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "probe filter requires both PolyData input and UnstructuredGrid source");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_poly_data_validate(input) != FVIZ_OK || fviz_unstructured_grid_validate(source) != FVIZ_OK)
        return fviz_last_error_code();
    point_count = fviz_poly_data_point_count(input);
    source_points = fviz_unstructured_grid_point_count(source);
    if (fviz_poly_data_deep_copy(input, &output) != FVIZ_OK ||
        fviz_point_locator_create(&locator) != FVIZ_OK ||
        fviz_point_locator_set_grid(locator, source) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &valid_mask) != FVIZ_OK ||
        fviz_data_array_reserve(valid_mask, point_count) != FVIZ_OK)
        goto fail;

    {
        const FVizAttributeSet* source_set = fviz_unstructured_grid_point_data(source);
        const char* active_scalar_name = fviz_attribute_set_active_name(source_set, FVIZ_ATTRIBUTE_SCALARS);
        const FVizSize candidate_count = fviz_attribute_set_count(source_set);
        if (candidate_count != 0u)
        {
            if (fviz_size_multiply(candidate_count, sizeof(*arrays), &bytes) != FVIZ_OK) goto fail;
            arrays = (FVizProbeArray*)fviz_alloc(bytes);
            if (arrays == NULL) goto fail;
            memset(arrays, 0, bytes);
        }
        for (i = 0u; i < candidate_count; ++i)
        {
            const FVizDataArray* source_array = fviz_attribute_set_const_array_at(source_set, i);
            const char* name = fviz_attribute_set_name_at(source_set, i);
            FVizDataArray* destination = NULL;
            FVizAttributeRole role;
            if (fviz_data_array_tuple_count(source_array) != source_points) continue;
            if (fviz_data_array_create(
                    fviz_data_array_type(source_array), fviz_data_array_components(source_array), &destination) != FVIZ_OK ||
                fviz_data_array_reserve(destination, point_count) != FVIZ_OK)
            {
                fviz_release(destination);
                goto fail;
            }
            if (fviz_data_array_tuple_stride(source_array) > max_stride)
                max_stride = fviz_data_array_tuple_stride(source_array);
            (void)fviz_attribute_set_remove(fviz_poly_data_point_data(output), name);
            if (fviz_attribute_set_add(fviz_poly_data_point_data(output), name, destination) != FVIZ_OK)
            {
                fviz_release(destination);
                goto fail;
            }
            arrays[array_count].name = name;
            arrays[array_count].source = source_array;
            arrays[array_count].destination = destination;
            if (active_scalar_name != NULL && strcmp(active_scalar_name, name) == 0)
                probed_active_scalars = destination;
            for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
            {
                const char* active = fviz_attribute_set_active_name(source_set, role);
                if (active != NULL && strcmp(active, name) == 0)
                    (void)fviz_attribute_set_set_active(fviz_poly_data_point_data(output), role, name);
            }
            ++array_count;
        }
    }
    scratch = (unsigned char*)fviz_alloc(max_stride);
    if (scratch == NULL) goto fail;
    for (i = 0u; i < point_count; ++i)
    {
        FVizLocatedCell location;
        uint8_t valid = 0u;
        const FVizBool located = fviz_point_locator_locate_point(locator, fviz_poly_data_points(input)[i], &location);
        FVizId ids[20];
        double weights[20];
        FVizSize array_index;
        if (located != FVIZ_FALSE)
        {
            if (fviz_probe_location_weights(source, &location, ids, weights) != FVIZ_OK) goto fail;
            valid = 1u;
        }
        for (array_index = 0u; array_index < array_count; ++array_index)
        {
            FVizDataArray* destination = arrays[array_index].destination;
            if (valid != 0u)
            {
                if (fviz_probe_interpolate_tuple(
                        arrays[array_index].source, ids, weights, location.point_count, scratch) != FVIZ_OK ||
                    fviz_data_array_append_tuple(destination, scratch) != FVIZ_OK)
                    goto fail;
            }
            else
            {
                memset(scratch, 0, fviz_data_array_tuple_stride(destination));
                if (fviz_data_array_append_tuple(destination, scratch) != FVIZ_OK) goto fail;
            }
        }
        if (fviz_data_array_append_tuple(valid_mask, &valid) != FVIZ_OK) goto fail;
        if ((i & 4095u) == 4095u)
        {
            if (fviz_algorithm_abort_requested(algorithm) != FVIZ_FALSE)
            {
                fviz_internal_set_error(FVIZ_ERROR_CANCELLED, "probe filter was aborted");
                goto fail;
            }
            if (point_count != 0u)
                (void)fviz_algorithm_report_progress(algorithm, (double)(i + 1u) / (double)point_count);
        }
    }
    (void)fviz_attribute_set_remove(fviz_poly_data_point_data(output), "FVizValidPointMask");
    if (fviz_attribute_set_add(fviz_poly_data_point_data(output), "FVizValidPointMask", valid_mask) != FVIZ_OK ||
        (probed_active_scalars != NULL && fviz_poly_data_set_scalars(output, probed_active_scalars) != FVIZ_OK) ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    (void)fviz_algorithm_report_progress(algorithm, 1.0);
    for (i = 0u; i < array_count; ++i) fviz_release(arrays[i].destination);
    fviz_free(scratch);
    fviz_free(arrays);
    fviz_release(valid_mask);
    fviz_release(locator);
    fviz_release(output);
    return FVIZ_OK;
fail:
    for (i = 0u; i < array_count; ++i) fviz_release(arrays[i].destination);
    fviz_free(scratch);
    fviz_free(arrays);
    fviz_release(valid_mask);
    fviz_release(locator);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_probe_filter_create(FVizProbeFilter** out_filter)
{
    FVizProbeFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizProbeFilter*)fviz_internal_object_allocate(sizeof(*filter), &g_fviz_probe_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_probe_filter_process_request;
    callbacks.get_state_mtime = fviz_probe_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(2u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 1u, FVIZ_TYPE_UNSTRUCTURED_GRID, FVIZ_FALSE, FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_probe_filter_set_input_data(FVizProbeFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input) : FVIZ_ERROR_INVALID_ARGUMENT;
}
FVizResult fviz_probe_filter_set_input_connection(FVizProbeFilter* filter, FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input) : FVIZ_ERROR_INVALID_ARGUMENT;
}
FVizResult fviz_probe_filter_set_source_data(FVizProbeFilter* filter, FVizUnstructuredGrid* source)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 1u, (FVizDataObject*)source) : FVIZ_ERROR_INVALID_ARGUMENT;
}
FVizResult fviz_probe_filter_set_source_connection(FVizProbeFilter* filter, FVizAlgorithmOutput* source)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 1u, source) : FVIZ_ERROR_INVALID_ARGUMENT;
}
FVizAlgorithm* fviz_probe_filter_algorithm(FVizProbeFilter* filter) { return filter != NULL ? filter->algorithm : NULL; }
FVizAlgorithmOutput* fviz_probe_filter_output_port(FVizProbeFilter* filter) { return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL; }
FVizPolyData* fviz_probe_filter_output(FVizProbeFilter* filter) { return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL; }
FVizResult fviz_probe_filter_update(FVizProbeFilter* filter) { return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT; }
