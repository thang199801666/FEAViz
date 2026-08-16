#ifndef FVIZ_ALGORITHMS_WARP_VECTOR_FILTER_H
#define FVIZ_ALGORITHMS_WARP_VECTOR_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizWarpVectorFilter FVizWarpVectorFilter;
#define FVIZ_TYPE_WARP_VECTOR_FILTER UINT64_C(0xF61B29D47A8C305E)

FVIZ_API FVizResult fviz_warp_vector_filter_create(FVizWarpVectorFilter** out_filter);
FVIZ_API FVizResult fviz_warp_vector_filter_set_input_data(FVizWarpVectorFilter* filter, FVizPolyData* input);
FVIZ_API FVizResult fviz_warp_vector_filter_set_input_connection(FVizWarpVectorFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizResult fviz_warp_vector_filter_set_vector_name(FVizWarpVectorFilter* filter, const char* name);
FVIZ_API const char* fviz_warp_vector_filter_vector_name(const FVizWarpVectorFilter* filter);
FVIZ_API void fviz_warp_vector_filter_set_scale(FVizWarpVectorFilter* filter, double scale);
FVIZ_API double fviz_warp_vector_filter_scale(const FVizWarpVectorFilter* filter);
FVIZ_API void fviz_warp_vector_filter_set_recompute_normals(FVizWarpVectorFilter* filter, FVizBool enabled);
FVIZ_API FVizBool fviz_warp_vector_filter_recompute_normals(const FVizWarpVectorFilter* filter);
FVIZ_API FVizAlgorithm* fviz_warp_vector_filter_algorithm(FVizWarpVectorFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_warp_vector_filter_output_port(FVizWarpVectorFilter* filter);
FVIZ_API FVizPolyData* fviz_warp_vector_filter_output(FVizWarpVectorFilter* filter);
FVIZ_API FVizResult fviz_warp_vector_filter_update(FVizWarpVectorFilter* filter);

FVIZ_EXTERN_C_END

#endif
