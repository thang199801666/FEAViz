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
    /* Numeric values intentionally follow VTK cell type IDs where applicable. */
    FVIZ_CELL_VERTEX = 1,
    FVIZ_CELL_POLY_VERTEX = 2,
    FVIZ_CELL_LINE = 3,
    FVIZ_CELL_POLY_LINE = 4,
    FVIZ_CELL_TRIANGLE = 5,
    FVIZ_CELL_TRIANGLE_STRIP = 6,
    FVIZ_CELL_POLYGON = 7,
    FVIZ_CELL_QUAD = 9,
    FVIZ_CELL_TETRA = 10,
    FVIZ_CELL_HEXAHEDRON = 12,
    FVIZ_CELL_WEDGE = 13,
    FVIZ_CELL_PYRAMID = 14,
    FVIZ_CELL_QUADRATIC_EDGE = 21,
    FVIZ_CELL_QUADRATIC_TRIANGLE = 22,
    FVIZ_CELL_QUADRATIC_QUAD = 23,
    FVIZ_CELL_QUADRATIC_TETRA = 24,
    FVIZ_CELL_QUADRATIC_HEXAHEDRON = 25,
    FVIZ_CELL_QUADRATIC_WEDGE = 26,
    FVIZ_CELL_QUADRATIC_PYRAMID = 27,
    FVIZ_CELL_BIQUADRATIC_QUAD = 28
} FVizCellType;

typedef enum FVizIdStorage
{
    FVIZ_ID_STORAGE_UINT32 = 0,
    FVIZ_ID_STORAGE_UINT64 = 1
} FVizIdStorage;

typedef struct FVizCellView
{
    FVizCellType type;
    FVizSize point_count;
    FVizIdStorage id_storage;
    const void* point_ids;
} FVizCellView;

FVIZ_API FVizResult fviz_cell_array_create(FVizCellArray** out_cells);
FVIZ_API FVizResult fviz_cell_array_create_with_storage(FVizIdStorage storage, FVizCellArray** out_cells);
FVIZ_API FVizResult fviz_cell_array_deep_copy(const FVizCellArray* source, FVizCellArray** out_copy);
FVIZ_API void fviz_cell_array_clear(FVizCellArray* cells);
FVIZ_API FVizResult fviz_cell_array_reserve(FVizCellArray* cells, FVizSize cell_capacity, FVizSize connectivity_capacity);
FVIZ_API FVizIdStorage fviz_cell_array_id_storage(const FVizCellArray* cells);
/* Converts connectivity storage. UINT64->UINT32 is checked and fails on overflow. */
FVIZ_API FVizResult fviz_cell_array_convert_id_storage(FVizCellArray* cells, FVizIdStorage storage);
FVIZ_API FVizResult fviz_cell_array_append(FVizCellArray* cells, FVizCellType type, FVizSize point_count, const uint32_t* point_ids);
/* Fast path for batches of fixed-width cells such as lines and triangles. */
FVIZ_API FVizResult fviz_cell_array_append_fixed(
    FVizCellArray* cells, FVizCellType type, FVizSize points_per_cell,
    FVizSize cell_count, const uint32_t* point_ids);
/* Native ID path. Storage automatically promotes to UINT64 when required. */
FVIZ_API FVizResult fviz_cell_array_append_ids(
    FVizCellArray* cells,
    FVizCellType type,
    FVizSize point_count,
    const FVizId* point_ids);
FVIZ_API FVizResult fviz_cell_array_append_fixed_ids(
    FVizCellArray* cells,
    FVizCellType type,
    FVizSize points_per_cell,
    FVizSize cell_count,
    const FVizId* point_ids);
FVIZ_API FVizSize fviz_cell_array_count(const FVizCellArray* cells);
FVIZ_API FVizSize fviz_cell_array_connectivity_size(const FVizCellArray* cells);
FVIZ_API FVizCellType fviz_cell_array_type(const FVizCellArray* cells, FVizSize cell_id);
FVIZ_API FVizSize fviz_cell_array_point_count(const FVizCellArray* cells, FVizSize cell_id);
/* Compatibility pointer APIs. They return NULL when native connectivity uses UINT64 storage. */
FVIZ_API const uint32_t* fviz_cell_array_point_ids(const FVizCellArray* cells, FVizSize cell_id);
FVIZ_API const uint32_t* fviz_cell_array_connectivity(const FVizCellArray* cells);
FVIZ_API const uint64_t* fviz_cell_array_point_ids64(const FVizCellArray* cells, FVizSize cell_id);
FVIZ_API const uint64_t* fviz_cell_array_connectivity64(const FVizCellArray* cells);
FVIZ_API const FVizSize* fviz_cell_array_offsets(const FVizCellArray* cells);
FVIZ_API FVizResult fviz_cell_array_cell_view(
    const FVizCellArray* cells, FVizSize cell_id, FVizCellView* out_view);
FVIZ_API FVizId fviz_cell_view_point_id(const FVizCellView* view, FVizSize local_point_id);
FVIZ_API FVizResult fviz_cell_array_point_id(
    const FVizCellArray* cells, FVizSize cell_id, FVizSize local_point_id, FVizId* out_point_id);
FVIZ_API FVizResult fviz_cell_array_copy_point_ids(
    const FVizCellArray* cells, FVizSize cell_id, FVizId* out_point_ids, FVizSize capacity);
FVIZ_API FVizResult fviz_cell_array_validate(const FVizCellArray* cells, FVizSize point_count);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MESH_CELL_ARRAY_H */
