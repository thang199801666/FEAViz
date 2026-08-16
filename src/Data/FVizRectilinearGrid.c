#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizRectilinearGrid.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/Data/FVizRectilinearGridPrivate.h>

static void fviz_rectilinear_grid_destroy(FVizObject* object);
static FVizMTime fviz_rectilinear_grid_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_rectilinear_grid_class = {
    FVIZ_TYPE_RECTILINEAR_GRID, "FVizRectilinearGrid", &g_fviz_data_object_class,
    fviz_rectilinear_grid_destroy, fviz_rectilinear_grid_mtime
};

static FVizBool fviz_rectilinear_grid_dependency_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    FVizRectilinearGrid* grid = (FVizRectilinearGrid*)client_data;
    (void)caller; (void)event_id; (void)call_data;
    if (grid != NULL && grid->dependency_suppression == 0u)
        fviz_object_modified((FVizObject*)grid);
    return FVIZ_FALSE;
}

static FVizMTime fviz_rectilinear_grid_mtime(const FVizObject* object)
{
    /* All owned dependencies bridge ModifiedEvent, so aggregate MTime is O(1). */
    return fviz_internal_object_local_mtime(object);
}

static FVizResult fviz_rectilinear_dimensions_from_extent(
    const int64_t extent[6], FVizSize dims[3])
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
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "rectilinear grid extent exceeds FVizSize");
            return FVIZ_ERROR_OVERFLOW;
        }
        dims[axis] = (FVizSize)width;
    }
    return FVIZ_OK;
}

static FVizResult fviz_rectilinear_counts(
    const int64_t extent[6], FVizSize* out_points, FVizSize* out_cells)
{
    FVizSize dims[3] = {0u, 0u, 0u};
    FVizSize points;
    FVizSize cells = 1u;
    uint32_t axis;
    if (fviz_rectilinear_dimensions_from_extent(extent, dims) != FVIZ_OK)
        return fviz_last_error_code();
    if (dims[0] == 0u || dims[1] == 0u || dims[2] == 0u)
    {
        *out_points = 0u; *out_cells = 0u;
        return FVIZ_OK;
    }
    if (fviz_size_multiply(dims[0], dims[1], &points) != FVIZ_OK ||
        fviz_size_multiply(points, dims[2], &points) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    for (axis = 0u; axis < 3u; ++axis)
    {
        const FVizSize cell_dim = dims[axis] > 1u ? dims[axis] - 1u : 1u;
        if (fviz_size_multiply(cells, cell_dim, &cells) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
    }
    *out_points = points;
    *out_cells = cells;
    return FVIZ_OK;
}

static void fviz_rectilinear_cell_dimensions(
    const FVizRectilinearGrid* grid, FVizSize cell_dims[3])
{
    FVizSize dims[3];
    uint32_t axis;
    fviz_rectilinear_grid_dimensions(grid, dims);
    for (axis = 0u; axis < 3u; ++axis)
        cell_dims[axis] = dims[axis] > 1u ? dims[axis] - 1u : (dims[axis] == 1u ? 1u : 0u);
}

static FVizResult fviz_rectilinear_validate_coordinate_array(
    const FVizDataArray* coordinates, FVizSize expected)
{
    FVizSize i;
    double previous = 0.0;
    int direction = 0;
    if (coordinates == NULL || fviz_data_array_components(coordinates) != 1u ||
        fviz_data_array_tuple_count(coordinates) != expected)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear coordinate array shape is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < expected; ++i)
    {
        double value = 0.0;
        if (fviz_data_array_get_component(coordinates, i, 0u, &value) != FVIZ_OK || !isfinite(value))
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear coordinates must be finite");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        if (i != 0u)
        {
            const double delta = value - previous;
            const int current = delta > 0.0 ? 1 : (delta < 0.0 ? -1 : 0);
            if (current == 0 || (direction != 0 && current != direction))
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                    "rectilinear coordinates must be strictly monotonic");
                return FVIZ_ERROR_INVALID_ARGUMENT;
            }
            direction = current;
        }
        previous = value;
    }
    return FVIZ_OK;
}

