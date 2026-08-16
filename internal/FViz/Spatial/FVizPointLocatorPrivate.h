#ifndef FVIZ_INTERNAL_SPATIAL_POINT_LOCATOR_PRIVATE_H
#define FVIZ_INTERNAL_SPATIAL_POINT_LOCATOR_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Spatial/FVizPointLocator.h>
#include <FViz/Math/FVizBounds.h>

typedef struct FVizPointLocatorNode
{
    FVizBounds bounds;
    int32_t left;
    int32_t right;
    FVizSize begin;
    FVizSize end;
} FVizPointLocatorNode;

struct FVizPointLocator
{
    FVizObject base;
    FVizUnstructuredGrid* grid;
    FVizPointLocatorNode* nodes;
    FVizSize node_count;
    FVizSize node_capacity;
    FVizSize* cell_ids;
    FVizBounds* cell_bounds;
    FVizVec3* cell_centroids;
    FVizSize cell_count;
    FVizMTime build_mtime;
    FVizMTime build_points_mtime;
    FVizMTime build_cells_mtime;
};

#endif /* FVIZ_INTERNAL_SPATIAL_POINT_LOCATOR_PRIVATE_H */
