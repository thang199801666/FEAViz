#ifndef FVIZ_SPATIAL_BVH_H
#define FVIZ_SPATIAL_BVH_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizRay.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Parallel/FVizParallel.h>

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

typedef struct FVizClosestPoint
{
    FVizVec3 point;
    FVizVec3 normal;
    FVizVec3 barycentric;
    float distance_squared;
    FVizSize triangle_index;
} FVizClosestPoint;

FVIZ_API FVizResult fviz_bvh_create(FVizBVH** out_bvh);
FVIZ_API FVizResult fviz_bvh_build(FVizBVH* bvh, const FVizPolyData* poly_data);
/* Recompute node bounds while preserving the existing primitive partition.
 * This is intended for deformed/animated geometry whose triangle count is unchanged. */
FVIZ_API FVizResult fviz_bvh_refit(FVizBVH* bvh);
/* Synchronize the acceleration structure with its retained PolyData source.
 * Geometry-only changes choose the O(N) refit path; topology changes rebuild. */
FVIZ_API FVizResult fviz_bvh_update(FVizBVH* bvh);
FVIZ_API FVizBool fviz_bvh_valid(const FVizBVH* bvh);
FVIZ_API FVizBool fviz_bvh_current(const FVizBVH* bvh);
FVIZ_API FVizBool fviz_bvh_refit_required(const FVizBVH* bvh);
FVIZ_API FVizSize fviz_bvh_triangle_count(const FVizBVH* bvh);
FVIZ_API FVizBool fviz_bvh_ray_cast(const FVizBVH* bvh, FVizRay ray, FVizRayHit* out_hit);
FVIZ_API FVizBool fviz_bvh_ray_cast_any(const FVizBVH* bvh, FVizRay ray);
/* Finds the closest point on any indexed triangle. A negative max_distance
 * means unbounded search; otherwise NOT_FOUND is returned when no triangle is
 * within that world-space distance. */
FVIZ_API FVizResult fviz_bvh_closest_point(const FVizBVH* bvh, FVizVec3 query, float max_distance,
                                           FVizClosestPoint* out_result);
/* Executes independent queries in parallel when profitable.  Each result flag
 * is written deterministically at the same index as its input query. */
FVIZ_API FVizResult fviz_bvh_ray_cast_batch(const FVizBVH* bvh, const FVizRay* rays, FVizSize query_count,
                                            FVizRayHit* out_hits, FVizBool* out_hit_flags,
                                            FVizCancellationToken* cancellation);
FVIZ_API FVizResult fviz_bvh_closest_point_batch(const FVizBVH* bvh, const FVizVec3* queries, FVizSize query_count,
                                                 float max_distance, FVizClosestPoint* out_results,
                                                 FVizBool* out_found_flags, FVizCancellationToken* cancellation);
/* CPU bytes owned directly by the acceleration structure, excluding retained
 * source PolyData memory. */
FVIZ_API FVizSize fviz_bvh_memory_size(const FVizBVH* bvh);
/* Broad-phase overlap against primitive AABBs.  Returns false only when no
 * triangle bounds overlap the query; callers requiring exact triangle/box
 * intersection may refine the returned candidates. */
FVIZ_API FVizBool fviz_bvh_intersects_bounds(const FVizBVH* bvh, const FVizBounds* bounds);

FVIZ_EXTERN_C_END

#endif /* FVIZ_SPATIAL_BVH_H */
