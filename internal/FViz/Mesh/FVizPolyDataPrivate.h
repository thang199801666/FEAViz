#ifndef FVIZ_INTERNAL_MESH_POLY_DATA_PRIVATE_H
#define FVIZ_INTERNAL_MESH_POLY_DATA_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Mesh/FVizPolyData.h>

struct FVizPolyData
{
    FVizObject base;
    FVizArray* points;
    FVizArray* normals;
    FVizArray* indices;
    FVizBounds bounds;
    FVizBool bounds_dirty;
    FVizBool normals_dirty;
};

#endif /* FVIZ_INTERNAL_MESH_POLY_DATA_PRIVATE_H */
