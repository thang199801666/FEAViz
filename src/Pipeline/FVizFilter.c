#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Pipeline/FVizFilter.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Pipeline/FVizFilterPrivate.h>

static void fviz_filter_destroy(FVizObject* object);
static FVizMTime fviz_filter_mtime(const FVizObject* object);
static FVizResult fviz_filter_execute_algorithm(FVizAlgorithm* algorithm);
static const FVizObjectClass g_fviz_filter_class = {
    FVIZ_TYPE_FILTER, "FVizFilter", &g_fviz_algorithm_class, fviz_filter_destroy, fviz_filter_mtime
};
static const FVizObjectClass g_fviz_threshold_filter_class = {
    FVIZ_TYPE_THRESHOLD_FILTER, "FVizThresholdFilter", &g_fviz_filter_class, fviz_filter_destroy
};
static const FVizObjectClass g_fviz_warp_filter_class = {
    FVIZ_TYPE_WARP_FILTER, "FVizWarpFilter", &g_fviz_filter_class, fviz_filter_destroy
};
static const FVizObjectClass g_fviz_cell_to_point_filter_class = {
    FVIZ_TYPE_CELL_DATA_TO_POINT_FILTER, "FVizCellDataToPointFilter", &g_fviz_filter_class, fviz_filter_destroy
};
static const FVizObjectClass g_fviz_surface_filter_class = {
    FVIZ_TYPE_SURFACE_FILTER, "FVizSurfaceFilter", &g_fviz_filter_class, fviz_filter_destroy
};
static const FVizObjectClass g_fviz_slice_filter_class = {
    FVIZ_TYPE_SLICE_FILTER, "FVizSliceFilter", &g_fviz_filter_class, fviz_filter_destroy
};
static const FVizObjectClass g_fviz_transform_filter_class = {
    FVIZ_TYPE_TRANSFORM_FILTER, "FVizTransformFilter", &g_fviz_filter_class, fviz_filter_destroy, fviz_filter_mtime
};

