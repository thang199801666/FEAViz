#ifndef FVIZ_PIPELINE_EXECUTIVE_H
#define FVIZ_PIPELINE_EXECUTIVE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizAlgorithm FVizAlgorithm;
typedef struct FVizExecutive FVizExecutive;

#define FVIZ_TYPE_EXECUTIVE UINT64_C(0xB9D3416E2A705CF8)

typedef enum FVizPipelineRequest
{
    FVIZ_PIPELINE_REQUEST_NONE = 0,
    FVIZ_PIPELINE_REQUEST_INFORMATION = 1,
    FVIZ_PIPELINE_REQUEST_DATA_OBJECT = 2,
    FVIZ_PIPELINE_REQUEST_UPDATE_EXTENT = 3,
    FVIZ_PIPELINE_REQUEST_DATA = 4
} FVizPipelineRequest;

FVIZ_API FVizAlgorithm* fviz_executive_algorithm(FVizExecutive* executive);
FVIZ_API FVizResult fviz_executive_update(FVizExecutive* executive, uint32_t output_port);
FVIZ_API FVizPipelineRequest fviz_executive_last_request(const FVizExecutive* executive);
FVIZ_API uint64_t fviz_executive_execution_count(const FVizExecutive* executive);
FVIZ_API uint64_t fviz_executive_cache_hit_count(const FVizExecutive* executive);
FVIZ_API FVizResult fviz_executive_last_result(const FVizExecutive* executive);
FVIZ_API void fviz_executive_reset_statistics(FVizExecutive* executive);

FVIZ_EXTERN_C_END

#endif /* FVIZ_PIPELINE_EXECUTIVE_H */
