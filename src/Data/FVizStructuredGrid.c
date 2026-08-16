#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizStructuredGrid.h>
#include <FViz/Mesh/FVizPoints.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/Data/FVizStructuredGridPrivate.h>

static void fviz_structured_grid_destroy(FVizObject* object);
static FVizMTime fviz_structured_grid_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_structured_grid_class = {FVIZ_TYPE_STRUCTURED_GRID, "FVizStructuredGrid",
                                                             &g_fviz_data_object_class, fviz_structured_grid_destroy,
                                                             fviz_structured_grid_mtime};

static FVizBool fviz_structured_grid_dependency_modified(FVizObject* caller, FVizEventId event_id, void* call_data,
                                                         void* client_data)
{
    FVizStructuredGrid* grid = (FVizStructuredGrid*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (grid != NULL && grid->dependency_suppression == 0u) fviz_object_modified((FVizObject*)grid);
    return FVIZ_FALSE;
}

static FVizMTime fviz_structured_grid_mtime(const FVizObject* object)
{
    /* Points/DataSet ModifiedEvents are bridged, keeping repeated MTime queries O(1). */
    return fviz_internal_object_local_mtime(object);
}

static void fviz_structured_grid_destroy(FVizObject* object)
{
    FVizStructuredGrid* grid = (FVizStructuredGrid*)object;
    if (grid->points != NULL && grid->points_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)grid->points, grid->points_modified_tag);
    if (grid->data_set != NULL && grid->data_set_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)grid->data_set, grid->data_set_modified_tag);
    fviz_release(grid->points);
    fviz_release(grid->data_set);
}

static FVizResult fviz_structured_extent_dimensions(const int64_t extent[6], FVizSize dims[3])
{
    uint32_t axis;
    for (axis = 0u; axis < 3u; ++axis)
    {
        const int64_t minimum = extent[axis * 2u];
        const int64_t maximum = extent[axis * 2u + 1u];
        uint64_t width;
        if (maximum < minimum)
        {
            dims[0] = dims[1] = dims[2] = 0u;
            return FVIZ_OK;
        }
        width = (uint64_t)maximum - (uint64_t)minimum + UINT64_C(1);
        if (width == 0u || width > (uint64_t)((FVizSize)-1))
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "structured grid extent exceeds FVizSize");
            return FVIZ_ERROR_OVERFLOW;
        }
        dims[axis] = (FVizSize)width;
    }
    return FVIZ_OK;
}

