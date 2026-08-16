#ifndef FVIZ_INTERNAL_SPATIAL_BVH_PRIVATE_H
#define FVIZ_INTERNAL_SPATIAL_BVH_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Spatial/FVizBVH.h>

#define FVIZ_BVH_LEAF_SIZE 8u
#define FVIZ_BVH_MAX_DEPTH 32u

typedef struct FVizBVHNode
{
    FVizBounds bounds;
    int32_t left;
    int32_t right;
    int32_t triangle_begin;
    int32_t triangle_end;
} FVizBVHNode;

struct FVizBVH
{
    FVizObject base;
    FVizBVHNode* nodes;
    FVizSize node_count;
    FVizSize node_capacity;
    int32_t root;
    uint32_t* triangle_indices;
    FVizSize triangle_count;
    FVizSize triangle_capacity;
    FVizBounds* triangle_bounds_cache;
    FVizVec3* triangle_centroids;
    FVizPolyData* poly_data;
    FVizMTime source_geometry_mtime;
    FVizMTime source_topology_mtime;
    FVizBool valid;
};

#endif /* FVIZ_INTERNAL_SPATIAL_BVH_PRIVATE_H */