static void fviz_rectilinear_detach_coordinate(FVizRectilinearGrid* grid, uint32_t axis)
{
    if (grid->coordinates[axis] != NULL &&
        grid->coordinate_modified_tags[axis] != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer(
            (FVizObject*)grid->coordinates[axis], grid->coordinate_modified_tags[axis]);
    fviz_release(grid->coordinates[axis]);
    grid->coordinates[axis] = NULL;
    grid->coordinate_modified_tags[axis] = FVIZ_OBSERVER_TAG_INVALID;
}

static void fviz_rectilinear_grid_destroy(FVizObject* object)
{
    FVizRectilinearGrid* grid = (FVizRectilinearGrid*)object;
    uint32_t axis;
    for (axis = 0u; axis < 3u; ++axis) fviz_rectilinear_detach_coordinate(grid, axis);
    if (grid->data_set != NULL && grid->data_set_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)grid->data_set, grid->data_set_modified_tag);
    fviz_release(grid->data_set);
    grid->data_set = NULL;
}

FVizResult fviz_rectilinear_grid_create(FVizRectilinearGrid** out_grid)
{
    FVizRectilinearGrid* grid;
    static const int64_t empty_extent[6] = {0, -1, 0, -1, 0, -1};
    uint32_t axis;
    if (out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    grid = (FVizRectilinearGrid*)fviz_internal_object_allocate(
        sizeof(*grid), &g_fviz_rectilinear_grid_class, NULL);
    if (grid == NULL) return fviz_last_error_code();
    (void)memcpy(grid->extent, empty_extent, sizeof(empty_extent));
    for (axis = 0u; axis < 3u; ++axis)
        grid->coordinate_modified_tags[axis] = FVIZ_OBSERVER_TAG_INVALID;
    grid->data_set_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (fviz_data_set_create(&grid->data_set) != FVIZ_OK ||
        fviz_object_add_observer(
            (FVizObject*)grid->data_set, FVIZ_EVENT_MODIFIED, 0.0f,
            fviz_rectilinear_grid_dependency_modified, grid,
            &grid->data_set_modified_tag) != FVIZ_OK)
    {
        fviz_release(grid);
        return fviz_last_error_code();
    }
    *out_grid = grid;
    return FVIZ_OK;
}

void fviz_rectilinear_grid_clear(FVizRectilinearGrid* grid)
{
    static const int64_t empty_extent[6] = {0, -1, 0, -1, 0, -1};
    uint32_t axis;
    FVizBool changed = FVIZ_FALSE;
    if (grid == NULL) return;
    for (axis = 0u; axis < 3u; ++axis)
    {
        if (grid->coordinates[axis] != NULL) changed = FVIZ_TRUE;
        fviz_rectilinear_detach_coordinate(grid, axis);
    }
    ++grid->dependency_suppression;
    if (fviz_attribute_set_count(fviz_data_set_point_data(grid->data_set)) != 0u ||
        fviz_attribute_set_count(fviz_data_set_cell_data(grid->data_set)) != 0u ||
        fviz_attribute_set_count(fviz_data_set_field_data(grid->data_set)) != 0u)
        changed = FVIZ_TRUE;
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

FVizResult fviz_rectilinear_grid_set_extent(
    FVizRectilinearGrid* grid, const int64_t extent[6])
{
    FVizSize points = 0u, cells = 0u, dims[3] = {0u, 0u, 0u};
    uint32_t axis;
    if (grid == NULL || extent == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear extent arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_rectilinear_counts(extent, &points, &cells) != FVIZ_OK ||
        fviz_rectilinear_dimensions_from_extent(extent, dims) != FVIZ_OK)
        return fviz_last_error_code();
    for (axis = 0u; axis < 3u; ++axis)
        if (grid->coordinates[axis] != NULL &&
            fviz_data_array_tuple_count(grid->coordinates[axis]) != dims[axis])
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                "rectilinear extent conflicts with existing coordinate arrays");
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
    if (memcmp(grid->extent, extent, sizeof(grid->extent)) != 0)
    {
        (void)memcpy(grid->extent, extent, sizeof(grid->extent));
        fviz_object_modified((FVizObject*)grid);
    }
    return FVIZ_OK;
}

void fviz_rectilinear_grid_extent(
    const FVizRectilinearGrid* grid, int64_t out_extent[6])
{
    static const int64_t empty_extent[6] = {0, -1, 0, -1, 0, -1};
    if (out_extent != NULL)
        (void)memcpy(out_extent, grid != NULL ? grid->extent : empty_extent, sizeof(empty_extent));
}

void fviz_rectilinear_grid_dimensions(
    const FVizRectilinearGrid* grid, FVizSize out_dimensions[3])
{
    if (out_dimensions == NULL) return;
    out_dimensions[0] = out_dimensions[1] = out_dimensions[2] = 0u;
    if (grid != NULL) (void)fviz_rectilinear_dimensions_from_extent(grid->extent, out_dimensions);
}

uint32_t fviz_rectilinear_grid_dimension(const FVizRectilinearGrid* grid)
{
    FVizSize dims[3];
    uint32_t axis, dimension = 0u;
    fviz_rectilinear_grid_dimensions(grid, dims);
    for (axis = 0u; axis < 3u; ++axis) if (dims[axis] > 1u) ++dimension;
    return dimension;
}

FVizSize fviz_rectilinear_grid_point_count(const FVizRectilinearGrid* grid)
{ return grid != NULL ? fviz_data_set_point_count(grid->data_set) : 0u; }
FVizSize fviz_rectilinear_grid_cell_count(const FVizRectilinearGrid* grid)
{ return grid != NULL ? fviz_data_set_cell_count(grid->data_set) : 0u; }

FVizCellType fviz_rectilinear_grid_cell_type(const FVizRectilinearGrid* grid)
{
    switch (fviz_rectilinear_grid_dimension(grid))
    {
        case 0u: return fviz_rectilinear_grid_point_count(grid) != 0u ? FVIZ_CELL_VERTEX : (FVizCellType)0;
        case 1u: return FVIZ_CELL_LINE;
        case 2u: return FVIZ_CELL_QUAD;
        case 3u: return FVIZ_CELL_HEXAHEDRON;
        default: return (FVizCellType)0;
    }
}

FVizResult fviz_rectilinear_grid_set_coordinates(
    FVizRectilinearGrid* grid, uint32_t axis, FVizDataArray* coordinates)
{
    FVizSize dims[3];
    FVizDataArray* retained;
    FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
    if (grid == NULL || axis >= 3u || coordinates == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear coordinate arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_rectilinear_grid_dimensions(grid, dims);
    if (fviz_rectilinear_validate_coordinate_array(coordinates, dims[axis]) != FVIZ_OK)
        return fviz_last_error_code();
    if (grid->coordinates[axis] == coordinates) return FVIZ_OK;
    retained = (FVizDataArray*)fviz_retain(coordinates);
    if (retained == NULL) return fviz_last_error_code();
    if (fviz_object_add_observer(
            (FVizObject*)retained, FVIZ_EVENT_MODIFIED, 0.0f,
            fviz_rectilinear_grid_dependency_modified, grid, &tag) != FVIZ_OK)
    {
        fviz_release(retained);
        return fviz_last_error_code();
    }
    fviz_rectilinear_detach_coordinate(grid, axis);
    grid->coordinates[axis] = retained;
    grid->coordinate_modified_tags[axis] = tag;
    fviz_object_modified((FVizObject*)grid);
    return FVIZ_OK;
}

FVizResult fviz_rectilinear_grid_set_coordinate_values(
    FVizRectilinearGrid* grid, uint32_t axis, const double* values, FVizSize count)
{
    FVizDataArray* coordinates = NULL;
    FVizResult result;
    if (grid == NULL || axis >= 3u || (count != 0u && values == NULL))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear coordinate values are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &coordinates) != FVIZ_OK)
        return fviz_last_error_code();
    result = fviz_data_array_append_tuples(coordinates, values, count);
    if (result == FVIZ_OK) result = fviz_rectilinear_grid_set_coordinates(grid, axis, coordinates);
    fviz_release(coordinates);
    return result;
}

