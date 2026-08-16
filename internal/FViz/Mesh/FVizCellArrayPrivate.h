#ifndef FVIZ_INTERNAL_MESH_CELL_ARRAY_PRIVATE_H
#define FVIZ_INTERNAL_MESH_CELL_ARRAY_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Mesh/FVizCellArray.h>

struct FVizCellArray
{
    FVizObject base;
    FVizArray* types;
    FVizArray* offsets;
    FVizArray* connectivity;
    FVizIdStorage id_storage;
};

#endif /* FVIZ_INTERNAL_MESH_CELL_ARRAY_PRIVATE_H */
