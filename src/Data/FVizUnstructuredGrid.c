#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Parallel/FVizParallel.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizArray.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/Data/FVizGhost.h>
#include <FViz/Data/FVizUnstructuredGridPrivate.h>
#include <FViz/Data/FVizSurfaceFacePrivate.h>
#include <FViz/Mesh/FVizPointsPrivate.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>
#include <FViz/Mesh/FVizCellArrayPrivate.h>
#include <FViz/Mesh/FVizCellLinks.h>
#include <FViz/Algorithms/FVizFieldOperations.h>
#include <FViz/Spatial/FVizPointLocator.h>

typedef struct FVizSurfaceDefinition
{
    FVizCellType type;
    uint32_t face_count;
    uint32_t faces[6][4];
    uint32_t sizes[6];
} FVizSurfaceDefinition;

static const uint8_t* fviz_unstructured_grid_ghost_cell_flags(const FVizUnstructuredGrid* grid)
{
    const FVizDataArray* ghosts;
    if (grid == NULL) return NULL;
    ghosts = fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid),
        FVIZ_GHOST_ARRAY_NAME);
    if (ghosts == NULL ||
        fviz_data_array_type(ghosts) != FVIZ_DATA_UINT8 ||
        fviz_data_array_components(ghosts) != 1u ||
        fviz_data_array_tuple_count(ghosts) != fviz_unstructured_grid_cell_count(grid))
        return NULL;
    return (const uint8_t*)fviz_data_array_const_data(ghosts);
}

static FVizBool fviz_unstructured_grid_cell_is_render_ghost(
    const uint8_t* ghost_flags, FVizSize cell_id)
{
    return ghost_flags != NULL &&
        (ghost_flags[cell_id] & (uint8_t)(FVIZ_GHOST_DUPLICATE | FVIZ_GHOST_HIDDEN)) != 0u
        ? FVIZ_TRUE : FVIZ_FALSE;
}

static void fviz_unstructured_grid_destroy(FVizObject* object);
static FVizMTime fviz_unstructured_grid_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_unstructured_grid_class = {
    FVIZ_TYPE_UNSTRUCTURED_GRID, "FVizUnstructuredGrid", &g_fviz_data_object_class,
    fviz_unstructured_grid_destroy, fviz_unstructured_grid_mtime
};

static FVizBool fviz_unstructured_grid_dependency_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    FVizUnstructuredGrid* grid = (FVizUnstructuredGrid*)client_data;
    (void)caller; (void)event_id; (void)call_data;
    if (grid != NULL && grid->dependency_suppression == 0u)
        fviz_object_modified((FVizObject*)grid);
    return FVIZ_FALSE;
}

static FVizResult fviz_unstructured_grid_observe_dependency(
    FVizUnstructuredGrid* grid, FVizObject* dependency, FVizObserverTag* out_tag)
{
    if (out_tag == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (dependency == NULL) return FVIZ_OK;
    return fviz_object_add_observer(
        dependency, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_unstructured_grid_dependency_modified, grid, out_tag);
}

static FVizMTime fviz_unstructured_grid_mtime(const FVizObject* object)
{
    /* Points, cells and attributes all bridge ModifiedEvent into the grid. */
    return fviz_internal_object_local_mtime(object);
}

static void fviz_unstructured_grid_destroy(FVizObject* object)
{
    FVizUnstructuredGrid* grid = (FVizUnstructuredGrid*)object;
    if (grid->points != NULL && grid->points_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)grid->points, grid->points_modified_tag);
    if (grid->cells != NULL && grid->cells_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)grid->cells, grid->cells_modified_tag);
    if (grid->data_set != NULL && grid->data_set_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)grid->data_set, grid->data_set_modified_tag);
    fviz_release(grid->points);
    fviz_release(grid->cells);
    fviz_release(grid->data_set);
}

FVizResult fviz_unstructured_grid_create(FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* grid;
    if (out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    grid = (FVizUnstructuredGrid*)fviz_internal_object_allocate(
        sizeof(FVizUnstructuredGrid), &g_fviz_unstructured_grid_class, NULL);
    if (grid == NULL) return fviz_last_error_code();
    if (fviz_points_create(&grid->points) != FVIZ_OK ||
        fviz_cell_array_create(&grid->cells) != FVIZ_OK ||
        fviz_data_set_create(&grid->data_set) != FVIZ_OK)
    {
        fviz_release(grid);
        return fviz_last_error_code();
    }
    grid->dependency_suppression = 0u;
    if (fviz_unstructured_grid_observe_dependency(grid, (FVizObject*)grid->points, &grid->points_modified_tag) != FVIZ_OK ||
        fviz_unstructured_grid_observe_dependency(grid, (FVizObject*)grid->cells, &grid->cells_modified_tag) != FVIZ_OK ||
        fviz_unstructured_grid_observe_dependency(grid, (FVizObject*)grid->data_set, &grid->data_set_modified_tag) != FVIZ_OK)
    {
        fviz_release(grid);
        return fviz_last_error_code();
    }
    *out_grid = grid;
    return FVIZ_OK;
}

void fviz_unstructured_grid_clear(FVizUnstructuredGrid* grid)
{
    FVizBool changed;
    if (grid == NULL) return;
    changed = (fviz_points_count(grid->points) != 0u ||
        fviz_cell_array_count(grid->cells) != 0u ||
        fviz_attribute_set_count(fviz_data_set_point_data(grid->data_set)) != 0u ||
        fviz_attribute_set_count(fviz_data_set_cell_data(grid->data_set)) != 0u ||
        fviz_attribute_set_count(fviz_data_set_field_data(grid->data_set)) != 0u) ? FVIZ_TRUE : FVIZ_FALSE;
    ++grid->dependency_suppression;
    fviz_points_clear(grid->points);
    fviz_cell_array_clear(grid->cells);
    fviz_attribute_set_clear(fviz_data_set_point_data(grid->data_set));
    fviz_attribute_set_clear(fviz_data_set_cell_data(grid->data_set));
    fviz_attribute_set_clear(fviz_data_set_field_data(grid->data_set));
    (void)fviz_data_set_set_point_count(grid->data_set, 0u);
    (void)fviz_data_set_set_cell_count(grid->data_set, 0u);
    --grid->dependency_suppression;
    if (changed != FVIZ_FALSE) fviz_object_modified((FVizObject*)grid);
}

FVizPoints* fviz_unstructured_grid_points(FVizUnstructuredGrid* grid) { return grid != NULL ? grid->points : NULL; }
FVizCellArray* fviz_unstructured_grid_cells(FVizUnstructuredGrid* grid) { return grid != NULL ? grid->cells : NULL; }
FVizResult fviz_unstructured_grid_reserve(
    FVizUnstructuredGrid* grid, FVizSize point_capacity, FVizSize cell_capacity, FVizSize connectivity_capacity)
{
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_points_reserve(grid->points, point_capacity) != FVIZ_OK ||
        fviz_cell_array_reserve(grid->cells, cell_capacity, connectivity_capacity) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

FVizResult fviz_unstructured_grid_add_points(
    FVizUnstructuredGrid* grid, const FVizVec3* points, FVizSize point_count, uint32_t* out_first_id)
{
    FVizResult result;
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    ++grid->dependency_suppression;
    result = fviz_points_append_many(grid->points, points, point_count, out_first_id);
    if (result == FVIZ_OK) result = fviz_data_set_set_point_count(grid->data_set, fviz_points_count(grid->points));
    --grid->dependency_suppression;
    if (result == FVIZ_OK && point_count != 0u) fviz_object_modified((FVizObject*)grid);
    return result;
}

FVizResult fviz_unstructured_grid_add_points_ids(
    FVizUnstructuredGrid* grid, const FVizVec3* points, FVizSize point_count, FVizId* out_first_id)
{
    FVizResult result;
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    ++grid->dependency_suppression;
    result = fviz_points_append_many_ids(grid->points, points, point_count, out_first_id);
    if (result == FVIZ_OK) result = fviz_data_set_set_point_count(grid->data_set, fviz_points_count(grid->points));
    --grid->dependency_suppression;
    if (result == FVIZ_OK && point_count != 0u) fviz_object_modified((FVizObject*)grid);
    return result;
}

FVizResult fviz_unstructured_grid_add_point(FVizUnstructuredGrid* grid, FVizVec3 point, uint32_t* out_id)
{
    return fviz_unstructured_grid_add_points(grid, &point, 1u, out_id);
}

FVizResult fviz_unstructured_grid_add_cells_fixed(
    FVizUnstructuredGrid* grid, FVizCellType type, FVizSize points_per_cell, FVizSize cell_count, const uint32_t* point_ids)
{
    FVizResult result;
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    ++grid->dependency_suppression;
    result = fviz_cell_array_append_fixed(grid->cells, type, points_per_cell, cell_count, point_ids);
    if (result == FVIZ_OK) result = fviz_data_set_set_cell_count(grid->data_set, fviz_cell_array_count(grid->cells));
    --grid->dependency_suppression;
    if (result == FVIZ_OK && cell_count != 0u) fviz_object_modified((FVizObject*)grid);
    return result;
}

FVizResult fviz_unstructured_grid_add_cells_fixed_ids(
    FVizUnstructuredGrid* grid, FVizCellType type, FVizSize points_per_cell, FVizSize cell_count, const FVizId* point_ids)
{
    FVizResult result;
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    ++grid->dependency_suppression;
    result = fviz_cell_array_append_fixed_ids(grid->cells, type, points_per_cell, cell_count, point_ids);
    if (result == FVIZ_OK) result = fviz_data_set_set_cell_count(grid->data_set, fviz_cell_array_count(grid->cells));
    --grid->dependency_suppression;
    if (result == FVIZ_OK && cell_count != 0u) fviz_object_modified((FVizObject*)grid);
    return result;
}

FVizResult fviz_unstructured_grid_add_cell_ids(
    FVizUnstructuredGrid* grid, FVizCellType type, FVizSize point_count, const FVizId* point_ids)
{
    return fviz_unstructured_grid_add_cells_fixed_ids(grid, type, point_count, 1u, point_ids);
}

FVizResult fviz_unstructured_grid_add_cell(FVizUnstructuredGrid* grid, FVizCellType type, FVizSize point_count, const uint32_t* point_ids)
{
    return fviz_unstructured_grid_add_cells_fixed(grid, type, point_count, 1u, point_ids);
}
FVizAttributeSet* fviz_unstructured_grid_point_data(FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_data_set_point_data(grid->data_set) : NULL; }
FVizAttributeSet* fviz_unstructured_grid_cell_data(FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_data_set_cell_data(grid->data_set) : NULL; }
FVizAttributeSet* fviz_unstructured_grid_field_data(FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_data_set_field_data(grid->data_set) : NULL; }
FVizSize fviz_unstructured_grid_point_count(const FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_points_count(grid->points) : 0u; }
FVizSize fviz_unstructured_grid_cell_count(const FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_cell_array_count(grid->cells) : 0u; }
FVizBounds fviz_unstructured_grid_bounds(const FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_points_bounds(grid->points) : fviz_bounds_empty(); }

FVizResult fviz_unstructured_grid_validate(const FVizUnstructuredGrid* grid)
{
    FVizResult result;
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result = fviz_cell_array_validate(grid->cells, fviz_points_count(grid->points));
    if (result != FVIZ_OK) return result;
    result = fviz_data_set_validate(grid->data_set);
    if (result != FVIZ_OK) return result;
    if (fviz_data_set_point_count(grid->data_set) != fviz_points_count(grid->points) ||
        fviz_data_set_cell_count(grid->data_set) != fviz_cell_array_count(grid->cells))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "grid topology and dataset counts differ");
        return FVIZ_ERROR_INVALID_STATE;
    }
    return FVIZ_OK;
}

static FVizBool fviz_surface_definition(FVizCellType type, FVizSurfaceDefinition* definition)
{
    const FVizCellTypeTraits traits = fviz_cell_type_traits(type);
    uint32_t face;
    (void)memset(definition, 0, sizeof(*definition));
    definition->type = type;
    if (traits.dimension != 3u || traits.face_count == 0u || traits.face_count > 6u)
        return FVIZ_FALSE;
    definition->face_count = traits.face_count;
    for (face = 0u; face < traits.face_count; ++face)
    {
        uint32_t count = 0u;
        if (fviz_cell_type_face(type, face, definition->faces[face], 4u, &count) != FVIZ_OK || count > 4u)
            return FVIZ_FALSE;
        definition->sizes[face] = count;
    }
    return FVIZ_TRUE;
}

static void fviz_surface_compare_swap(uint32_t* left, uint32_t* right)
{
    if (*left > *right)
    {
        const uint32_t value = *left;
        *left = *right;
        *right = value;
    }
}

static void fviz_surface_sort(uint32_t* values, uint32_t count)
{
    /* Surface ownership only supports triangles/quads. Fixed sorting networks
     * avoid data-dependent inner loops in the HEX8 extraction hot path. */
    if (count == 3u)
    {
        fviz_surface_compare_swap(&values[0], &values[1]);
        fviz_surface_compare_swap(&values[1], &values[2]);
        fviz_surface_compare_swap(&values[0], &values[1]);
    }
    else if (count == 4u)
    {
        fviz_surface_compare_swap(&values[0], &values[1]);
        fviz_surface_compare_swap(&values[2], &values[3]);
        fviz_surface_compare_swap(&values[0], &values[2]);
        fviz_surface_compare_swap(&values[1], &values[3]);
        fviz_surface_compare_swap(&values[1], &values[2]);
    }
}

typedef struct FVizSurfaceFaceTable
{
    FVizSurfaceFace* faces;
    FVizSize face_count;
    FVizSize face_capacity;
    FVizSize* slots; /* zero = empty, otherwise face index + 1 */
    FVizSize slot_count;
} FVizSurfaceFaceTable;

static uint64_t fviz_surface_face_hash(const uint32_t* sorted, uint32_t count)
{
    uint64_t x = ((uint64_t)sorted[0] << 32u) | (uint64_t)sorted[1];
    uint64_t y = ((uint64_t)sorted[2] << 32u) |
        (uint64_t)(count == 4u ? sorted[3] : UINT32_C(0xffffffff));
    /* Two-word integer mix followed by Murmur's finalizer. Equality is still
     * verified against canonical IDs, so this only affects bucket quality. */
    x ^= y + UINT64_C(0x9e3779b97f4a7c15) + (x << 6u) + (x >> 2u);
    x ^= (uint64_t)count * UINT64_C(0x94d049bb133111eb);
    x ^= x >> 33u;
    x *= UINT64_C(0xff51afd7ed558ccd);
    x ^= x >> 33u;
    x *= UINT64_C(0xc4ceb9fe1a85ec53);
    x ^= x >> 33u;
    return x;
}

static FVizResult fviz_surface_face_table_create(
    FVizSize face_capacity,
    FVizSurfaceFaceTable* table)
{
    FVizSize face_bytes = 0u;
    FVizSize slot_bytes = 0u;
    FVizSize desired_slots;
    FVizSize slot_count = 16u;
    if (table == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    (void)memset(table, 0, sizeof(*table));
    if (face_capacity == 0u) return FVIZ_OK;
    /* Keep the table below roughly 75% load. Power-of-two rounding usually
     * leaves additional headroom without the 2x slot cost of a 50% table. */
    if (face_capacity > (SIZE_MAX - 2u) / 4u)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "surface face table capacity overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    desired_slots = (face_capacity * 4u + 2u) / 3u;
    while (slot_count < desired_slots)
    {
        if (slot_count > SIZE_MAX / 2u)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "surface hash table size overflow");
            return FVIZ_ERROR_OVERFLOW;
        }
        slot_count *= 2u;
    }
    if (fviz_size_multiply(face_capacity, sizeof(FVizSurfaceFace), &face_bytes) != FVIZ_OK ||
        fviz_size_multiply(slot_count, sizeof(FVizSize), &slot_bytes) != FVIZ_OK)
        return fviz_last_error_code();
    table->faces = (FVizSurfaceFace*)fviz_alloc(face_bytes);
    table->slots = (FVizSize*)fviz_alloc(slot_bytes);
    if (table->faces == NULL || table->slots == NULL)
    {
        fviz_free(table->faces);
        fviz_free(table->slots);
        (void)memset(table, 0, sizeof(*table));
        return fviz_last_error_code();
    }
    (void)memset(table->slots, 0, slot_bytes);
    table->face_capacity = face_capacity;
    table->slot_count = slot_count;
    return FVIZ_OK;
}

static void fviz_surface_face_table_destroy(FVizSurfaceFaceTable* table)
{
    if (table == NULL) return;
    fviz_free(table->faces);
    fviz_free(table->slots);
    (void)memset(table, 0, sizeof(*table));
}

static FVizResult fviz_surface_face_table_insert(
    FVizSurfaceFaceTable* table,
    const uint32_t* ids,
    uint32_t count,
    FVizId source_cell,
    FVizId source_face)
{
    uint32_t sorted[4] = {0u, 0u, 0u, 0u};
    FVizSize slot;
    FVizSize probe_count = 0u;
    uint32_t i;
    if (table == NULL || ids == NULL || count < 3u || count > 4u || table->slot_count == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < count; ++i) sorted[i] = ids[i];
    fviz_surface_sort(sorted, count);
    slot = (FVizSize)(fviz_surface_face_hash(sorted, count) & (uint64_t)(table->slot_count - 1u));
    while (probe_count < table->slot_count)
    {
        const FVizSize stored = table->slots[slot];
        if (stored == 0u)
        {
            FVizSurfaceFace* face;
            if (table->face_count >= table->face_capacity)
            {
                fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "surface face table capacity was exceeded");
                return FVIZ_ERROR_INTERNAL;
            }
            face = &table->faces[table->face_count];
            (void)memset(face, 0, sizeof(*face));
            face->count = count;
            face->occurrences = 1u;
            face->source_cell = source_cell;
            face->source_face = source_face;
            for (i = 0u; i < count; ++i)
            {
                face->ids[i] = ids[i];
                face->sorted[i] = sorted[i];
            }
            table->slots[slot] = table->face_count + 1u;
            ++table->face_count;
            return FVIZ_OK;
        }
        else
        {
            FVizSurfaceFace* face = &table->faces[stored - 1u];
            if (face->count == count &&
                memcmp(face->sorted, sorted, (FVizSize)count * sizeof(uint32_t)) == 0)
            {
                ++face->occurrences;
                return FVIZ_OK;
            }
        }
        slot = (slot + 1u) & (table->slot_count - 1u);
        ++probe_count;
    }
    fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "surface face hash table is unexpectedly full");
    return FVIZ_ERROR_INTERNAL;
}

