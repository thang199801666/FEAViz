#ifndef FVIZ_ALGORITHMS_CYLINDER_SOURCE_H
#define FVIZ_ALGORITHMS_CYLINDER_SOURCE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizCylinderSource FVizCylinderSource;
#define FVIZ_TYPE_CYLINDER_SOURCE UINT64_C(0x42E8C1A9F5B67D30)

FVIZ_FILTERS_API FVizResult fviz_cylinder_source_create(FVizCylinderSource** out_source);
FVIZ_FILTERS_API void fviz_cylinder_source_set_height(FVizCylinderSource* source, double height);
FVIZ_FILTERS_API void fviz_cylinder_source_set_radius(FVizCylinderSource* source, double radius);
FVIZ_FILTERS_API void fviz_cylinder_source_set_resolution(FVizCylinderSource* source, uint32_t resolution);
FVIZ_FILTERS_API void fviz_cylinder_source_set_center(FVizCylinderSource* source, FVizVec3 center);
FVIZ_FILTERS_API void fviz_cylinder_source_set_direction(FVizCylinderSource* source, FVizVec3 direction);
FVIZ_FILTERS_API FVizResult fviz_cylinder_source_set_capping(FVizCylinderSource* source, FVizBool capping);
FVIZ_FILTERS_API double fviz_cylinder_source_height(const FVizCylinderSource* source);
FVIZ_FILTERS_API double fviz_cylinder_source_radius(const FVizCylinderSource* source);
FVIZ_FILTERS_API uint32_t fviz_cylinder_source_resolution(const FVizCylinderSource* source);
FVIZ_FILTERS_API FVizVec3 fviz_cylinder_source_center(const FVizCylinderSource* source);
FVIZ_FILTERS_API FVizVec3 fviz_cylinder_source_direction(const FVizCylinderSource* source);
FVIZ_FILTERS_API FVizBool fviz_cylinder_source_capping(const FVizCylinderSource* source);
FVIZ_FILTERS_API FVizAlgorithm* fviz_cylinder_source_algorithm(FVizCylinderSource* source);
FVIZ_FILTERS_API FVizAlgorithmOutput* fviz_cylinder_source_output_port(FVizCylinderSource* source);
FVIZ_FILTERS_API FVizPolyData* fviz_cylinder_source_output(FVizCylinderSource* source);
FVIZ_FILTERS_API FVizResult fviz_cylinder_source_update(FVizCylinderSource* source);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_CYLINDER_SOURCE_H */
