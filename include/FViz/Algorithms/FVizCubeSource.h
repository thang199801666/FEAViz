#ifndef FVIZ_ALGORITHMS_CUBE_SOURCE_H
#define FVIZ_ALGORITHMS_CUBE_SOURCE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizCubeSource FVizCubeSource;
#define FVIZ_TYPE_CUBE_SOURCE UINT64_C(0x7E54A91C2D38B6F0)

FVIZ_API FVizResult fviz_cube_source_create(FVizCubeSource** out_source);
FVIZ_API void fviz_cube_source_set_center(FVizCubeSource* source, FVizVec3 center);
FVIZ_API FVizResult fviz_cube_source_set_lengths(
    FVizCubeSource* source,
    double x_length,
    double y_length,
    double z_length);
FVIZ_API FVizResult fviz_cube_source_set_bounds(FVizCubeSource* source, FVizBounds bounds);
FVIZ_API FVizVec3 fviz_cube_source_center(const FVizCubeSource* source);
FVIZ_API double fviz_cube_source_x_length(const FVizCubeSource* source);
FVIZ_API double fviz_cube_source_y_length(const FVizCubeSource* source);
FVIZ_API double fviz_cube_source_z_length(const FVizCubeSource* source);
FVIZ_API FVizAlgorithm* fviz_cube_source_algorithm(FVizCubeSource* source);
FVIZ_API FVizAlgorithmOutput* fviz_cube_source_output_port(FVizCubeSource* source);
FVIZ_API FVizPolyData* fviz_cube_source_output(FVizCubeSource* source);
FVIZ_API FVizResult fviz_cube_source_update(FVizCubeSource* source);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_CUBE_SOURCE_H */