static FVizBool fviz_component_value(const FVizDataArray* array, FVizSize index, uint32_t component, double* out_value)
{
    const void* tuple;
    if (array == NULL || out_value == NULL || component >= fviz_data_array_components(array)) return FVIZ_FALSE;
    tuple = fviz_data_array_const_tuple(array, index);
    if (tuple == NULL) return FVIZ_FALSE;
    switch (fviz_data_array_type(array))
    {
        case FVIZ_DATA_INT8: *out_value = ((const int8_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_UINT8: *out_value = ((const uint8_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_INT16: *out_value = ((const int16_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_UINT16: *out_value = ((const uint16_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_INT32: *out_value = ((const int32_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_UINT32: *out_value = ((const uint32_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_INT64: *out_value = (double)((const int64_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_UINT64: *out_value = (double)((const uint64_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_FLOAT32: *out_value = ((const float*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_FLOAT64: *out_value = ((const double*)tuple)[component]; return FVIZ_TRUE;
        default: return FVIZ_FALSE;
    }
}

static FVizBool fviz_scalar_value(const FVizDataArray* array, FVizSize index, double* out_value)
{
    return fviz_component_value(array, index, 0u, out_value);
}

static FVizResult fviz_copy_attribute_set(FVizAttributeSet* source, FVizAttributeSet* destination)
{
    FVizSize i;
    FVizAttributeRole role;
    if (source == NULL) return FVIZ_OK;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        FVizDataArray* array = fviz_attribute_set_array_at(source, i);
        if (name == NULL || array == NULL) continue;
        if (fviz_attribute_set_add(destination, name, array) != FVIZ_OK) return fviz_last_error_code();
    }
    for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
    {
        const char* active = fviz_attribute_set_active_name(source, role);
        if (active != NULL && fviz_attribute_set_set_active(destination, role, active) != FVIZ_OK)
            return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_copy_filtered_attribute_set(
    const FVizAttributeSet* source, FVizAttributeSet* destination,
    const uint8_t* selected, FVizSize source_count, FVizSize selected_count)
{
    FVizSize array_index;
    FVizAttributeRole role;
    if (source == NULL || destination == NULL || (source_count != 0u && selected == NULL))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source, array_index);
        const FVizDataArray* input = fviz_attribute_set_const_array_at(source, array_index);
        FVizDataArray* output = NULL;
        FVizSize i;
        if (name == NULL || input == NULL || fviz_data_array_tuple_count(input) != source_count) continue;
        if (fviz_data_array_create(fviz_data_array_type(input), fviz_data_array_components(input), &output) != FVIZ_OK ||
            fviz_data_array_reserve(output, selected_count) != FVIZ_OK)
        {
            fviz_release(output);
            return fviz_last_error_code();
        }
        for (i = 0u; i < source_count; ++i)
            if (selected[i] != 0u &&
                fviz_data_array_append_tuple(output, fviz_data_array_const_tuple(input, i)) != FVIZ_OK)
            {
                fviz_release(output);
                return fviz_last_error_code();
            }
        if (fviz_attribute_set_add(destination, name, output) != FVIZ_OK)
        {
            fviz_release(output);
            return fviz_last_error_code();
        }
        fviz_release(output);
    }
    for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
    {
        const char* active = fviz_attribute_set_active_name(source, role);
        if (active != NULL && fviz_attribute_set_const_get(destination, active) != NULL &&
            fviz_attribute_set_set_active(destination, role, active) != FVIZ_OK)
            return fviz_last_error_code();
    }
    return FVIZ_OK;
}

FVizResult fviz_unstructured_grid_shallow_copy(
    const FVizUnstructuredGrid* source, FVizUnstructuredGrid** out_copy)
{
    FVizUnstructuredGrid* copy = NULL;
    if (source == NULL || out_copy == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "source and out_copy must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_copy = NULL;
    if (fviz_unstructured_grid_create(&copy) != FVIZ_OK) return fviz_last_error_code();
    fviz_release(copy->points);
    fviz_release(copy->cells);
    copy->points = (FVizPoints*)fviz_retain(source->points);
    copy->cells = (FVizCellArray*)fviz_retain(source->cells);
    if (copy->points == NULL || copy->cells == NULL ||
        fviz_data_set_set_point_count(copy->data_set, fviz_points_count(source->points)) != FVIZ_OK ||
        fviz_data_set_set_cell_count(copy->data_set, fviz_cell_array_count(source->cells)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)source),
            fviz_unstructured_grid_point_data(copy)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)source),
            fviz_unstructured_grid_cell_data(copy)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_field_data((FVizUnstructuredGrid*)source),
            fviz_unstructured_grid_field_data(copy)) != FVIZ_OK)
    {
        fviz_release(copy);
        return fviz_last_error_code();
    }
    *out_copy = copy;
    return FVIZ_OK;
}

static FVizResult fviz_append_source_cell(
    FVizUnstructuredGrid* destination, const FVizCellArray* source_cells, FVizSize cell_id)
{
    FVizCellView view;
    if (fviz_cell_array_cell_view(source_cells, cell_id, &view) != FVIZ_OK) return fviz_last_error_code();
    if (view.id_storage == FVIZ_ID_STORAGE_UINT64)
        return fviz_unstructured_grid_add_cell_ids(destination, view.type, view.point_count, (const FVizId*)view.point_ids);
    return fviz_unstructured_grid_add_cell(destination, view.type, view.point_count, (const uint32_t*)view.point_ids);
}

static FVizResult fviz_clone_topology(const FVizUnstructuredGrid* grid, FVizUnstructuredGrid** out_result)
{
    FVizUnstructuredGrid* result = NULL;
    const FVizVec3* points;
    FVizSize i;
    if (fviz_unstructured_grid_create(&result) != FVIZ_OK) return fviz_last_error_code();
    points = fviz_points_data(grid->points);
    if (fviz_unstructured_grid_reserve(result, fviz_points_count(grid->points),
            fviz_cell_array_count(grid->cells), fviz_cell_array_connectivity_size(grid->cells)) != FVIZ_OK ||
        fviz_unstructured_grid_add_points_ids(result, points, fviz_points_count(grid->points), NULL) != FVIZ_OK)
        goto fail;
    for (i = 0u; i < fviz_cell_array_count(grid->cells); ++i)
        if (fviz_append_source_cell(result, grid->cells, i) != FVIZ_OK) goto fail;
    if (fviz_copy_attribute_set(fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_point_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_cell_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_field_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_field_data(result)) != FVIZ_OK)
    {
        goto fail;
    }
    *out_result = result;
    return FVIZ_OK;
fail:
    fviz_release(result);
    return fviz_last_error_code();
}

typedef struct FVizTransformRangeContext
{
    FVizVec3* points;
    const FVizTransform* transform;
} FVizTransformRangeContext;

static void fviz_transform_point_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizTransformRangeContext* context = (FVizTransformRangeContext*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i)
        context->points[i] = fviz_transform_point(context->transform, context->points[i]);
}

FVizResult fviz_unstructured_grid_transform(
    const FVizUnstructuredGrid* grid,
    const FVizTransform* transform,
    FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* result = NULL;
    FVizTransformRangeContext context;
    FVizVec3* points;
    FVizSize i;
    if (grid == NULL || transform == NULL || out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid, transform and out_grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    if (fviz_clone_topology(grid, &result) != FVIZ_OK) return fviz_last_error_code();
    points = (FVizVec3*)fviz_array_data(result->points->data);
    context.points = points;
    context.transform = transform;
    if (fviz_parallel_for(
            0u, fviz_points_count(result->points), 256u,
            fviz_transform_point_range, &context) != FVIZ_OK)
    {
        fviz_release(result);
        return fviz_last_error_code();
    }
    result->points->bounds = fviz_bounds_empty();
    for (i = 0u; i < fviz_points_count(result->points); ++i)
        fviz_bounds_include_point(&result->points->bounds, points[i]);
    fviz_object_modified((FVizObject*)result->points->data);
    *out_grid = result;
    return FVIZ_OK;
}

typedef struct FVizWarpRangeContext
{
    const FVizVec3* points;
    const unsigned char* vector_data;
    FVizSize vector_stride;
    FVizDataType vector_type;
    FVizVec3* displaced;
    double scale;
} FVizWarpRangeContext;

static void fviz_warp_read_vector(const FVizWarpRangeContext* context,FVizSize i,double* x,double* y,double* z)
{
    const unsigned char* p=context->vector_data+i*context->vector_stride;
    switch(context->vector_type)
    {
        case FVIZ_DATA_INT8: { const int8_t* v=(const int8_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_UINT8: { const uint8_t* v=(const uint8_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_INT16: { const int16_t* v=(const int16_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_UINT16: { const uint16_t* v=(const uint16_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_INT32: { const int32_t* v=(const int32_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_UINT32: { const uint32_t* v=(const uint32_t*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_INT64: { const int64_t* v=(const int64_t*)p; *x=(double)v[0]; *y=(double)v[1]; *z=(double)v[2]; break; }
        case FVIZ_DATA_UINT64: { const uint64_t* v=(const uint64_t*)p; *x=(double)v[0]; *y=(double)v[1]; *z=(double)v[2]; break; }
        case FVIZ_DATA_FLOAT32: { const float* v=(const float*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        case FVIZ_DATA_FLOAT64: { const double* v=(const double*)p; *x=v[0]; *y=v[1]; *z=v[2]; break; }
        default: *x=0.0; *y=0.0; *z=0.0; break;
    }
}

static void fviz_warp_point_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizWarpRangeContext* context = (FVizWarpRangeContext*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i)
    {
        double vx = 0.0, vy = 0.0, vz = 0.0;
        fviz_warp_read_vector(context,i,&vx,&vy,&vz);
        context->displaced[i] = fviz_vec3(
            context->points[i].x + (float)(vx * context->scale),
            context->points[i].y + (float)(vy * context->scale),
            context->points[i].z + (float)(vz * context->scale));
    }
}

FVizResult fviz_unstructured_grid_warp_by_array(
    const FVizUnstructuredGrid* grid,
    const FVizDataArray* vectors,
    double scale,
    FVizUnstructuredGrid** out_grid)
{
    const FVizVec3* points;
    FVizVec3* displaced_points = NULL;
    FVizUnstructuredGrid* result = NULL;
    FVizWarpRangeContext context;
    const FVizSize point_count = grid != NULL ? fviz_points_count(grid->points) : 0u;
    FVizSize i;
    if (grid == NULL || vectors == NULL || out_grid == NULL || !isfinite(scale))
    {
        if (out_grid != NULL) *out_grid = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "warp requires grid, vector array, finite scale and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_components(vectors) != 3u ||
        fviz_data_array_tuple_count(vectors) != fviz_points_count(grid->points))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "warp vector must be a three-component point array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_unstructured_grid_create(&result) != FVIZ_OK) return fviz_last_error_code();
    points = fviz_points_data(grid->points);
    if (point_count > 0u)
    {
        displaced_points = (FVizVec3*)fviz_alloc(point_count * sizeof(FVizVec3));
        if (displaced_points == NULL)
        {
            fviz_release(result);
            return fviz_last_error_code();
        }
        context.points = points;
        context.vector_data = (const unsigned char*)fviz_data_array_const_data(vectors);
        context.vector_stride = fviz_data_array_tuple_stride(vectors);
        context.vector_type = fviz_data_array_type(vectors);
        context.displaced = displaced_points;
        context.scale = scale;
        if (fviz_parallel_for(0u, point_count, 256u, fviz_warp_point_range, &context) != FVIZ_OK)
        {
            fviz_free(displaced_points);
            fviz_release(result);
            return fviz_last_error_code();
        }
    }
    if (fviz_unstructured_grid_reserve(result, point_count, fviz_cell_array_count(grid->cells),
            fviz_cell_array_connectivity_size(grid->cells)) != FVIZ_OK ||
        fviz_unstructured_grid_add_points_ids(result, displaced_points, point_count, NULL) != FVIZ_OK)
    {
        fviz_free(displaced_points);
        fviz_release(result);
        return fviz_last_error_code();
    }
    fviz_free(displaced_points);
    for (i = 0u; i < fviz_cell_array_count(grid->cells); ++i)
    {
        if (fviz_append_source_cell(result, grid->cells, i) != FVIZ_OK)
        {
            fviz_release(result);
            return fviz_last_error_code();
        }
    }
    if (fviz_copy_attribute_set(fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_point_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_cell_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_field_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_field_data(result)) != FVIZ_OK)
    {
        fviz_release(result);
        return fviz_last_error_code();
    }
    *out_grid = result;
    return FVIZ_OK;
}

FVizResult fviz_unstructured_grid_warp_by_vector(
    const FVizUnstructuredGrid* grid,
    const char* vector_name,
    double scale,
    FVizUnstructuredGrid** out_grid)
{
    const FVizDataArray* vectors;
    if (grid == NULL || vector_name == NULL || vector_name[0] == '\0' || out_grid == NULL)
    {
        if (out_grid != NULL) *out_grid = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "warp requires grid, vector name and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    vectors = fviz_attribute_set_const_get(
        fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid), vector_name);
    if (vectors == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "warp vector array was not found in point data");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_unstructured_grid_warp_by_array(grid, vectors, scale, out_grid);
}


typedef struct FVizCellToPointRangeContext
{
    const void* source_values;
    FVizDataType source_type;
    uint32_t components;
    const FVizSize* offsets;
    const FVizSize* adjacent_cells;
    void* output_values;
    FVizDataType output_type;
} FVizCellToPointRangeContext;

static double fviz_cell_to_point_component(
    const FVizCellToPointRangeContext* context, FVizSize cell_id, uint32_t component)
{
    const FVizSize index = cell_id * (FVizSize)context->components + (FVizSize)component;
    switch (context->source_type)
    {
        case FVIZ_DATA_INT8: return ((const int8_t*)context->source_values)[index];
        case FVIZ_DATA_UINT8: return ((const uint8_t*)context->source_values)[index];
        case FVIZ_DATA_INT16: return ((const int16_t*)context->source_values)[index];
        case FVIZ_DATA_UINT16: return ((const uint16_t*)context->source_values)[index];
        case FVIZ_DATA_INT32: return ((const int32_t*)context->source_values)[index];
        case FVIZ_DATA_UINT32: return ((const uint32_t*)context->source_values)[index];
        case FVIZ_DATA_INT64: return (double)((const int64_t*)context->source_values)[index];
        case FVIZ_DATA_UINT64: return (double)((const uint64_t*)context->source_values)[index];
        case FVIZ_DATA_FLOAT32: return ((const float*)context->source_values)[index];
        case FVIZ_DATA_FLOAT64: return ((const double*)context->source_values)[index];
        default: return 0.0;
    }
}

static void fviz_cell_to_point_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizCellToPointRangeContext* context = (FVizCellToPointRangeContext*)user_data;
    FVizSize point_id;
    for (point_id = begin; point_id < end; ++point_id)
    {
        const FVizSize adjacency_begin = context->offsets[point_id];
        const FVizSize adjacency_end = context->offsets[point_id + 1u];
        const FVizSize degree = adjacency_end - adjacency_begin;
        const FVizSize output_base = point_id * (FVizSize)context->components;
        uint32_t component;
        for (component = 0u; component < context->components; ++component)
        {
            double sum = 0.0;
            FVizSize cursor;
            for (cursor = adjacency_begin; cursor < adjacency_end; ++cursor)
                sum += fviz_cell_to_point_component(context, context->adjacent_cells[cursor], component);
            if (context->output_type == FVIZ_DATA_FLOAT32)
                ((float*)context->output_values)[output_base + component] = degree != 0u ? (float)(sum / (double)degree) : 0.0f;
            else
                ((double*)context->output_values)[output_base + component] = degree != 0u ? sum / (double)degree : 0.0;
        }
    }
}

static FVizBool fviz_cell_to_point_supported_type(FVizDataType type)
{
    switch (type)
    {
        case FVIZ_DATA_INT8:
        case FVIZ_DATA_UINT8:
        case FVIZ_DATA_INT16:
        case FVIZ_DATA_UINT16:
        case FVIZ_DATA_INT32:
        case FVIZ_DATA_UINT32:
        case FVIZ_DATA_INT64:
        case FVIZ_DATA_UINT64:
        case FVIZ_DATA_FLOAT32:
        case FVIZ_DATA_FLOAT64:
            return FVIZ_TRUE;
        default:
            return FVIZ_FALSE;
    }
}

static FVizBool fviz_cell_to_point_should_average(
    const FVizAttributeSet* cell_data, const char* name)
{
    const char* global_ids;
    if (cell_data == NULL || name == NULL) return FVIZ_FALSE;
    if (strcmp(name, FVIZ_GHOST_ARRAY_NAME) == 0 ||
        strcmp(name, FVIZ_GHOST_LEVEL_ARRAY_NAME) == 0 ||
        strcmp(name, "FVizOriginalCellIds") == 0)
        return FVIZ_FALSE;
    global_ids = fviz_attribute_set_active_name(cell_data, FVIZ_ATTRIBUTE_GLOBAL_IDS);
    if (global_ids != NULL && strcmp(global_ids, name) == 0) return FVIZ_FALSE;
    return FVIZ_TRUE;
}

static FVizResult fviz_unstructured_grid_cell_data_to_point_data_serial(
    const FVizUnstructuredGrid* grid,
    FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* result = NULL;
    FVizAttributeSet* cell_data;
    FVizAttributeSet* point_data;
    const FVizSize point_count = grid != NULL ? fviz_points_count(grid->points) : 0u;
    const FVizSize cell_count = grid != NULL ? fviz_cell_array_count(grid->cells) : 0u;
    uint32_t* counts = NULL;
    FVizSize count_bytes = 0u;
    FVizSize i;
    /* Caller has already validated the grid and initialized out_grid. */
    /* Geometry/topology and existing attributes are immutable for this operation;
     * share them instead of copying an entire large FEA mesh.  The copy owns an
     * independent DataSet/AttributeSet so newly generated point arrays cannot
     * mutate the input. */
    if (fviz_unstructured_grid_shallow_copy(grid, &result) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_size_multiply(point_count, sizeof(uint32_t), &count_bytes) != FVIZ_OK) goto fail;
    if (point_count != 0u)
    {
        counts = (uint32_t*)fviz_alloc(count_bytes);
        if (counts == NULL) goto fail;
        (void)memset(counts, 0, count_bytes);
    }
    /* Point valence is independent of the field. Compute it once even when the
     * result contains many scalar/vector/tensor cell arrays. */
    for (i = 0u; i < cell_count; ++i)
    {
        FVizCellView view;
        FVizSize local;
        if (fviz_cell_array_cell_view(grid->cells, i, &view) != FVIZ_OK) goto fail;
        for (local = 0u; local < view.point_count; ++local)
        {
            const FVizId id = fviz_cell_view_point_id(&view, local);
            if (id >= (FVizId)point_count)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cell references an invalid point during averaging");
                goto fail;
            }
            if (counts[(FVizSize)id] != UINT32_MAX) ++counts[(FVizSize)id];
        }
    }
    cell_data = fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid);
    point_data = fviz_unstructured_grid_point_data(result);
    for (i = 0u; i < fviz_attribute_set_count(cell_data); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(cell_data, i);
        const char* name = fviz_attribute_set_name_at(cell_data, i);
        const uint32_t components = array != NULL ? fviz_data_array_components(array) : 0u;
        const FVizDataType source_type = array != NULL ? fviz_data_array_type(array) : FVIZ_DATA_FLOAT64;
        const FVizDataType output_type = source_type == FVIZ_DATA_FLOAT32 ? FVIZ_DATA_FLOAT32 : FVIZ_DATA_FLOAT64;
        FVizDataArray* output = NULL;
        double* sums = NULL;
        double* tuple_values = NULL;
        FVizSize sum_count = 0u, sum_bytes = 0u, tuple_bytes = 0u;
        FVizSize cell_id, point_id;
        uint32_t component;
        if (array == NULL || name == NULL || components == 0u ||
            fviz_data_array_tuple_count(array) != cell_count ||
            fviz_cell_to_point_should_average(cell_data, name) == FVIZ_FALSE)
            continue;
        if (fviz_size_multiply(point_count, (FVizSize)components, &sum_count) != FVIZ_OK ||
            fviz_size_multiply(sum_count, sizeof(double), &sum_bytes) != FVIZ_OK ||
            fviz_size_multiply((FVizSize)components, sizeof(double), &tuple_bytes) != FVIZ_OK)
            goto fail;
        if (sum_count != 0u)
        {
            sums = (double*)fviz_alloc(sum_bytes);
            if (sums == NULL) goto field_fail;
            (void)memset(sums, 0, sum_bytes);
        }
        if (components != 0u)
        {
            tuple_values = (double*)fviz_alloc(tuple_bytes);
            if (tuple_values == NULL) goto field_fail;
        }
        for (cell_id = 0u; cell_id < cell_count; ++cell_id)
        {
            FVizCellView view;
            FVizSize local;
            for (component = 0u; component < components; ++component)
            {
                if (fviz_component_value(array, cell_id, component, &tuple_values[component]) == FVIZ_FALSE)
                {
                    fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell attribute type is unsupported for point averaging");
                    goto field_fail;
                }
            }
            if (fviz_cell_array_cell_view(grid->cells, cell_id, &view) != FVIZ_OK) goto field_fail;
            for (local = 0u; local < view.point_count; ++local)
            {
                const FVizId id = fviz_cell_view_point_id(&view, local);
                const FVizSize base = (FVizSize)id * (FVizSize)components;
                for (component = 0u; component < components; ++component)
                    sums[base + component] += tuple_values[component];
            }
        }
        if (fviz_data_array_create(output_type, components, &output) != FVIZ_OK ||
            fviz_data_array_resize(output, point_count) != FVIZ_OK)
            goto field_fail;
        if (output_type == FVIZ_DATA_FLOAT32)
        {
            float* values = (float*)fviz_data_array_data(output);
            for (point_id = 0u; point_id < point_count; ++point_id)
            {
                const double divisor = counts[point_id] != 0u ? (double)counts[point_id] : 1.0;
                const FVizSize base = point_id * (FVizSize)components;
                for (component = 0u; component < components; ++component)
                    values[base + component] = counts[point_id] != 0u ? (float)(sums[base + component] / divisor) : 0.0f;
            }
        }
        else
        {
            double* values = (double*)fviz_data_array_data(output);
            for (point_id = 0u; point_id < point_count; ++point_id)
            {
                const double divisor = counts[point_id] != 0u ? (double)counts[point_id] : 1.0;
                const FVizSize base = point_id * (FVizSize)components;
                for (component = 0u; component < components; ++component)
                    values[base + component] = counts[point_id] != 0u ? sums[base + component] / divisor : 0.0;
            }
        }
        if (fviz_attribute_set_add(point_data, name, output) != FVIZ_OK) goto field_fail;
        {
            FVizAttributeRole role;
            for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
            {
                const char* active = fviz_attribute_set_active_name(cell_data, role);
                if (active != NULL && strcmp(active, name) == 0)
                    (void)fviz_attribute_set_set_active(point_data, role, name);
            }
        }
        fviz_release(output); output = NULL;
        fviz_free(sums); sums = NULL;
        fviz_free(tuple_values); tuple_values = NULL;
        continue;
field_fail:
        fviz_release(output);
        fviz_free(sums);
        fviz_free(tuple_values);
        goto fail;
    }
    fviz_free(counts);
    *out_grid = result;
    return FVIZ_OK;
fail:
    fviz_free(counts);
    fviz_release(result);
    return fviz_last_error_code();
}


FVizResult fviz_unstructured_grid_cell_data_to_point_data(
    const FVizUnstructuredGrid* grid,
    FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* result = NULL;
    FVizAttributeSet* cell_data;
    FVizAttributeSet* point_data;
    const FVizSize point_count = grid != NULL ? fviz_points_count(grid->points) : 0u;
    const FVizSize cell_count = grid != NULL ? fviz_cell_array_count(grid->cells) : 0u;
    const FVizSize adjacency_count = grid != NULL ? fviz_cell_array_connectivity_size(grid->cells) : 0u;
    uint32_t* counts = NULL;
    FVizSize* offsets = NULL;
    FVizSize* adjacent_cells = NULL;
    FVizSize* cursors = NULL;
    FVizSize count_bytes = 0u, offset_bytes = 0u, adjacency_bytes = 0u;
    FVizSize i;
    if (grid == NULL || out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid and out_grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    {
        const FVizAttributeSet* source_cell_data = fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid);
        FVizSize averaging_fields = 0u;
        FVizSize field_index;
        for (field_index = 0u; field_index < fviz_attribute_set_count(source_cell_data); ++field_index)
        {
            const FVizDataArray* field = fviz_attribute_set_const_array_at(source_cell_data, field_index);
            const char* field_name = fviz_attribute_set_name_at(source_cell_data, field_index);
            if (field != NULL && field_name != NULL && fviz_data_array_components(field) != 0u &&
                fviz_data_array_tuple_count(field) == cell_count &&
                fviz_cell_to_point_should_average(source_cell_data, field_name) != FVIZ_FALSE)
                ++averaging_fields;
        }
        /* A point->cell adjacency costs an additional two topology traversals.  It
         * amortizes very well across the many result arrays typical of FEA frames,
         * but the legacy scatter path is measurably faster for a single array. */
        if (averaging_fields <= 1u)
            return fviz_unstructured_grid_cell_data_to_point_data_serial(grid, out_grid);
    }
    /* Geometry/topology and existing attributes are immutable for this operation;
     * share them instead of copying an entire large FEA mesh.  The copy owns an
     * independent DataSet/AttributeSet so newly generated point arrays cannot
     * mutate the input. */
    if (fviz_unstructured_grid_shallow_copy(grid, &result) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_size_multiply(point_count, sizeof(uint32_t), &count_bytes) != FVIZ_OK ||
        fviz_size_multiply(point_count + 1u, sizeof(FVizSize), &offset_bytes) != FVIZ_OK ||
        fviz_size_multiply(adjacency_count, sizeof(FVizSize), &adjacency_bytes) != FVIZ_OK)
        goto fail;
    if (point_count != 0u)
    {
        counts = (uint32_t*)fviz_alloc(count_bytes);
        offsets = (FVizSize*)fviz_alloc(offset_bytes);
        cursors = (FVizSize*)fviz_alloc(offset_bytes);
        if (counts == NULL || offsets == NULL || cursors == NULL) goto fail;
        (void)memset(counts, 0, count_bytes);
    }
    if (adjacency_count != 0u)
    {
        adjacent_cells = (FVizSize*)fviz_alloc(adjacency_bytes);
        if (adjacent_cells == NULL) goto fail;
    }

    /* Build point->cell adjacency once.  A point-centric gather avoids the large
     * per-field double accumulation buffer and gives each parallel worker exclusive
     * output tuples, so no atomics are needed.  Cell ids are inserted in ascending
     * order, preserving the old deterministic summation order for every point. */
    for (i = 0u; i < cell_count; ++i)
    {
        FVizCellView view;
        FVizSize local;
        if (fviz_cell_array_cell_view(grid->cells, i, &view) != FVIZ_OK) goto fail;
        for (local = 0u; local < view.point_count; ++local)
        {
            const FVizId id = fviz_cell_view_point_id(&view, local);
            if (id >= (FVizId)point_count)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cell references an invalid point during averaging");
                goto fail;
            }
            if (counts[(FVizSize)id] == UINT32_MAX)
            {
                fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "point valence exceeds the supported range");
                goto fail;
            }
            ++counts[(FVizSize)id];
        }
    }
    if (point_count != 0u)
    {
        FVizSize running = 0u;
        offsets[0] = 0u;
        for (i = 0u; i < point_count; ++i)
        {
            if ((FVizSize)counts[i] > (FVizSize)-1 - running)
            {
                fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "point adjacency size overflow");
                goto fail;
            }
            running += (FVizSize)counts[i];
            offsets[i + 1u] = running;
            cursors[i] = offsets[i];
        }
        if (running != adjacency_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cell connectivity and point adjacency counts disagree");
            goto fail;
        }
        for (i = 0u; i < cell_count; ++i)
        {
            FVizCellView view;
            FVizSize local;
            if (fviz_cell_array_cell_view(grid->cells, i, &view) != FVIZ_OK) goto fail;
            for (local = 0u; local < view.point_count; ++local)
            {
                const FVizSize point_id = (FVizSize)fviz_cell_view_point_id(&view, local);
                adjacent_cells[cursors[point_id]++] = i;
            }
        }
    }

    cell_data = fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid);
    point_data = fviz_unstructured_grid_point_data(result);
    for (i = 0u; i < fviz_attribute_set_count(cell_data); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(cell_data, i);
        const char* name = fviz_attribute_set_name_at(cell_data, i);
        const uint32_t components = array != NULL ? fviz_data_array_components(array) : 0u;
        const FVizDataType source_type = array != NULL ? fviz_data_array_type(array) : FVIZ_DATA_FLOAT64;
        const FVizDataType output_type = source_type == FVIZ_DATA_FLOAT32 ? FVIZ_DATA_FLOAT32 : FVIZ_DATA_FLOAT64;
        FVizDataArray* output = NULL;
        FVizCellToPointRangeContext context;
        FVizSize output_count = 0u;
        if (array == NULL || name == NULL || components == 0u ||
            fviz_data_array_tuple_count(array) != cell_count ||
            fviz_cell_to_point_should_average(cell_data, name) == FVIZ_FALSE)
            continue;
        if (fviz_cell_to_point_supported_type(source_type) == FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell attribute type is unsupported for point averaging");
            goto fail;
        }
        if (fviz_size_multiply(point_count, (FVizSize)components, &output_count) != FVIZ_OK) goto fail;
        (void)output_count;
        if (fviz_data_array_create(output_type, components, &output) != FVIZ_OK ||
            fviz_data_array_resize(output, point_count) != FVIZ_OK)
            goto field_fail;
        context.source_values = fviz_data_array_const_data(array);
        context.source_type = source_type;
        context.components = components;
        context.offsets = offsets;
        context.adjacent_cells = adjacent_cells;
        context.output_values = fviz_data_array_data(output);
        context.output_type = output_type;
        if (point_count != 0u &&
            fviz_parallel_for(0u, point_count, 512u, fviz_cell_to_point_range, &context) != FVIZ_OK)
            goto field_fail;
        if (fviz_attribute_set_add(point_data, name, output) != FVIZ_OK) goto field_fail;
        {
            FVizAttributeRole role;
            for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
            {
                const char* active = fviz_attribute_set_active_name(cell_data, role);
                if (active != NULL && strcmp(active, name) == 0)
                    (void)fviz_attribute_set_set_active(point_data, role, name);
            }
        }
        fviz_release(output);
        continue;
field_fail:
        fviz_release(output);
        goto fail;
    }
    fviz_free(cursors);
    fviz_free(adjacent_cells);
    fviz_free(offsets);
    fviz_free(counts);
    *out_grid = result;
    return FVIZ_OK;
fail:
    fviz_free(cursors);
    fviz_free(adjacent_cells);
    fviz_free(offsets);
    fviz_free(counts);
    fviz_release(result);
    return fviz_last_error_code();
}

FVizResult fviz_unstructured_grid_threshold_cells(
    const FVizUnstructuredGrid* grid,
    const char* scalar_name,
    double minimum,
    double maximum,
    FVizUnstructuredGrid** out_grid)
{
    const FVizDataArray* scalar;
    FVizUnstructuredGrid* result = NULL;
    uint8_t* selected = NULL;
    FVizSize selected_count = 0u;
    FVizSize cell_count;
    FVizSize i;
    const FVizVec3* points;
    if (grid == NULL || scalar_name == NULL || scalar_name[0] == '\0' || out_grid == NULL || minimum > maximum)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "threshold requires grid, scalar name, range and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    scalar = fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid), scalar_name);
    cell_count = fviz_unstructured_grid_cell_count(grid);
    if (scalar == NULL || fviz_data_array_components(scalar) != 1u ||
        fviz_data_array_tuple_count(scalar) != cell_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "threshold scalar must be a one-component cell array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (cell_count != 0u)
    {
        selected = (uint8_t*)fviz_alloc(cell_count);
        if (selected == NULL) return fviz_last_error_code();
        for (i = 0u; i < cell_count; ++i)
        {
            double value;
            if (!fviz_scalar_value(scalar, i, &value))
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "threshold scalar type is unsupported");
                goto fail;
            }
            selected[i] = value >= minimum && value <= maximum ? 1u : 0u;
            selected_count += selected[i] != 0u ? 1u : 0u;
        }
    }
    if (fviz_unstructured_grid_create(&result) != FVIZ_OK) goto fail;
    points = fviz_points_data(grid->points);
    if (fviz_unstructured_grid_reserve(result, fviz_points_count(grid->points),
            selected_count, fviz_cell_array_connectivity_size(grid->cells)) != FVIZ_OK ||
        fviz_unstructured_grid_add_points_ids(result, points, fviz_points_count(grid->points), NULL) != FVIZ_OK)
        goto fail;
    for (i = 0u; i < cell_count; ++i)
        if (selected[i] != 0u && fviz_append_source_cell(result, grid->cells, i) != FVIZ_OK) goto fail;
    /* Points are intentionally not compacted by this legacy API, so point and
       field attributes can be retained directly. Cell attributes are gathered
       through the exact same selection mask, preserving ghost/provenance data. */
    if (fviz_copy_attribute_set(
            fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_point_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(
            fviz_unstructured_grid_field_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_field_data(result)) != FVIZ_OK ||
        fviz_copy_filtered_attribute_set(
            fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_cell_data(result), selected, cell_count, selected_count) != FVIZ_OK)
        goto fail;
    fviz_free(selected);
    *out_grid = result;
    return FVIZ_OK;
fail:
    fviz_free(selected);
    fviz_release(result);
    return fviz_last_error_code();
}

static FVizResult fviz_transfer_point_scalars(const FVizUnstructuredGrid* grid, FVizPolyData* surface)
{
    FVizAttributeSet* source = fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid);
    FVizDataArray* first_active = NULL;
    FVizSize i;
    if (source == NULL) return FVIZ_OK;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(source, i);
        const char* name = fviz_attribute_set_name_at(source, i);
        FVizDataArray* output = NULL;
        FVizSize j;
        if (array == NULL || name == NULL || fviz_data_array_components(array) != 1u ||
            fviz_data_array_tuple_count(array) != fviz_points_count(grid->points))
        {
            continue;
        }
        if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &output) != FVIZ_OK) return fviz_last_error_code();
        for (j = 0u; j < fviz_data_array_tuple_count(array); ++j)
        {
            double value;
            float output_value;
            if (!fviz_scalar_value(array, j, &value))
            {
                fviz_release(output);
                fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "point scalar type is unsupported");
                return FVIZ_ERROR_INTERNAL;
            }
            output_value = (float)value;
            if (fviz_data_array_append_tuple(output, &output_value) != FVIZ_OK)
            {
                fviz_release(output);
                return fviz_last_error_code();
            }
        }
        if (fviz_attribute_set_add(fviz_poly_data_point_data(surface), name, output) != FVIZ_OK)
        {
            fviz_release(output);
            return fviz_last_error_code();
        }
        fviz_release(output);
        if (first_active == NULL) first_active = fviz_attribute_set_get(fviz_poly_data_point_data(surface), name);
    }
    if (first_active != NULL)
    {
        return fviz_poly_data_set_scalars(surface, first_active);
    }
    return FVIZ_OK;
}


typedef struct FVizHighOrderSurfaceFace
{
    uint32_t ids[9];
    uint32_t sorted[9];
    uint8_t count;
    uint8_t reserved[3];
    uint32_t occurrences;
    FVizId source_cell;
    FVizId source_face;
} FVizHighOrderSurfaceFace;

typedef struct FVizHighOrderSurfaceTable
{
    FVizHighOrderSurfaceFace* faces;
    FVizSize face_count;
    FVizSize face_capacity;
    FVizSize* slots;
    FVizSize slot_capacity;
} FVizHighOrderSurfaceTable;

static uint64_t fviz_high_order_face_hash(const uint32_t* sorted, uint32_t count)
{
    uint64_t h = UINT64_C(1469598103934665603);
    uint32_t i;
    h ^= (uint64_t)count; h *= UINT64_C(1099511628211);
    for (i=0u;i<count;++i)
    {
        h ^= (uint64_t)sorted[i]; h *= UINT64_C(1099511628211);
    }
    h ^= h >> 33u; h *= UINT64_C(0xff51afd7ed558ccd);
    h ^= h >> 33u;
    return h;
}

static void fviz_high_order_sort_ids(uint32_t* ids, uint32_t count)
{
    uint32_t i;
    for (i=1u;i<count;++i)
    {
        const uint32_t value=ids[i];
        uint32_t j=i;
        while (j!=0u && ids[j-1u]>value) { ids[j]=ids[j-1u]; --j; }
        ids[j]=value;
    }
}

static FVizResult fviz_high_order_surface_table_create(FVizSize max_faces,FVizHighOrderSurfaceTable* table)
{
    FVizSize face_bytes=0u, desired=0u, slot_bytes=0u, slots=8u;
    if (table==NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    (void)memset(table,0,sizeof(*table));
    if (max_faces==0u) return FVIZ_OK;
    if (fviz_size_multiply(max_faces,sizeof(*table->faces),&face_bytes)!=FVIZ_OK) return fviz_last_error_code();
    table->faces=(FVizHighOrderSurfaceFace*)fviz_alloc(face_bytes);
    if (table->faces==NULL) return fviz_last_error_code();
    table->face_capacity=max_faces;
    /* Keep <= 70% occupancy; high-order surfaces are much smaller than the HEX8 fast path. */
    if (max_faces > ((FVizSize)-1)/10u*7u) { fviz_free(table->faces); (void)memset(table,0,sizeof(*table)); return FVIZ_ERROR_OVERFLOW; }
    desired=(max_faces*10u+6u)/7u;
    while (slots<desired)
    {
        if (slots>((FVizSize)-1)/2u) { fviz_free(table->faces); (void)memset(table,0,sizeof(*table)); return FVIZ_ERROR_OVERFLOW; }
        slots*=2u;
    }
    if (fviz_size_multiply(slots,sizeof(*table->slots),&slot_bytes)!=FVIZ_OK) { fviz_free(table->faces); (void)memset(table,0,sizeof(*table)); return fviz_last_error_code(); }
    table->slots=(FVizSize*)fviz_alloc(slot_bytes);
    if (table->slots==NULL) { fviz_free(table->faces); (void)memset(table,0,sizeof(*table)); return fviz_last_error_code(); }
    (void)memset(table->slots,0,slot_bytes);
    table->slot_capacity=slots;
    return FVIZ_OK;
}

static void fviz_high_order_surface_table_destroy(FVizHighOrderSurfaceTable* table)
{
    if (table==NULL) return;
    fviz_free(table->faces); fviz_free(table->slots); (void)memset(table,0,sizeof(*table));
}

static FVizBool fviz_high_order_face_equal(const FVizHighOrderSurfaceFace* face,const uint32_t* sorted,uint32_t count)
{
    uint32_t i;
    if (face->count!=(uint8_t)count) return FVIZ_FALSE;
    for (i=0u;i<count;++i) if (face->sorted[i]!=sorted[i]) return FVIZ_FALSE;
    return FVIZ_TRUE;
}

static FVizResult fviz_high_order_surface_table_insert(
    FVizHighOrderSurfaceTable* table,const uint32_t* ids,uint32_t count,FVizId cell_id,FVizId face_id)
{
    uint32_t sorted[9];
    uint64_t hash;
    FVizSize slot;
    uint32_t i;
    if (table==NULL || ids==NULL || count<3u || count>9u || table->slot_capacity==0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"invalid high-order surface face");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i=0u;i<count;++i) sorted[i]=ids[i];
    fviz_high_order_sort_ids(sorted,count);
    hash=fviz_high_order_face_hash(sorted,count);
    slot=(FVizSize)hash & (table->slot_capacity-1u);
    for (;;)
    {
        const FVizSize stored=table->slots[slot];
        if (stored==0u)
        {
            FVizHighOrderSurfaceFace* face;
            if (table->face_count>=table->face_capacity)
            {
                fviz_internal_set_error(FVIZ_ERROR_INTERNAL,"high-order surface face table capacity exceeded");
                return FVIZ_ERROR_INTERNAL;
            }
            face=&table->faces[table->face_count];
            (void)memset(face,0,sizeof(*face));
            face->count=(uint8_t)count; face->occurrences=1u; face->source_cell=cell_id; face->source_face=face_id;
            for (i=0u;i<count;++i) { face->ids[i]=ids[i]; face->sorted[i]=sorted[i]; }
            table->slots[slot]=table->face_count+1u;
            ++table->face_count;
            return FVIZ_OK;
        }
        if (fviz_high_order_face_equal(&table->faces[stored-1u],sorted,count)!=FVIZ_FALSE)
        {
            ++table->faces[stored-1u].occurrences;
            return FVIZ_OK;
        }
        slot=(slot+1u)&(table->slot_capacity-1u);
    }
}

static FVizResult fviz_append_surface_triangle(
    FVizPolyData* surface,FVizDataArray* original_cell_ids,FVizDataArray* original_face_ids,
    uint32_t a,uint32_t b,uint32_t c,FVizId source_cell,FVizId source_face)
{
    if (fviz_poly_data_add_triangle(surface,a,b,c)!=FVIZ_OK ||
        fviz_data_array_append_tuple(original_cell_ids,&source_cell)!=FVIZ_OK ||
        fviz_data_array_append_tuple(original_face_ids,&source_face)!=FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizResult fviz_tessellate_high_order_face(
    FVizPolyData* surface,FVizDataArray* original_cell_ids,FVizDataArray* original_face_ids,
    const FVizHighOrderSurfaceFace* face)
{
    const uint32_t* p=face->ids;
    if (face->count==3u)
        return fviz_append_surface_triangle(surface,original_cell_ids,original_face_ids,p[0],p[1],p[2],face->source_cell,face->source_face);
    if (face->count==4u)
    {
        if (fviz_append_surface_triangle(surface,original_cell_ids,original_face_ids,p[0],p[1],p[2],face->source_cell,face->source_face)!=FVIZ_OK) return fviz_last_error_code();
        return fviz_append_surface_triangle(surface,original_cell_ids,original_face_ids,p[0],p[2],p[3],face->source_cell,face->source_face);
    }
    if (face->count==6u) /* quadratic triangle: c0,c1,c2,m01,m12,m20 */
    {
        static const uint8_t tris[4][3]={{0,3,5},{3,1,4},{5,4,2},{3,4,5}};
        uint32_t i;
        for (i=0u;i<4u;++i)
            if (fviz_append_surface_triangle(surface,original_cell_ids,original_face_ids,
                    p[tris[i][0]],p[tris[i][1]],p[tris[i][2]],face->source_cell,face->source_face)!=FVIZ_OK)
                return fviz_last_error_code();
        return FVIZ_OK;
    }
    if (face->count==8u) /* quadratic quad: fan around ordered curved boundary */
    {
        const uint8_t ring[8]={0,4,1,5,2,6,3,7};
        uint32_t i;
        for (i=1u;i+1u<8u;++i)
            if (fviz_append_surface_triangle(surface,original_cell_ids,original_face_ids,
                    p[ring[0]],p[ring[i]],p[ring[i+1u]],face->source_cell,face->source_face)!=FVIZ_OK)
                return fviz_last_error_code();
        return FVIZ_OK;
    }
    if (face->count==9u) /* biquadratic quad: center node 8 */
    {
        const uint8_t ring[8]={0,4,1,5,2,6,3,7};
        uint32_t i;
        for (i=0u;i<8u;++i)
            if (fviz_append_surface_triangle(surface,original_cell_ids,original_face_ids,
                    p[8],p[ring[i]],p[ring[(i+1u)&7u]],face->source_cell,face->source_face)!=FVIZ_OK)
                return fviz_last_error_code();
        return FVIZ_OK;
    }
    fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"unsupported high-order face topology");
    return FVIZ_ERROR_NOT_SUPPORTED;
}

static FVizBool fviz_cell_is_first_order_volume(FVizCellType type)
{
    return type==FVIZ_CELL_TETRA || type==FVIZ_CELL_HEXAHEDRON || type==FVIZ_CELL_WEDGE || type==FVIZ_CELL_PYRAMID
        ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_extract_surface_generic_volume(
    const FVizUnstructuredGrid* grid,FVizPolyData** out_surface,FVizBool with_scalars)
{
    FVizHighOrderSurfaceTable table;
    FVizPolyData* surface=NULL;
    FVizDataArray* original_point_ids=NULL;
    FVizDataArray* original_cell_ids=NULL;
    FVizDataArray* original_face_ids=NULL;
    FVizSize max_faces=0u,cell_id,i;
    const FVizVec3* points;
    const uint8_t* ghost_flags=fviz_unstructured_grid_ghost_cell_flags(grid);
    (void)memset(&table,0,sizeof(table));
    *out_surface=NULL;
    for (cell_id=0u;cell_id<fviz_unstructured_grid_cell_count(grid);++cell_id)
    {
        const FVizCellType type=fviz_cell_array_type(grid->cells,cell_id);
        const FVizCellTypeTraits traits=fviz_cell_type_traits(type);
        FVizSize next=0u;
        if (traits.dimension!=3u || traits.face_count==0u)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"surface extraction generic path requires volume cells");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        if (fviz_size_add(max_faces,(FVizSize)traits.face_count,&next)!=FVIZ_OK) return fviz_last_error_code();
        max_faces=next;
    }
    if (fviz_high_order_surface_table_create(max_faces,&table)!=FVIZ_OK || fviz_poly_data_create(&surface)!=FVIZ_OK) goto fail;
    for (cell_id=0u;cell_id<fviz_unstructured_grid_cell_count(grid);++cell_id)
    {
        FVizCellView view;
        const FVizCellTypeTraits traits=fviz_cell_type_traits(fviz_cell_array_type(grid->cells,cell_id));
        uint32_t face_id;
        if (fviz_cell_array_cell_view(grid->cells,cell_id,&view)!=FVIZ_OK) goto fail;
        for (face_id=0u;face_id<traits.face_count;++face_id)
        {
            uint32_t local[9],ids[9],count=0u,j;
            if (fviz_cell_type_face(view.type,face_id,local,9u,&count)!=FVIZ_OK) goto fail;
            for (j=0u;j<count;++j)
            {
                const FVizId id=fviz_cell_view_point_id(&view,(FVizSize)local[j]);
                if (id>(FVizId)UINT32_MAX)
                {
                    fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"high-order surface point ID exceeds renderable UINT32 range");
                    goto fail;
                }
                ids[j]=(uint32_t)id;
            }
            if (fviz_high_order_surface_table_insert(&table,ids,count,(FVizId)cell_id,(FVizId)face_id)!=FVIZ_OK) goto fail;
        }
    }
    points=fviz_points_data(grid->points);
    if (fviz_poly_data_reserve(surface,fviz_points_count(grid->points),0u)!=FVIZ_OK ||
        fviz_poly_data_add_points(surface,points,fviz_points_count(grid->points),NULL)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&original_point_ids)!=FVIZ_OK ||
        fviz_data_array_resize(original_point_ids,fviz_points_count(grid->points))!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&original_cell_ids)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&original_face_ids)!=FVIZ_OK) goto fail;
    {
        FVizId* ids=(FVizId*)fviz_data_array_data(original_point_ids);
        for (i=0u;i<fviz_points_count(grid->points);++i) ids[i]=(FVizId)i;
    }
    if (fviz_attribute_set_add(fviz_poly_data_point_data(surface),"FVizOriginalPointIds",original_point_ids)!=FVIZ_OK) goto fail;
    for (i=0u;i<table.face_count;++i)
        if (table.faces[i].occurrences==1u &&
            fviz_unstructured_grid_cell_is_render_ghost(ghost_flags,(FVizSize)table.faces[i].source_cell)==FVIZ_FALSE &&
            fviz_tessellate_high_order_face(surface,original_cell_ids,original_face_ids,&table.faces[i])!=FVIZ_OK)
            goto fail;
    if (fviz_attribute_set_add(fviz_poly_data_cell_data(surface),"FVizOriginalCellIds",original_cell_ids)!=FVIZ_OK ||
        fviz_attribute_set_add(fviz_poly_data_cell_data(surface),"FVizOriginalFaceIds",original_face_ids)!=FVIZ_OK) goto fail;
    if (with_scalars!=FVIZ_FALSE && fviz_transfer_point_scalars(grid,surface)!=FVIZ_OK) goto fail;
    fviz_release(original_point_ids); fviz_release(original_cell_ids); fviz_release(original_face_ids);
    fviz_high_order_surface_table_destroy(&table);
    *out_surface=surface;
    return FVIZ_OK;
fail:
    fviz_release(original_point_ids); fviz_release(original_cell_ids); fviz_release(original_face_ids);
    fviz_high_order_surface_table_destroy(&table); fviz_release(surface);
    return fviz_last_error_code();
}

static FVizResult fviz_extract_surface_internal(
    const FVizUnstructuredGrid* grid,
    FVizPolyData** out_surface,
    FVizBool with_scalars)
{
    FVizSurfaceFaceTable face_table;
    FVizPolyData* surface = NULL;
    FVizSize cell_id;
    FVizSize i;
    FVizSize max_face_count = 0u;
    FVizDataArray* original_cell_ids = NULL;
    FVizDataArray* original_face_ids = NULL;
    FVizDataArray* original_point_ids = NULL;
    const FVizVec3* points;
    const uint8_t* ghost_flags;
    FVizBool fast_hex32 = fviz_cell_array_id_storage(grid != NULL ? grid->cells : NULL) == FVIZ_ID_STORAGE_UINT32 ? FVIZ_TRUE : FVIZ_FALSE;
    (void)memset(&face_table, 0, sizeof(face_table));
    if (out_surface == NULL || grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid and out_surface must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_surface = NULL;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    ghost_flags = fviz_unstructured_grid_ghost_cell_flags(grid);
    if (fviz_unstructured_grid_point_count(grid) > (FVizSize)UINT32_MAX + 1u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
            "surface extraction currently requires renderable UINT32 PolyData point IDs");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    /* Keep the linear TET/HEX/WEDGE/PYRAMID data-oriented path untouched.
     * Any high-order volume switches to the full-face generic table so midside
     * nodes participate in face ownership and boundary tessellation. */
    for (cell_id=0u;cell_id<fviz_cell_array_count(grid->cells);++cell_id)
        if (fviz_cell_is_first_order_volume(fviz_cell_array_type(grid->cells,cell_id))==FVIZ_FALSE)
            return fviz_extract_surface_generic_volume(grid,out_surface,with_scalars);
    /* Validate supported cells and compute an exact upper bound for face storage. */
    for (cell_id = 0u; cell_id < fviz_cell_array_count(grid->cells); ++cell_id)
    {
        FVizSurfaceDefinition definition;
        FVizSize next_count;
        const FVizCellType cell_type=fviz_cell_array_type(grid->cells, cell_id);
        if (!fviz_surface_definition(cell_type, &definition))
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "surface extraction requires a supported volume cell");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        if (cell_type!=FVIZ_CELL_HEXAHEDRON || fviz_cell_array_point_count(grid->cells,cell_id)!=8u) fast_hex32=FVIZ_FALSE;
        if (fviz_size_add(max_face_count, (FVizSize)definition.face_count, &next_count) != FVIZ_OK)
            return fviz_last_error_code();
        max_face_count = next_count;
    }
    if (fviz_surface_face_table_create(max_face_count, &face_table) != FVIZ_OK ||
        fviz_poly_data_create(&surface) != FVIZ_OK)
        goto fail;
    points = fviz_points_data(grid->points);

    /* Build ownership in one cache-friendly pass. Canonical sorted vertex IDs
     * make neighboring volume cells collide in O(1) expected time. A dedicated
     * homogeneous HEX8/UInt32 path removes CellView/traits overhead from the most
     * common large FEA solid-mesh workload. */
    if (fast_hex32 != FVIZ_FALSE)
    {
        static const uint8_t face_local[6][4] = {
            {0,3,2,1}, {4,5,6,7}, {0,1,5,4},
            {1,2,6,5}, {2,3,7,6}, {3,0,4,7}
        };
        const uint32_t* connectivity=(const uint32_t*)fviz_array_const_data(grid->cells->connectivity);
        const FVizSize* offsets=(const FVizSize*)fviz_array_const_data(grid->cells->offsets);
        for (cell_id=0u;cell_id<fviz_cell_array_count(grid->cells);++cell_id)
        {
            const uint32_t* cell=connectivity+offsets[cell_id];
            uint32_t face_id;
            for (face_id=0u;face_id<6u;++face_id)
            {
                uint32_t ids[4];
                ids[0]=cell[face_local[face_id][0]]; ids[1]=cell[face_local[face_id][1]];
                ids[2]=cell[face_local[face_id][2]]; ids[3]=cell[face_local[face_id][3]];
                if (fviz_surface_face_table_insert(&face_table,ids,4u,(FVizId)cell_id,(FVizId)face_id)!=FVIZ_OK)
                    goto fail;
            }
        }
    }
    else
    {
        for (cell_id = 0u; cell_id < fviz_cell_array_count(grid->cells); ++cell_id)
        {
            FVizSurfaceDefinition definition;
            FVizCellView view;
            uint32_t face_id;
            if (fviz_cell_array_cell_view(grid->cells, cell_id, &view) != FVIZ_OK) goto fail;
            (void)fviz_surface_definition(view.type, &definition);
            for (face_id = 0u; face_id < definition.face_count; ++face_id)
            {
                uint32_t ids[4] = {0u, 0u, 0u, 0u};
                uint32_t corner;
                const uint32_t count = definition.sizes[face_id];
                for (corner = 0u; corner < count; ++corner)
                {
                    const FVizId point_id = fviz_cell_view_point_id(&view, definition.faces[face_id][corner]);
                    if (point_id > (FVizId)UINT32_MAX)
                    {
                        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                            "surface extraction encountered a point ID that cannot be rendered with UINT32 PolyData");
                        goto fail;
                    }
                    ids[corner] = (uint32_t)point_id;
                }
                if (fviz_surface_face_table_insert(
                        &face_table, ids, count, (FVizId)cell_id, (FVizId)face_id) != FVIZ_OK)
                    goto fail;
            }
        }
    }

    {
        FVizSize boundary_triangle_count = 0u;
        FVizSize triangle_index_count = 0u;
        uint32_t* boundary_indices = NULL;
        FVizId* boundary_cell_ids = NULL;
        FVizId* boundary_face_ids = NULL;
        FVizSize output_triangle = 0u;
        for (i = 0u; i < face_table.face_count; ++i)
        {
            const FVizSurfaceFace* face = &face_table.faces[i];
            FVizSize added = 0u;
            if (face->occurrences != 1u ||
                fviz_unstructured_grid_cell_is_render_ghost(ghost_flags,(FVizSize)face->source_cell)!=FVIZ_FALSE) continue;
            if (face->count == 3u) added = 1u;
            else if (face->count == 4u) added = 2u;
            if (added > (FVizSize)-1 - boundary_triangle_count)
            {
                fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "surface triangle count overflow");
                goto fail;
            }
            boundary_triangle_count += added;
        }
        if (fviz_size_multiply(boundary_triangle_count, 3u, &triangle_index_count) != FVIZ_OK)
            goto fail;
        if (boundary_triangle_count != 0u)
        {
            FVizSize index_bytes = 0u;
            FVizSize provenance_bytes = 0u;
            if (fviz_size_multiply(triangle_index_count, sizeof(uint32_t), &index_bytes) != FVIZ_OK ||
                fviz_size_multiply(boundary_triangle_count, sizeof(FVizId), &provenance_bytes) != FVIZ_OK)
                goto fail;
            boundary_indices = (uint32_t*)fviz_alloc(index_bytes);
            boundary_cell_ids = (FVizId*)fviz_alloc(provenance_bytes);
            boundary_face_ids = (FVizId*)fviz_alloc(provenance_bytes);
            if (boundary_indices == NULL || boundary_cell_ids == NULL || boundary_face_ids == NULL)
            {
                fviz_free(boundary_indices);
                fviz_free(boundary_cell_ids);
                fviz_free(boundary_face_ids);
                goto fail;
            }
            for (i = 0u; i < face_table.face_count; ++i)
            {
                const FVizSurfaceFace* face = &face_table.faces[i];
                FVizSize base;
                if (face->occurrences != 1u ||
                    fviz_unstructured_grid_cell_is_render_ghost(ghost_flags,(FVizSize)face->source_cell)!=FVIZ_FALSE) continue;
                if (face->count == 3u)
                {
                    base = output_triangle * 3u;
                    boundary_indices[base] = face->ids[0];
                    boundary_indices[base + 1u] = face->ids[1];
                    boundary_indices[base + 2u] = face->ids[2];
                    boundary_cell_ids[output_triangle] = face->source_cell;
                    boundary_face_ids[output_triangle] = face->source_face;
                    ++output_triangle;
                }
                else if (face->count == 4u)
                {
                    base = output_triangle * 3u;
                    boundary_indices[base] = face->ids[0];
                    boundary_indices[base + 1u] = face->ids[1];
                    boundary_indices[base + 2u] = face->ids[2];
                    boundary_cell_ids[output_triangle] = face->source_cell;
                    boundary_face_ids[output_triangle] = face->source_face;
                    ++output_triangle;
                    base = output_triangle * 3u;
                    boundary_indices[base] = face->ids[0];
                    boundary_indices[base + 1u] = face->ids[2];
                    boundary_indices[base + 2u] = face->ids[3];
                    boundary_cell_ids[output_triangle] = face->source_cell;
                    boundary_face_ids[output_triangle] = face->source_face;
                    ++output_triangle;
                }
            }
        }
        if (output_triangle != boundary_triangle_count)
        {
            fviz_free(boundary_indices);
            fviz_free(boundary_cell_ids);
            fviz_free(boundary_face_ids);
            fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "surface boundary compaction count mismatch");
            goto fail;
        }
        if (fviz_poly_data_reserve(surface, fviz_points_count(grid->points), boundary_triangle_count) != FVIZ_OK ||
            fviz_poly_data_add_points(surface, points, fviz_points_count(grid->points), NULL) != FVIZ_OK ||
            (boundary_triangle_count != 0u &&
             fviz_poly_data_add_triangles(surface, boundary_indices, boundary_triangle_count) != FVIZ_OK) ||
            fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &original_cell_ids) != FVIZ_OK ||
            fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &original_face_ids) != FVIZ_OK ||
            (boundary_triangle_count != 0u &&
             fviz_data_array_append_tuples(original_cell_ids, boundary_cell_ids, boundary_triangle_count) != FVIZ_OK) ||
            (boundary_triangle_count != 0u &&
             fviz_data_array_append_tuples(original_face_ids, boundary_face_ids, boundary_triangle_count) != FVIZ_OK))
        {
            fviz_free(boundary_indices);
            fviz_free(boundary_cell_ids);
            fviz_free(boundary_face_ids);
            goto fail;
        }
        fviz_free(boundary_indices);
        fviz_free(boundary_cell_ids);
        fviz_free(boundary_face_ids);
    }
    if (fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &original_point_ids) != FVIZ_OK ||
        fviz_data_array_resize(original_point_ids, fviz_points_count(grid->points)) != FVIZ_OK)
        goto fail;
    {
        FVizId* source_ids = (FVizId*)fviz_data_array_data(original_point_ids);
        for (i = 0u; i < fviz_points_count(grid->points); ++i) source_ids[i] = (FVizId)i;
    }
    if (fviz_attribute_set_add(
            fviz_poly_data_point_data(surface), "FVizOriginalPointIds", original_point_ids) != FVIZ_OK)
        goto fail;
    if (fviz_attribute_set_add(
            fviz_poly_data_cell_data(surface), "FVizOriginalCellIds", original_cell_ids) != FVIZ_OK ||
        fviz_attribute_set_add(
            fviz_poly_data_cell_data(surface), "FVizOriginalFaceIds", original_face_ids) != FVIZ_OK)
        goto fail;
    if (with_scalars == FVIZ_TRUE && fviz_transfer_point_scalars(grid, surface) != FVIZ_OK) goto fail;
    fviz_release(original_cell_ids);
    fviz_release(original_face_ids);
    fviz_release(original_point_ids);
    fviz_surface_face_table_destroy(&face_table);
    *out_surface = surface;
    return FVIZ_OK;
fail:
    fviz_release(original_cell_ids);
    fviz_release(original_face_ids);
    fviz_release(original_point_ids);
    fviz_surface_face_table_destroy(&face_table);
    fviz_release(surface);
    return fviz_last_error_code();
}

FVizResult fviz_unstructured_grid_extract_surface(const FVizUnstructuredGrid* grid, FVizPolyData** out_surface)
{
    return fviz_extract_surface_internal(grid, out_surface, FVIZ_FALSE);
}

FVizResult fviz_unstructured_grid_extract_surface_scalars(const FVizUnstructuredGrid* grid, FVizPolyData** out_surface)
{
    return fviz_extract_surface_internal(grid, out_surface, FVIZ_TRUE);
}

static FVizResult fviz_geometry_append_provenance(
    FVizDataArray* cell_ids,FVizDataArray* face_ids,FVizId cell_id,FVizId face_id,FVizSize repeat)
{
    FVizSize i;
    for (i=0u;i<repeat;++i)
        if (fviz_data_array_append_tuple(cell_ids,&cell_id)!=FVIZ_OK ||
            fviz_data_array_append_tuple(face_ids,&face_id)!=FVIZ_OK)
            return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizResult fviz_geometry_emit_line(
    FVizPolyData* out,FVizDataArray* cell_ids,FVizDataArray* face_ids,
    uint32_t a,uint32_t b,FVizId source_cell)
{
    const FVizId none=UINT64_MAX;
    if (fviz_poly_data_add_line(out,a,b)!=FVIZ_OK) return fviz_last_error_code();
    return fviz_geometry_append_provenance(cell_ids,face_ids,source_cell,none,1u);
}

static FVizResult fviz_geometry_emit_triangle(
    FVizPolyData* out,FVizDataArray* cell_ids,FVizDataArray* face_ids,
    uint32_t a,uint32_t b,uint32_t c,FVizId source_cell,FVizId source_face)
{
    if (fviz_poly_data_add_triangle(out,a,b,c)!=FVIZ_OK) return fviz_last_error_code();
    return fviz_geometry_append_provenance(cell_ids,face_ids,source_cell,source_face,1u);
}

static FVizResult fviz_geometry_emit_surface_cell(
    FVizPolyData* out,FVizDataArray* cell_ids,FVizDataArray* face_ids,
    const FVizCellView* view,FVizId source_cell)
{
    FVizId ids64[32];
    uint32_t p[32];
    FVizSize i;
    const FVizId no_face=UINT64_MAX;
    if (view->point_count>32u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"geometry extraction surface cell exceeds supported point count");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    for (i=0u;i<view->point_count;++i)
    {
        ids64[i]=fviz_cell_view_point_id(view,i);
        if (ids64[i]>(FVizId)UINT32_MAX)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"geometry extraction point ID exceeds renderable UINT32 range");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        p[i]=(uint32_t)ids64[i];
    }
    switch (view->type)
    {
        case FVIZ_CELL_TRIANGLE:
            return fviz_geometry_emit_triangle(out,cell_ids,face_ids,p[0],p[1],p[2],source_cell,no_face);
        case FVIZ_CELL_QUAD:
            if (fviz_geometry_emit_triangle(out,cell_ids,face_ids,p[0],p[1],p[2],source_cell,no_face)!=FVIZ_OK) return fviz_last_error_code();
            return fviz_geometry_emit_triangle(out,cell_ids,face_ids,p[0],p[2],p[3],source_cell,no_face);
        case FVIZ_CELL_QUADRATIC_TRIANGLE:
        {
            static const uint8_t t[4][3]={{0,3,5},{3,1,4},{5,4,2},{3,4,5}};
            uint32_t k;
            for (k=0u;k<4u;++k)
                if (fviz_geometry_emit_triangle(out,cell_ids,face_ids,p[t[k][0]],p[t[k][1]],p[t[k][2]],source_cell,no_face)!=FVIZ_OK)
                    return fviz_last_error_code();
            return FVIZ_OK;
        }
        case FVIZ_CELL_QUADRATIC_QUAD:
        {
            const uint8_t ring[8]={0,4,1,5,2,6,3,7};
            uint32_t k;
            for (k=1u;k+1u<8u;++k)
                if (fviz_geometry_emit_triangle(out,cell_ids,face_ids,p[ring[0]],p[ring[k]],p[ring[k+1u]],source_cell,no_face)!=FVIZ_OK)
                    return fviz_last_error_code();
            return FVIZ_OK;
        }
        case FVIZ_CELL_BIQUADRATIC_QUAD:
        {
            const uint8_t ring[8]={0,4,1,5,2,6,3,7};
            uint32_t k;
            for (k=0u;k<8u;++k)
                if (fviz_geometry_emit_triangle(out,cell_ids,face_ids,p[8],p[ring[k]],p[ring[(k+1u)&7u]],source_cell,no_face)!=FVIZ_OK)
                    return fviz_last_error_code();
            return FVIZ_OK;
        }
        case FVIZ_CELL_POLYGON:
            if (view->point_count<3u) return FVIZ_OK;
            for (i=1u;i+1u<view->point_count;++i)
                if (fviz_geometry_emit_triangle(out,cell_ids,face_ids,p[0],p[i],p[i+1u],source_cell,no_face)!=FVIZ_OK)
                    return fviz_last_error_code();
            return FVIZ_OK;
        case FVIZ_CELL_TRIANGLE_STRIP:
            if (view->point_count<3u) return FVIZ_OK;
            for (i=0u;i+2u<view->point_count;++i)
            {
                const uint32_t a=(i&1u)==0u?p[i]:p[i+1u];
                const uint32_t b=(i&1u)==0u?p[i+1u]:p[i];
                if (fviz_geometry_emit_triangle(out,cell_ids,face_ids,a,b,p[i+2u],source_cell,no_face)!=FVIZ_OK)
                    return fviz_last_error_code();
            }
            return FVIZ_OK;
        default:
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"unsupported 2D cell in mixed geometry extraction");
            return FVIZ_ERROR_NOT_SUPPORTED;
    }
}

static FVizResult fviz_geometry_copy_point_and_field_data(const FVizUnstructuredGrid* grid,FVizPolyData* out)
{
    FVizAttributeSet* source=fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid);
    FVizAttributeSet* dest=fviz_poly_data_point_data(out);
    FVizSize i;
    FVizAttributeRole role;
    for (i=0u;i<fviz_attribute_set_count(source);++i)
    {
        const char* name=fviz_attribute_set_name_at(source,i);
        FVizDataArray* array=fviz_attribute_set_array_at(source,i);
        if (name!=NULL && array!=NULL && fviz_data_array_tuple_count(array)==fviz_unstructured_grid_point_count(grid))
            if (fviz_attribute_set_add(dest,name,array)!=FVIZ_OK) return fviz_last_error_code();
    }
    for (role=FVIZ_ATTRIBUTE_SCALARS;role<FVIZ_ATTRIBUTE_ROLE_COUNT;++role)
    {
        const char* active=fviz_attribute_set_active_name(source,role);
        if (active!=NULL && fviz_attribute_set_const_get(dest,active)!=NULL)
            (void)fviz_attribute_set_set_active(dest,role,active);
    }
    source=fviz_unstructured_grid_field_data((FVizUnstructuredGrid*)grid);
    dest=fviz_poly_data_field_data(out);
    for (i=0u;i<fviz_attribute_set_count(source);++i)
    {
        const char* name=fviz_attribute_set_name_at(source,i);
        FVizDataArray* array=fviz_attribute_set_array_at(source,i);
        if (name!=NULL && array!=NULL && fviz_attribute_set_add(dest,name,array)!=FVIZ_OK) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_geometry_copy_cell_data(
    const FVizUnstructuredGrid* grid,FVizPolyData* out,const FVizDataArray* provenance)
{
    FVizAttributeSet* source=fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid);
    FVizAttributeSet* dest=fviz_poly_data_cell_data(out);
    const FVizSize source_count=fviz_unstructured_grid_cell_count(grid);
    FVizSize a,t;
    for (a=0u;a<fviz_attribute_set_count(source);++a)
    {
        const char* name=fviz_attribute_set_name_at(source,a);
        const FVizDataArray* src=fviz_attribute_set_const_array_at(source,a);
        FVizDataArray* dst=NULL;
        FVizAttributeRole role;
        if (name==NULL || src==NULL || strcmp(name,"FVizOriginalCellIds")==0 || fviz_data_array_tuple_count(src)!=source_count) continue;
        if (fviz_data_array_create(fviz_data_array_type(src),fviz_data_array_components(src),&dst)!=FVIZ_OK ||
            fviz_data_array_reserve(dst,fviz_data_array_tuple_count(provenance))!=FVIZ_OK) goto field_fail;
        for (t=0u;t<fviz_data_array_tuple_count(provenance);++t)
        {
            double id_value=0.0;
            FVizSize id;
            if (fviz_data_array_get_component(provenance,t,0u,&id_value)!=FVIZ_OK) goto field_fail;
            id=(FVizSize)id_value;
            if (id>=source_count || fviz_data_array_append_tuple(dst,fviz_data_array_const_tuple(src,id))!=FVIZ_OK) goto field_fail;
        }
        if (fviz_attribute_set_add(dest,name,dst)!=FVIZ_OK) goto field_fail;
        for (role=FVIZ_ATTRIBUTE_SCALARS;role<FVIZ_ATTRIBUTE_ROLE_COUNT;++role)
        {
            const char* active=fviz_attribute_set_active_name(source,role);
            if (active!=NULL && strcmp(active,name)==0) (void)fviz_attribute_set_set_active(dest,role,name);
        }
        fviz_release(dst);
        continue;
field_fail:
        fviz_release(dst);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

FVizResult fviz_unstructured_grid_extract_geometry(const FVizUnstructuredGrid* grid,FVizPolyData** out_geometry)
{
    FVizPolyData* out=NULL;
    FVizDataArray* cell_ids=NULL;
    FVizDataArray* face_ids=NULL;
    FVizDataArray* original_point_ids=NULL;
    FVizHighOrderSurfaceTable volume_faces;
    FVizSize cell_id,i,max_faces=0u;
    FVizBool has_volume=FVIZ_FALSE;
    const FVizVec3* points;
    const uint8_t* ghost_flags;
    (void)memset(&volume_faces,0,sizeof(volume_faces));
    if (grid==NULL || out_geometry==NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"mixed geometry extraction requires grid and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_geometry=NULL;
    if (fviz_unstructured_grid_validate(grid)!=FVIZ_OK) return fviz_last_error_code();
    ghost_flags=fviz_unstructured_grid_ghost_cell_flags(grid);
    if (fviz_unstructured_grid_point_count(grid)>(FVizSize)UINT32_MAX+1u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"mixed geometry extraction requires renderable UINT32 point IDs");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    for (cell_id=0u;cell_id<fviz_unstructured_grid_cell_count(grid);++cell_id)
    {
        const FVizCellTypeTraits traits=fviz_cell_type_traits(fviz_cell_array_type(grid->cells,cell_id));
        if (fviz_cell_type_is_supported(fviz_cell_array_type(grid->cells,cell_id))==FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"mixed geometry extraction encountered an unsupported cell type");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        if (traits.dimension==3u)
        {
            FVizSize next;
            has_volume=FVIZ_TRUE;
            if (fviz_size_add(max_faces,(FVizSize)traits.face_count,&next)!=FVIZ_OK) return fviz_last_error_code();
            max_faces=next;
        }
    }
    if (has_volume!=FVIZ_FALSE && fviz_high_order_surface_table_create(max_faces,&volume_faces)!=FVIZ_OK) goto fail;
    if (fviz_poly_data_create(&out)!=FVIZ_OK ||
        fviz_poly_data_reserve(out,fviz_unstructured_grid_point_count(grid),0u)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&cell_ids)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&face_ids)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&original_point_ids)!=FVIZ_OK ||
        fviz_data_array_resize(original_point_ids,fviz_unstructured_grid_point_count(grid))!=FVIZ_OK) goto fail;
    points=fviz_points_data(grid->points);
    if (fviz_poly_data_add_points(out,points,fviz_unstructured_grid_point_count(grid),NULL)!=FVIZ_OK) goto fail;
    {
        FVizId* ids=(FVizId*)fviz_data_array_data(original_point_ids);
        for (i=0u;i<fviz_unstructured_grid_point_count(grid);++i) ids[i]=(FVizId)i;
    }
    if (fviz_attribute_set_add(fviz_poly_data_point_data(out),"FVizOriginalPointIds",original_point_ids)!=FVIZ_OK ||
        fviz_geometry_copy_point_and_field_data(grid,out)!=FVIZ_OK) goto fail;

    /* 0D first: PolyData cell-data order is verts, then lines, then polys. */
    for (cell_id=0u;cell_id<fviz_unstructured_grid_cell_count(grid);++cell_id)
    {
        FVizCellView view;
        FVizSize j;
        const FVizId none=UINT64_MAX;
        if (fviz_cell_array_cell_view(grid->cells,cell_id,&view)!=FVIZ_OK) goto fail;
        if (fviz_unstructured_grid_cell_is_render_ghost(ghost_flags,cell_id)!=FVIZ_FALSE) continue;
        if (fviz_cell_type_traits(view.type).dimension!=0u) continue;
        for (j=0u;j<view.point_count;++j)
        {
            const FVizId id=fviz_cell_view_point_id(&view,j);
            if (id>(FVizId)UINT32_MAX || fviz_poly_data_add_vertex(out,(uint32_t)id)!=FVIZ_OK ||
                fviz_geometry_append_provenance(cell_ids,face_ids,(FVizId)cell_id,none,1u)!=FVIZ_OK) goto fail;
        }
    }
    /* 1D cells next. */
    for (cell_id=0u;cell_id<fviz_unstructured_grid_cell_count(grid);++cell_id)
    {
        FVizCellView view;
        FVizSize j;
        if (fviz_cell_array_cell_view(grid->cells,cell_id,&view)!=FVIZ_OK) goto fail;
        if (fviz_unstructured_grid_cell_is_render_ghost(ghost_flags,cell_id)!=FVIZ_FALSE) continue;
        if (fviz_cell_type_traits(view.type).dimension!=1u) continue;
        if (view.type==FVIZ_CELL_QUADRATIC_EDGE && view.point_count==3u)
        {
            const FVizId a=fviz_cell_view_point_id(&view,0u),b=fviz_cell_view_point_id(&view,1u),m=fviz_cell_view_point_id(&view,2u);
            if (a>UINT32_MAX || b>UINT32_MAX || m>UINT32_MAX ||
                fviz_geometry_emit_line(out,cell_ids,face_ids,(uint32_t)a,(uint32_t)m,(FVizId)cell_id)!=FVIZ_OK ||
                fviz_geometry_emit_line(out,cell_ids,face_ids,(uint32_t)m,(uint32_t)b,(FVizId)cell_id)!=FVIZ_OK) goto fail;
        }
        else
        {
            for (j=0u;j+1u<view.point_count;++j)
            {
                const FVizId a=fviz_cell_view_point_id(&view,j),b=fviz_cell_view_point_id(&view,j+1u);
                if (a>UINT32_MAX || b>UINT32_MAX ||
                    fviz_geometry_emit_line(out,cell_ids,face_ids,(uint32_t)a,(uint32_t)b,(FVizId)cell_id)!=FVIZ_OK) goto fail;
            }
        }
    }
    /* 2D shell/membrane cells. */
    for (cell_id=0u;cell_id<fviz_unstructured_grid_cell_count(grid);++cell_id)
    {
        FVizCellView view;
        if (fviz_cell_array_cell_view(grid->cells,cell_id,&view)!=FVIZ_OK) goto fail;
        if (fviz_unstructured_grid_cell_is_render_ghost(ghost_flags,cell_id)!=FVIZ_FALSE) continue;
        if (fviz_cell_type_traits(view.type).dimension==2u &&
            fviz_geometry_emit_surface_cell(out,cell_ids,face_ids,&view,(FVizId)cell_id)!=FVIZ_OK) goto fail;
    }
    /* Build volume face ownership after lower-dimensional primitives so the
     * provenance arrays stay aligned with PolyData's grouped cell order. */
    if (has_volume!=FVIZ_FALSE)
    {
        for (cell_id=0u;cell_id<fviz_unstructured_grid_cell_count(grid);++cell_id)
        {
            FVizCellView view;
            const FVizCellTypeTraits traits=fviz_cell_type_traits(fviz_cell_array_type(grid->cells,cell_id));
            uint32_t face;
            if (traits.dimension!=3u) continue;
            if (fviz_cell_array_cell_view(grid->cells,cell_id,&view)!=FVIZ_OK) goto fail;
            for (face=0u;face<traits.face_count;++face)
            {
                uint32_t local[9],ids[9],count=0u,j;
                if (fviz_cell_type_face(view.type,face,local,9u,&count)!=FVIZ_OK) goto fail;
                for (j=0u;j<count;++j)
                {
                    const FVizId id=fviz_cell_view_point_id(&view,(FVizSize)local[j]);
                    if (id>UINT32_MAX) { fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"volume boundary point ID exceeds UINT32 range"); goto fail; }
                    ids[j]=(uint32_t)id;
                }
                if (fviz_high_order_surface_table_insert(&volume_faces,ids,count,(FVizId)cell_id,(FVizId)face)!=FVIZ_OK) goto fail;
            }
        }
        for (i=0u;i<volume_faces.face_count;++i)
        {
            const FVizHighOrderSurfaceFace* f=&volume_faces.faces[i];
            FVizSize before;
            if (f->occurrences!=1u ||
                fviz_unstructured_grid_cell_is_render_ghost(ghost_flags,(FVizSize)f->source_cell)!=FVIZ_FALSE) continue;
            before=fviz_poly_data_triangle_count(out);
            if (fviz_tessellate_high_order_face(out,cell_ids,face_ids,f)!=FVIZ_OK) goto fail;
            (void)before;
        }
    }
    if (fviz_attribute_set_add(fviz_poly_data_cell_data(out),"FVizOriginalCellIds",cell_ids)!=FVIZ_OK ||
        fviz_attribute_set_add(fviz_poly_data_cell_data(out),"FVizOriginalFaceIds",face_ids)!=FVIZ_OK ||
        fviz_geometry_copy_cell_data(grid,out,cell_ids)!=FVIZ_OK) goto fail;
    /* Preserve transitive provenance when the input itself was already derived. */
    {
        const FVizDataArray* upstream=fviz_attribute_set_const_get(fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid),"FVizOriginalCellIds");
        if (upstream!=NULL && fviz_data_array_tuple_count(upstream)==fviz_unstructured_grid_cell_count(grid))
        {
            FVizDataArray* remapped=NULL;
            if (fviz_data_array_create(fviz_data_array_type(upstream),fviz_data_array_components(upstream),&remapped)!=FVIZ_OK ||
                fviz_data_array_reserve(remapped,fviz_data_array_tuple_count(cell_ids))!=FVIZ_OK) { fviz_release(remapped); goto fail; }
            for (i=0u;i<fviz_data_array_tuple_count(cell_ids);++i)
            {
                double id=0.0;
                if (fviz_data_array_get_component(cell_ids,i,0u,&id)!=FVIZ_OK ||
                    (FVizSize)id>=fviz_data_array_tuple_count(upstream) ||
                    fviz_data_array_append_tuple(remapped,fviz_data_array_const_tuple(upstream,(FVizSize)id))!=FVIZ_OK)
                { fviz_release(remapped); goto fail; }
            }
            if (fviz_attribute_set_add(fviz_poly_data_cell_data(out),"FVizOriginalCellIds",remapped)!=FVIZ_OK) { fviz_release(remapped); goto fail; }
            fviz_release(remapped);
        }
    }
    if (fviz_poly_data_triangle_count(out)!=0u && fviz_poly_data_compute_normals(out)!=FVIZ_OK) goto fail;
    fviz_release(cell_ids); fviz_release(face_ids); fviz_release(original_point_ids); fviz_high_order_surface_table_destroy(&volume_faces);
    *out_geometry=out;
    return FVIZ_OK;
fail:
    fviz_release(cell_ids); fviz_release(face_ids); fviz_release(original_point_ids); fviz_high_order_surface_table_destroy(&volume_faces); fviz_release(out);
    return fviz_last_error_code();
}

#define FVIZ_SLICE_EPSILON 1.0e-6f
#define FVIZ_SLICE_MAX_VERTICES 16u

typedef struct FVizSliceField
{
    const FVizDataArray* array;
    char name[128];
} FVizSliceField;

static const uint32_t g_fviz_edges_tetra[6][2] = {
    {0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}
};
static const uint32_t g_fviz_edges_hex[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};
static const uint32_t g_fviz_edges_wedge[9][2] = {
    {0, 1}, {1, 2}, {2, 0},
    {3, 4}, {4, 5}, {5, 3},
    {0, 3}, {1, 4}, {2, 5}
};
static const uint32_t g_fviz_edges_pyramid[8][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {0, 4}, {1, 4}, {2, 4}, {3, 4}
};

static const uint32_t* fviz_cell_edge_table(FVizCellType type, uint32_t* out_edge_count)
{
    switch (type)
    {
        case FVIZ_CELL_TETRA: *out_edge_count = 6u; return &g_fviz_edges_tetra[0][0];
        case FVIZ_CELL_HEXAHEDRON: *out_edge_count = 12u; return &g_fviz_edges_hex[0][0];
        case FVIZ_CELL_WEDGE: *out_edge_count = 9u; return &g_fviz_edges_wedge[0][0];
        case FVIZ_CELL_PYRAMID: *out_edge_count = 8u; return &g_fviz_edges_pyramid[0][0];
        default: *out_edge_count = 0u; return NULL;
    }
}

static void fviz_slice_sort_angles(const float* angles, FVizSize* order, FVizSize count)
{
    FVizSize i;
    for (i = 1u; i < count; ++i)
    {
        const float angle = angles[order[i]];
        const FVizSize value = order[i];
        FVizSize j = i;
        while (j > 0u && angles[order[j - 1u]] > angle)
        {
            order[j] = order[j - 1u];
            --j;
        }
        order[j] = value;
    }
}

typedef struct FVizSliceClassificationContext
{
    const FVizUnstructuredGrid* grid;
    const uint8_t* ghost_flags;
    FVizPlane plane;
    uint8_t* intersects;
} FVizSliceClassificationContext;

static void fviz_slice_classify_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizSliceClassificationContext* context = (FVizSliceClassificationContext*)user_data;
    FVizSize cell_id;
    for (cell_id = begin; cell_id < end; ++cell_id)
    {
        const FVizCellType type = fviz_cell_array_type(context->grid->cells, cell_id);
        if (fviz_unstructured_grid_cell_is_render_ghost(context->ghost_flags, cell_id) != FVIZ_FALSE)
            continue;
        FVizCellView view;
        const FVizSize count = fviz_cell_array_point_count(context->grid->cells, cell_id);
        uint32_t edge_count;
        const uint32_t* edges = fviz_cell_edge_table(type, &edge_count);
        FVizBool positive = FVIZ_FALSE;
        FVizBool negative = FVIZ_FALSE;
        FVizSize i;
        if (edges == NULL || count > 8u ||
            fviz_cell_array_cell_view(context->grid->cells, cell_id, &view) != FVIZ_OK) continue;
        for (i = 0u; i < count; ++i)
        {
            const FVizId point_id = fviz_cell_view_point_id(&view, i);
            const float distance = fviz_plane_distance_to_point(
                context->plane, fviz_points_data(context->grid->points)[(FVizSize)point_id]);
            if (distance >= -FVIZ_SLICE_EPSILON) positive = FVIZ_TRUE;
            if (distance <= FVIZ_SLICE_EPSILON) negative = FVIZ_TRUE;
        }
        context->intersects[cell_id] = positive != FVIZ_FALSE && negative != FVIZ_FALSE
            ? FVIZ_TRUE : FVIZ_FALSE;
    }
}

FVizResult fviz_unstructured_grid_slice(
    const FVizUnstructuredGrid* grid,
    FVizPlane plane,
    FVizPolyData** out_slice)
{
    FVizArray* fields = NULL;
    FVizDataArray** outputs = NULL;
    FVizPolyData* slice = NULL;
    const FVizVec3* points;
    FVizVec3 u_basis;
    FVizVec3 v_basis;
    FVizSize field_count = 0u;
    float* polygon_values = NULL;
    FVizSize cell_id;
    FVizSize field_id;
    uint8_t* intersects = NULL;
    FVizSliceClassificationContext classification;

    if (grid == NULL || out_slice == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid and out_slice must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_slice = NULL;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_poly_data_create(&slice) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizSliceField), &fields) != FVIZ_OK) goto fail;
    points = fviz_points_data(grid->points);
    if (fviz_cell_array_count(grid->cells) > 0u)
    {
        intersects = (uint8_t*)fviz_alloc(fviz_cell_array_count(grid->cells));
        if (intersects == NULL) goto fail;
        (void)memset(intersects, 0, fviz_cell_array_count(grid->cells));
        classification.grid = grid;
        classification.ghost_flags = fviz_unstructured_grid_ghost_cell_flags(grid);
        classification.plane = plane;
        classification.intersects = intersects;
        if (fviz_parallel_for(0u, fviz_cell_array_count(grid->cells), 64u,
                fviz_slice_classify_range, &classification) != FVIZ_OK)
            goto fail;
    }

    {
        FVizAttributeSet* point_data = fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid);
        for (field_id = 0u; field_id < fviz_attribute_set_count(point_data); ++field_id)
        {
            const FVizDataArray* array = fviz_attribute_set_const_array_at(point_data, field_id);
            const char* name = fviz_attribute_set_name_at(point_data, field_id);
            FVizSliceField field;
            if (array == NULL || name == NULL || fviz_data_array_components(array) != 1u ||
                fviz_data_array_tuple_count(array) != fviz_points_count(grid->points))
            {
                continue;
            }
            field.array = array;
            (void)strncpy(field.name, name, sizeof(field.name) - 1u);
            field.name[sizeof(field.name) - 1u] = '\0';
            if (fviz_array_push(fields, &field) != FVIZ_OK) goto fail;
        }
    }
    field_count = fviz_array_count(fields);
    if (field_count > 0u)
    {
        outputs = (FVizDataArray**)fviz_alloc(field_count * sizeof(FVizDataArray*));
        if (outputs == NULL) goto fail;
        for (field_id = 0u; field_id < field_count; ++field_id)
        {
            outputs[field_id] = NULL;
            if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &outputs[field_id]) != FVIZ_OK) goto fail;
        }
    }

    {
        FVizVec3 up = fviz_vec3(0.0f, 1.0f, 0.0f);
        if (fabsf(plane.normal.y) > 0.9f) up = fviz_vec3(1.0f, 0.0f, 0.0f);
        u_basis = fviz_vec3_normalize(fviz_vec3_cross(plane.normal, up));
        v_basis = fviz_vec3_cross(plane.normal, u_basis);
    }

    for (cell_id = 0u; cell_id < fviz_cell_array_count(grid->cells); ++cell_id)
    {
        const FVizCellType type = fviz_cell_array_type(grid->cells, cell_id);
        FVizCellView view;
        FVizId cell_ids[8];
        const FVizSize cell_point_count = fviz_cell_array_point_count(grid->cells, cell_id);
        uint32_t edge_count;
        const uint32_t* edges = fviz_cell_edge_table(type, &edge_count);
        float distances[8];
        FVizVec3 polygon_positions[FVIZ_SLICE_MAX_VERTICES];
        FVizSize polygon_count = 0u;
        FVizSize k;
        if (intersects != NULL && intersects[cell_id] == FVIZ_FALSE) continue;
        if (edges == NULL || cell_point_count > 8u ||
            fviz_cell_array_cell_view(grid->cells, cell_id, &view) != FVIZ_OK) continue;
        for (k = 0u; k < cell_point_count; ++k) cell_ids[k] = fviz_cell_view_point_id(&view, k);

        for (k = 0u; k < cell_point_count; ++k)
        {
            distances[k] = fviz_plane_distance_to_point(plane, points[(FVizSize)cell_ids[k]]);
        }
        {
            FVizBool has_nonnegative = FVIZ_FALSE;
            FVizBool has_nonpositive = FVIZ_FALSE;
            FVizBool all_on_plane = FVIZ_TRUE;
            for (k = 0u; k < cell_point_count; ++k)
            {
                if (distances[k] >= -FVIZ_SLICE_EPSILON) has_nonnegative = FVIZ_TRUE;
                if (distances[k] <= FVIZ_SLICE_EPSILON) has_nonpositive = FVIZ_TRUE;
                if (distances[k] > FVIZ_SLICE_EPSILON || distances[k] < -FVIZ_SLICE_EPSILON)
                {
                    all_on_plane = FVIZ_FALSE;
                }
            }
            if (all_on_plane == FVIZ_TRUE || has_nonnegative == FVIZ_FALSE || has_nonpositive == FVIZ_FALSE)
            {
                continue;
            }
        }

        if (field_count > 0u)
        {
            polygon_values = (float*)fviz_alloc(FVIZ_SLICE_MAX_VERTICES * field_count * sizeof(float));
            if (polygon_values == NULL) goto fail;
        }

        for (k = 0u; k < cell_point_count; ++k)
        {
            if (fabsf(distances[k]) <= FVIZ_SLICE_EPSILON)
            {
                FVizSize f;
                if (polygon_count >= FVIZ_SLICE_MAX_VERTICES) goto free_values_fail;
                polygon_positions[polygon_count] = points[(FVizSize)cell_ids[k]];
                for (f = 0u; f < field_count; ++f)
                {
                    const FVizSliceField* field = (const FVizSliceField*)fviz_array_const_at(fields, f);
                    double value = 0.0;
                    (void)fviz_scalar_value(field->array, (FVizSize)cell_ids[k], &value);
                    polygon_values[polygon_count * field_count + f] = (float)value;
                }
                ++polygon_count;
            }
        }
        for (k = 0u; k < edge_count; ++k)
        {
            const FVizId a = cell_ids[edges[k * 2u + 0u]];
            const FVizId b = cell_ids[edges[k * 2u + 1u]];
            const float da = distances[edges[k * 2u + 0u]];
            const float db = distances[edges[k * 2u + 1u]];
            const float t = da / (da - db);
            FVizSize f;
            if ((da >= 0.0f && db >= 0.0f) || (da <= 0.0f && db <= 0.0f)) continue;
            if (polygon_count >= FVIZ_SLICE_MAX_VERTICES) goto free_values_fail;
            polygon_positions[polygon_count] = fviz_vec3_add(points[(FVizSize)a], fviz_vec3_scale(fviz_vec3_sub(points[(FVizSize)b], points[(FVizSize)a]), t));
            for (f = 0u; f < field_count; ++f)
            {
                const FVizSliceField* field = (const FVizSliceField*)fviz_array_const_at(fields, f);
                double va = 0.0;
                double vb = 0.0;
                (void)fviz_scalar_value(field->array, (FVizSize)a, &va);
                (void)fviz_scalar_value(field->array, (FVizSize)b, &vb);
                polygon_values[polygon_count * field_count + f] = (float)(va + (vb - va) * t);
            }
            ++polygon_count;
        }

        if (polygon_count >= 3u)
        {
            FVizVec3 centroid = fviz_vec3(0.0f, 0.0f, 0.0f);
            float angles[FVIZ_SLICE_MAX_VERTICES];
            FVizSize order[FVIZ_SLICE_MAX_VERTICES];
            uint32_t new_ids[FVIZ_SLICE_MAX_VERTICES];
            FVizSize i;
            for (i = 0u; i < polygon_count; ++i)
            {
                centroid = fviz_vec3_add(centroid, polygon_positions[i]);
            }
            centroid = fviz_vec3_scale(centroid, 1.0f / (float)polygon_count);
            for (i = 0u; i < polygon_count; ++i)
            {
                const FVizVec3 rel = fviz_vec3_sub(polygon_positions[i], centroid);
                angles[i] = atan2f(fviz_vec3_dot(v_basis, rel), fviz_vec3_dot(u_basis, rel));
                order[i] = i;
            }
            fviz_slice_sort_angles(angles, order, polygon_count);
            for (i = 0u; i < polygon_count; ++i)
            {
                if (fviz_poly_data_add_point(slice, polygon_positions[order[i]], &new_ids[i]) != FVIZ_OK)
                {
                    goto free_values_fail;
                }
            }
            for (i = 1u; i + 1u < polygon_count; ++i)
            {
                if (fviz_poly_data_add_triangle(slice, new_ids[0], new_ids[i], new_ids[i + 1u]) != FVIZ_OK)
                {
                    goto free_values_fail;
                }
            }
            for (field_id = 0u; field_id < field_count; ++field_id)
            {
                FVizSize v;
                for (v = 0u; v < polygon_count; ++v)
                {
                    const float value = polygon_values[order[v] * field_count + field_id];
                    if (fviz_data_array_append_tuple(outputs[field_id], &value) != FVIZ_OK) goto free_values_fail;
                }
            }
        }
        if (polygon_values != NULL)
        {
            fviz_free(polygon_values);
            polygon_values = NULL;
        }
    }

    for (field_id = 0u; field_id < field_count; ++field_id)
    {
        const FVizSliceField* field = (const FVizSliceField*)fviz_array_const_at(fields, field_id);
        if (fviz_attribute_set_add(fviz_poly_data_point_data(slice), field->name, outputs[field_id]) != FVIZ_OK)
        {
            goto fail;
        }
    }
    if (field_count > 0u)
    {
        const FVizSliceField* first = (const FVizSliceField*)fviz_array_const_at(fields, 0u);
        FVizDataArray* active = fviz_attribute_set_get(fviz_poly_data_point_data(slice), first->name);
        if (fviz_poly_data_set_scalars(slice, active) != FVIZ_OK) goto fail;
    }

    if (fviz_poly_data_compute_normals(slice) != FVIZ_OK) goto fail;
    fviz_release(fields);
    fviz_free(intersects);
    for (field_id = 0u; field_id < field_count; ++field_id)
    {
        fviz_release(outputs[field_id]);
    }
    fviz_free(outputs);
    *out_slice = slice;
    return FVIZ_OK;

