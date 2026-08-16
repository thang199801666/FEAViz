#include <FViz/Algorithms/FVizUnstructuredGridGeometryFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizUnstructuredGridGeometryFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
};

static void fviz_ug_geometry_destroy(FVizObject* object)
{
    FVizUnstructuredGridGeometryFilter* filter = (FVizUnstructuredGridGeometryFilter*)object;
    fviz_release(filter->algorithm);
}

static const FVizObjectClass g_fviz_ug_geometry_class = {FVIZ_TYPE_UNSTRUCTURED_GRID_GEOMETRY_FILTER,
                                                         "FVizUnstructuredGridGeometryFilter", &g_fviz_object_class,
                                                         fviz_ug_geometry_destroy, NULL};

static FVizMTime fviz_ug_geometry_state_mtime(const void* state)
{
    return state != NULL ? fviz_object_mtime((const FVizObject*)state) : 0u;
}

static FVizResult fviz_ug_geometry_process(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                           void* state)
{
    FVizUnstructuredGrid* input;
    FVizPolyData* output = NULL;
    (void)state;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizUnstructuredGrid*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "UnstructuredGrid geometry filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_unstructured_grid_extract_geometry(input, &output) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_algorithm_set_output_data(algorithm, 0u, (FVizDataObject*)output) != FVIZ_OK)
    {
        fviz_release(output);
        return fviz_last_error_code();
    }
    fviz_release(output);
    return FVIZ_OK;
}

FVizResult fviz_unstructured_grid_geometry_filter_create(FVizUnstructuredGridGeometryFilter** out_filter)
{
    FVizUnstructuredGridGeometryFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizUnstructuredGridGeometryFilter*)fviz_internal_object_allocate(sizeof(*filter),
                                                                                &g_fviz_ug_geometry_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_ug_geometry_process;
    callbacks.get_state_mtime = fviz_ug_geometry_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_UNSTRUCTURED_GRID, FVIZ_FALSE,
                                            FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_unstructured_grid_geometry_filter_set_input_data(FVizUnstructuredGridGeometryFilter* filter,
                                                                 FVizUnstructuredGrid* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_unstructured_grid_geometry_filter_set_input_connection(FVizUnstructuredGridGeometryFilter* filter,
                                                                       FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_unstructured_grid_geometry_filter_algorithm(FVizUnstructuredGridGeometryFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_unstructured_grid_geometry_filter_output_port(FVizUnstructuredGridGeometryFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_unstructured_grid_geometry_filter_output(FVizUnstructuredGridGeometryFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_unstructured_grid_geometry_filter_update(FVizUnstructuredGridGeometryFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
