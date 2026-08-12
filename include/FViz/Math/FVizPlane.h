#ifndef FVIZ_MATH_PLANE_H
#define FVIZ_MATH_PLANE_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPlane
{
    FVizVec3 normal;
    float distance;
} FVizPlane;

FVIZ_API FVizPlane fviz_plane_from_point_normal(FVizVec3 point, FVizVec3 normal);
FVIZ_API float fviz_plane_distance_to_point(FVizPlane plane, FVizVec3 point);
FVIZ_API FVizVec3 fviz_plane_project_point(FVizPlane plane, FVizVec3 point);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_PLANE_H */