free_values_fail:
    if (polygon_values != NULL)
    {
        fviz_free(polygon_values);
        polygon_values = NULL;
    }
fail:
    fviz_free(intersects);
    if (outputs != NULL)
    {
        for (field_id = 0u; field_id < field_count; ++field_id)
        {
            fviz_release(outputs[field_id]);
        }
        fviz_free(outputs);
    }
    fviz_release(fields);
    fviz_release(slice);
    return fviz_last_error_code();
}

/* Point gradient via the VTK vtkGradientFilter Green-Gauss method:
 * 1. For each cell, fit the linear field over the cell's own points (least
 *    squares) and record the cell gradient.
 * 2. Each point gradient is the (unweighted) average of its incident cells'
 *    gradients.
 * This is exact for linear fields on affine cells, including at boundary
 * points where a direct per-point LSQ is underdetermined. */
FVizResult fviz_unstructured_grid_gradient(
    const FVizUnstructuredGrid* grid,
    const char* scalar_array_name,
    const char* output_name,
    FVizUnstructuredGrid** out_grid)
{
    const FVizDataArray* scalars;
    const FVizVec3* points;
    FVizCellLinks* links = NULL;
    FVizUnstructuredGrid* result = NULL;
    FVizDataArray* gradient = NULL;
    FVizDataArray* cell_gradients = NULL;
    FVizSize point_count;
    FVizSize cell_count;
    FVizSize components;
    FVizSize i;
    if (out_grid != NULL) *out_grid = NULL;
    if (grid == NULL || scalar_array_name == NULL || scalar_array_name[0] == '\0' ||
        output_name == NULL || output_name[0] == '\0' || out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "gradient requires a grid, input array name, and output name");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    scalars = fviz_attribute_set_const_get(
        fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid), scalar_array_name);
    if (scalars == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND,
            "gradient input array was not found on point data");
        return FVIZ_ERROR_NOT_FOUND;
    }
    point_count = fviz_unstructured_grid_point_count(grid);
    cell_count = fviz_unstructured_grid_cell_count(grid);
    components = fviz_data_array_components(scalars);
    if (components == 0u || components > 3u || fviz_data_array_tuple_count(scalars) != point_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
            "gradient input array must have one tuple per point with 1-3 components");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_cell_links_build(grid->cells, point_count, &links) != FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, (uint32_t)(3u * components), &gradient) != FVIZ_OK ||
        fviz_data_array_resize(gradient, point_count) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT64, (uint32_t)(3u * components), &cell_gradients) != FVIZ_OK ||
        fviz_data_array_resize(cell_gradients, cell_count) != FVIZ_OK)
        goto fail;
    points = fviz_points_data(grid->points);

    /* Pass 1: per-cell gradients. For a linear (affine) cell the least-squares
     * fit over the cell's own points reproduces the true gradient. */
    for (i = 0u; i < cell_count; ++i)
    {
        FVizCellView view;
        FVizSize point_count_in_cell;
        FVizSize s;
        FVizSize component;
        double basis[64 * 4];
        double values[64 * 3];
        double normal[4 * 4];
        double rhs[4 * 3];
        double a[16];
        double b[12];
        double coefficients[4 * 3];
        FVizSize row, col, r, c, rr;
        double grad_tuple[9];
        if (fviz_cell_array_cell_view(grid->cells, i, &view) != FVIZ_OK) continue;
        point_count_in_cell = view.point_count;
        if (point_count_in_cell == 0u || point_count_in_cell > 64u) continue;
        for (s = 0u; s < point_count_in_cell; ++s)
        {
            const FVizId pid = fviz_cell_view_point_id(&view, s);
            basis[s * 4u + 0u] = 1.0;
            basis[s * 4u + 1u] = (double)points[(FVizSize)pid].x;
            basis[s * 4u + 2u] = (double)points[(FVizSize)pid].y;
            basis[s * 4u + 3u] = (double)points[(FVizSize)pid].z;
            for (component = 0u; component < components; ++component)
            {
                double value = 0.0;
                (void)fviz_data_array_get_component(scalars, (FVizSize)pid, (uint32_t)component, &value);
                values[s * components + component] = value;
            }
        }
        for (row = 0u; row < 4u; ++row)
            for (col = 0u; col < 4u; ++col)
            {
                double sum = 0.0;
                for (s = 0u; s < point_count_in_cell; ++s) sum += basis[s * 4u + row] * basis[s * 4u + col];
                normal[row * 4u + col] = sum;
            }
        for (row = 0u; row < 4u; ++row)
            for (component = 0u; component < components; ++component)
            {
                double sum = 0.0;
                for (s = 0u; s < point_count_in_cell; ++s)
                    sum += basis[s * 4u + row] * values[s * components + component];
                rhs[row * components + component] = sum;
            }
        (void)memcpy(a, normal, sizeof(a));
        (void)memset(b, 0, sizeof(b));
        (void)memset(coefficients, 0, sizeof(coefficients));
        for (r = 0u; r < 4u; ++r)
            for (c = 0u; c < components; ++c)
                b[r * components + c] = rhs[r * components + c];
        for (r = 0u; r < 4u; ++r)
        {
            FVizSize pivot = r;
            double pivot_value;
            for (rr = r + 1u; rr < 4u; ++rr)
                if (fabs(a[rr * 4u + r]) > fabs(a[pivot * 4u + r])) pivot = rr;
            if (pivot != r)
            {
                for (c = 0u; c < 4u; ++c)
                {
                    const double t = a[r * 4u + c]; a[r * 4u + c] = a[pivot * 4u + c]; a[pivot * 4u + c] = t;
                }
                for (c = 0u; c < components; ++c)
                {
                    const double t = b[r * components + c]; b[r * components + c] = b[pivot * components + c]; b[pivot * components + c] = t;
                }
            }
            pivot_value = a[r * 4u + r];
            if (fabs(pivot_value) < 1.0e-12) continue;
            for (c = r; c < 4u; ++c) a[r * 4u + c] /= pivot_value;
            for (c = 0u; c < components; ++c) b[r * components + c] /= pivot_value;
            for (rr = r + 1u; rr < 4u; ++rr)
            {
                const double factor = a[rr * 4u + r];
                if (factor == 0.0) continue;
                for (c = r; c < 4u; ++c) a[rr * 4u + c] -= factor * a[r * 4u + c];
                for (c = 0u; c < components; ++c) b[rr * components + c] -= factor * b[r * components + c];
            }
        }
        for (r = 4u; r-- > 0u;)
            for (c = 0u; c < components; ++c)
            {
                double sum = b[r * components + c];
                for (rr = r + 1u; rr < 4u; ++rr) sum -= a[r * 4u + rr] * coefficients[rr * components + c];
                coefficients[r * components + c] = sum;
            }
        for (component = 0u; component < components; ++component)
        {
            grad_tuple[component * 3u + 0u] = coefficients[1 * components + component];
            grad_tuple[component * 3u + 1u] = coefficients[2 * components + component];
            grad_tuple[component * 3u + 2u] = coefficients[3 * components + component];
        }
        if (fviz_data_array_set_tuple(cell_gradients, i, grad_tuple) != FVIZ_OK) goto fail;
    }

    /* Pass 2: average incident cell gradients at each point. */
    for (i = 0u; i < point_count; ++i)
    {
        FVizSize count_for_point = 0u;
        const FVizId* incident_cells = fviz_cell_links_cells_for_point(links, i, &count_for_point);
        double sum[9];
        double grad_tuple[9];
        FVizSize component;
        FVizSize c;
        for (component = 0u; component < 3u * components; ++component) sum[component] = 0.0;
        for (c = 0u; c < count_for_point; ++c)
        {
            FVizSize k;
            for (k = 0u; k < 3u * components; ++k)
            {
                double value = 0.0;
                if (fviz_data_array_get_component(cell_gradients, (FVizSize)incident_cells[c], (uint32_t)k, &value) == FVIZ_OK)
                    sum[k] += value;
            }
        }
        for (component = 0u; component < 3u * components; ++component)
            grad_tuple[component] = count_for_point != 0u ? sum[component] / (double)count_for_point : 0.0;
        if (fviz_data_array_set_tuple(gradient, i, grad_tuple) != FVIZ_OK) goto fail;
    }

    /* Build the shallow copy result and attach the gradient. */
    if (fviz_unstructured_grid_shallow_copy(grid, &result) != FVIZ_OK) goto fail;
    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(result), output_name, gradient) != FVIZ_OK) goto fail;
    fviz_release(cell_gradients);
    fviz_release(gradient);
    fviz_release(links);
    *out_grid = result;
    return FVIZ_OK;
