#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* temperature = NULL;
    uint32_t ids[4];
    float values[4] = { 10.0f, 20.0f, 30.0f, 40.0f };
    uint32_t i;

    CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_point(grid, fviz_vec3(0, 0, 0), NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_point(grid, fviz_vec3(1, 0, 0), NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_point(grid, fviz_vec3(0, 1, 0), NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_point(grid, fviz_vec3(0, 0, 1), NULL) == FVIZ_OK);
    ids[0] = 0u; ids[1] = 1u; ids[2] = 2u; ids[3] = 3u;
    CHECK(fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_TETRA, 4u, ids) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_point_count(grid) == 4u);
    CHECK(fviz_unstructured_grid_cell_count(grid) == 1u);

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &temperature) == FVIZ_OK);
    for (i = 0u; i < 4u; ++i) CHECK(fviz_data_array_append_tuple(temperature, &values[i]) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "temperature", temperature) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_validate(grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_bounds(grid).valid == FVIZ_TRUE);

    fviz_release(temperature);
    fviz_release(grid);
    return 0;
}
