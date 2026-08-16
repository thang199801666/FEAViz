#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizBool count_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    int* count = (int*)client_data;
    (void)caller; (void)event_id; (void)call_data;
    ++(*count);
    return FVIZ_FALSE;
}

int main(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* temperature = NULL;
    uint32_t ids[4];
    float values[4] = { 10.0f, 20.0f, 30.0f, 40.0f };
    uint32_t i;
    FVizObserverTag modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    int modified_count = 0;

    {
        const FVizVec3 points[4] = {
            {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
        };
        CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
        CHECK(fviz_unstructured_grid_reserve(grid, 4u, 1u, 4u) == FVIZ_OK);
        CHECK(fviz_unstructured_grid_add_points(grid, points, 4u, NULL) == FVIZ_OK);
    }
    ids[0] = 0u; ids[1] = 1u; ids[2] = 2u; ids[3] = 3u;
    CHECK(fviz_unstructured_grid_add_cells_fixed(grid, FVIZ_CELL_TETRA, 4u, 1u, ids) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_point_count(grid) == 4u);
    CHECK(fviz_unstructured_grid_cell_count(grid) == 1u);

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &temperature) == FVIZ_OK);
    for (i = 0u; i < 4u; ++i) CHECK(fviz_data_array_append_tuple(temperature, &values[i]) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "temperature", temperature) == FVIZ_OK);
    CHECK(fviz_object_add_observer(
        (FVizObject*)grid, FVIZ_EVENT_MODIFIED, 0.0f, count_modified, &modified_count, &modified_tag) == FVIZ_OK);
    modified_count = 0;
    values[2] = 31.0f;
    CHECK(fviz_data_array_set_tuple(temperature, 2u, &values[2]) == FVIZ_OK);
    CHECK(modified_count == 1);
    CHECK(fviz_unstructured_grid_validate(grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_bounds(grid).valid == FVIZ_TRUE);

    CHECK(fviz_object_remove_observer((FVizObject*)grid, modified_tag) == FVIZ_OK);
    fviz_release(temperature);
    fviz_release(grid);
    return 0;
}