fail:
    fviz_release(cell_gradients);
    fviz_release(gradient);
    fviz_release(links);
    fviz_release(result);
    return fviz_last_error_code();
}

FVizResult fviz_unstructured_grid_cell_derivatives(
    const FVizUnstructuredGrid* grid,
    const char* scalar_array_name,
    const char* output_name,
    FVizUnstructuredGrid** out_grid)
{
    const FVizDataArray* scalars;
    const FVizVec3* points;
    FVizUnstructuredGrid* result = NULL;
    FVizDataArray* derivatives = NULL;
    FVizSize point_count;
    FVizSize cell_count;
    FVizSize components;
    FVizSize i;
    if (out_grid != NULL) *out_grid = NULL;
    if (grid == NULL || scalar_array_name == NULL || scalar_array_name[0] == '\0' ||
        output_name == NULL || output_name[0] == '\0' || out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "cell derivatives require a grid, input array name, and output name");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    scalars = fviz_attribute_set_const_get(
        fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid), scalar_array_name);
    if (scalars == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND,
            "cell derivatives input array was not found on point data");
        return FVIZ_ERROR_NOT_FOUND;
    }
    point_count = fviz_unstructured_grid_point_count(grid);
    cell_count = fviz_unstructured_grid_cell_count(grid);
    components = fviz_data_array_components(scalars);
    if (components == 0u || components > 3u || fviz_data_array_tuple_count(scalars) != point_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
            "cell derivatives input must have one 1-3 component tuple per point");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, (uint32_t)(3u * components), &derivatives) != FVIZ_OK ||
        fviz_data_array_resize(derivatives, cell_count) != FVIZ_OK)
        goto fail;
    points = fviz_points_data(grid->points);
    for (i = 0u; i < cell_count; ++i)
    {
        FVizCellView view;
        FVizSize point_count_in_cell;
        FVizSize s;
        FVizSize component;
        double basis[64 * 4];
        double values[64 * 3];
        double normal[4 * 4];
        double rhs[4 * 3];
        double a[16];
        double b[12];
        double coefficients[4 * 3];
        double grad_tuple[9];
        FVizSize row, col, r, c, rr;
        if (fviz_cell_array_cell_view(grid->cells, i, &view) != FVIZ_OK) continue;
        point_count_in_cell = view.point_count;
        if (point_count_in_cell == 0u || point_count_in_cell > 64u) continue;
        for (s = 0u; s < point_count_in_cell; ++s)
        {
            const FVizId pid = fviz_cell_view_point_id(&view, s);
            basis[s * 4u + 0u] = 1.0;
            basis[s * 4u + 1u] = (double)points[(FVizSize)pid].x;
            basis[s * 4u + 2u] = (double)points[(FVizSize)pid].y;
            basis[s * 4u + 3u] = (double)points[(FVizSize)pid].z;
            for (component = 0u; component < components; ++component)
            {
                double value = 0.0;
                (void)fviz_data_array_get_component(scalars, (FVizSize)pid, (uint32_t)component, &value);
                values[s * components + component] = value;
            }
        }
        for (row = 0u; row < 4u; ++row)
            for (col = 0u; col < 4u; ++col)
            {
                double sum = 0.0;
                for (s = 0u; s < point_count_in_cell; ++s) sum += basis[s * 4u + row] * basis[s * 4u + col];
                normal[row * 4u + col] = sum;
            }
        for (row = 0u; row < 4u; ++row)
            for (component = 0u; component < components; ++component)
            {
                double sum = 0.0;
                for (s = 0u; s < point_count_in_cell; ++s)
                    sum += basis[s * 4u + row] * values[s * components + component];
                rhs[row * components + component] = sum;
            }
        (void)memcpy(a, normal, sizeof(a));
        (void)memset(b, 0, sizeof(b));
        (void)memset(coefficients, 0, sizeof(coefficients));
        for (r = 0u; r < 4u; ++r)
            for (c = 0u; c < components; ++c)
                b[r * components + c] = rhs[r * components + c];
        for (r = 0u; r < 4u; ++r)
        {
            FVizSize pivot = r;
            double pivot_value;
            for (rr = r + 1u; rr < 4u; ++rr)
                if (fabs(a[rr * 4u + r]) > fabs(a[pivot * 4u + r])) pivot = rr;
            if (pivot != r)
            {
                for (c = 0u; c < 4u; ++c)
                {
                    const double t = a[r * 4u + c]; a[r * 4u + c] = a[pivot * 4u + c]; a[pivot * 4u + c] = t;
                }
                for (c = 0u; c < components; ++c)
                {
                    const double t = b[r * components + c]; b[r * components + c] = b[pivot * components + c]; b[pivot * components + c] = t;
                }
            }
            pivot_value = a[r * 4u + r];
            if (fabs(pivot_value) < 1.0e-12) continue;
            for (c = r; c < 4u; ++c) a[r * 4u + c] /= pivot_value;
            for (c = 0u; c < components; ++c) b[r * components + c] /= pivot_value;
            for (rr = r + 1u; rr < 4u; ++rr)
            {
                const double factor = a[rr * 4u + r];
                if (factor == 0.0) continue;
                for (c = r; c < 4u; ++c) a[rr * 4u + c] -= factor * a[r * 4u + c];
                for (c = 0u; c < components; ++c) b[rr * components + c] -= factor * b[r * components + c];
            }
        }
        for (r = 4u; r-- > 0u;)
            for (c = 0u; c < components; ++c)
            {
                double sum = b[r * components + c];
                for (rr = r + 1u; rr < 4u; ++rr) sum -= a[r * 4u + rr] * coefficients[rr * components + c];
                coefficients[r * components + c] = sum;
            }
        for (component = 0u; component < components; ++component)
        {
            grad_tuple[component * 3u + 0u] = coefficients[1 * components + component];
            grad_tuple[component * 3u + 1u] = coefficients[2 * components + component];
            grad_tuple[component * 3u + 2u] = coefficients[3 * components + component];
        }
        if (fviz_data_array_set_tuple(derivatives, i, grad_tuple) != FVIZ_OK) goto fail;
    }
    if (fviz_unstructured_grid_shallow_copy(grid, &result) != FVIZ_OK) goto fail;
    if (fviz_attribute_set_add(fviz_unstructured_grid_cell_data(result), output_name, derivatives) != FVIZ_OK) goto fail;
    fviz_release(derivatives);
    *out_grid = result;
    return FVIZ_OK;
