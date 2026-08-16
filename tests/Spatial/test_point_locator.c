#include <math.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizResult build_grid(FVizUnstructuredGrid** out_grid, FVizDataArray** out_temperature, FVizDataArray** out_displacement)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* temperature = NULL;
    FVizDataArray* displacement = NULL;
    FVizSize z;
    FVizSize y;
    FVizSize x;
    const FVizSize n = 4u;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return fviz_last_error_code();
    for (z = 0u; z < n; ++z)
        for (y = 0u; y < n; ++y)
            for (x = 0u; x < n; ++x)
                if (fviz_unstructured_grid_add_point(grid, fviz_vec3((float)x, (float)y, (float)z), NULL) != FVIZ_OK)
                    return fviz_last_error_code();
    for (z = 0u; z + 1u < n; ++z)
        for (y = 0u; y + 1u < n; ++y)
            for (x = 0u; x + 1u < n; ++x)
            {
                const uint32_t base = (uint32_t)(z * n * n + y * n + x);
                const uint32_t n32 = (uint32_t)n;
                const uint32_t ids[8] = {
                    base, base + 1u, base + n32 + 1u, base + n32,
                    base + n32 * n32, base + n32 * n32 + 1u,
                    base + n32 * n32 + n32 + 1u, base + n32 * n32 + n32
                };
                if (fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, ids) != FVIZ_OK)
                    return fviz_last_error_code();
            }
    if (fviz_cell_array_convert_id_storage(fviz_unstructured_grid_cells(grid), FVIZ_ID_STORAGE_UINT64) != FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &temperature) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &displacement) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_resize(temperature, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_resize(displacement, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK) return fviz_last_error_code();
    {
        const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(grid));
        FVizSize i;
        for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        {
            const float temp = points[i].x + points[i].y + points[i].z;
            const float disp[3] = {points[i].x * 0.5f, points[i].y * 0.5f, 0.0f};
            if (fviz_data_array_set_tuple(temperature, i, &temp) != FVIZ_OK) return fviz_last_error_code();
            if (fviz_data_array_set_tuple(displacement, i, disp) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "temperature", temperature) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "displacement", displacement) != FVIZ_OK) return fviz_last_error_code();
    *out_grid = grid;
    *out_temperature = temperature;
    *out_displacement = displacement;
    return FVIZ_OK;
}

