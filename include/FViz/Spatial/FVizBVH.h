#ifndef FVIZ_SPATIAL_BVH_H
#define FVIZ_SPATIAL_BVH_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizRay.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizBVH FVizBVH;
#define FVIZ_TYPE_BVH UINT64_C(0x2B4D7E9F1A3C5E87)

typedef struct FVizRayHit
{
    FVizVec3 point;
    FVizVec3 normal;
    float distance;
    FVizSize triangle_index;
} FVizRayHit;

FVIZ_API FVizResult fviz_bvh_create(FVizBVH** out_bvh);
FVIZ_API FVizResult fviz_bvh_build(FVizBVH* bvh, const FVizPolyData* poly_data);
FVIZ_API FVizBool fviz_bvh_valid(const FVizBVH* bvh);
FVIZ_API FVizSize fviz_bvh_triangle_count(const FVizBVH* bvh);
FVIZ_API FVizBool fviz_bvh_ray_cast(const FVizBVH* bvh, FVizRay ray, FVizRayHit* out_hit);
FVIZ_API FVizBool fviz_bvh_ray_cast_any(const FVizBVH* bvh, FVizRay ray);
FVIZ_API FVizBool fviz_bvh_intersects_bounds(const FVizBVH* bvh, const FVizBounds* bounds);

FVIZ_EXTERN_C_END

#endif /* FVIZ_SPATIAL_BVH_H */
