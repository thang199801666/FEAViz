#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int test_tetra_surface(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizPolyData* surface = NULL;
    uint32_t ids[4] = { 0u, 1u, 2u, 3u };
    CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_point(grid, fviz_vec3(0, 0, 0), NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_point(grid, fviz_vec3(1, 0, 0), NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_point(grid, fviz_vec3(0, 1, 0), NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_point(grid, fviz_vec3(0, 0, 1), NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_TETRA, 4u, ids) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_extract_surface(grid, &surface) == FVIZ_OK);
    CHECK(fviz_poly_data_point_count(surface) == 4u);
    CHECK(fviz_poly_data_triangle_count(surface) == 4u);
    CHECK(fviz_poly_data_validate(surface) == FVIZ_OK);
    fviz_release(surface);
    fviz_release(grid);
    return 0;
}

static int test_adjacent_hexes(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizPolyData* surface = NULL;
    uint32_t first[8] = { 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u };
    uint32_t second[8] = { 1u, 8u, 9u, 2u, 5u, 10u, 11u, 6u };
    uint32_t i;
    CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
    for (i = 0u; i < 12u; ++i)
        CHECK(fviz_unstructured_grid_add_point(grid, fviz_vec3((float)i, 0, 0), NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, first) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, second) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_extract_surface(grid, &surface) == FVIZ_OK);
    CHECK(fviz_poly_data_triangle_count(surface) == 20u);
    fviz_release(surface);
    fviz_release(grid);
    return 0;
}

int main(void)
{
    int result = test_tetra_surface();
    if (result != 0) return result;
    return test_adjacent_hexes();
}
