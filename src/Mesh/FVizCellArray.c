#include <limits.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>

#include <FViz/Core/FVizArrayPrivate.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Mesh/FVizCellArrayPrivate.h>

static void fviz_cell_array_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_cell_array_class = {
    FVIZ_TYPE_CELL_ARRAY, "FVizCellArray", &g_fviz_object_class,
    fviz_cell_array_destroy, NULL
};

static FVizSize fviz_cell_array_connectivity_stride(FVizIdStorage storage)
{
    return storage == FVIZ_ID_STORAGE_UINT64 ? sizeof(uint64_t) : sizeof(uint32_t);
}

static FVizBool fviz_cell_array_storage_valid(FVizIdStorage storage)
{
    return (storage == FVIZ_ID_STORAGE_UINT32 || storage == FVIZ_ID_STORAGE_UINT64) ? FVIZ_TRUE : FVIZ_FALSE;
}

static void fviz_cell_array_destroy(FVizObject* object)
{
    FVizCellArray* cells = (FVizCellArray*)object;
    fviz_release(cells->types);
    fviz_release(cells->offsets);
    fviz_release(cells->connectivity);
}

FVizResult fviz_cell_array_create_with_storage(FVizIdStorage storage, FVizCellArray** out_cells)
{
    FVizCellArray* cells;
    FVizSize zero = 0u;
    if (out_cells == NULL || fviz_cell_array_storage_valid(storage) == FVIZ_FALSE)
    {
        if (out_cells != NULL) *out_cells = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid cell-array output or ID storage");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_cells = NULL;
    cells = (FVizCellArray*)fviz_internal_object_allocate(sizeof(FVizCellArray), &g_fviz_cell_array_class, NULL);
    if (cells == NULL) return fviz_last_error_code();
    cells->id_storage = storage;
    if (fviz_array_create(sizeof(FVizCellType), &cells->types) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizSize), &cells->offsets) != FVIZ_OK ||
        fviz_array_create(fviz_cell_array_connectivity_stride(storage), &cells->connectivity) != FVIZ_OK ||
        fviz_internal_array_append(cells->offsets, &zero, 1u) != FVIZ_OK)
    {
        fviz_release(cells);
        return fviz_last_error_code();
    }
    *out_cells = cells;
    return FVIZ_OK;
}

FVizResult fviz_cell_array_create(FVizCellArray** out_cells)
{
    return fviz_cell_array_create_with_storage(FVIZ_ID_STORAGE_UINT32, out_cells);
}

static FVizSize fviz_cell_array_growth_capacity(FVizSize current, FVizSize required)
{
    FVizSize capacity = current == 0u ? 8u : current;
    while (capacity < required)
    {
        if (capacity > ((FVizSize)-1) / 2u) return required;
        capacity *= 2u;
    }
    return capacity;
}