FVizDataArray* fviz_rectilinear_grid_coordinates(FVizRectilinearGrid* grid, uint32_t axis)
{ return grid != NULL && axis < 3u ? grid->coordinates[axis] : NULL; }
const FVizDataArray* fviz_rectilinear_grid_const_coordinates(
    const FVizRectilinearGrid* grid, uint32_t axis)
{ return grid != NULL && axis < 3u ? grid->coordinates[axis] : NULL; }

FVizResult fviz_rectilinear_grid_point_id(
    const FVizRectilinearGrid* grid, int64_t i, int64_t j, int64_t k, FVizId* out_point_id)
{
    FVizSize dims[3], plane, id, x, y, z;
    if (out_point_id != NULL) *out_point_id = FVIZ_INVALID_ID;
    if (grid == NULL || out_point_id == NULL || i < grid->extent[0] || i > grid->extent[1] ||
        j < grid->extent[2] || j > grid->extent[3] || k < grid->extent[4] || k > grid->extent[5])
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear point index is outside extent");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_rectilinear_grid_dimensions(grid, dims);
    x = (FVizSize)(i - grid->extent[0]);
    y = (FVizSize)(j - grid->extent[2]);
    z = (FVizSize)(k - grid->extent[4]);
    if (fviz_size_multiply(dims[0], dims[1], &plane) != FVIZ_OK ||
        fviz_size_multiply(z, plane, &id) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    id += y * dims[0] + x;
    *out_point_id = (FVizId)id;
    return FVIZ_OK;
}

FVizResult fviz_rectilinear_grid_point_ijk(
    const FVizRectilinearGrid* grid, FVizId point_id, int64_t out_ijk[3])
{
    FVizSize dims[3], plane, id;
    if (grid == NULL || out_ijk == NULL || point_id >= (FVizId)fviz_rectilinear_grid_point_count(grid))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear point ID is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_rectilinear_grid_dimensions(grid, dims);
    if (fviz_size_multiply(dims[0], dims[1], &plane) != FVIZ_OK || plane == 0u)
        return FVIZ_ERROR_INVALID_STATE;
    id = (FVizSize)point_id;
    out_ijk[2] = grid->extent[4] + (int64_t)(id / plane);
    id %= plane;
    out_ijk[1] = grid->extent[2] + (int64_t)(id / dims[0]);
    out_ijk[0] = grid->extent[0] + (int64_t)(id % dims[0]);
    return FVIZ_OK;
}

FVizResult fviz_rectilinear_grid_point(
    const FVizRectilinearGrid* grid, FVizId point_id, FVizVec3* out_point)
{
    int64_t ijk[3];
    uint32_t axis;
    float values[3];
    if (grid == NULL || out_point == NULL ||
        fviz_rectilinear_grid_point_ijk(grid, point_id, ijk) != FVIZ_OK)
        return fviz_last_error_code();
    for (axis = 0u; axis < 3u; ++axis)
    {
        const int64_t minimum = grid->extent[axis * 2u];
        double value = 0.0;
        if (grid->coordinates[axis] == NULL ||
            fviz_data_array_get_component(
                grid->coordinates[axis], (FVizSize)(ijk[axis] - minimum), 0u, &value) != FVIZ_OK)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "rectilinear coordinate array is missing");
            return FVIZ_ERROR_INVALID_STATE;
        }
        values[axis] = (float)value;
    }
    *out_point = fviz_vec3(values[0], values[1], values[2]);
    return FVIZ_OK;
}

