#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* filtered = NULL;
    FVizDataArray* stress = NULL;
    uint32_t first[4] = { 0u, 1u, 2u, 3u };
    uint32_t second[4] = { 1u, 2u, 3u, 4u };
    float values[2] = { 10.0f, 90.0f };
    uint32_t i;

    CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
    for (i = 0u; i < 5u; ++i)
        CHECK(fviz_unstructured_grid_add_point(grid, fviz_vec3((float)i, 0, 0), NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_TETRA, 4u, first) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_TETRA, 4u, second) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &stress) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(stress, &values[0]) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(stress, &values[1]) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid), "stress", stress) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_threshold_cells(grid, "stress", 0.0, 50.0, &filtered) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_point_count(filtered) == 5u);
    CHECK(fviz_unstructured_grid_cell_count(filtered) == 1u);
    CHECK(fviz_data_array_tuple_count(fviz_attribute_set_const_get(
        fviz_unstructured_grid_cell_data(filtered), "stress")) == 1u);
    CHECK(fviz_unstructured_grid_validate(filtered) == FVIZ_OK);

    fviz_release(filtered);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}
