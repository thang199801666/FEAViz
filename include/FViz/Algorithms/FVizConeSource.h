#ifndef FVIZ_ALGORITHMS_CONE_SOURCE_H
#define FVIZ_ALGORITHMS_CONE_SOURCE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizConeSource FVizConeSource;
#define FVIZ_TYPE_CONE_SOURCE UINT64_C(0x1A3F6B82C947D5E0)

FVIZ_FILTERS_API FVizResult fviz_cone_source_create(FVizConeSource** out_source);
FVIZ_FILTERS_API void fviz_cone_source_set_height(FVizConeSource* source, double height);
FVIZ_FILTERS_API void fviz_cone_source_set_radius(FVizConeSource* source, double radius);
FVIZ_FILTERS_API void fviz_cone_source_set_resolution(FVizConeSource* source, uint32_t resolution);
FVIZ_FILTERS_API void fviz_cone_source_set_center(FVizConeSource* source, FVizVec3 center);
FVIZ_FILTERS_API void fviz_cone_source_set_direction(FVizConeSource* source, FVizVec3 direction);
FVIZ_FILTERS_API FVizResult fviz_cone_source_set_capping(FVizConeSource* source, FVizBool capping);
FVIZ_FILTERS_API double fviz_cone_source_height(const FVizConeSource* source);
FVIZ_FILTERS_API double fviz_cone_source_radius(const FVizConeSource* source);
FVIZ_FILTERS_API uint32_t fviz_cone_source_resolution(const FVizConeSource* source);
FVIZ_FILTERS_API FVizVec3 fviz_cone_source_center(const FVizConeSource* source);
FVIZ_FILTERS_API FVizVec3 fviz_cone_source_direction(const FVizConeSource* source);
FVIZ_FILTERS_API FVizBool fviz_cone_source_capping(const FVizConeSource* source);
FVIZ_FILTERS_API FVizAlgorithm* fviz_cone_source_algorithm(FVizConeSource* source);
FVIZ_FILTERS_API FVizAlgorithmOutput* fviz_cone_source_output_port(FVizConeSource* source);
FVIZ_FILTERS_API FVizPolyData* fviz_cone_source_output(FVizConeSource* source);
FVIZ_FILTERS_API FVizResult fviz_cone_source_update(FVizConeSource* source);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_CONE_SOURCE_H */
