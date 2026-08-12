#ifndef FVIZ_MESH_CELL_ARRAY_H
#define FVIZ_MESH_CELL_ARRAY_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizCellArray FVizCellArray;
#define FVIZ_TYPE_CELL_ARRAY UINT64_C(0xE4B8296C1F507AD3)

typedef enum FVizCellType
{
    FVIZ_CELL_VERTEX = 1,
    FVIZ_CELL_LINE = 3,
    FVIZ_CELL_TRIANGLE = 5,
    FVIZ_CELL_QUAD = 9,
    FVIZ_CELL_TETRA = 10,
    FVIZ_CELL_HEXAHEDRON = 12,
    FVIZ_CELL_WEDGE = 13,
    FVIZ_CELL_PYRAMID = 14
} FVizCellType;

FVIZ_API FVizResult fviz_cell_array_create(FVizCellArray** out_cells);
FVIZ_API void fviz_cell_array_clear(FVizCellArray* cells);
FVIZ_API FVizResult fviz_cell_array_reserve(FVizCellArray* cells, FVizSize cell_capacity, FVizSize connectivity_capacity);
FVIZ_API FVizResult fviz_cell_array_append(FVizCellArray* cells, FVizCellType type, FVizSize point_count, const uint32_t* point_ids);
FVIZ_API FVizSize fviz_cell_array_count(const FVizCellArray* cells);
FVIZ_API FVizSize fviz_cell_array_connectivity_size(const FVizCellArray* cells);
FVIZ_API FVizCellType fviz_cell_array_type(const FVizCellArray* cells, FVizSize cell_id);
FVIZ_API FVizSize fviz_cell_array_point_count(const FVizCellArray* cells, FVizSize cell_id);
FVIZ_API const uint32_t* fviz_cell_array_point_ids(const FVizCellArray* cells, FVizSize cell_id);
FVIZ_API const FVizSize* fviz_cell_array_offsets(const FVizCellArray* cells);
FVIZ_API const uint32_t* fviz_cell_array_connectivity(const FVizCellArray* cells);
FVIZ_API FVizResult fviz_cell_array_validate(const FVizCellArray* cells, FVizSize point_count);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MESH_CELL_ARRAY_H */
