#ifndef FVIZ_MATH_RAY_H
#define FVIZ_MATH_RAY_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRay
{
    FVizVec3 origin;
    FVizVec3 direction;
} FVizRay;

FVIZ_CORE_API FVizRay fviz_ray(FVizVec3 origin, FVizVec3 direction);
FVIZ_CORE_API FVizVec3 fviz_ray_point_at(FVizRay ray, float t);
FVIZ_CORE_API float fviz_ray_distance_to_point(FVizRay ray, FVizVec3 point);
FVIZ_CORE_API FVizBool fviz_ray_intersects_sphere(FVizRay ray, FVizVec3 center, float radius, float* out_t);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_RAY_H */
