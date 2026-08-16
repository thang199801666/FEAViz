#ifndef FVIZ_ALGORITHMS_TUBE_FILTER_H
#define FVIZ_ALGORITHMS_TUBE_FILTER_H

#include <stdint.h>
#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizTubeFilter FVizTubeFilter;
#define FVIZ_TYPE_TUBE_FILTER UINT64_C(0x82A716D4E9C35B01)

FVIZ_API FVizResult fviz_tube_filter_create(FVizTubeFilter** out_filter);
FVIZ_API FVizResult fviz_tube_filter_set_input_data(FVizTubeFilter* filter,FVizPolyData* input);
FVIZ_API FVizResult fviz_tube_filter_set_input_connection(FVizTubeFilter* filter,FVizAlgorithmOutput* input);
FVIZ_API void fviz_tube_filter_set_radius(FVizTubeFilter* filter,double radius);
FVIZ_API double fviz_tube_filter_radius(const FVizTubeFilter* filter);
FVIZ_API void fviz_tube_filter_set_sides(FVizTubeFilter* filter,uint32_t sides);
FVIZ_API uint32_t fviz_tube_filter_sides(const FVizTubeFilter* filter);
FVIZ_API void fviz_tube_filter_set_capping(FVizTubeFilter* filter,FVizBool enabled);
FVIZ_API FVizBool fviz_tube_filter_capping(const FVizTubeFilter* filter);
FVIZ_API FVizAlgorithm* fviz_tube_filter_algorithm(FVizTubeFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_tube_filter_output_port(FVizTubeFilter* filter);
FVIZ_API FVizPolyData* fviz_tube_filter_output(FVizTubeFilter* filter);
FVIZ_API FVizResult fviz_tube_filter_update(FVizTubeFilter* filter);

FVIZ_EXTERN_C_END
#endif
