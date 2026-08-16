#include <string.h>

#include <FViz/Algorithms/FVizRectilinearGridGeometryFilter.h>
#include <FViz/Algorithms/FVizStructuredGridGeometryFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizStructuredGrid.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizRectilinearGridGeometryFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
};

static void fviz_rectilinear_geometry_destroy(FVizObject* object)
{
    FVizRectilinearGridGeometryFilter* filter = (FVizRectilinearGridGeometryFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_rectilinear_geometry_class = {
    FVIZ_TYPE_RECTILINEAR_GRID_GEOMETRY_FILTER,
    "FVizRectilinearGridGeometryFilter",
    &g_fviz_object_class,
    fviz_rectilinear_geometry_destroy,
    NULL
};

static FVizMTime fviz_rectilinear_geometry_state_mtime(const void* state)
{ return fviz_object_mtime((const FVizObject*)state); }

static FVizResult fviz_rectilinear_geometry_copy_attributes(
    const FVizAttributeSet* source, FVizAttributeSet* destination)
{
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        const FVizDataArray* array = fviz_attribute_set_const_array_at(source, i);
        FVizDataArray* copy = NULL;
        FVizAttributeRole role;
        if (fviz_data_array_deep_copy(array, &copy) != FVIZ_OK ||
            fviz_attribute_set_add(destination, name, copy) != FVIZ_OK)
        {
            fviz_release(copy);
            return fviz_last_error_code();
        }
        fviz_release(copy);
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(destination, role, name);
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_rectilinear_geometry_process_request(
    FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request, void* state)
{
    FVizRectilinearGrid* input;
    FVizStructuredGrid* structured = NULL;
    FVizStructuredGridGeometryFilter* delegate = NULL;
    FVizVec3* points = NULL;
    FVizSize point_count;
    FVizSize i;
    int64_t extent[6];
    FVizPolyData* output;
    FVizResult result = FVIZ_OK;
    (void)state;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizRectilinearGrid*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL || fviz_rectilinear_grid_validate(input) != FVIZ_OK)
    {
        if (input == NULL)
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "RectilinearGrid geometry filter has no input");
        return input == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
    }
    point_count = fviz_rectilinear_grid_point_count(input);
    if (point_count != 0u)
    {
        FVizSize bytes;
        if (fviz_size_multiply(point_count, sizeof(FVizVec3), &bytes) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        points = (FVizVec3*)fviz_alloc(bytes);
        if (points == NULL) return fviz_last_error_code();
        for (i = 0u; i < point_count; ++i)
            if (fviz_rectilinear_grid_point(input, (FVizId)i, &points[i]) != FVIZ_OK)
            {
                result = fviz_last_error_code();
                goto done;
            }
    }
    fviz_rectilinear_grid_extent(input, extent);
    if (fviz_structured_grid_create(&structured) != FVIZ_OK ||
        fviz_structured_grid_set_extent(structured, extent) != FVIZ_OK ||
        fviz_structured_grid_set_points(structured, points, point_count) != FVIZ_OK ||
        fviz_rectilinear_geometry_copy_attributes(
            fviz_rectilinear_grid_const_point_data(input), fviz_structured_grid_point_data(structured)) != FVIZ_OK ||
        fviz_rectilinear_geometry_copy_attributes(
            fviz_rectilinear_grid_const_cell_data(input), fviz_structured_grid_cell_data(structured)) != FVIZ_OK ||
        fviz_rectilinear_geometry_copy_attributes(
            fviz_rectilinear_grid_const_field_data(input), fviz_structured_grid_field_data(structured)) != FVIZ_OK ||
        fviz_structured_grid_geometry_filter_create(&delegate) != FVIZ_OK ||
        fviz_structured_grid_geometry_filter_set_input_data(delegate, structured) != FVIZ_OK ||
        fviz_structured_grid_geometry_filter_update(delegate) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto done;
    }
    output = fviz_structured_grid_geometry_filter_output(delegate);
    if (output == NULL ||
        fviz_algorithm_set_output_data(
            algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
    {
        result = output == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
        if (output == NULL)
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "RectilinearGrid geometry delegate produced no output");
        goto done;
    }
done:
    fviz_release(delegate);
    fviz_release(structured);
    fviz_free(points);
    return result;
}

FVizResult fviz_rectilinear_grid_geometry_filter_create(
    FVizRectilinearGridGeometryFilter** out_filter)
{
    FVizRectilinearGridGeometryFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizRectilinearGridGeometryFilter*)fviz_internal_object_allocate(
        sizeof(*filter), &g_fviz_rectilinear_geometry_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_rectilinear_geometry_process_request;
    callbacks.get_state_mtime = fviz_rectilinear_geometry_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(
            filter->algorithm, 0u, FVIZ_TYPE_RECTILINEAR_GRID, FVIZ_FALSE, FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_rectilinear_grid_geometry_filter_set_input_data(
    FVizRectilinearGridGeometryFilter* filter, FVizRectilinearGrid* input)
{
    if (filter == NULL || input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "RectilinearGrid geometry filter input is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input);
}

FVizResult fviz_rectilinear_grid_geometry_filter_set_input_connection(
    FVizRectilinearGridGeometryFilter* filter, FVizAlgorithmOutput* input)
{
    if (filter == NULL || input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "RectilinearGrid geometry filter connection is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_algorithm_set_input_connection(filter->algorithm, 0u, input);
}

FVizAlgorithm* fviz_rectilinear_grid_geometry_filter_algorithm(FVizRectilinearGridGeometryFilter* filter)
{ return filter != NULL ? filter->algorithm : NULL; }
FVizAlgorithmOutput* fviz_rectilinear_grid_geometry_filter_output_port(FVizRectilinearGridGeometryFilter* filter)
{ return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL; }
FVizPolyData* fviz_rectilinear_grid_geometry_filter_output(FVizRectilinearGridGeometryFilter* filter)
{ return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL; }
FVizResult fviz_rectilinear_grid_geometry_filter_update(FVizRectilinearGridGeometryFilter* filter)
{
    if (filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_algorithm_update(filter->algorithm);
}
