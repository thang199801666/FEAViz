#ifndef FVIZ_ALGORITHMS_LINE_SOURCE_H
#define FVIZ_ALGORITHMS_LINE_SOURCE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizLineSource FVizLineSource;
#define FVIZ_TYPE_LINE_SOURCE UINT64_C(0x5E9A1C7D32B8F406)

FVIZ_FILTERS_API FVizResult fviz_line_source_create(FVizLineSource** out_source);
FVIZ_FILTERS_API FVizResult fviz_line_source_set_points(FVizLineSource* source, FVizVec3 point0, FVizVec3 point1);
FVIZ_FILTERS_API FVizResult fviz_line_source_set_resolution(FVizLineSource* source, uint32_t resolution);
FVIZ_FILTERS_API FVizVec3 fviz_line_source_point0(const FVizLineSource* source);
FVIZ_FILTERS_API FVizVec3 fviz_line_source_point1(const FVizLineSource* source);
FVIZ_FILTERS_API uint32_t fviz_line_source_resolution(const FVizLineSource* source);
FVIZ_FILTERS_API FVizAlgorithm* fviz_line_source_algorithm(FVizLineSource* source);
FVIZ_FILTERS_API FVizAlgorithmOutput* fviz_line_source_output_port(FVizLineSource* source);
FVIZ_FILTERS_API FVizPolyData* fviz_line_source_output(FVizLineSource* source);
FVIZ_FILTERS_API FVizResult fviz_line_source_update(FVizLineSource* source);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_LINE_SOURCE_H */