fail:
    fviz_release(derivatives);
    fviz_release(result);
    return fviz_last_error_code();
}

FVizResult fviz_unstructured_grid_warp_scalar(
    const FVizUnstructuredGrid* grid,
    const char* scalar_array_name,
    double scale,
    const char* normal_array_name,
    FVizUnstructuredGrid** out_grid)
{
    const FVizDataArray* scalars;
    const FVizDataArray* normals = NULL;
    const FVizVec3* points;
    FVizUnstructuredGrid* result = NULL;
    FVizVec3* displaced = NULL;
    FVizSize point_count;
    FVizSize i;
    if (out_grid != NULL) *out_grid = NULL;
    if (grid == NULL || scalar_array_name == NULL || scalar_array_name[0] == '\0' || out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "warp scalar requires a grid, input array name, and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    scalars = fviz_attribute_set_const_get(
        fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid), scalar_array_name);
    if (scalars == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND,
            "warp scalar input array was not found on point data");
        return FVIZ_ERROR_NOT_FOUND;
    }
    if (fviz_data_array_tuple_count(scalars) != fviz_unstructured_grid_point_count(grid))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
            "warp scalar input must have one tuple per point");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (normal_array_name != NULL && normal_array_name[0] != '\0')
        normals = fviz_attribute_set_const_get(
            fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid), normal_array_name);
    point_count = fviz_unstructured_grid_point_count(grid);
    points = fviz_points_data(grid->points);
    displaced = (FVizVec3*)fviz_alloc(point_count * sizeof(*displaced));
    if (displaced == NULL) return fviz_last_error_code();
    for (i = 0u; i < point_count; ++i)
    {
        double value = 0.0;
        (void)fviz_data_array_get_component(scalars, i, 0u, &value);
        if (normals != NULL && i < fviz_data_array_tuple_count(normals) &&
            fviz_data_array_components(normals) >= 3u)
        {
            double nx = 0.0, ny = 0.0, nz = 0.0;
            (void)fviz_data_array_get_component(normals, i, 0u, &nx);
            (void)fviz_data_array_get_component(normals, i, 1u, &ny);
            (void)fviz_data_array_get_component(normals, i, 2u, &nz);
            displaced[i] = fviz_vec3_add(points[i],
                fviz_vec3_scale(fviz_vec3((float)nx, (float)ny, (float)nz), (float)(value * scale)));
        }
        else
        {
            displaced[i] = fviz_vec3_add(points[i],
                fviz_vec3(0.0f, 0.0f, (float)(value * scale)));
        }
    }
    /* Build a fresh grid so the source points are not mutated. */
    if (fviz_unstructured_grid_create(&result) != FVIZ_OK ||
        fviz_unstructured_grid_reserve(result, point_count,
            fviz_cell_array_count(grid->cells), fviz_cell_array_connectivity_size(grid->cells)) != FVIZ_OK ||
        fviz_unstructured_grid_add_points_ids(result, displaced, point_count, NULL) != FVIZ_OK)
    {
        fviz_free(displaced);
        fviz_release(result);
        return fviz_last_error_code();
    }
    fviz_free(displaced);
    for (i = 0u; i < fviz_cell_array_count(grid->cells); ++i)
        if (fviz_append_source_cell(result, grid->cells, i) != FVIZ_OK)
        {
            fviz_release(result);
            return fviz_last_error_code();
        }
    if (fviz_copy_attribute_set(fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_point_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_cell_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_field_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_field_data(result)) != FVIZ_OK)
    {
        fviz_release(result);
        return fviz_last_error_code();
    }
    *out_grid = result;
    return FVIZ_OK;
}

