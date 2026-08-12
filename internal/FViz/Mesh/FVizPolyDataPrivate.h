#ifndef FVIZ_INTERNAL_MESH_POLY_DATA_PRIVATE_H
#define FVIZ_INTERNAL_MESH_POLY_DATA_PRIVATE_H

#include <stdint.h>

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizTypes.h>
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
    uint32_t generation;
};

uint32_t fviz_internal_poly_data_generation(const FVizPolyData* poly_data);

#endif /* FVIZ_INTERNAL_MESH_POLY_DATA_PRIVATE_H */
