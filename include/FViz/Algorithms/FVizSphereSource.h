#ifndef FVIZ_ALGORITHMS_SPHERE_SOURCE_H
#define FVIZ_ALGORITHMS_SPHERE_SOURCE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizSphereSource FVizSphereSource;
#define FVIZ_TYPE_SPHERE_SOURCE UINT64_C(0xC16E590B4A72D83F)

FVIZ_API FVizResult fviz_sphere_source_create(FVizSphereSource** out_source);
FVIZ_API void fviz_sphere_source_set_center(FVizSphereSource* source, FVizVec3 center);
FVIZ_API FVizResult fviz_sphere_source_set_radius(FVizSphereSource* source, double radius);
FVIZ_API FVizResult fviz_sphere_source_set_resolution(FVizSphereSource* source, uint32_t theta_resolution,
                                                      uint32_t phi_resolution);
FVIZ_API FVizVec3 fviz_sphere_source_center(const FVizSphereSource* source);
FVIZ_API double fviz_sphere_source_radius(const FVizSphereSource* source);
FVIZ_API uint32_t fviz_sphere_source_theta_resolution(const FVizSphereSource* source);
FVIZ_API uint32_t fviz_sphere_source_phi_resolution(const FVizSphereSource* source);
FVIZ_API FVizAlgorithm* fviz_sphere_source_algorithm(FVizSphereSource* source);
FVIZ_API FVizAlgorithmOutput* fviz_sphere_source_output_port(FVizSphereSource* source);
FVIZ_API FVizPolyData* fviz_sphere_source_output(FVizSphereSource* source);
FVIZ_API FVizResult fviz_sphere_source_update(FVizSphereSource* source);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_SPHERE_SOURCE_H */
