#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGridPieceFilter* filter = NULL;
    FVizUnstructuredGrid* piece;
    const FVizDataArray* original_cells;
    const FVizDataArray* ghost_cells;
    const FVizDataArray* ghost_levels;
    const FVizDataArray* ghost_points;
    FVizPolyData* surface = NULL;
    FVizPolyData* geometry = NULL;
    const FVizVec3 points[7] = {
        {0,0,0}, {1,0,0}, {0,1,0}, {0,0,1},
        {1,1,0}, {1,0,1}, {1,1,1}
    };
    const FVizId cells[16] = {
        0,1,2,3,
        1,4,2,3,
        4,5,2,3,
        5,6,2,3
    };

    CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(grid, points, 7u, NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cells_fixed_ids(
        grid, FVIZ_CELL_TETRA, 4u, 4u, cells) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_piece_filter_create(&filter) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_piece_filter_set_input_data(filter, grid) == FVIZ_OK);

    CHECK(fviz_unstructured_grid_piece_filter_update_piece(filter, 0u, 2u, 1u) == FVIZ_OK);
    piece = fviz_unstructured_grid_piece_filter_output(filter);
    CHECK(piece != NULL && fviz_unstructured_grid_cell_count(piece) == 3u);
    original_cells = fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data(piece), "FVizOriginalCellIds");
    ghost_cells = fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data(piece), FVIZ_GHOST_ARRAY_NAME);
    ghost_levels = fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data(piece), FVIZ_GHOST_LEVEL_ARRAY_NAME);
    ghost_points = fviz_attribute_set_const_get(
        fviz_unstructured_grid_point_data(piece), FVIZ_GHOST_ARRAY_NAME);
    CHECK(original_cells != NULL && ghost_cells != NULL && ghost_levels != NULL && ghost_points != NULL);
    CHECK(*(const uint64_t*)fviz_data_array_const_tuple(original_cells, 0u) == 0u);
    CHECK(*(const uint64_t*)fviz_data_array_const_tuple(original_cells, 1u) == 1u);
    CHECK(*(const uint64_t*)fviz_data_array_const_tuple(original_cells, 2u) == 2u);
    CHECK(*(const uint8_t*)fviz_data_array_const_tuple(ghost_cells, 0u) == FVIZ_GHOST_NONE);
    CHECK(*(const uint8_t*)fviz_data_array_const_tuple(ghost_cells, 1u) == FVIZ_GHOST_NONE);
    CHECK(*(const uint8_t*)fviz_data_array_const_tuple(ghost_cells, 2u) == FVIZ_GHOST_DUPLICATE);
    CHECK(*(const uint16_t*)fviz_data_array_const_tuple(ghost_levels, 2u) == 1u);
    /* Point 5 is introduced only by the ghost cell. */
    CHECK(*(const uint8_t*)fviz_data_array_const_tuple(ghost_points, 5u) == FVIZ_GHOST_DUPLICATE);

    /* Ghost cells participate in face ownership so the owned/ghost interface is
     * suppressed, but exterior faces owned only by duplicate cells must not be
     * emitted. Three chained tetrahedra have eight exterior faces in total;
     * only five belong to the two owned cells. */
    CHECK(fviz_unstructured_grid_extract_surface(piece, &surface) == FVIZ_OK);
    CHECK(surface != NULL && fviz_poly_data_triangle_count(surface) == 5u);
    CHECK(fviz_unstructured_grid_extract_geometry(piece, &geometry) == FVIZ_OK);
    CHECK(geometry != NULL && fviz_poly_data_triangle_count(geometry) == 5u);
    fviz_release(surface); surface = NULL;
    fviz_release(geometry); geometry = NULL;

    CHECK(fviz_unstructured_grid_piece_filter_update_piece(filter, 0u, 2u, 2u) == FVIZ_OK);
    piece = fviz_unstructured_grid_piece_filter_output(filter);
    CHECK(piece != NULL && fviz_unstructured_grid_cell_count(piece) == 4u);
    original_cells = fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data(piece), "FVizOriginalCellIds");
    ghost_levels = fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data(piece), FVIZ_GHOST_LEVEL_ARRAY_NAME);
    CHECK(*(const uint64_t*)fviz_data_array_const_tuple(original_cells, 3u) == 3u);
    CHECK(*(const uint16_t*)fviz_data_array_const_tuple(ghost_levels, 3u) == 2u);

    fviz_release(filter);
    fviz_release(grid);
    return 0;
}
