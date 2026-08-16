#include <math.h>
#include <string.h>

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
    FVizUnstructuredGrid* parallel_converted = NULL;
    FVizDataArray* cell_values = NULL;
    FVizDataArray* cell_vectors = NULL;
    FVizDataArray* cell_ghosts = NULL;
    FVizDataArray* cell_ghost_levels = NULL;
    FVizDataArray* original_cell_ids = NULL;
    FVizDataArray* point_ghosts = NULL;
    const FVizDataArray* point_values;
    const FVizDataArray* point_vectors;
    const float* values;
    const float* parallel_values;
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
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 3u, &cell_vectors) == FVIZ_OK);
    CHECK(fviz_data_array_resize(cell_vectors, fviz_unstructured_grid_cell_count(grid)) == FVIZ_OK);
    for (i = 0u; i < fviz_unstructured_grid_cell_count(grid); ++i)
    {
        const double v[3] = {(double)i, 2.0*(double)i, -(double)i};
        CHECK(fviz_data_array_set_tuple(cell_vectors, i, v) == FVIZ_OK);
    }
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid), "reaction", cell_vectors) == FVIZ_OK);
    CHECK(fviz_attribute_set_set_active(fviz_unstructured_grid_cell_data(grid), FVIZ_ATTRIBUTE_VECTORS, "reaction") == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &cell_ghosts) == FVIZ_OK);
    CHECK(fviz_data_array_resize(cell_ghosts, fviz_unstructured_grid_cell_count(grid)) == FVIZ_OK);
    ((uint8_t*)fviz_data_array_data(cell_ghosts))[7] = (uint8_t)FVIZ_GHOST_DUPLICATE;
    fviz_object_modified((FVizObject*)cell_ghosts);
    CHECK(fviz_attribute_set_add(
        fviz_unstructured_grid_cell_data(grid), FVIZ_GHOST_ARRAY_NAME, cell_ghosts) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT16, 1u, &cell_ghost_levels) == FVIZ_OK);
    CHECK(fviz_data_array_resize(cell_ghost_levels, fviz_unstructured_grid_cell_count(grid)) == FVIZ_OK);
    ((uint16_t*)fviz_data_array_data(cell_ghost_levels))[7] = 1u;
    fviz_object_modified((FVizObject*)cell_ghost_levels);
    CHECK(fviz_attribute_set_add(
        fviz_unstructured_grid_cell_data(grid), FVIZ_GHOST_LEVEL_ARRAY_NAME, cell_ghost_levels) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &original_cell_ids) == FVIZ_OK);
    CHECK(fviz_data_array_resize(original_cell_ids, fviz_unstructured_grid_cell_count(grid)) == FVIZ_OK);
    for (i = 0u; i < fviz_unstructured_grid_cell_count(grid); ++i)
        ((uint64_t*)fviz_data_array_data(original_cell_ids))[i] = (uint64_t)(100u + i);
    fviz_object_modified((FVizObject*)original_cell_ids);
    CHECK(fviz_attribute_set_add(
        fviz_unstructured_grid_cell_data(grid), "FVizOriginalCellIds", original_cell_ids) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &point_ghosts) == FVIZ_OK);
    CHECK(fviz_data_array_resize(point_ghosts, fviz_unstructured_grid_point_count(grid)) == FVIZ_OK);
    ((uint8_t*)fviz_data_array_data(point_ghosts))[0] = (uint8_t)FVIZ_GHOST_DUPLICATE;
    fviz_object_modified((FVizObject*)point_ghosts);
    CHECK(fviz_attribute_set_add(
        fviz_unstructured_grid_point_data(grid), FVIZ_GHOST_ARRAY_NAME, point_ghosts) == FVIZ_OK);

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
    point_vectors = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(converted), "reaction");
    CHECK(point_vectors != NULL && fviz_data_array_components(point_vectors) == 3u);
    CHECK(fviz_data_array_type(point_vectors) == FVIZ_DATA_FLOAT64);
    {
        double x=0.0,y=0.0,z=0.0;
        CHECK(fviz_data_array_get_component(point_vectors,13u,0u,&x)==FVIZ_OK);
        CHECK(fviz_data_array_get_component(point_vectors,13u,1u,&y)==FVIZ_OK);
        CHECK(fviz_data_array_get_component(point_vectors,13u,2u,&z)==FVIZ_OK);
        CHECK(fabs(x-(double)expected_center)<1.0e-10);
        CHECK(fabs(y-2.0*(double)expected_center)<1.0e-10);
        CHECK(fabs(z+(double)expected_center)<1.0e-10);
    }
    CHECK(strcmp(fviz_attribute_set_active_name(fviz_unstructured_grid_point_data(converted),FVIZ_ATTRIBUTE_VECTORS),"reaction")==0);
    {
        const FVizDataArray* preserved_ghosts = fviz_attribute_set_const_get(
            fviz_unstructured_grid_point_data(converted), FVIZ_GHOST_ARRAY_NAME);
        CHECK(preserved_ghosts != NULL);
        CHECK(fviz_data_array_type(preserved_ghosts) == FVIZ_DATA_UINT8);
        CHECK(fviz_data_array_tuple_count(preserved_ghosts) == fviz_unstructured_grid_point_count(converted));
        CHECK(((const uint8_t*)fviz_data_array_const_data(preserved_ghosts))[0] == (uint8_t)FVIZ_GHOST_DUPLICATE);
        CHECK(fviz_attribute_set_const_get(
            fviz_unstructured_grid_point_data(converted), FVIZ_GHOST_LEVEL_ARRAY_NAME) == NULL);
        CHECK(fviz_attribute_set_const_get(
            fviz_unstructured_grid_point_data(converted), "FVizOriginalCellIds") == NULL);
    }

    fviz_parallel_set_thread_limit(4u);
    CHECK(fviz_unstructured_grid_cell_data_to_point_data(grid, &parallel_converted) == FVIZ_OK);
    parallel_values = (const float*)fviz_data_array_const_data(
        fviz_attribute_set_const_get(
            fviz_unstructured_grid_point_data(parallel_converted), "stress"));
    for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        CHECK(fabsf(values[i] - parallel_values[i]) < 1.0e-6f);
    fviz_parallel_set_thread_limit(0u);

    fviz_release(converted);
    fviz_release(parallel_converted);
    fviz_release(point_ghosts);
    fviz_release(original_cell_ids);
    fviz_release(cell_ghost_levels);
    fviz_release(cell_ghosts);
    fviz_release(cell_values);
    fviz_release(cell_vectors);
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
