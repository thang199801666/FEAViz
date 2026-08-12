#include <math.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizResult build_grid(FVizUnstructuredGrid** out_grid, FVizDataArray** out_stress, FVizDataArray** out_displacement)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
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
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &stress) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &displacement) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_resize(stress, fviz_unstructured_grid_cell_count(grid)) != FVIZ_OK) return fviz_last_error_code();
    {
        FVizSize i;
        for (i = 0u; i < fviz_unstructured_grid_cell_count(grid); ++i)
        {
            const float value = (float)i;
            if (fviz_data_array_set_tuple(stress, i, &value) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_data_array_resize(displacement, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK) return fviz_last_error_code();
    {
        const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(grid));
        FVizSize i;
        for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        {
            const float tuple[3] = {points[i].x * 0.1f, 0.0f, 0.0f};
            if (fviz_data_array_set_tuple(displacement, i, tuple) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid), "stress", stress) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "displacement", displacement) != FVIZ_OK) return fviz_last_error_code();
    *out_grid = grid;
    *out_stress = stress;
    *out_displacement = displacement;
    return FVIZ_OK;
}

static int test_threshold_filter(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizFilter* filter = NULL;
    FVizUnstructuredGrid* output = NULL;
    CHECK(build_grid(&grid, &stress, &displacement) == FVIZ_OK);
    CHECK(fviz_threshold_filter_create("stress", 2.0, 5.0, &filter) == FVIZ_OK);
    CHECK(filter != NULL);
    CHECK(fviz_object_type_id((const FVizObject*)filter) == FVIZ_TYPE_THRESHOLD_FILTER);
    CHECK(fviz_filter_set_input(filter, grid) == FVIZ_OK);
    CHECK(fviz_filter_update(filter) == FVIZ_OK);
    output = fviz_filter_output(filter);
    CHECK(output != NULL);
    CHECK(fviz_unstructured_grid_cell_count(output) <= fviz_unstructured_grid_cell_count(grid));
    CHECK(fviz_unstructured_grid_cell_count(output) > 0u);
    CHECK(fviz_filter_update(filter) == FVIZ_OK);
    CHECK(fviz_filter_output(filter) == output);
    fviz_release(filter);
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

static int test_warp_filter(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizFilter* filter = NULL;
    FVizUnstructuredGrid* output = NULL;
    const FVizVec3* points;
    const FVizVec3* original;
    FVizSize i;
    CHECK(build_grid(&grid, &stress, &displacement) == FVIZ_OK);
    CHECK(fviz_warp_filter_create("displacement", 2.0, &filter) == FVIZ_OK);
    CHECK(fviz_object_type_id((const FVizObject*)filter) == FVIZ_TYPE_WARP_FILTER);
    CHECK(fviz_filter_set_input(filter, grid) == FVIZ_OK);
    CHECK(fviz_filter_update(filter) == FVIZ_OK);
    output = fviz_filter_output(filter);
    CHECK(output != NULL);
    points = fviz_points_data(fviz_unstructured_grid_points(output));
    original = fviz_points_data(fviz_unstructured_grid_points(grid));
    for (i = 0u; i < fviz_unstructured_grid_point_count(output); ++i)
    {
        CHECK(fabsf(points[i].x - (original[i].x + original[i].x * 0.1f * 2.0f)) < 1.0e-4f);
    }
    fviz_release(filter);
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

static int test_cell_to_point_filter(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizFilter* filter = NULL;
    FVizUnstructuredGrid* output = NULL;
    const FVizDataArray* values;
    const float* data;
    CHECK(build_grid(&grid, &stress, &displacement) == FVIZ_OK);
    CHECK(fviz_cell_data_to_point_filter_create(&filter) == FVIZ_OK);
    CHECK(fviz_object_type_id((const FVizObject*)filter) == FVIZ_TYPE_CELL_DATA_TO_POINT_FILTER);
    CHECK(fviz_filter_set_input(filter, grid) == FVIZ_OK);
    CHECK(fviz_filter_update(filter) == FVIZ_OK);
    output = fviz_filter_output(filter);
    CHECK(output != NULL);
    values = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(output), "stress");
    CHECK(values != NULL);
    data = (const float*)fviz_data_array_const_data((FVizDataArray*)values);
    CHECK(fabsf(data[13] - 3.5f) < 1.0e-4f);
    fviz_release(filter);
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

int main(void)
{
    CHECK(test_threshold_filter() == 0);
    CHECK(test_warp_filter() == 0);
    CHECK(test_cell_to_point_filter() == 0);
    return 0;
}
