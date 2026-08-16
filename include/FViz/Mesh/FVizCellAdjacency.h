#ifndef FVIZ_MESH_CELL_ADJACENCY_H
#define FVIZ_MESH_CELL_ADJACENCY_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Mesh/FVizCellArray.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizCellAdjacency FVizCellAdjacency;
#define FVIZ_TYPE_CELL_ADJACENCY UINT64_C(0xF2C0A14B8935DE67)

/* Builds same-dimensional codimension-one adjacency: 3D cells through faces,
 * 2D cells through edges, and 1D cells through endpoints. This is the topology
 * used by unstructured ghost-layer expansion. Triangle strips are currently
 * rejected because their exterior edge set depends on strip parity. */
FVIZ_API FVizResult fviz_cell_adjacency_build(
    const FVizCellArray* cells,
    FVizSize point_count,
    FVizCellAdjacency** out_adjacency);
FVIZ_API FVizSize fviz_cell_adjacency_cell_count(const FVizCellAdjacency* adjacency);
FVIZ_API FVizSize fviz_cell_adjacency_neighbor_count(
    const FVizCellAdjacency* adjacency,
    FVizId cell_id);
FVIZ_API const FVizId* fviz_cell_adjacency_neighbors(
    const FVizCellAdjacency* adjacency,
    FVizId cell_id,
    FVizSize* out_count);
FVIZ_API FVizSize fviz_cell_adjacency_edge_count(const FVizCellAdjacency* adjacency);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MESH_CELL_ADJACENCY_H */
