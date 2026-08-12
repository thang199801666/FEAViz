#include <FViz/Core/FVizError.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Pipeline/FVizAlgorithmPrivate.h>
#include <FViz/Pipeline/FVizExecutivePrivate.h>

static const FVizObjectClass g_fviz_executive_class = {
    FVIZ_TYPE_EXECUTIVE,
    "FVizExecutive",
    &g_fviz_object_class,
    NULL,
    NULL
};

FVizResult fviz_internal_executive_create(
    FVizAlgorithm* algorithm,
    FVizExecutive** out_executive)
{
    FVizExecutive* executive;
    if (algorithm == NULL || out_executive == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "executive requires an algorithm and output pointer");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_executive = NULL;
    executive = (FVizExecutive*)fviz_internal_object_allocate(
        sizeof(FVizExecutive), &g_fviz_executive_class, NULL);
    if (executive == NULL) return fviz_last_error_code();
    executive->algorithm = algorithm;
    executive->last_result = FVIZ_OK;
    *out_executive = executive;
    return FVIZ_OK;
}

FVizAlgorithm* fviz_executive_algorithm(FVizExecutive* executive)
{
    return executive != NULL ? executive->algorithm : NULL;
}

FVizResult fviz_executive_update(FVizExecutive* executive, uint32_t output_port)
{
    FVizResult result;
    if (executive == NULL || executive->algorithm == NULL ||
        output_port >= executive->algorithm->output_port_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "executive output port is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    executive->last_request = FVIZ_PIPELINE_REQUEST_INFORMATION;
    executive->last_request = FVIZ_PIPELINE_REQUEST_DATA_OBJECT;
    executive->last_request = FVIZ_PIPELINE_REQUEST_UPDATE_EXTENT;
    executive->last_request = FVIZ_PIPELINE_REQUEST_DATA;
    result = fviz_internal_algorithm_update_now(executive->algorithm);
    executive->last_result = result;
    if (result == FVIZ_OK)
    {
        if (executive->algorithm->last_update_executed == FVIZ_TRUE)
            ++executive->execution_count;
        else
            ++executive->cache_hit_count;
    }
    return result;
}

FVizPipelineRequest fviz_executive_last_request(const FVizExecutive* executive)
{
    return executive != NULL ? executive->last_request : FVIZ_PIPELINE_REQUEST_NONE;
}

uint64_t fviz_executive_execution_count(const FVizExecutive* executive)
{
    return executive != NULL ? executive->execution_count : 0u;
}

uint64_t fviz_executive_cache_hit_count(const FVizExecutive* executive)
{
    return executive != NULL ? executive->cache_hit_count : 0u;
}

FVizResult fviz_executive_last_result(const FVizExecutive* executive)
{
    return executive != NULL ? executive->last_result : FVIZ_ERROR_INVALID_ARGUMENT;
}

void fviz_executive_reset_statistics(FVizExecutive* executive)
{
    if (executive == NULL) return;
    executive->execution_count = 0u;
    executive->cache_hit_count = 0u;
}