static FVizResult fviz_structured_counts(const int64_t extent[6], FVizSize* out_points, FVizSize* out_cells)
{
    FVizSize dims[3] = {0u, 0u, 0u};
    FVizSize points;
    FVizSize cells = 1u;
    uint32_t axis;
    if (fviz_structured_extent_dimensions(extent, dims) != FVIZ_OK) return fviz_last_error_code();
    if (dims[0] == 0u || dims[1] == 0u || dims[2] == 0u)
    {
        *out_points = 0u;
        *out_cells = 0u;
        return FVIZ_OK;
    }
    if (fviz_size_multiply(dims[0], dims[1], &points) != FVIZ_OK ||
        fviz_size_multiply(points, dims[2], &points) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    for (axis = 0u; axis < 3u; ++axis)
    {
        const FVizSize cell_dim = dims[axis] > 1u ? dims[axis] - 1u : 1u;
        if (fviz_size_multiply(cells, cell_dim, &cells) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    }
    *out_points = points;
    *out_cells = cells;
    return FVIZ_OK;
}

static void fviz_structured_cell_dimensions(const FVizStructuredGrid* grid, FVizSize cell_dims[3])
{
    FVizSize dims[3];
    uint32_t axis;
    fviz_structured_grid_dimensions(grid, dims);
    for (axis = 0u; axis < 3u; ++axis)
        cell_dims[axis] = dims[axis] > 1u ? dims[axis] - 1u : (dims[axis] == 1u ? 1u : 0u);
}

FVizResult fviz_structured_grid_create(FVizStructuredGrid** out_grid)
{
    FVizStructuredGrid* grid;
    const int64_t empty_extent[6] = {0, -1, 0, -1, 0, -1};
    if (out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    grid = (FVizStructuredGrid*)fviz_internal_object_allocate(sizeof(FVizStructuredGrid), &g_fviz_structured_grid_class,
                                                              NULL);
    if (grid == NULL) return fviz_last_error_code();
    (void)memcpy(grid->extent, empty_extent, sizeof(empty_extent));
    if (fviz_points_create(&grid->points) != FVIZ_OK || fviz_data_set_create(&grid->data_set) != FVIZ_OK)
    {
        fviz_release(grid);
        return fviz_last_error_code();
    }
    if (fviz_object_add_observer((FVizObject*)grid->points, FVIZ_EVENT_MODIFIED, 0.0f,
                                 fviz_structured_grid_dependency_modified, grid,
                                 &grid->points_modified_tag) != FVIZ_OK ||
        fviz_object_add_observer((FVizObject*)grid->data_set, FVIZ_EVENT_MODIFIED, 0.0f,
                                 fviz_structured_grid_dependency_modified, grid,
                                 &grid->data_set_modified_tag) != FVIZ_OK)
    {
        fviz_release(grid);
        return fviz_last_error_code();
    }
    *out_grid = grid;
    return FVIZ_OK;
}

void fviz_structured_grid_clear(FVizStructuredGrid* grid)
{
    const int64_t empty_extent[6] = {0, -1, 0, -1, 0, -1};
    FVizBool changed;
    if (grid == NULL) return;
    changed = fviz_points_count(grid->points) != 0u ||
                      fviz_attribute_set_count(fviz_data_set_point_data(grid->data_set)) != 0u ||
                      fviz_attribute_set_count(fviz_data_set_cell_data(grid->data_set)) != 0u ||
                      fviz_attribute_set_count(fviz_data_set_field_data(grid->data_set)) != 0u
                  ? FVIZ_TRUE
                  : FVIZ_FALSE;
    ++grid->dependency_suppression;
    fviz_points_clear(grid->points);
    fviz_attribute_set_clear(fviz_data_set_point_data(grid->data_set));
    fviz_attribute_set_clear(fviz_data_set_cell_data(grid->data_set));
    fviz_attribute_set_clear(fviz_data_set_field_data(grid->data_set));
    (void)fviz_data_set_set_point_count(grid->data_set, 0u);
    (void)fviz_data_set_set_cell_count(grid->data_set, 0u);
    --grid->dependency_suppression;
    if (memcmp(grid->extent, empty_extent, sizeof(empty_extent)) != 0) changed = FVIZ_TRUE;
    (void)memcpy(grid->extent, empty_extent, sizeof(empty_extent));
    if (changed != FVIZ_FALSE) fviz_object_modified((FVizObject*)grid);
}

FVizResult fviz_structured_grid_set_extent(FVizStructuredGrid* grid, const int64_t extent[6])
{
    FVizSize points = 0u, cells = 0u;
    const FVizSize stored_points = grid != NULL ? fviz_points_count(grid->points) : 0u;
    if (grid == NULL || extent == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured grid extent arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_structured_counts(extent, &points, &cells) != FVIZ_OK) return fviz_last_error_code();
    if (stored_points != 0u && stored_points != points)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "structured grid extent conflicts with existing points");
        return FVIZ_ERROR_INVALID_STATE;
    }
    ++grid->dependency_suppression;
    if (fviz_data_set_set_point_count(grid->data_set, points) != FVIZ_OK ||
        fviz_data_set_set_cell_count(grid->data_set, cells) != FVIZ_OK)
    {
        --grid->dependency_suppression;
        return fviz_last_error_code();
    }
    --grid->dependency_suppression;
    if (memcmp(grid->extent, extent, 6u * sizeof(int64_t)) != 0)
    {
        (void)memcpy(grid->extent, extent, 6u * sizeof(int64_t));
        fviz_object_modified((FVizObject*)grid);
    }
    return FVIZ_OK;
}

void fviz_structured_grid_extent(const FVizStructuredGrid* grid, int64_t out_extent[6])
{
    const int64_t empty_extent[6] = {0, -1, 0, -1, 0, -1};
    if (out_extent == NULL) return;
    (void)memcpy(out_extent, grid != NULL ? grid->extent : empty_extent, 6u * sizeof(int64_t));
}

void fviz_structured_grid_dimensions(const FVizStructuredGrid* grid, FVizSize out_dimensions[3])
{
    if (out_dimensions == NULL) return;
    out_dimensions[0] = out_dimensions[1] = out_dimensions[2] = 0u;
    if (grid != NULL) (void)fviz_structured_extent_dimensions(grid->extent, out_dimensions);
}

uint32_t fviz_structured_grid_dimension(const FVizStructuredGrid* grid)
{
    FVizSize dims[3];
    uint32_t dimension = 0u, axis;
    fviz_structured_grid_dimensions(grid, dims);
    for (axis = 0u; axis < 3u; ++axis)
        if (dims[axis] > 1u) ++dimension;
    return dimension;
}

FVizSize fviz_structured_grid_point_count(const FVizStructuredGrid* grid)
{
    return grid != NULL ? fviz_data_set_point_count(grid->data_set) : 0u;
}

FVizSize fviz_structured_grid_cell_count(const FVizStructuredGrid* grid)
{
    return grid != NULL ? fviz_data_set_cell_count(grid->data_set) : 0u;
}

FVizCellType fviz_structured_grid_cell_type(const FVizStructuredGrid* grid)
{
    switch (fviz_structured_grid_dimension(grid))
    {
        case 0u:
            return fviz_structured_grid_point_count(grid) != 0u ? FVIZ_CELL_VERTEX : (FVizCellType)0;
        case 1u:
            return FVIZ_CELL_LINE;
        case 2u:
            return FVIZ_CELL_QUAD;
        case 3u:
            return FVIZ_CELL_HEXAHEDRON;
        default:
            return (FVizCellType)0;
    }
}

FVizResult fviz_structured_grid_set_points(FVizStructuredGrid* grid, const FVizVec3* points, FVizSize point_count)
{
    const FVizSize expected = fviz_structured_grid_point_count(grid);
    FVizResult result = FVIZ_OK;
    if (grid == NULL || (point_count != 0u && points == NULL) || point_count != expected)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured grid point count must match its extent");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    ++grid->dependency_suppression;
    if (fviz_points_count(grid->points) == point_count)
        result = point_count != 0u ? fviz_points_set_many(grid->points, 0u, points, point_count) : FVIZ_OK;
    else
    {
        fviz_points_clear(grid->points);
        if (point_count != 0u) result = fviz_points_append_many_ids(grid->points, points, point_count, NULL);
    }
    --grid->dependency_suppression;
    if (result == FVIZ_OK && point_count != 0u) fviz_object_modified((FVizObject*)grid);
    return result;
}

FVizResult fviz_structured_grid_set_point(FVizStructuredGrid* grid, FVizSize point_id, FVizVec3 point)
{
    if (grid == NULL || point_id >= fviz_points_count(grid->points))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured grid point ID is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_points_set(grid->points, point_id, point);
}

const FVizVec3* fviz_structured_grid_points(const FVizStructuredGrid* grid)
{
    return grid != NULL ? fviz_points_data(grid->points) : NULL;
}

FVizResult fviz_structured_grid_point(const FVizStructuredGrid* grid, FVizId point_id, FVizVec3* out_point)
{
    if (grid == NULL || out_point == NULL || point_id >= (FVizId)fviz_points_count(grid->points))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured grid point ID is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_point = fviz_points_data(grid->points)[(FVizSize)point_id];
    return FVIZ_OK;
}

FVizBounds fviz_structured_grid_bounds(const FVizStructuredGrid* grid)
{
    return grid != NULL ? fviz_points_bounds(grid->points) : fviz_bounds_empty();
}

FVizResult fviz_structured_grid_point_id(const FVizStructuredGrid* grid, int64_t i, int64_t j, int64_t k,
                                         FVizId* out_point_id)
{
    FVizSize dims[3], plane, id;
    FVizSize x, y, z;
    if (out_point_id != NULL) *out_point_id = FVIZ_INVALID_ID;
    if (grid == NULL || out_point_id == NULL || i < grid->extent[0] || i > grid->extent[1] || j < grid->extent[2] ||
        j > grid->extent[3] || k < grid->extent[4] || k > grid->extent[5])
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured point index is outside extent");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_structured_grid_dimensions(grid, dims);
    x = (FVizSize)(i - grid->extent[0]);
    y = (FVizSize)(j - grid->extent[2]);
    z = (FVizSize)(k - grid->extent[4]);
    if (fviz_size_multiply(dims[0], dims[1], &plane) != FVIZ_OK || fviz_size_multiply(z, plane, &id) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    id += y * dims[0] + x;
    *out_point_id = (FVizId)id;
    return FVIZ_OK;
}

FVizResult fviz_structured_grid_point_ijk(const FVizStructuredGrid* grid, FVizId point_id, int64_t out_ijk[3])
{
    FVizSize dims[3], plane, id;
    if (grid == NULL || out_ijk == NULL || point_id >= (FVizId)fviz_structured_grid_point_count(grid))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured grid point ID is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_structured_grid_dimensions(grid, dims);
    if (fviz_size_multiply(dims[0], dims[1], &plane) != FVIZ_OK || plane == 0u) return FVIZ_ERROR_INVALID_STATE;
    id = (FVizSize)point_id;
    out_ijk[2] = grid->extent[4] + (int64_t)(id / plane);
    id %= plane;
    out_ijk[1] = grid->extent[2] + (int64_t)(id / dims[0]);
    out_ijk[0] = grid->extent[0] + (int64_t)(id % dims[0]);
    return FVIZ_OK;
}

FVizResult fviz_structured_grid_cell_id(const FVizStructuredGrid* grid, int64_t i, int64_t j, int64_t k,
                                        FVizId* out_cell_id)
{
    FVizSize cell_dims[3], x, y, z, plane, id;
    if (out_cell_id != NULL) *out_cell_id = FVIZ_INVALID_ID;
    if (grid == NULL || out_cell_id == NULL || fviz_structured_grid_cell_count(grid) == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured grid cell request is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_structured_cell_dimensions(grid, cell_dims);
    if (i < grid->extent[0] || (FVizSize)(i - grid->extent[0]) >= cell_dims[0] || j < grid->extent[2] ||
        (FVizSize)(j - grid->extent[2]) >= cell_dims[1] || k < grid->extent[4] ||
        (FVizSize)(k - grid->extent[4]) >= cell_dims[2])
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured cell index is outside extent");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    x = (FVizSize)(i - grid->extent[0]);
    y = (FVizSize)(j - grid->extent[2]);
    z = (FVizSize)(k - grid->extent[4]);
    if (fviz_size_multiply(cell_dims[0], cell_dims[1], &plane) != FVIZ_OK ||
        fviz_size_multiply(z, plane, &id) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    id += y * cell_dims[0] + x;
    *out_cell_id = (FVizId)id;
    return FVIZ_OK;
}

FVizResult fviz_structured_grid_cell_ijk(const FVizStructuredGrid* grid, FVizId cell_id, int64_t out_ijk[3])
{
    FVizSize cell_dims[3], plane, id;
    if (grid == NULL || out_ijk == NULL || cell_id >= (FVizId)fviz_structured_grid_cell_count(grid))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured grid cell ID is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_structured_cell_dimensions(grid, cell_dims);
    if (fviz_size_multiply(cell_dims[0], cell_dims[1], &plane) != FVIZ_OK || plane == 0u)
        return FVIZ_ERROR_INVALID_STATE;
    id = (FVizSize)cell_id;
    out_ijk[2] = grid->extent[4] + (int64_t)(id / plane);
    id %= plane;
    out_ijk[1] = grid->extent[2] + (int64_t)(id / cell_dims[0]);
    out_ijk[0] = grid->extent[0] + (int64_t)(id % cell_dims[0]);
    return FVIZ_OK;
}

FVizResult fviz_structured_grid_cell_point_ids(const FVizStructuredGrid* grid, FVizId cell_id, FVizId out_point_ids[8],
                                               uint32_t* out_point_count)
{
    int64_t ijk[3];
    FVizSize dims[3];
    uint32_t active[3], active_count = 0u, axis;
    uint32_t corner, corner_count;
    if (out_point_count != NULL) *out_point_count = 0u;
    if (grid == NULL || out_point_ids == NULL || out_point_count == NULL ||
        fviz_structured_grid_cell_ijk(grid, cell_id, ijk) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_structured_grid_dimensions(grid, dims);
    for (axis = 0u; axis < 3u; ++axis)
        if (dims[axis] > 1u) active[active_count++] = axis;
    corner_count = 1u << active_count;
    for (corner = 0u; corner < corner_count; ++corner)
    {
        int64_t pijk[3] = {ijk[0], ijk[1], ijk[2]};
        uint32_t bit;
        /* For 2D/3D use VTK-like cyclic corner order rather than raw binary Gray order. */
        static const uint8_t order2[4] = {0u, 1u, 3u, 2u};
        static const uint8_t order3[8] = {0u, 1u, 3u, 2u, 4u, 5u, 7u, 6u};
        const uint32_t code = active_count == 2u ? order2[corner] : (active_count == 3u ? order3[corner] : corner);
        for (bit = 0u; bit < active_count; ++bit)
            if ((code & (1u << bit)) != 0u) ++pijk[active[bit]];
        if (fviz_structured_grid_point_id(grid, pijk[0], pijk[1], pijk[2], &out_point_ids[corner]) != FVIZ_OK)
            return fviz_last_error_code();
    }
    *out_point_count = corner_count;
    return FVIZ_OK;
}

FVizAttributeSet* fviz_structured_grid_point_data(FVizStructuredGrid* grid)
{
    return grid != NULL ? fviz_data_set_point_data(grid->data_set) : NULL;
}

FVizAttributeSet* fviz_structured_grid_cell_data(FVizStructuredGrid* grid)
{
    return grid != NULL ? fviz_data_set_cell_data(grid->data_set) : NULL;
}

FVizAttributeSet* fviz_structured_grid_field_data(FVizStructuredGrid* grid)
{
    return grid != NULL ? fviz_data_set_field_data(grid->data_set) : NULL;
}

const FVizAttributeSet* fviz_structured_grid_const_point_data(const FVizStructuredGrid* grid)
{
    return grid != NULL ? fviz_data_set_point_data(grid->data_set) : NULL;
}

const FVizAttributeSet* fviz_structured_grid_const_cell_data(const FVizStructuredGrid* grid)
{
    return grid != NULL ? fviz_data_set_cell_data(grid->data_set) : NULL;
}

const FVizAttributeSet* fviz_structured_grid_const_field_data(const FVizStructuredGrid* grid)
{
    return grid != NULL ? fviz_data_set_field_data(grid->data_set) : NULL;
}

FVizResult fviz_structured_grid_validate(const FVizStructuredGrid* grid)
{
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_points_count(grid->points) != fviz_structured_grid_point_count(grid))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "structured grid extent and point count differ");
        return FVIZ_ERROR_INVALID_STATE;
    }
    return fviz_data_set_validate(grid->data_set);
}
