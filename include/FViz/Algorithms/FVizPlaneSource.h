#ifndef FVIZ_ALGORITHMS_PLANE_SOURCE_H
#define FVIZ_ALGORITHMS_PLANE_SOURCE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPlaneSource FVizPlaneSource;
#define FVIZ_TYPE_PLANE_SOURCE UINT64_C(0x2A6B84D913E7C501)

FVIZ_API FVizResult fviz_plane_source_create(FVizPlaneSource** out_source);
FVIZ_API void fviz_plane_source_set_origin(FVizPlaneSource* source, FVizVec3 origin);
FVIZ_API void fviz_plane_source_set_point1(FVizPlaneSource* source, FVizVec3 point1);
FVIZ_API void fviz_plane_source_set_point2(FVizPlaneSource* source, FVizVec3 point2);
FVIZ_API FVizResult fviz_plane_source_set_resolution(FVizPlaneSource* source, uint32_t x_resolution,
                                                     uint32_t y_resolution);
FVIZ_API FVizVec3 fviz_plane_source_origin(const FVizPlaneSource* source);
FVIZ_API FVizVec3 fviz_plane_source_point1(const FVizPlaneSource* source);
FVIZ_API FVizVec3 fviz_plane_source_point2(const FVizPlaneSource* source);
FVIZ_API uint32_t fviz_plane_source_x_resolution(const FVizPlaneSource* source);
FVIZ_API uint32_t fviz_plane_source_y_resolution(const FVizPlaneSource* source);
FVIZ_API FVizAlgorithm* fviz_plane_source_algorithm(FVizPlaneSource* source);
FVIZ_API FVizAlgorithmOutput* fviz_plane_source_output_port(FVizPlaneSource* source);
FVIZ_API FVizPolyData* fviz_plane_source_output(FVizPlaneSource* source);
FVIZ_API FVizResult fviz_plane_source_update(FVizPlaneSource* source);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_PLANE_SOURCE_H */
