#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Mesh/FVizCellArray.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Mesh/FVizCellArrayPrivate.h>

static void fviz_cell_array_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_cell_array_class = {
    FVIZ_TYPE_CELL_ARRAY, "FVizCellArray", &g_fviz_object_class, fviz_cell_array_destroy
};

static FVizSize fviz_cell_type_points(FVizCellType type)
{
    switch (type)
    {
        case FVIZ_CELL_VERTEX: return 1u;
        case FVIZ_CELL_LINE: return 2u;
        case FVIZ_CELL_TRIANGLE: return 3u;
        case FVIZ_CELL_QUAD: return 4u;
        case FVIZ_CELL_TETRA: return 4u;
        case FVIZ_CELL_HEXAHEDRON: return 8u;
        case FVIZ_CELL_WEDGE: return 6u;
        case FVIZ_CELL_PYRAMID: return 5u;
        default: return 0u;
    }
}

static void fviz_cell_array_destroy(FVizObject* object)
{
    FVizCellArray* cells = (FVizCellArray*)object;
    fviz_release(cells->types);
    fviz_release(cells->offsets);
    fviz_release(cells->connectivity);
}

FVizResult fviz_cell_array_create(FVizCellArray** out_cells)
{
    FVizCellArray* cells;
    FVizSize zero = 0u;
    if (out_cells == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_cells must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_cells = NULL;
    cells = (FVizCellArray*)fviz_internal_object_allocate(sizeof(FVizCellArray), &g_fviz_cell_array_class, NULL);
    if (cells == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizCellType), &cells->types) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizSize), &cells->offsets) != FVIZ_OK ||
        fviz_array_create(sizeof(uint32_t), &cells->connectivity) != FVIZ_OK ||
        fviz_array_push(cells->offsets, &zero) != FVIZ_OK)
    {
        fviz_release(cells);
        return fviz_last_error_code();
    }
    *out_cells = cells;
    return FVIZ_OK;
}

void fviz_cell_array_clear(FVizCellArray* cells)
{
    FVizSize zero = 0u;
    if (cells == NULL) return;
    fviz_array_clear(cells->types);
    fviz_array_clear(cells->offsets);
    fviz_array_clear(cells->connectivity);
    (void)fviz_array_push(cells->offsets, &zero);
}

FVizResult fviz_cell_array_reserve(FVizCellArray* cells, FVizSize cell_capacity, FVizSize connectivity_capacity)
{
    if (cells == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cells must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_array_reserve(cells->types, cell_capacity) != FVIZ_OK ||
        fviz_array_reserve(cells->offsets, cell_capacity + 1u) != FVIZ_OK ||
        fviz_array_reserve(cells->connectivity, connectivity_capacity) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

FVizResult fviz_cell_array_append(FVizCellArray* cells, FVizCellType type, FVizSize point_count, const uint32_t* point_ids)
{
    FVizSize expected;
    FVizSize offset;
    FVizSize i;
    if (cells == NULL || point_ids == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cells and point_ids must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    expected = fviz_cell_type_points(type);
    if (expected == 0u || point_count != expected)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell type has an invalid point count");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    offset = fviz_array_count(cells->connectivity);
    for (i = 0u; i < point_count; ++i)
        if (fviz_array_push(cells->connectivity, &point_ids[i]) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_array_push(cells->types, &type) != FVIZ_OK ||
        fviz_array_push(cells->offsets, &(FVizSize){offset + point_count}) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

FVizSize fviz_cell_array_count(const FVizCellArray* cells) { return cells != NULL ? fviz_array_count(cells->types) : 0u; }
FVizSize fviz_cell_array_connectivity_size(const FVizCellArray* cells) { return cells != NULL ? fviz_array_count(cells->connectivity) : 0u; }

FVizCellType fviz_cell_array_type(const FVizCellArray* cells, FVizSize cell_id)
{
    const FVizCellType* type = cells != NULL ? (const FVizCellType*)fviz_array_const_at(cells->types, cell_id) : NULL;
    return type != NULL ? *type : (FVizCellType)0;
}

FVizSize fviz_cell_array_point_count(const FVizCellArray* cells, FVizSize cell_id)
{
    const FVizSize* offsets;
    if (cells == NULL || cell_id >= fviz_cell_array_count(cells)) return 0u;
    offsets = (const FVizSize*)fviz_array_const_data(cells->offsets);
    return offsets[cell_id + 1u] - offsets[cell_id];
}

const uint32_t* fviz_cell_array_point_ids(const FVizCellArray* cells, FVizSize cell_id)
{
    const FVizSize* offsets;
    if (cells == NULL || cell_id >= fviz_cell_array_count(cells)) return NULL;
    offsets = (const FVizSize*)fviz_array_const_data(cells->offsets);
    return ((const uint32_t*)fviz_array_const_data(cells->connectivity)) + offsets[cell_id];
}

const FVizSize* fviz_cell_array_offsets(const FVizCellArray* cells) { return cells != NULL ? (const FVizSize*)fviz_array_const_data(cells->offsets) : NULL; }
const uint32_t* fviz_cell_array_connectivity(const FVizCellArray* cells) { return cells != NULL ? (const uint32_t*)fviz_array_const_data(cells->connectivity) : NULL; }

FVizResult fviz_cell_array_validate(const FVizCellArray* cells, FVizSize point_count)
{
    FVizSize i;
    const FVizSize* offsets;
    const uint32_t* connectivity;
    if (cells == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cells must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_array_count(cells->offsets) != fviz_array_count(cells->types) + 1u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cell offsets do not match cell count");
        return FVIZ_ERROR_INVALID_STATE;
    }
    offsets = fviz_cell_array_offsets(cells);
    connectivity = fviz_cell_array_connectivity(cells);
    for (i = 0u; i < fviz_cell_array_count(cells); ++i)
    {
        const FVizSize start = offsets[i];
        const FVizSize end = offsets[i + 1u];
        FVizSize j;
        if (start > end || end > fviz_cell_array_connectivity_size(cells) ||
            end - start != fviz_cell_type_points(fviz_cell_array_type(cells, i)))
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cell connectivity is malformed");
            return FVIZ_ERROR_INVALID_STATE;
        }
        for (j = start; j < end; ++j)
            if ((FVizSize)connectivity[j] >= point_count)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cell references an out-of-range point");
                return FVIZ_ERROR_INVALID_STATE;
            }
    }
    return FVIZ_OK;
}
