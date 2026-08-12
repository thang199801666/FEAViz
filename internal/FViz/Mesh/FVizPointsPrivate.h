#ifndef FVIZ_INTERNAL_MESH_POINTS_PRIVATE_H
#define FVIZ_INTERNAL_MESH_POINTS_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Mesh/FVizPoints.h>

struct FVizPoints
{
    FVizObject base;
    FVizArray* data;
    FVizBounds bounds;
};

#endif /* FVIZ_INTERNAL_MESH_POINTS_PRIVATE_H */