FVizBounds fviz_rectilinear_grid_bounds(const FVizRectilinearGrid* grid)
{
    FVizBounds bounds = fviz_bounds_empty();
    double minimum[3], maximum[3];
    uint32_t axis;
    if (grid == NULL || fviz_rectilinear_grid_point_count(grid) == 0u) return bounds;
    for (axis = 0u; axis < 3u; ++axis)
    {
        if (grid->coordinates[axis] == NULL ||
            fviz_data_array_get_range(grid->coordinates[axis], 0, FVIZ_TRUE,
                &minimum[axis], &maximum[axis]) != FVIZ_OK)
            return fviz_bounds_empty();
    }
    bounds.min = fviz_vec3((float)minimum[0], (float)minimum[1], (float)minimum[2]);
    bounds.max = fviz_vec3((float)maximum[0], (float)maximum[1], (float)maximum[2]);
    return bounds;
}

FVizResult fviz_rectilinear_grid_cell_id(
    const FVizRectilinearGrid* grid, int64_t i, int64_t j, int64_t k, FVizId* out_cell_id)
{
    FVizSize cell_dims[3], x, y, z, plane, id;
    if (out_cell_id != NULL) *out_cell_id = FVIZ_INVALID_ID;
    if (grid == NULL || out_cell_id == NULL || fviz_rectilinear_grid_cell_count(grid) == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear cell request is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_rectilinear_cell_dimensions(grid, cell_dims);
    if (i < grid->extent[0] || (FVizSize)(i - grid->extent[0]) >= cell_dims[0] ||
        j < grid->extent[2] || (FVizSize)(j - grid->extent[2]) >= cell_dims[1] ||
        k < grid->extent[4] || (FVizSize)(k - grid->extent[4]) >= cell_dims[2])
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear cell index is outside extent");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    x = (FVizSize)(i - grid->extent[0]); y = (FVizSize)(j - grid->extent[2]); z = (FVizSize)(k - grid->extent[4]);
    if (fviz_size_multiply(cell_dims[0], cell_dims[1], &plane) != FVIZ_OK ||
        fviz_size_multiply(z, plane, &id) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    id += y * cell_dims[0] + x;
    *out_cell_id = (FVizId)id;
    return FVIZ_OK;
}

FVizResult fviz_rectilinear_grid_cell_ijk(
    const FVizRectilinearGrid* grid, FVizId cell_id, int64_t out_ijk[3])
{
    FVizSize cell_dims[3], plane, id;
    if (grid == NULL || out_ijk == NULL || cell_id >= (FVizId)fviz_rectilinear_grid_cell_count(grid))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear cell ID is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_rectilinear_cell_dimensions(grid, cell_dims);
    if (fviz_size_multiply(cell_dims[0], cell_dims[1], &plane) != FVIZ_OK || plane == 0u)
        return FVIZ_ERROR_INVALID_STATE;
    id = (FVizSize)cell_id;
    out_ijk[2] = grid->extent[4] + (int64_t)(id / plane);
    id %= plane;
    out_ijk[1] = grid->extent[2] + (int64_t)(id / cell_dims[0]);
    out_ijk[0] = grid->extent[0] + (int64_t)(id % cell_dims[0]);
    return FVIZ_OK;
}

FVizResult fviz_rectilinear_grid_cell_point_ids(
    const FVizRectilinearGrid* grid, FVizId cell_id, FVizId out_point_ids[8], uint32_t* out_point_count)
{
    int64_t ijk[3];
    FVizSize dims[3];
    uint32_t active[3], active_count = 0u, axis, corner, corner_count;
    if (out_point_count != NULL) *out_point_count = 0u;
    if (grid == NULL || out_point_ids == NULL || out_point_count == NULL ||
        fviz_rectilinear_grid_cell_ijk(grid, cell_id, ijk) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_rectilinear_grid_dimensions(grid, dims);
    for (axis = 0u; axis < 3u; ++axis) if (dims[axis] > 1u) active[active_count++] = axis;
    corner_count = 1u << active_count;
    for (corner = 0u; corner < corner_count; ++corner)
    {
        int64_t pijk[3] = {ijk[0], ijk[1], ijk[2]};
        uint32_t bit;
        static const uint8_t order2[4] = {0u, 1u, 3u, 2u};
        static const uint8_t order3[8] = {0u, 1u, 3u, 2u, 4u, 5u, 7u, 6u};
        const uint32_t code = active_count == 2u ? order2[corner] :
            (active_count == 3u ? order3[corner] : corner);
        for (bit = 0u; bit < active_count; ++bit)
            if ((code & (1u << bit)) != 0u) ++pijk[active[bit]];
        if (fviz_rectilinear_grid_point_id(
                grid, pijk[0], pijk[1], pijk[2], &out_point_ids[corner]) != FVIZ_OK)
            return fviz_last_error_code();
    }
    *out_point_count = corner_count;
    return FVIZ_OK;
}

FVizAttributeSet* fviz_rectilinear_grid_point_data(FVizRectilinearGrid* grid)
{ return grid != NULL ? fviz_data_set_point_data(grid->data_set) : NULL; }
FVizAttributeSet* fviz_rectilinear_grid_cell_data(FVizRectilinearGrid* grid)
{ return grid != NULL ? fviz_data_set_cell_data(grid->data_set) : NULL; }
FVizAttributeSet* fviz_rectilinear_grid_field_data(FVizRectilinearGrid* grid)
{ return grid != NULL ? fviz_data_set_field_data(grid->data_set) : NULL; }
const FVizAttributeSet* fviz_rectilinear_grid_const_point_data(const FVizRectilinearGrid* grid)
{ return grid != NULL ? fviz_data_set_point_data(grid->data_set) : NULL; }
const FVizAttributeSet* fviz_rectilinear_grid_const_cell_data(const FVizRectilinearGrid* grid)
{ return grid != NULL ? fviz_data_set_cell_data(grid->data_set) : NULL; }
const FVizAttributeSet* fviz_rectilinear_grid_const_field_data(const FVizRectilinearGrid* grid)
{ return grid != NULL ? fviz_data_set_field_data(grid->data_set) : NULL; }

FVizResult fviz_rectilinear_grid_validate(const FVizRectilinearGrid* grid)
{
    FVizSize dims[3];
    uint32_t axis;
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rectilinear grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_rectilinear_grid_dimensions(grid, dims);
    if (fviz_rectilinear_grid_point_count(grid) != 0u)
        for (axis = 0u; axis < 3u; ++axis)
            if (fviz_rectilinear_validate_coordinate_array(grid->coordinates[axis], dims[axis]) != FVIZ_OK)
                return fviz_last_error_code();
    return fviz_data_set_validate(grid->data_set);
}