FVizResult fviz_unstructured_grid_stream_tracer(
    const FVizUnstructuredGrid* grid,
    const char* vector_array_name,
    const FVizVec3* seed_points,
    FVizSize seed_count,
    double step_length,
    FVizSize max_steps,
    FVizPolyData** out_lines)
{
    const FVizDataArray* vectors;
    FVizPointLocator* locator = NULL;
    FVizPolyData* output = NULL;
    FVizSize i;
    if (out_lines != NULL) *out_lines = NULL;
    if (grid == NULL || vector_array_name == NULL || vector_array_name[0] == '\0' ||
        (seed_count != 0u && seed_points == NULL) || out_lines == NULL || step_length <= 0.0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "stream tracer requires a grid, vector array, seeds, and positive step");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    vectors = fviz_attribute_set_const_get(
        fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid), vector_array_name);
    if (vectors == NULL || fviz_data_array_components(vectors) < 3u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND,
            "stream tracer vector array was not found on point data");
        return FVIZ_ERROR_NOT_FOUND;
    }
    if (fviz_point_locator_create(&locator) != FVIZ_OK ||
        fviz_point_locator_set_grid(locator, grid) != FVIZ_OK ||
        fviz_point_locator_build(locator) != FVIZ_OK)
        goto fail;
    if (fviz_poly_data_create(&output) != FVIZ_OK) goto fail;
    for (i = 0u; i < seed_count; ++i)
    {
        FVizVec3 point = seed_points[i];
        FVizSize step;
        FVizSize first_point_index;
        if (fviz_poly_data_add_point(output, point, NULL) != FVIZ_OK) goto fail;
        first_point_index = fviz_poly_data_point_count(output) - 1u;
        for (step = 0u; step < max_steps; ++step)
        {
            const FVizVec3 v = fviz_point_locator_interpolate_vector(locator, vector_array_name, point);
            const float speed = fviz_vec3_length(v);
            if (speed < 1.0e-9f) break;
            {
                FVizVec3 next = fviz_vec3_add(point, fviz_vec3_scale(v, (float)step_length));
                /* RK2 midpoint refinement. */
                {
                    const FVizVec3 vm = fviz_point_locator_interpolate_vector(locator, vector_array_name, next);
                    next = fviz_vec3_add(point, fviz_vec3_scale(fviz_vec3_scale(fviz_vec3_add(v, vm), 0.5f), (float)step_length));
                }
                {
                    uint32_t index = 0u;
                    if (fviz_poly_data_add_point(output, next, &index) != FVIZ_OK) goto fail;
                    if (fviz_poly_data_add_line(output, (uint32_t)(first_point_index + step), index) != FVIZ_OK) goto fail;
                }
                point = next;
            }
        }
    }
    if (fviz_poly_data_validate(output) != FVIZ_OK) goto fail;
    fviz_release(locator);
    *out_lines = output;
    return FVIZ_OK;