static int test_locate_hex_center(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* temperature = NULL;
    FVizDataArray* displacement = NULL;
    FVizPointLocator* locator = NULL;
    FVizLocatedCell location;
    float value = 0.0f;
    FVizVec3 vector;
    FVizVec3 query = fviz_vec3(1.5f, 1.5f, 1.5f);
    CHECK(build_grid(&grid, &temperature, &displacement) == FVIZ_OK);
    CHECK(fviz_point_locator_create(&locator) == FVIZ_OK);
    CHECK(fviz_point_locator_set_grid(locator, grid) == FVIZ_OK);
    CHECK(fviz_point_locator_acceleration_valid(locator) == FVIZ_TRUE);
    CHECK(fviz_point_locator_indexed_cell_count(locator) == fviz_unstructured_grid_cell_count(grid));
    CHECK(fviz_point_locator_locate_point(locator, query, &location) == FVIZ_TRUE);
    CHECK(fviz_point_locator_interpolate_scalar(locator, "temperature", query, &value) == FVIZ_OK);
    CHECK(fabsf(value - 4.5f) < 1.0e-3f);
    /* Field-result updates must not invalidate geometry acceleration. */
    CHECK(fviz_data_array_set_component(temperature, fviz_data_array_tuple_count(temperature) - 1u, 0u, 123.0) == FVIZ_OK);
    CHECK(fviz_point_locator_acceleration_valid(locator) == FVIZ_TRUE);
    /* Coordinate deformation invalidates bounds but can be refit without rebuilding
       the partition when the cell count remains unchanged. */
    CHECK(fviz_points_set(
        fviz_unstructured_grid_points(grid), fviz_unstructured_grid_point_count(grid) - 1u,
        fviz_vec3(3.1f, 3.0f, 3.0f)) == FVIZ_OK);
    CHECK(fviz_point_locator_acceleration_valid(locator) == FVIZ_FALSE);
    CHECK(fviz_point_locator_refit_required(locator) == FVIZ_TRUE);
    CHECK(fviz_point_locator_update(locator) == FVIZ_OK);
    CHECK(fviz_point_locator_refit_required(locator) == FVIZ_FALSE);
    CHECK(fviz_point_locator_acceleration_valid(locator) == FVIZ_TRUE);
    vector = fviz_point_locator_interpolate_vector(locator, "displacement", query);
    CHECK(fabsf(vector.x - 0.75f) < 1.0e-3f);
    CHECK(fabsf(vector.y - 0.75f) < 1.0e-3f);
    CHECK(fabsf(vector.z) < 1.0e-4f);
    query = fviz_vec3(0.25f, 0.25f, 0.25f);
    CHECK(fviz_point_locator_interpolate_scalar(locator, "temperature", query, &value) == FVIZ_OK);
    CHECK(fabsf(value - 0.75f) < 1.0e-3f);
    vector = fviz_point_locator_interpolate_vector(locator, "displacement", query);
    CHECK(fabsf(vector.x - 0.125f) < 1.0e-3f);
    CHECK(fabsf(vector.y - 0.125f) < 1.0e-3f);
    {
        const uint32_t extra_hex[8] = {
            0u, 1u, 5u, 4u,
            16u, 17u, 21u, 20u
        };
        CHECK(fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, extra_hex) == FVIZ_OK);
    }
    CHECK(fviz_point_locator_acceleration_valid(locator) == FVIZ_FALSE);
    CHECK(fviz_point_locator_refit_required(locator) == FVIZ_FALSE);
    /* A stale locator still has the documented brute-force query fallback. */
    CHECK(fviz_point_locator_locate_point(locator, query, &location) == FVIZ_TRUE);
    CHECK(fviz_point_locator_update(locator) == FVIZ_OK);
    CHECK(fviz_point_locator_acceleration_valid(locator) == FVIZ_TRUE);
    fviz_release(locator);
    fviz_release(displacement);
    fviz_release(temperature);
    fviz_release(grid);
    return 0;
}

static int test_locate_outside(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* temperature = NULL;
    FVizDataArray* displacement = NULL;
    FVizPointLocator* locator = NULL;
    FVizLocatedCell location;
    float value = 0.0f;
    CHECK(build_grid(&grid, &temperature, &displacement) == FVIZ_OK);
    CHECK(fviz_point_locator_create(&locator) == FVIZ_OK);
    CHECK(fviz_point_locator_set_grid(locator, grid) == FVIZ_OK);
    CHECK(fviz_point_locator_locate_point(locator, fviz_vec3(10.0f, 10.0f, 10.0f), &location) == FVIZ_FALSE);
    CHECK(fviz_point_locator_interpolate_scalar(locator, "temperature", fviz_vec3(10.0f, 10.0f, 10.0f), &value) == FVIZ_ERROR_NOT_FOUND);
    fviz_release(locator);
    fviz_release(displacement);
    fviz_release(temperature);
    fviz_release(grid);
    return 0;
}

static int test_locate_missing_field(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* temperature = NULL;
    FVizDataArray* displacement = NULL;
    FVizPointLocator* locator = NULL;
    float value = 0.0f;
    CHECK(build_grid(&grid, &temperature, &displacement) == FVIZ_OK);
    CHECK(fviz_point_locator_create(&locator) == FVIZ_OK);
    CHECK(fviz_point_locator_set_grid(locator, grid) == FVIZ_OK);
    CHECK(fviz_point_locator_interpolate_scalar(locator, "missing", fviz_vec3(1.5f, 1.5f, 1.5f), &value) == FVIZ_ERROR_NOT_FOUND);
    fviz_release(locator);
    fviz_release(displacement);
    fviz_release(temperature);
    fviz_release(grid);
    return 0;
}

int main(void)
{
    CHECK(test_locate_hex_center() == 0);
    CHECK(test_locate_outside() == 0);
    CHECK(test_locate_missing_field() == 0);
    return 0;
}
