#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Pipeline/FVizFilter.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/FEA/FVizUnstructuredGridPrivate.h>
#include <FViz/Pipeline/FVizFilterPrivate.h>

static void fviz_filter_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_filter_class = {
    FVIZ_TYPE_FILTER, "FVizFilter", &g_fviz_object_class, fviz_filter_destroy
};
static const FVizObjectClass g_fviz_threshold_filter_class = {
    FVIZ_TYPE_THRESHOLD_FILTER, "FVizThresholdFilter", &g_fviz_object_class, fviz_filter_destroy
};
static const FVizObjectClass g_fviz_warp_filter_class = {
    FVIZ_TYPE_WARP_FILTER, "FVizWarpFilter", &g_fviz_object_class, fviz_filter_destroy
};
static const FVizObjectClass g_fviz_cell_to_point_filter_class = {
    FVIZ_TYPE_CELL_DATA_TO_POINT_FILTER, "FVizCellDataToPointFilter", &g_fviz_object_class, fviz_filter_destroy
};

static void fviz_filter_destroy(FVizObject* object)
{
    FVizFilter* filter = (FVizFilter*)object;
    fviz_release(filter->input);
    fviz_release(filter->output);
    filter->input = NULL;
    filter->output = NULL;
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
    if (filter == NULL)
    {
        return fviz_last_error_code();
    }
    filter->kind = kind;
    filter->input = NULL;
    filter->output = NULL;
    filter->input_generation = 0u;
    filter->updated = FVIZ_FALSE;
    *out_filter = filter;
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
    {
        return fviz_last_error_code();
    }
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
    {
        return fviz_last_error_code();
    }
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

FVizResult fviz_filter_set_input(FVizFilter* filter, const FVizUnstructuredGrid* input)
{
    if (filter == NULL || input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "filter and input must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_retain((FVizUnstructuredGrid*)input) == NULL)
    {
        return fviz_last_error_code();
    }
    fviz_release(filter->input);
    filter->input = (FVizUnstructuredGrid*)input;
    filter->updated = FVIZ_FALSE;
    return FVIZ_OK;
}

const FVizUnstructuredGrid* fviz_filter_const_input(const FVizFilter* filter)
{
    return filter != NULL ? filter->input : NULL;
}

FVizUnstructuredGrid* fviz_filter_output(FVizFilter* filter)
{
    return filter != NULL ? filter->output : NULL;
}

static FVizResult fviz_filter_execute(FVizFilter* filter, FVizUnstructuredGrid** out_result)
{
    FVizUnstructuredGrid* result = NULL;
    FVizResult status;
    switch (filter->kind)
    {
        case FVIZ_FILTER_THRESHOLD:
            status = fviz_unstructured_grid_threshold_cells(
                filter->input, filter->scalar_name, filter->minimum, filter->maximum, &result);
            break;
        case FVIZ_FILTER_WARP:
            status = fviz_unstructured_grid_warp_by_vector(
                filter->input, filter->vector_name, filter->scale, &result);
            break;
        case FVIZ_FILTER_CELL_TO_POINT:
            status = fviz_unstructured_grid_cell_data_to_point_data(filter->input, &result);
            break;
        default:
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "unknown filter kind");
            return FVIZ_ERROR_INVALID_STATE;
    }
    if (status != FVIZ_OK)
    {
        fviz_release(result);
        return status;
    }
    *out_result = result;
    return FVIZ_OK;
}

FVizResult fviz_filter_update(FVizFilter* filter)
{
    FVizUnstructuredGrid* result = NULL;
    uint32_t generation;
    FVizResult status;
    if (filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (filter->input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    generation = fviz_internal_unstructured_grid_generation(filter->input);
    if (filter->updated == FVIZ_TRUE && filter->input_generation == generation)
    {
        return FVIZ_OK;
    }
    status = fviz_filter_execute(filter, &result);
    if (status != FVIZ_OK)
    {
        return status;
    }
    fviz_release(filter->output);
    filter->output = result;
    filter->input_generation = generation;
    filter->updated = FVIZ_TRUE;
    return FVIZ_OK;
}
