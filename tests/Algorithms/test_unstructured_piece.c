#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGridPieceFilter* filter = NULL;
    FVizUnstructuredGrid* piece;
    FVizDataArray* point_values = NULL;
    FVizDataArray* cell_values = NULL;
    const FVizDataArray* provenance_points;
    const FVizDataArray* provenance_cells;
    const FVizDataArray* output_points;
    const FVizDataArray* output_cells;
    const FVizCellArray* cells;
    FVizCellView view;
    const FVizVec3 points[8] = {
        {0.0f,0.0f,0.0f}, {1.0f,0.0f,0.0f}, {0.0f,1.0f,0.0f}, {0.0f,0.0f,1.0f},
        {2.0f,0.0f,0.0f}, {2.0f,1.0f,0.0f}, {2.0f,0.0f,1.0f}, {3.0f,0.0f,0.0f}
    };
    const FVizId tetrahedra[16] = {
        0u,1u,2u,3u,
        1u,4u,2u,3u,
        4u,5u,2u,6u,
        4u,7u,5u,6u
    };
    const float point_data[8] = {0,1,2,3,4,5,6,7};
    const float cell_data[4] = {100,200,300,400};
    const uint64_t expected_point_ids[5] = {4u,5u,2u,6u,7u};
    const uint64_t expected_cell_ids[2] = {2u,3u};
    FVizSize i;

    CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(grid, points, 8u, NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cells_fixed_ids(
        grid, FVIZ_CELL_TETRA, 4u, 4u, tetrahedra) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &point_values) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &cell_values) == FVIZ_OK);
    CHECK(fviz_data_array_resize(point_values, 8u) == FVIZ_OK);
    CHECK(fviz_data_array_resize(cell_values, 4u) == FVIZ_OK);
    CHECK(fviz_data_array_set_tuples(point_values, 0u, point_data, 8u) == FVIZ_OK);
    CHECK(fviz_data_array_set_tuples(cell_values, 0u, cell_data, 4u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(
        fviz_unstructured_grid_point_data(grid), "P", point_values) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(
        fviz_unstructured_grid_cell_data(grid), "E", cell_values) == FVIZ_OK);

    CHECK(fviz_unstructured_grid_piece_filter_create(&filter) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_piece_filter_set_input_data(filter, grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_piece_filter_update_piece(filter, 1u, 2u, 0u) == FVIZ_OK);
    piece = fviz_unstructured_grid_piece_filter_output(filter);
    CHECK(piece != NULL);
    CHECK(fviz_unstructured_grid_point_count(piece) == 5u);
    CHECK(fviz_unstructured_grid_cell_count(piece) == 2u);
    CHECK(fviz_unstructured_grid_validate(piece) == FVIZ_OK);

    provenance_points = fviz_attribute_set_const_get(
        fviz_unstructured_grid_point_data(piece), "FVizOriginalPointIds");
    provenance_cells = fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data(piece), "FVizOriginalCellIds");
    CHECK(provenance_points != NULL && fviz_data_array_tuple_count(provenance_points) == 5u);
    CHECK(provenance_cells != NULL && fviz_data_array_tuple_count(provenance_cells) == 2u);
    for (i = 0u; i < 5u; ++i)
    {
        const uint64_t* value = (const uint64_t*)fviz_data_array_const_tuple(provenance_points, i);
        CHECK(value != NULL && *value == expected_point_ids[i]);
    }
    for (i = 0u; i < 2u; ++i)
    {
        const uint64_t* value = (const uint64_t*)fviz_data_array_const_tuple(provenance_cells, i);
        CHECK(value != NULL && *value == expected_cell_ids[i]);
    }

    output_points = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(piece), "P");
    output_cells = fviz_attribute_set_const_get(fviz_unstructured_grid_cell_data(piece), "E");
    CHECK(output_points != NULL && output_cells != NULL);
    CHECK(*(const float*)fviz_data_array_const_tuple(output_points, 0u) == 4.0f);
    CHECK(*(const float*)fviz_data_array_const_tuple(output_points, 2u) == 2.0f);
    CHECK(*(const float*)fviz_data_array_const_tuple(output_cells, 0u) == 300.0f);
    CHECK(*(const float*)fviz_data_array_const_tuple(output_cells, 1u) == 400.0f);

    cells = fviz_unstructured_grid_cells(piece);
    CHECK(fviz_cell_array_cell_view(cells, 0u, &view) == FVIZ_OK);
    CHECK(view.point_count == 4u);
    CHECK(fviz_cell_view_point_id(&view, 0u) == 0u);
    CHECK(fviz_cell_view_point_id(&view, 1u) == 1u);
    CHECK(fviz_cell_view_point_id(&view, 2u) == 2u);
    CHECK(fviz_cell_view_point_id(&view, 3u) == 3u);
    CHECK(fviz_cell_array_cell_view(cells, 1u, &view) == FVIZ_OK);
    CHECK(fviz_cell_view_point_id(&view, 0u) == 0u);
    CHECK(fviz_cell_view_point_id(&view, 1u) == 4u);
    CHECK(fviz_cell_view_point_id(&view, 2u) == 1u);
    CHECK(fviz_cell_view_point_id(&view, 3u) == 3u);

    /* Ghost requests are valid even when this particular partition boundary
       has no codimension-one neighbor. */
    CHECK(fviz_unstructured_grid_piece_filter_update_piece(filter, 0u, 2u, 1u) == FVIZ_OK);
    piece = fviz_unstructured_grid_piece_filter_output(filter);
    CHECK(piece != NULL && fviz_unstructured_grid_cell_count(piece) == 2u);

    fviz_release(filter);
    fviz_release(cell_values);
    fviz_release(point_values);
    fviz_release(grid);
    return 0;
}