static FVizMTime fviz_filter_mtime(const FVizObject* object)
{
    const FVizFilter* filter = (const FVizFilter*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    const FVizMTime transform_mtime = fviz_object_mtime((const FVizObject*)filter->transform);
    if (transform_mtime > mtime) mtime = transform_mtime;
    return mtime;
}

static FVizFilterOutputType fviz_filter_kind_output_type(FVizFilterKind kind)
{
    return kind == FVIZ_FILTER_SURFACE || kind == FVIZ_FILTER_SLICE
        ? FVIZ_FILTER_OUTPUT_POLY_DATA
        : FVIZ_FILTER_OUTPUT_UNSTRUCTURED_GRID;
}

static void fviz_filter_destroy(FVizObject* object)
{
    FVizFilter* filter = (FVizFilter*)object;
    fviz_internal_algorithm_deinitialize(&filter->base);
    fviz_release(filter->transform);
    filter->transform = NULL;
}

static FVizResult fviz_filter_create_internal(
    FVizFilterKind kind,
    const FVizObjectClass* object_class,
    FVizFilter** out_filter)
{
    FVizFilter* filter;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizFilter*)fviz_internal_object_allocate(sizeof(FVizFilter), object_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->kind = kind;
    filter->transfer_scalars = FVIZ_TRUE;
    if (fviz_internal_algorithm_initialize(&filter->base, 1u, 1u, fviz_filter_execute_algorithm) != FVIZ_OK ||
        fviz_internal_algorithm_configure_input_port(
            &filter->base, 0u, FVIZ_TYPE_UNSTRUCTURED_GRID, FVIZ_FALSE, FVIZ_FALSE) != FVIZ_OK ||
        fviz_internal_algorithm_configure_output_port(
            &filter->base,
            0u,
            fviz_filter_kind_output_type(kind) == FVIZ_FILTER_OUTPUT_POLY_DATA
                ? FVIZ_TYPE_POLY_DATA
                : FVIZ_TYPE_UNSTRUCTURED_GRID) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

static FVizResult fviz_filter_require_kind(
    FVizFilter* filter,
    FVizFilterKind expected,
    const char* message)
{
    if (filter == NULL || filter->kind != expected)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, message);
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return FVIZ_OK;
}

FVizResult fviz_threshold_filter_create(
    const char* scalar_name,
    double minimum,
    double maximum,
    FVizFilter** out_filter)
{
    FVizFilter* filter;
    if (scalar_name == NULL || scalar_name[0] == '\0' || minimum > maximum)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "threshold filter requires a scalar name and valid range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_filter_create_internal(FVIZ_FILTER_THRESHOLD, &g_fviz_threshold_filter_class, &filter) != FVIZ_OK)
        return fviz_last_error_code();
    (void)strncpy(filter->scalar_name, scalar_name, sizeof(filter->scalar_name) - 1u);
    filter->scalar_name[sizeof(filter->scalar_name) - 1u] = '\0';
    filter->minimum = minimum;
    filter->maximum = maximum;
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_warp_filter_create(
    const char* vector_name,
    double scale,
    FVizFilter** out_filter)
{
    FVizFilter* filter;
    if (vector_name == NULL || vector_name[0] == '\0')
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "warp filter requires a vector name");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_filter_create_internal(FVIZ_FILTER_WARP, &g_fviz_warp_filter_class, &filter) != FVIZ_OK)
        return fviz_last_error_code();
    (void)strncpy(filter->vector_name, vector_name, sizeof(filter->vector_name) - 1u);
    filter->vector_name[sizeof(filter->vector_name) - 1u] = '\0';
    filter->scale = scale;
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_cell_data_to_point_filter_create(FVizFilter** out_filter)
{
    return fviz_filter_create_internal(FVIZ_FILTER_CELL_TO_POINT, &g_fviz_cell_to_point_filter_class, out_filter);
}

FVizResult fviz_surface_filter_create(FVizBool transfer_scalars, FVizFilter** out_filter)
{
    FVizResult result = fviz_filter_create_internal(
        FVIZ_FILTER_SURFACE, &g_fviz_surface_filter_class, out_filter);
    if (result == FVIZ_OK)
        (*out_filter)->transfer_scalars = transfer_scalars != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    return result;
}

FVizResult fviz_slice_filter_create(FVizPlane plane, FVizFilter** out_filter)
{
    FVizResult result;
    if (fviz_vec3_length(plane.normal) <= 1.0e-8f)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "slice plane normal must not be zero");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result = fviz_filter_create_internal(FVIZ_FILTER_SLICE, &g_fviz_slice_filter_class, out_filter);
    if (result == FVIZ_OK) (*out_filter)->plane = plane;
    return result;
}

FVizResult fviz_transform_filter_create(FVizTransform* transform, FVizFilter** out_filter)
{
    FVizResult result;
    if (transform == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "transform must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result = fviz_filter_create_internal(
        FVIZ_FILTER_TRANSFORM, &g_fviz_transform_filter_class, out_filter);
    if (result != FVIZ_OK) return result;
    if (fviz_retain(transform) == NULL)
    {
        fviz_release(*out_filter);
        *out_filter = NULL;
        return fviz_last_error_code();
    }
    (*out_filter)->transform = transform;
    return FVIZ_OK;
}

FVizResult fviz_threshold_filter_set_scalar_name(FVizFilter* filter, const char* scalar_name)
{
    if (fviz_filter_require_kind(filter, FVIZ_FILTER_THRESHOLD, "filter is not a threshold filter") != FVIZ_OK)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (scalar_name == NULL || scalar_name[0] == '\0')
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "scalar_name must not be empty");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)strncpy(filter->scalar_name, scalar_name, sizeof(filter->scalar_name) - 1u);
    filter->scalar_name[sizeof(filter->scalar_name) - 1u] = '\0';
    filter->base.updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizResult fviz_threshold_filter_set_range(FVizFilter* filter, double minimum, double maximum)
{
    if (fviz_filter_require_kind(filter, FVIZ_FILTER_THRESHOLD, "filter is not a threshold filter") != FVIZ_OK)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (minimum > maximum)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "threshold minimum must not exceed maximum");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    filter->minimum = minimum;
    filter->maximum = maximum;
    filter->base.updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizResult fviz_warp_filter_set_vector_name(FVizFilter* filter, const char* vector_name)
{
    if (fviz_filter_require_kind(filter, FVIZ_FILTER_WARP, "filter is not a warp filter") != FVIZ_OK)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (vector_name == NULL || vector_name[0] == '\0')
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "vector_name must not be empty");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)strncpy(filter->vector_name, vector_name, sizeof(filter->vector_name) - 1u);
    filter->vector_name[sizeof(filter->vector_name) - 1u] = '\0';
    filter->base.updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizResult fviz_warp_filter_set_scale(FVizFilter* filter, double scale)
{
    if (fviz_filter_require_kind(filter, FVIZ_FILTER_WARP, "filter is not a warp filter") != FVIZ_OK)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    filter->scale = scale;
    filter->base.updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizResult fviz_surface_filter_set_transfer_scalars(FVizFilter* filter, FVizBool transfer_scalars)
{
    if (fviz_filter_require_kind(filter, FVIZ_FILTER_SURFACE, "filter is not a surface filter") != FVIZ_OK)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    filter->transfer_scalars = transfer_scalars != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    filter->base.updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizResult fviz_slice_filter_set_plane(FVizFilter* filter, FVizPlane plane)
{
    if (fviz_filter_require_kind(filter, FVIZ_FILTER_SLICE, "filter is not a slice filter") != FVIZ_OK)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_vec3_length(plane.normal) <= 1.0e-8f)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "slice plane normal must not be zero");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    filter->plane = plane;
    filter->base.updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizResult fviz_transform_filter_set_transform(FVizFilter* filter, FVizTransform* transform)
{
    if (fviz_filter_require_kind(filter, FVIZ_FILTER_TRANSFORM, "filter is not a transform filter") != FVIZ_OK)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (transform == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "transform must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_retain(transform) == NULL) return fviz_last_error_code();
    fviz_release(filter->transform);
    filter->transform = transform;
    filter->base.updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizTransform* fviz_transform_filter_transform(FVizFilter* filter)
{
    return filter != NULL && filter->kind == FVIZ_FILTER_TRANSFORM ? filter->transform : NULL;
}

FVizResult fviz_filter_set_input(FVizFilter* filter, const FVizUnstructuredGrid* input)
{
    if (filter == NULL || input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "filter and input must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_algorithm_set_input_data(
        &filter->base, 0u, (FVizDataObject*)(FVizUnstructuredGrid*)input);
}

FVizResult fviz_filter_set_input_connection(FVizFilter* filter, FVizFilter* upstream)
{
    if (filter == NULL || upstream == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "filter and upstream must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_algorithm_set_input_connection(
        &filter->base, 0u, fviz_algorithm_output_port(&upstream->base, 0u));
}

FVizFilter* fviz_filter_input_connection(FVizFilter* filter)
{
    FVizAlgorithmOutput* output = filter != NULL
        ? fviz_algorithm_input_connection(&filter->base, 0u, 0u)
        : NULL;
    FVizAlgorithm* producer = fviz_algorithm_output_producer(output);
    return producer != NULL && fviz_object_is_type((const FVizObject*)producer, FVIZ_TYPE_FILTER)
        ? (FVizFilter*)producer
        : NULL;
}

FVizAlgorithm* fviz_filter_algorithm(FVizFilter* filter)
{
    return filter != NULL ? &filter->base : NULL;
}

FVizAlgorithmOutput* fviz_filter_output_port(FVizFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(&filter->base, 0u) : NULL;
}

const FVizUnstructuredGrid* fviz_filter_const_input(const FVizFilter* filter)
{
    return filter != NULL
        ? (const FVizUnstructuredGrid*)fviz_algorithm_input_data(&filter->base, 0u)
        : NULL;
}

FVizFilterOutputType fviz_filter_output_type(const FVizFilter* filter)
{
    return filter != NULL
        ? fviz_filter_kind_output_type(filter->kind)
        : FVIZ_FILTER_OUTPUT_NONE;
}

FVizUnstructuredGrid* fviz_filter_output(FVizFilter* filter)
{
    return filter != NULL && fviz_filter_output_type(filter) == FVIZ_FILTER_OUTPUT_UNSTRUCTURED_GRID
        ? (FVizUnstructuredGrid*)fviz_algorithm_output_data(&filter->base, 0u) : NULL;
}

FVizPolyData* fviz_filter_poly_data_output(FVizFilter* filter)
{
    return filter != NULL && fviz_filter_output_type(filter) == FVIZ_FILTER_OUTPUT_POLY_DATA
        ? (FVizPolyData*)fviz_algorithm_output_data(&filter->base, 0u) : NULL;
}

static FVizResult fviz_filter_execute_algorithm(FVizAlgorithm* algorithm)
{
    FVizFilter* filter = (FVizFilter*)algorithm;
    FVizUnstructuredGrid* input =
        (FVizUnstructuredGrid*)fviz_internal_algorithm_resolved_input(algorithm, 0u, 0u);
    FVizUnstructuredGrid* grid_result = NULL;
    FVizPolyData* poly_result = NULL;
    FVizResult status;
    switch (filter->kind)
    {
        case FVIZ_FILTER_THRESHOLD:
            status = fviz_unstructured_grid_threshold_cells(
                input, filter->scalar_name, filter->minimum, filter->maximum, &grid_result);
            break;
        case FVIZ_FILTER_WARP:
            status = fviz_unstructured_grid_warp_by_vector(
                input, filter->vector_name, filter->scale, &grid_result);
            break;
        case FVIZ_FILTER_CELL_TO_POINT:
            status = fviz_unstructured_grid_cell_data_to_point_data(input, &grid_result);
            break;
        case FVIZ_FILTER_SURFACE:
            status = filter->transfer_scalars == FVIZ_TRUE
                ? fviz_unstructured_grid_extract_surface_scalars(input, &poly_result)
                : fviz_unstructured_grid_extract_surface(input, &poly_result);
            if (status == FVIZ_OK && fviz_poly_data_triangle_count(poly_result) > 0u)
                status = fviz_poly_data_compute_normals(poly_result);
            break;
        case FVIZ_FILTER_SLICE:
            status = fviz_unstructured_grid_slice(input, filter->plane, &poly_result);
            break;
        case FVIZ_FILTER_TRANSFORM:
            status = fviz_unstructured_grid_transform(input, filter->transform, &grid_result);
            break;
        default:
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "unknown filter kind");
            return FVIZ_ERROR_INVALID_STATE;
    }
    if (status == FVIZ_OK)
    {
        status = fviz_internal_algorithm_set_output_data(
            algorithm,
            0u,
            grid_result != NULL
                ? (FVizDataObject*)grid_result
                : (FVizDataObject*)poly_result);
    }
    fviz_release(grid_result);
    fviz_release(poly_result);
    return status;
}

FVizResult fviz_filter_update(FVizFilter* filter)
{
    return fviz_algorithm_update(filter != NULL ? &filter->base : NULL);
}
