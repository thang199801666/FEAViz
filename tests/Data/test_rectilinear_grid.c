#include <math.h>
#include <string.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int nearf32(float a, float b) { return fabsf(a - b) < 1.0e-5f; }

int main(void)
{
    FVizRectilinearGrid* grid = NULL;
    const int64_t extent[6] = {10, 12, -2, -1, 5, 6};
    const double x[3] = {0.0, 2.0, 5.0};
    const double y[2] = {-1.0, 1.0};
    const double z[2] = {10.0, 20.0};
    const double bad_x[3] = {0.0, 2.0, 1.0};
    FVizSize dims[3];
    FVizId id = FVIZ_INVALID_ID;
    FVizId corners[8];
    uint32_t corner_count = 0u;
    FVizVec3 point;
    FVizBounds bounds;
    FVizMTime before;
    FVizDataArray* coordinate;
    FVizDataArray* point_values = NULL;
    FVizDataArray* cell_values = NULL;
    double replacement = 3.0;

    CHECK(fviz_rectilinear_grid_create(&grid) == FVIZ_OK);
    CHECK(fviz_rectilinear_grid_set_extent(grid, extent) == FVIZ_OK);
    fviz_rectilinear_grid_dimensions(grid, dims);
    CHECK(dims[0] == 3u && dims[1] == 2u && dims[2] == 2u);
    CHECK(fviz_rectilinear_grid_point_count(grid) == 12u);
    CHECK(fviz_rectilinear_grid_cell_count(grid) == 2u);
    CHECK(fviz_rectilinear_grid_cell_type(grid) == FVIZ_CELL_HEXAHEDRON);

    CHECK(fviz_rectilinear_grid_set_coordinate_values(grid, 0u, x, 3u) == FVIZ_OK);
    CHECK(fviz_rectilinear_grid_set_coordinate_values(grid, 1u, y, 2u) == FVIZ_OK);
    CHECK(fviz_rectilinear_grid_set_coordinate_values(grid, 2u, z, 2u) == FVIZ_OK);
    CHECK(fviz_rectilinear_grid_validate(grid) == FVIZ_OK);

    CHECK(fviz_rectilinear_grid_point_id(grid, 12, -1, 6, &id) == FVIZ_OK && id == 11u);
    CHECK(fviz_rectilinear_grid_point(grid, id, &point) == FVIZ_OK);
    CHECK(nearf32(point.x, 5.0f) && nearf32(point.y, 1.0f) && nearf32(point.z, 20.0f));
    bounds = fviz_rectilinear_grid_bounds(grid);
    CHECK(nearf32(bounds.min.x, 0.0f) && nearf32(bounds.max.x, 5.0f));
    CHECK(nearf32(bounds.min.y, -1.0f) && nearf32(bounds.max.y, 1.0f));
    CHECK(nearf32(bounds.min.z, 10.0f) && nearf32(bounds.max.z, 20.0f));

    CHECK(fviz_rectilinear_grid_cell_id(grid, 11, -2, 5, &id) == FVIZ_OK && id == 1u);
    CHECK(fviz_rectilinear_grid_cell_point_ids(grid, id, corners, &corner_count) == FVIZ_OK);
    CHECK(corner_count == 8u);
    CHECK(corners[0] == 1u && corners[1] == 2u && corners[2] == 5u && corners[3] == 4u);
    CHECK(corners[4] == 7u && corners[5] == 8u && corners[6] == 11u && corners[7] == 10u);

    /* Coordinate ModifiedEvent is bridged to the grid in O(1) MTime queries. */
    coordinate = fviz_rectilinear_grid_coordinates(grid, 0u);
    before = fviz_object_mtime((const FVizObject*)grid);
    CHECK(fviz_data_array_set_tuple(coordinate, 1u, &replacement) == FVIZ_OK);
    CHECK(fviz_object_mtime((const FVizObject*)grid) > before);
    CHECK(fviz_rectilinear_grid_validate(grid) == FVIZ_OK);

    CHECK(fviz_rectilinear_grid_set_coordinate_values(grid, 0u, bad_x, 3u) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_rectilinear_grid_validate(grid) == FVIZ_OK); /* Failed replacement keeps old coordinates. */

    {
        float point_data[12];
        float cell_data[2] = {10.0f, 20.0f};
        FVizSize index;
        for (index = 0u; index < 12u; ++index) point_data[index] = (float)index;
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &point_values) == FVIZ_OK);
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &cell_values) == FVIZ_OK);
        CHECK(fviz_data_array_resize(point_values, 12u) == FVIZ_OK);
        CHECK(fviz_data_array_resize(cell_values, 2u) == FVIZ_OK);
        CHECK(fviz_data_array_set_tuples(point_values, 0u, point_data, 12u) == FVIZ_OK);
        CHECK(fviz_data_array_set_tuples(cell_values, 0u, cell_data, 2u) == FVIZ_OK);
        CHECK(fviz_attribute_set_add(
            fviz_rectilinear_grid_point_data(grid), "P", point_values) == FVIZ_OK);
        CHECK(fviz_attribute_set_add(
            fviz_rectilinear_grid_cell_data(grid), "E", cell_values) == FVIZ_OK);
    }

    {
        FVizRectilinearGridGeometryFilter* filter = NULL;
        FVizPolyData* surface;
        CHECK(fviz_rectilinear_grid_geometry_filter_create(&filter) == FVIZ_OK);
        CHECK(fviz_rectilinear_grid_geometry_filter_set_input_data(filter, grid) == FVIZ_OK);
        CHECK(fviz_rectilinear_grid_geometry_filter_update(filter) == FVIZ_OK);
        surface = fviz_rectilinear_grid_geometry_filter_output(filter);
        CHECK(surface != NULL);
        CHECK(fviz_poly_data_point_count(surface) == 12u);
        CHECK(fviz_poly_data_poly_cell_count(surface) == 20u);
        CHECK(fviz_attribute_set_const_get(
            fviz_poly_data_const_cell_data(surface), "FVizOriginalCellIds") != NULL);
        fviz_release(filter);
    }

    {
        FVizRectilinearGridExtractFilter* filter = NULL;
        FVizRectilinearGrid* extracted;
        const int64_t requested[6] = {11, 12, -2, -1, 5, 6};
        int64_t actual_extent[6];
        int64_t whole_extent[6];
        const FVizDataArray* x_coordinates;
        const FVizDataArray* extracted_cells;
        const float* cell_value;
        const double* x0;
        const double* x1;
        CHECK(fviz_rectilinear_grid_extract_filter_create(&filter) == FVIZ_OK);
        CHECK(fviz_rectilinear_grid_extract_filter_set_input_data(filter, grid) == FVIZ_OK);
        CHECK(fviz_rectilinear_grid_extract_filter_update_extent(filter, requested, 0u) == FVIZ_OK);
        extracted = fviz_rectilinear_grid_extract_filter_output(filter);
        CHECK(extracted != NULL);
        fviz_rectilinear_grid_extent(extracted, actual_extent);
        CHECK(memcmp(actual_extent, requested, sizeof(requested)) == 0);
        CHECK(fviz_rectilinear_grid_point_count(extracted) == 8u);
        CHECK(fviz_rectilinear_grid_cell_count(extracted) == 1u);
        x_coordinates = fviz_rectilinear_grid_const_coordinates(extracted, 0u);
        CHECK(x_coordinates != NULL && fviz_data_array_tuple_count(x_coordinates) == 2u);
        x0 = (const double*)fviz_data_array_const_tuple(x_coordinates, 0u);
        x1 = (const double*)fviz_data_array_const_tuple(x_coordinates, 1u);
        CHECK(x0 != NULL && x1 != NULL && *x0 == 3.0 && *x1 == 5.0);
        extracted_cells = fviz_attribute_set_const_get(
            fviz_rectilinear_grid_const_cell_data(extracted), "E");
        CHECK(extracted_cells != NULL && fviz_data_array_tuple_count(extracted_cells) == 1u);
        cell_value = (const float*)fviz_data_array_const_tuple(extracted_cells, 0u);
        CHECK(cell_value != NULL && *cell_value == 20.0f);
        CHECK(fviz_algorithm_output_whole_extent(
            fviz_rectilinear_grid_extract_filter_algorithm(filter), 0u, whole_extent) != FVIZ_FALSE);
        CHECK(memcmp(whole_extent, extent, sizeof(extent)) == 0);
        fviz_release(filter);
    }

    fviz_release(cell_values);
    fviz_release(point_values);
    fviz_release(grid);
    return 0;
}
