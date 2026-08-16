#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); return 1; \
} } while (0)

int main(void)
{
    FVizStructuredGrid* grid = NULL;
    const int64_t extent[6] = {10, 12, -2, -1, 5, 6};
    FVizVec3 points[12];
    FVizId id = FVIZ_INVALID_ID;
    FVizId cell_id = FVIZ_INVALID_ID;
    FVizId cell_points[8] = {0};
    uint32_t cell_point_count = 0u;
    int64_t ijk[3] = {0, 0, 0};
    FVizSize dims[3] = {0u, 0u, 0u};
    FVizBounds bounds;
    FVizDataArray* scalars = NULL;
    FVizDataArray* cell_scalars = NULL;
    FVizStructuredGridGeometryFilter* geometry = NULL;
    float values[12];
    float cell_values[2] = {10.0f, 20.0f};
    FVizMTime before;
    uint32_t i;

    CHECK(fviz_structured_grid_create(&grid) == FVIZ_OK);
    CHECK(fviz_object_is_type((FVizObject*)grid, FVIZ_TYPE_DATA_OBJECT) == FVIZ_TRUE);
    CHECK(fviz_structured_grid_set_extent(grid, extent) == FVIZ_OK);
    fviz_structured_grid_dimensions(grid, dims);
    CHECK(dims[0] == 3u && dims[1] == 2u && dims[2] == 2u);
    CHECK(fviz_structured_grid_dimension(grid) == 3u);
    CHECK(fviz_structured_grid_point_count(grid) == 12u);
    CHECK(fviz_structured_grid_cell_count(grid) == 2u);
    CHECK(fviz_structured_grid_cell_type(grid) == FVIZ_CELL_HEXAHEDRON);

    for (i = 0u; i < 12u; ++i)
    {
        int64_t pijk[3];
        CHECK(fviz_structured_grid_point_ijk(grid, (FVizId)i, pijk) == FVIZ_OK);
        points[i] = fviz_vec3(
            (float)(pijk[0] - extent[0]),
            (float)(2 * (pijk[1] - extent[2])),
            (float)(3 * (pijk[2] - extent[4])));
        values[i] = (float)i;
    }
    CHECK(fviz_structured_grid_set_points(grid, points, 12u) == FVIZ_OK);
    CHECK(fviz_structured_grid_validate(grid) == FVIZ_OK);

    CHECK(fviz_structured_grid_point_id(grid, 12, -1, 6, &id) == FVIZ_OK);
    CHECK(id == 11u);
    CHECK(fviz_structured_grid_point_ijk(grid, id, ijk) == FVIZ_OK);
    CHECK(ijk[0] == 12 && ijk[1] == -1 && ijk[2] == 6);

    CHECK(fviz_structured_grid_cell_id(grid, 11, -2, 5, &cell_id) == FVIZ_OK);
    CHECK(cell_id == 1u);
    CHECK(fviz_structured_grid_cell_point_ids(
        grid, cell_id, cell_points, &cell_point_count) == FVIZ_OK);
    CHECK(cell_point_count == 8u);
    CHECK(cell_points[0] == 1u && cell_points[1] == 2u);
    CHECK(cell_points[2] == 5u && cell_points[3] == 4u);
    CHECK(cell_points[4] == 7u && cell_points[5] == 8u);
    CHECK(cell_points[6] == 11u && cell_points[7] == 10u);

    bounds = fviz_structured_grid_bounds(grid);
    CHECK(bounds.valid == FVIZ_TRUE);
    CHECK(bounds.min.x == 0.0f && bounds.max.x == 2.0f);
    CHECK(bounds.min.y == 0.0f && bounds.max.y == 2.0f);
    CHECK(bounds.min.z == 0.0f && bounds.max.z == 3.0f);

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalars) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(scalars, values, 12u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(
        fviz_structured_grid_point_data(grid), "S", scalars) == FVIZ_OK);
    CHECK(fviz_structured_grid_validate(grid) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &cell_scalars) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(cell_scalars, cell_values, 2u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(
        fviz_structured_grid_cell_data(grid), "E", cell_scalars) == FVIZ_OK);
    {
        FVizStructuredGridExtractFilter* extract = NULL;
        FVizStructuredGrid* subset;
        const int64_t requested[6] = {11, 12, -2, -1, 5, 6};
        int64_t whole[6];
        const FVizDataArray* subset_points;
        const FVizDataArray* subset_cells;
        float v = 0.0f;
        CHECK(fviz_structured_grid_extract_filter_create(&extract) == FVIZ_OK);
        CHECK(fviz_structured_grid_extract_filter_set_input_data(extract, grid) == FVIZ_OK);
        CHECK(fviz_structured_grid_extract_filter_update_extent(extract, requested, 1u) == FVIZ_OK);
        subset = fviz_structured_grid_extract_filter_output(extract);
        CHECK(subset != NULL);
        CHECK(fviz_structured_grid_point_count(subset) == 8u);
        CHECK(fviz_structured_grid_cell_count(subset) == 1u);
        subset_points = fviz_attribute_set_const_get(fviz_structured_grid_const_point_data(subset), "S");
        subset_cells = fviz_attribute_set_const_get(fviz_structured_grid_const_cell_data(subset), "E");
        CHECK(subset_points != NULL && fviz_data_array_tuple_count(subset_points) == 8u);
        CHECK(subset_cells != NULL && fviz_data_array_tuple_count(subset_cells) == 1u);
        CHECK(fviz_data_array_get_component(subset_points, 0u, 0u, (double*)&(double){0}) == FVIZ_OK);
        CHECK(fviz_data_array_const_tuple(subset_cells, 0u) != NULL);
        (void)memcpy(&v, fviz_data_array_const_tuple(subset_cells, 0u), sizeof(v));
        CHECK(v == 20.0f);
        CHECK(fviz_algorithm_output_whole_extent(
            fviz_structured_grid_extract_filter_algorithm(extract), 0u, whole) == FVIZ_TRUE);
        CHECK(memcmp(whole, extent, sizeof(extent)) == 0);
        fviz_release(extract);
    }

    CHECK(fviz_structured_grid_geometry_filter_create(&geometry) == FVIZ_OK);
    CHECK(fviz_structured_grid_geometry_filter_set_input_data(geometry, grid) == FVIZ_OK);
    CHECK(fviz_structured_grid_geometry_filter_update(geometry) == FVIZ_OK);
    {
        FVizPolyData* surface = fviz_structured_grid_geometry_filter_output(geometry);
        const FVizDataArray* original;
        const FVizDataArray* copied_cells;
        CHECK(surface != NULL);
        CHECK(fviz_poly_data_point_count(surface) == 12u);
        CHECK(fviz_poly_data_triangle_count(surface) == 20u);
        original = fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(surface), "FVizOriginalCellIds");
        copied_cells = fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(surface), "E");
        CHECK(original != NULL && fviz_data_array_tuple_count(original) == 20u);
        CHECK(copied_cells != NULL && fviz_data_array_tuple_count(copied_cells) == 20u);
    }
    before = fviz_object_mtime((FVizObject*)grid);
    values[0] = 100.0f;
    CHECK(fviz_data_array_set_tuple(scalars, 0u, values) == FVIZ_OK);
    CHECK(fviz_object_mtime((FVizObject*)grid) > before);

    /* Changing extent to a point count that conflicts with existing geometry or
       attributes must fail rather than silently corrupt structured indexing. */
    {
        const int64_t bad_extent[6] = {0, 1, 0, 1, 0, 1};
        CHECK(fviz_structured_grid_set_extent(grid, bad_extent) == FVIZ_ERROR_INVALID_STATE);
    }

    fviz_release(geometry);
    fviz_release(cell_scalars);
    fviz_release(scalars);
    fviz_release(grid);
    puts("structured grid tests passed");
    return 0;
}
