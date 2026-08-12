#ifndef FVIZ_MESH_POINTS_H
#define FVIZ_MESH_POINTS_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPoints FVizPoints;
#define FVIZ_TYPE_POINTS UINT64_C(0xA1C7B52D8E304F69)

FVIZ_API FVizResult fviz_points_create(FVizPoints** out_points);
FVIZ_API void fviz_points_clear(FVizPoints* points);
FVIZ_API FVizResult fviz_points_reserve(FVizPoints* points, FVizSize capacity);
FVIZ_API FVizResult fviz_points_append(FVizPoints* points, FVizVec3 point, uint32_t* out_id);
FVIZ_API FVizSize fviz_points_count(const FVizPoints* points);
FVIZ_API const FVizVec3* fviz_points_data(const FVizPoints* points);
FVIZ_API FVizBounds fviz_points_bounds(const FVizPoints* points);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MESH_POINTS_H */