fail:
    fviz_release(locator);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_unstructured_grid_cutter(
    const FVizUnstructuredGrid* grid,
    const FVizPlane* planes,
    FVizSize plane_count,
    FVizPolyData** out_cut)
{
    FVizPolyData* cut = NULL;
    FVizSize i;
    if (out_cut != NULL) *out_cut = NULL;
    if (grid == NULL || (plane_count != 0u && planes == NULL) || out_cut == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "cutter requires a grid, planes, and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_poly_data_create(&cut) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < plane_count; ++i)
    {
        FVizPolyData* slice = NULL;
        if (fviz_unstructured_grid_slice(grid, planes[i], &slice) != FVIZ_OK)
        {
            fviz_release(slice);
            fviz_release(cut);
            return fviz_last_error_code();
        }
        if (slice != NULL)
        {
            if (fviz_poly_data_append(cut, slice) != FVIZ_OK)
            {
                fviz_release(slice);
                fviz_release(cut);
                return fviz_last_error_code();
            }
            fviz_release(slice);
        }
    }
    if (fviz_poly_data_validate(cut) != FVIZ_OK)
    {
        fviz_release(cut);
        return fviz_last_error_code();
    }
    *out_cut = cut;
    return FVIZ_OK;
}

/* Iso-surface extraction (marching tetra, VTK vtkContourFilter compatible).
 * Tetra cells are marched directly; hexahedra are decomposed into 5 tetrahedra
 * using the standard VTK convention. Each tetra produces one triangle when one
 * vertex is above the iso value, or a quadrilateral (two triangles) when two
 * are above. */
FVizResult fviz_unstructured_grid_iso_surface(
    const FVizUnstructuredGrid* grid,
    const char* scalar_array_name,
    double iso_value,
    FVizPolyData** out_surface)
{
    const FVizDataArray* scalars;
    const FVizVec3* points;
    FVizPolyData* output = NULL;
    FVizDataArray* scalar_values = NULL;
    FVizSize cell_count;
    FVizSize i;
    if (out_surface != NULL) *out_surface = NULL;
    if (grid == NULL || scalar_array_name == NULL || scalar_array_name[0] == '\0' || out_surface == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "iso-surface requires a grid, scalar name, and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    scalars = fviz_attribute_set_const_get(
        fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid), scalar_array_name);
    if (scalars == NULL || fviz_data_array_components(scalars) < 1u ||
        fviz_data_array_tuple_count(scalars) != fviz_unstructured_grid_point_count(grid))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
            "iso-surface scalar must be a one-component point array");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &scalar_values) != FVIZ_OK)
        return fviz_last_error_code();
    points = fviz_points_data(grid->points);
    cell_count = fviz_unstructured_grid_cell_count(grid);

    for (i = 0u; i < cell_count; ++i)
    {
        FVizCellView view;
        if (fviz_cell_array_cell_view(grid->cells, i, &view) != FVIZ_OK) continue;
        /* Only tetra and hexa cells are supported by the marching decomposition. */
        if (view.point_count == 4u)
        {
            FVizId ids[4];
            FVizSize e;
            for (e = 0u; e < 4u; ++e) ids[e] = fviz_cell_view_point_id(&view, e);
            {
                double v[4];
                FVizVec3 p[4];
                uint32_t above[4];
                FVizSize above_count = 0u;
                uint32_t below[4];
                FVizSize below_count = 0u;
                for (e = 0u; e < 4u; ++e)
                {
                    (void)fviz_data_array_get_component(scalars, (FVizSize)ids[e], 0u, &v[e]);
                    p[e] = points[(FVizSize)ids[e]];
                    if (v[e] >= iso_value) above[above_count++] = (uint32_t)e;
                    else below[below_count++] = (uint32_t)e;
                }
                if (above_count == 0u || above_count == 4u) continue; /* no crossing */
                if (above_count == 1u || above_count == 3u)
                {
                    /* One triangle: one vertex on the single side, three on the
                     * other. Edges from the isolated vertex to the other three. */
                    uint32_t iso = above_count == 1u ? above[0u] : below[0u];
                    uint32_t other[3];
                    FVizSize o = 0u;
                    for (e = 0u; e < 4u; ++e)
                        if (e != iso) other[o++] = (uint32_t)e;
                    {
                        FVizSize edge_pair[3][2] = {{0,1},{0,2},{1,2}};
                        FVizSize t;
                        uint32_t out_ids[3];
                        for (t = 0u; t < 3u; ++t)
                        {
                            const uint32_t a = other[edge_pair[t][0]];
                            const uint32_t b = other[edge_pair[t][1]];
                            double va = v[a], vb = v[b];
                            double t_frac = (iso_value - va) / (vb - va);
                            FVizVec3 interp = fviz_vec3_add(p[a],
                                fviz_vec3_scale(fviz_vec3_sub(p[b], p[a]), (float)t_frac));
                            uint32_t pid = 0u;
                            if (fviz_poly_data_add_point(output, interp, &pid) != FVIZ_OK ||
                                fviz_data_array_append_tuple(scalar_values, &iso_value) != FVIZ_OK)
                                goto fail;
                            out_ids[t] = pid;
                        }
                        if (fviz_poly_data_add_triangle(output, out_ids[0], out_ids[1], out_ids[2]) != FVIZ_OK)
                            goto fail;
                    }
                }
                else /* above_count == 2: quadrilateral = 2 triangles. */
                {
                    uint32_t a0 = above[0u], a1 = above[1u];
                    uint32_t b0 = below[0u], b1 = below[1u];
                    /* Edges: a0-a1 (crossing none), a0-b0, a0-b1, a1-b0, a1-b1,
                     * b0-b1. The surface connects the two above vertices to the
                     * two below vertices. */
                    {
                        FVizSize t;
                        uint32_t out_ids[4];
                        /* midpoint interpolation helper via local function. */
                        for (t = 0u; t < 4u; ++t)
                        {
                            uint32_t a, b;
                            double va, vb;
                            double t_frac;
                            FVizVec3 interp;
                            uint32_t pid = 0u;
                            switch (t)
                            {
                                case 0u: a = a0; b = b0; break;
                                case 1u: a = a0; b = b1; break;
                                case 2u: a = a1; b = b0; break;
                                default: a = a1; b = b1; break;
                            }
                            va = v[a]; vb = v[b];
                            t_frac = (iso_value - va) / (vb - va);
                            interp = fviz_vec3_add(p[a], fviz_vec3_scale(fviz_vec3_sub(p[b], p[a]), (float)t_frac));
                            if (fviz_poly_data_add_point(output, interp, &pid) != FVIZ_OK ||
                                fviz_data_array_append_tuple(scalar_values, &iso_value) != FVIZ_OK)
                                goto fail;
                            out_ids[t] = pid;
                        }
                        /* Two triangles: (a0b0, a0b1, a1b1) and (a0b0, a1b1, a1b0). */
                        if (fviz_poly_data_add_triangle(output, out_ids[0], out_ids[1], out_ids[3]) != FVIZ_OK ||
                            fviz_poly_data_add_triangle(output, out_ids[0], out_ids[3], out_ids[2]) != FVIZ_OK)
                            goto fail;
                    }
                }
            }
        }
        else if (view.point_count == 8u)
        {
            /* Hexahedron: decompose into 5 tetrahedra (VTK convention) and
             * march each. */
            FVizId ids[8];
            const uint32_t tetra_split[5][4] = {
                {0,1,2,5},{0,2,3,7},{0,4,5,7},{2,5,6,7},{0,2,5,7}};
            FVizSize t;
            FVizSize e;
            for (e = 0u; e < 8u; ++e) ids[e] = fviz_cell_view_point_id(&view, e);
            for (t = 0u; t < 5u; ++t)
            {
                FVizId tet[4];
                double v[4];
                FVizVec3 p[4];
                uint32_t above[4];
                FVizSize above_count = 0u;
                uint32_t below[4];
                FVizSize below_count = 0u;
                FVizSize k;
                for (k = 0u; k < 4u; ++k)
                {
                    tet[k] = ids[tetra_split[t][k]];
                    (void)fviz_data_array_get_component(scalars, (FVizSize)tet[k], 0u, &v[k]);
                    p[k] = points[(FVizSize)tet[k]];
                    if (v[k] >= iso_value) above[above_count++] = (uint32_t)k;
                    else below[below_count++] = (uint32_t)k;
                }
                if (above_count == 0u || above_count == 4u) continue;
                if (above_count == 1u || above_count == 3u)
                {
                    uint32_t iso = above_count == 1u ? above[0u] : below[0u];
                    uint32_t other[3];
                    FVizSize o = 0u;
                    for (k = 0u; k < 4u; ++k)
                        if (k != iso) other[o++] = (uint32_t)k;
                    {
                        FVizSize edge_pair[3][2] = {{0,1},{0,2},{1,2}};
                        FVizSize tt;
                        uint32_t out_ids[3];
                        for (tt = 0u; tt < 3u; ++tt)
                        {
                            const uint32_t a = other[edge_pair[tt][0]];
                            const uint32_t b = other[edge_pair[tt][1]];
                            double va = v[a], vb = v[b];
                            double t_frac = (iso_value - va) / (vb - va);
                            FVizVec3 interp = fviz_vec3_add(p[a],
                                fviz_vec3_scale(fviz_vec3_sub(p[b], p[a]), (float)t_frac));
                            uint32_t pid = 0u;
                            if (fviz_poly_data_add_point(output, interp, &pid) != FVIZ_OK ||
                                fviz_data_array_append_tuple(scalar_values, &iso_value) != FVIZ_OK)
                                goto fail;
                            out_ids[tt] = pid;
                        }
                        if (fviz_poly_data_add_triangle(output, out_ids[0], out_ids[1], out_ids[2]) != FVIZ_OK)
                            goto fail;
                    }
                }
                else
                {
                    uint32_t a0 = above[0u], a1 = above[1u];
                    uint32_t b0 = below[0u], b1 = below[1u];
                    FVizSize tt;
                    uint32_t out_ids[4];
                    for (tt = 0u; tt < 4u; ++tt)
                    {
                        uint32_t a, b;
                        double va, vb;
                        double t_frac;
                        FVizVec3 interp;
                        uint32_t pid = 0u;
                        switch (tt)
                        {
                            case 0u: a = a0; b = b0; break;
                            case 1u: a = a0; b = b1; break;
                            case 2u: a = a1; b = b0; break;
                            default: a = a1; b = b1; break;
                        }
                        va = v[a]; vb = v[b];
                        t_frac = (iso_value - va) / (vb - va);
                        interp = fviz_vec3_add(p[a], fviz_vec3_scale(fviz_vec3_sub(p[b], p[a]), (float)t_frac));
                        if (fviz_poly_data_add_point(output, interp, &pid) != FVIZ_OK ||
                            fviz_data_array_append_tuple(scalar_values, &iso_value) != FVIZ_OK)
                            goto fail;
                        out_ids[tt] = pid;
                    }
                    if (fviz_poly_data_add_triangle(output, out_ids[0], out_ids[1], out_ids[3]) != FVIZ_OK ||
                        fviz_poly_data_add_triangle(output, out_ids[0], out_ids[3], out_ids[2]) != FVIZ_OK)
                        goto fail;
                }
            }
        }
    }
    if (fviz_attribute_set_add(fviz_poly_data_point_data(output), scalar_array_name, scalar_values) != FVIZ_OK ||
        fviz_poly_data_validate(output) != FVIZ_OK) goto fail;
    fviz_release(scalar_values);
    *out_surface = output;
    return FVIZ_OK;
fail:
    fviz_release(scalar_values);
    fviz_release(output);
    return fviz_last_error_code();
}
