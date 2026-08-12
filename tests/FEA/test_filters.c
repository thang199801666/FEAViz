#include <math.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizResult build_grid(FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizSize z;
    FVizSize y;
    FVizSize x;
    const FVizSize n = 3u;
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
    *out_grid = grid;
    return FVIZ_OK;
}

static int test_warp_by_vector(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* warped = NULL;
    FVizDataArray* displacement = NULL;
    const FVizVec3* original_points;
    const FVizVec3* warped_points;
    FVizSize i;
    CHECK(build_grid(&grid) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &displacement) == FVIZ_OK);
    CHECK(fviz_data_array_resize(displacement, fviz_unstructured_grid_point_count(grid)) == FVIZ_OK);
    original_points = fviz_points_data(fviz_unstructured_grid_points(grid));
    for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
    {
        const float tuple[3] = {original_points[i].x * 0.1f, 0.0f, 0.0f};
        CHECK(fviz_data_array_set_tuple(displacement, i, tuple) == FVIZ_OK);
    }
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "displacement", displacement) == FVIZ_OK);

    CHECK(fviz_unstructured_grid_warp_by_vector(grid, "displacement", 2.0, &warped) == FVIZ_OK);
    CHECK(warped != NULL);
    CHECK(fviz_unstructured_grid_point_count(warped) == fviz_unstructured_grid_point_count(grid));
    CHECK(fviz_unstructured_grid_cell_count(warped) == fviz_unstructured_grid_cell_count(grid));
    warped_points = fviz_points_data(fviz_unstructured_grid_points(warped));
    for (i = 0u; i < fviz_unstructured_grid_point_count(warped); ++i)
    {
        CHECK(fabsf(warped_points[i].x - (original_points[i].x + original_points[i].x * 0.1f * 2.0f)) < 1.0e-4f);
        CHECK(fabsf(warped_points[i].y - original_points[i].y) < 1.0e-5f);
        CHECK(fabsf(warped_points[i].z - original_points[i].z) < 1.0e-5f);
    }
    CHECK(fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(warped), "displacement") != NULL);
    fviz_release(warped);
    fviz_release(displacement);
    fviz_release(grid);
    return 0;
}

static int test_warp_invalid(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* warped = NULL;
    CHECK(build_grid(&grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_warp_by_vector(grid, "missing", 1.0, &warped) == FVIZ_ERROR_INVALID_ARGUMENT);
    fviz_release(grid);
    return 0;
}

static int test_cell_data_to_point_data(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* converted = NULL;
    FVizDataArray* cell_values = NULL;
    const FVizDataArray* point_values;
    const float* values;
    float expected_center;
    FVizSize i;
    CHECK(build_grid(&grid) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &cell_values) == FVIZ_OK);
    CHECK(fviz_data_array_resize(cell_values, fviz_unstructured_grid_cell_count(grid)) == FVIZ_OK);
    for (i = 0u; i < fviz_unstructured_grid_cell_count(grid); ++i)
    {
        const float value = (float)i;
        CHECK(fviz_data_array_set_tuple(cell_values, i, &value) == FVIZ_OK);
    }
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid), "stress", cell_values) == FVIZ_OK);

    CHECK(fviz_unstructured_grid_cell_data_to_point_data(grid, &converted) == FVIZ_OK);
    CHECK(converted != NULL);
    point_values = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(converted), "stress");
    CHECK(point_values != NULL);
    CHECK(fviz_data_array_tuple_count(point_values) == fviz_unstructured_grid_point_count(converted));
    values = (const float*)fviz_data_array_const_data((FVizDataArray*)point_values);

    expected_center = (0.0f + 1.0f + 2.0f + 3.0f + 4.0f + 5.0f + 6.0f + 7.0f) / 8.0f;
    CHECK(fabsf(values[13] - expected_center) < 1.0e-4f);
    CHECK(fabsf(values[0] - 0.0f) < 1.0e-4f);
    CHECK(fabsf(values[26] - 7.0f) < 1.0e-4f);

    fviz_release(converted);
    fviz_release(cell_values);
    fviz_release(grid);
    return 0;
}

int main(void)
{
    CHECK(test_warp_by_vector() == 0);
    CHECK(test_warp_invalid() == 0);
    CHECK(test_cell_data_to_point_data() == 0);
    return 0;
}
