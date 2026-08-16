#ifndef FVIZ_MESH_CELL_LINKS_H
#define FVIZ_MESH_CELL_LINKS_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Mesh/FVizCellArray.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizCellLinks FVizCellLinks;
#define FVIZ_TYPE_CELL_LINKS UINT64_C(0xA15D39C4E720B681)

/* Compact point-to-cell incidence in CSR form. The links are a snapshot of the
 * supplied topology and do not retain the source cell array. */
FVIZ_API FVizResult fviz_cell_links_build(
    const FVizCellArray* cells,
    FVizSize point_count,
    FVizCellLinks** out_links);
FVIZ_API FVizSize fviz_cell_links_point_count(const FVizCellLinks* links);
FVIZ_API FVizSize fviz_cell_links_cell_count(const FVizCellLinks* links);
FVIZ_API FVizSize fviz_cell_links_incidence_count(const FVizCellLinks* links);
FVIZ_API FVizSize fviz_cell_links_cell_count_for_point(
    const FVizCellLinks* links,
    FVizId point_id);
FVIZ_API const FVizId* fviz_cell_links_cells_for_point(
    const FVizCellLinks* links,
    FVizId point_id,
    FVizSize* out_count);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MESH_CELL_LINKS_H */