static FVizResult fviz_cell_array_ensure_capacity(
    FVizCellArray* cells, FVizSize required_cells, FVizSize required_connectivity)
{
    const FVizSize type_capacity = fviz_array_capacity(cells->types);
    const FVizSize offset_capacity = fviz_array_capacity(cells->offsets);
    const FVizSize connectivity_capacity = fviz_array_capacity(cells->connectivity);
    const FVizSize required_offsets = required_cells + 1u;
    if (required_offsets == 0u) return FVIZ_ERROR_OVERFLOW;
    if (required_cells > type_capacity &&
        fviz_array_reserve(cells->types, fviz_cell_array_growth_capacity(type_capacity, required_cells)) != FVIZ_OK)
        return fviz_last_error_code();
    if (required_offsets > offset_capacity &&
        fviz_array_reserve(cells->offsets, fviz_cell_array_growth_capacity(offset_capacity, required_offsets)) != FVIZ_OK)
        return fviz_last_error_code();
    if (required_connectivity > connectivity_capacity &&
        fviz_array_reserve(cells->connectivity, fviz_cell_array_growth_capacity(connectivity_capacity, required_connectivity)) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

FVizIdStorage fviz_cell_array_id_storage(const FVizCellArray* cells)
{
    return cells != NULL ? cells->id_storage : FVIZ_ID_STORAGE_UINT32;
}

FVizResult fviz_cell_array_convert_id_storage(FVizCellArray* cells, FVizIdStorage storage)
{
    FVizArray* replacement = NULL;
    FVizSize count;
    FVizSize i;
    if (cells == NULL || fviz_cell_array_storage_valid(storage) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid cell-array ID storage conversion");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (storage == cells->id_storage) return FVIZ_OK;
    count = fviz_array_count(cells->connectivity);
    if (fviz_array_create(fviz_cell_array_connectivity_stride(storage), &replacement) != FVIZ_OK ||
        fviz_array_reserve(replacement, count) != FVIZ_OK)
        goto fail;
    if (storage == FVIZ_ID_STORAGE_UINT64)
    {
        const uint32_t* source = (const uint32_t*)fviz_array_const_data(cells->connectivity);
        uint64_t* destination = NULL;
        void* raw = NULL;
        if (fviz_internal_array_append_uninitialized(replacement, count, &raw) != FVIZ_OK) goto fail;
        destination = (uint64_t*)raw;
        for (i = 0u; i < count; ++i) destination[i] = (uint64_t)source[i];
    }
    else
    {
        const uint64_t* source = (const uint64_t*)fviz_array_const_data(cells->connectivity);
        uint32_t* destination = NULL;
        void* raw = NULL;
        for (i = 0u; i < count; ++i)
        {
            if (source[i] > UINT32_MAX)
            {
                fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "64-bit connectivity cannot be represented in UINT32 storage");
                goto fail;
            }
        }
        if (fviz_internal_array_append_uninitialized(replacement, count, &raw) != FVIZ_OK) goto fail;
        destination = (uint32_t*)raw;
        for (i = 0u; i < count; ++i) destination[i] = (uint32_t)source[i];
    }
    fviz_release(cells->connectivity);
    cells->connectivity = replacement;
    cells->id_storage = storage;
    fviz_object_modified((FVizObject*)cells);
    return FVIZ_OK;
fail:
    fviz_release(replacement);
    return fviz_last_error_code();
}

FVizResult fviz_cell_array_deep_copy(const FVizCellArray* source, FVizCellArray** out_copy)
{
    FVizCellArray* copy = NULL;
    if (source == NULL || out_copy == NULL)
    {
        if (out_copy != NULL) *out_copy = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell array deep copy requires source and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_copy = NULL;
    if (fviz_cell_array_create_with_storage(source->id_storage, &copy) != FVIZ_OK ||
        fviz_cell_array_reserve(copy, fviz_cell_array_count(source), fviz_cell_array_connectivity_size(source)) != FVIZ_OK)
        goto fail;
    fviz_internal_array_clear(copy->offsets);
    if (fviz_internal_array_append(copy->types, fviz_array_const_data(source->types), fviz_array_count(source->types)) != FVIZ_OK ||
        fviz_internal_array_append(copy->offsets, fviz_array_const_data(source->offsets), fviz_array_count(source->offsets)) != FVIZ_OK ||
        fviz_internal_array_append(copy->connectivity, fviz_array_const_data(source->connectivity), fviz_array_count(source->connectivity)) != FVIZ_OK)
        goto fail;
    fviz_object_modified((FVizObject*)copy);
    *out_copy = copy;
    return FVIZ_OK;
fail:
    fviz_release(copy);
    return fviz_last_error_code();
}

void fviz_cell_array_clear(FVizCellArray* cells)
{
    FVizSize zero = 0u;
    if (cells == NULL) return;
    fviz_internal_array_clear(cells->types);
    fviz_internal_array_clear(cells->offsets);
    fviz_internal_array_clear(cells->connectivity);
    (void)fviz_internal_array_append(cells->offsets, &zero, 1u);
    fviz_object_modified((FVizObject*)cells);
}

FVizResult fviz_cell_array_reserve(FVizCellArray* cells, FVizSize cell_capacity, FVizSize connectivity_capacity)
{
    if (cells == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cells must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (cell_capacity == (FVizSize)-1) return FVIZ_ERROR_OVERFLOW;
    return fviz_cell_array_ensure_capacity(cells, cell_capacity, connectivity_capacity);
}

static FVizResult fviz_cell_array_append_metadata(
    FVizCellArray* cells, FVizCellType type, FVizSize points_per_cell, FVizSize cell_count, FVizSize first_offset)
{
    void* type_slots = NULL;
    void* offset_slots = NULL;
    FVizCellType* types;
    FVizSize* offsets;
    FVizSize i;
    if (fviz_internal_array_append_uninitialized(cells->types, cell_count, &type_slots) != FVIZ_OK ||
        fviz_internal_array_append_uninitialized(cells->offsets, cell_count, &offset_slots) != FVIZ_OK)
        return fviz_last_error_code();
    types = (FVizCellType*)type_slots;
    offsets = (FVizSize*)offset_slots;
    for (i = 0u; i < cell_count; ++i)
    {
        types[i] = type;
        offsets[i] = first_offset + (i + 1u) * points_per_cell;
    }
    return FVIZ_OK;
}

FVizResult fviz_cell_array_append_fixed(
    FVizCellArray* cells, FVizCellType type, FVizSize points_per_cell,
    FVizSize cell_count, const uint32_t* point_ids)
{
    FVizSize offset;
    FVizSize added_connectivity;
    FVizSize required_cells;
    FVizSize required_connectivity;
    FVizSize i;
    if (cells == NULL || (point_ids == NULL && cell_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cells and point_ids must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (cell_count == 0u) return FVIZ_OK;
    if (fviz_cell_type_accepts_point_count(type, points_per_cell) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell type has an invalid point count");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    offset = fviz_array_count(cells->connectivity);
    if (fviz_size_multiply(points_per_cell, cell_count, &added_connectivity) != FVIZ_OK ||
        added_connectivity > (FVizSize)-1 - offset ||
        cell_count > (FVizSize)-1 - fviz_array_count(cells->types))
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "cell batch size overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    required_cells = fviz_array_count(cells->types) + cell_count;
    required_connectivity = offset + added_connectivity;
    if (fviz_cell_array_ensure_capacity(cells, required_cells, required_connectivity) != FVIZ_OK) return fviz_last_error_code();
    if (cells->id_storage == FVIZ_ID_STORAGE_UINT32)
    {
        if (fviz_internal_array_append(cells->connectivity, point_ids, added_connectivity) != FVIZ_OK) return fviz_last_error_code();
    }
    else
    {
        uint64_t* slots = NULL;
        void* raw = NULL;
        if (fviz_internal_array_append_uninitialized(cells->connectivity, added_connectivity, &raw) != FVIZ_OK) return fviz_last_error_code();
        slots = (uint64_t*)raw;
        for (i = 0u; i < added_connectivity; ++i) slots[i] = (uint64_t)point_ids[i];
    }
    if (fviz_cell_array_append_metadata(cells, type, points_per_cell, cell_count, offset) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)cells);
    return FVIZ_OK;
}

FVizResult fviz_cell_array_append(FVizCellArray* cells, FVizCellType type, FVizSize point_count, const uint32_t* point_ids)
{
    return fviz_cell_array_append_fixed(cells, type, point_count, 1u, point_ids);
}

FVizResult fviz_cell_array_append_fixed_ids(
    FVizCellArray* cells,
    FVizCellType type,
    FVizSize points_per_cell,
    FVizSize cell_count,
    const FVizId* point_ids)
{
    FVizSize added_connectivity;
    FVizSize offset;
    FVizSize required_cells;
    FVizSize required_connectivity;
    FVizSize i;
    FVizBool needs64 = FVIZ_FALSE;
    if (cells == NULL || (point_ids == NULL && cell_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "native cell connectivity is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (cell_count == 0u) return FVIZ_OK;
    if (fviz_cell_type_accepts_point_count(type, points_per_cell) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell type has an invalid point count");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_size_multiply(points_per_cell, cell_count, &added_connectivity) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    for (i = 0u; i < added_connectivity; ++i)
        if (point_ids[i] > UINT32_MAX) { needs64 = FVIZ_TRUE; break; }
    if (needs64 == FVIZ_TRUE && cells->id_storage == FVIZ_ID_STORAGE_UINT32 &&
        fviz_cell_array_convert_id_storage(cells, FVIZ_ID_STORAGE_UINT64) != FVIZ_OK)
        return fviz_last_error_code();
    offset = fviz_array_count(cells->connectivity);
    if (added_connectivity > (FVizSize)-1 - offset || cell_count > (FVizSize)-1 - fviz_array_count(cells->types))
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "cell batch size overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    required_cells = fviz_array_count(cells->types) + cell_count;
    required_connectivity = offset + added_connectivity;
    if (fviz_cell_array_ensure_capacity(cells, required_cells, required_connectivity) != FVIZ_OK) return fviz_last_error_code();
    if (cells->id_storage == FVIZ_ID_STORAGE_UINT64)
    {
        if (fviz_internal_array_append(cells->connectivity, point_ids, added_connectivity) != FVIZ_OK) return fviz_last_error_code();
    }
    else
    {
        uint32_t* slots = NULL;
        void* raw = NULL;
        if (fviz_internal_array_append_uninitialized(cells->connectivity, added_connectivity, &raw) != FVIZ_OK) return fviz_last_error_code();
        slots = (uint32_t*)raw;
        for (i = 0u; i < added_connectivity; ++i) slots[i] = (uint32_t)point_ids[i];
    }
    if (fviz_cell_array_append_metadata(cells, type, points_per_cell, cell_count, offset) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)cells);
    return FVIZ_OK;
}

FVizResult fviz_cell_array_append_ids(
    FVizCellArray* cells,
    FVizCellType type,
    FVizSize point_count,
    const FVizId* point_ids)
{
    return fviz_cell_array_append_fixed_ids(cells, type, point_count, 1u, point_ids);
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
    if (cells == NULL || cells->id_storage != FVIZ_ID_STORAGE_UINT32 || cell_id >= fviz_cell_array_count(cells)) return NULL;
    offsets = (const FVizSize*)fviz_array_const_data(cells->offsets);
    return ((const uint32_t*)fviz_array_const_data(cells->connectivity)) + offsets[cell_id];
}

const uint64_t* fviz_cell_array_point_ids64(const FVizCellArray* cells, FVizSize cell_id)
{
    const FVizSize* offsets;
    if (cells == NULL || cells->id_storage != FVIZ_ID_STORAGE_UINT64 || cell_id >= fviz_cell_array_count(cells)) return NULL;
    offsets = (const FVizSize*)fviz_array_const_data(cells->offsets);
    return ((const uint64_t*)fviz_array_const_data(cells->connectivity)) + offsets[cell_id];
}

const FVizSize* fviz_cell_array_offsets(const FVizCellArray* cells)
{
    return cells != NULL ? (const FVizSize*)fviz_array_const_data(cells->offsets) : NULL;
}

const uint32_t* fviz_cell_array_connectivity(const FVizCellArray* cells)
{
    return cells != NULL && cells->id_storage == FVIZ_ID_STORAGE_UINT32
        ? (const uint32_t*)fviz_array_const_data(cells->connectivity) : NULL;
}

const uint64_t* fviz_cell_array_connectivity64(const FVizCellArray* cells)
{
    return cells != NULL && cells->id_storage == FVIZ_ID_STORAGE_UINT64
        ? (const uint64_t*)fviz_array_const_data(cells->connectivity) : NULL;
}

FVizResult fviz_cell_array_cell_view(
    const FVizCellArray* cells, FVizSize cell_id, FVizCellView* out_view)
{
    const FVizSize* offsets;
    if (out_view != NULL) (void)memset(out_view, 0, sizeof(*out_view));
    if (cells == NULL || out_view == NULL || cell_id >= fviz_cell_array_count(cells))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell view lookup is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    offsets = fviz_cell_array_offsets(cells);
    out_view->type = fviz_cell_array_type(cells, cell_id);
    out_view->point_count = offsets[cell_id + 1u] - offsets[cell_id];
    out_view->id_storage = cells->id_storage;
    if (cells->id_storage == FVIZ_ID_STORAGE_UINT64)
        out_view->point_ids = ((const uint64_t*)fviz_array_const_data(cells->connectivity)) + offsets[cell_id];
    else
        out_view->point_ids = ((const uint32_t*)fviz_array_const_data(cells->connectivity)) + offsets[cell_id];
    return FVIZ_OK;
}

FVizId fviz_cell_view_point_id(const FVizCellView* view, FVizSize local_point_id)
{
    if (view == NULL || view->point_ids == NULL || local_point_id >= view->point_count) return FVIZ_INVALID_ID;
    return view->id_storage == FVIZ_ID_STORAGE_UINT64
        ? ((const uint64_t*)view->point_ids)[local_point_id]
        : (FVizId)((const uint32_t*)view->point_ids)[local_point_id];
}

FVizResult fviz_cell_array_point_id(
    const FVizCellArray* cells, FVizSize cell_id, FVizSize local_point_id, FVizId* out_point_id)
{
    FVizCellView view;
    if (out_point_id == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell point ID output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_point_id = FVIZ_INVALID_ID;
    if (fviz_cell_array_cell_view(cells, cell_id, &view) != FVIZ_OK) return fviz_last_error_code();
    if (local_point_id >= view.point_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell point ID lookup is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_point_id = fviz_cell_view_point_id(&view, local_point_id);
    return FVIZ_OK;
}

FVizResult fviz_cell_array_copy_point_ids(
    const FVizCellArray* cells, FVizSize cell_id, FVizId* out_point_ids, FVizSize capacity)
{
    FVizSize count;
    FVizSize i;
    if (cells == NULL || cell_id >= fviz_cell_array_count(cells))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell point copy has an invalid cell");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_cell_array_point_count(cells, cell_id);
    if (count != 0u && (out_point_ids == NULL || capacity < count))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell point copy output is too small");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (cells->id_storage == FVIZ_ID_STORAGE_UINT64)
    {
        const uint64_t* ids = fviz_cell_array_point_ids64(cells, cell_id);
        for (i = 0u; i < count; ++i) out_point_ids[i] = ids[i];
    }
    else
    {
        const uint32_t* ids = fviz_cell_array_point_ids(cells, cell_id);
        for (i = 0u; i < count; ++i) out_point_ids[i] = ids[i];
    }
    return FVIZ_OK;
}

FVizResult fviz_cell_array_validate(const FVizCellArray* cells, FVizSize point_count)
{
    FVizSize i;
    const FVizSize* offsets;
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
    for (i = 0u; i < fviz_cell_array_count(cells); ++i)
    {
        const FVizSize start = offsets[i];
        const FVizSize end = offsets[i + 1u];
        FVizSize j;
        if (start > end || end > fviz_cell_array_connectivity_size(cells) ||
            fviz_cell_type_accepts_point_count(fviz_cell_array_type(cells, i), end - start) == FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cell connectivity is malformed");
            return FVIZ_ERROR_INVALID_STATE;
        }
        for (j = start; j < end; ++j)
        {
            FVizId id = cells->id_storage == FVIZ_ID_STORAGE_UINT64
                ? ((const uint64_t*)fviz_array_const_data(cells->connectivity))[j]
                : ((const uint32_t*)fviz_array_const_data(cells->connectivity))[j];
            if (id >= (FVizId)point_count)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cell references an out-of-range point");
                return FVIZ_ERROR_INVALID_STATE;
            }
        }
    }
    return FVIZ_OK;
}
