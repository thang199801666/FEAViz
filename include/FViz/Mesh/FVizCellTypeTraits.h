#ifndef FVIZ_MESH_CELL_TYPE_TRAITS_H
#define FVIZ_MESH_CELL_TYPE_TRAITS_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizCellTypeTraits
{
    uint8_t dimension;
    uint8_t fixed_point_count;
    uint8_t edge_count;
    uint8_t face_count;
    FVizBool variable_point_count;
} FVizCellTypeTraits;

FVIZ_DATA_API FVizBool fviz_cell_type_is_supported(FVizCellType type);
FVIZ_DATA_API FVizCellTypeTraits fviz_cell_type_traits(FVizCellType type);
FVIZ_DATA_API FVizBool fviz_cell_type_accepts_point_count(FVizCellType type, FVizSize point_count);
FVIZ_DATA_API FVizResult fviz_cell_type_edge(FVizCellType type, uint32_t edge_index, uint32_t out_local_point_ids[2]);
FVIZ_DATA_API FVizResult fviz_cell_type_face(FVizCellType type, uint32_t face_index, uint32_t* out_local_point_ids,
                                        uint32_t capacity, uint32_t* out_point_count);
/* Shape/interpolation weights for the reference coordinates used by FEAViz.
 * Triangle/tetra use barycentric r,s,(t); line/quad/hex use [-1,1] coordinates.
 * Supports both linear and selected quadratic VTK-compatible cell types. */
FVIZ_DATA_API FVizResult fviz_cell_type_shape_weights(FVizCellType type, FVizVec3 parametric, double* out_weights,
                                                 FVizSize capacity, FVizSize* out_weight_count);

/* Compatibility entry point for the original linear-cell API. It intentionally
 * remains restricted to first-order cells; use fviz_cell_type_shape_weights()
 * when the cell may be quadratic. */
FVIZ_DATA_API FVizResult fviz_cell_type_linear_weights(FVizCellType type, FVizVec3 parametric, double* out_weights,
                                                  FVizSize capacity, FVizSize* out_weight_count);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MESH_CELL_TYPE_TRAITS_H */
