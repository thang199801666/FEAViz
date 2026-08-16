#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizPoints* points = NULL;
    FVizCellArray* cells = NULL;
    uint32_t ids[8];
    uint32_t id;
    FVizId wide_id;
    FVizId wide_ids[3] = {0u, 1u, 2u};

    CHECK(fviz_points_create(&points) == FVIZ_OK);
    CHECK(fviz_points_append(points, fviz_vec3(0, 0, 0), &id) == FVIZ_OK && id == 0u);
    CHECK(fviz_points_append(points, fviz_vec3(1, 0, 0), &id) == FVIZ_OK && id == 1u);
    CHECK(fviz_points_append(points, fviz_vec3(0, 1, 0), &id) == FVIZ_OK && id == 2u);
    CHECK(fviz_points_append_id(points, fviz_vec3(0, 0, 1), &wide_id) == FVIZ_OK && wide_id == 3u);
    CHECK(fviz_points_count(points) == 4u);
    CHECK(fviz_points_bounds(points).max.z == 1.0f);
    {
        FVizMTime mtime = fviz_object_mtime((FVizObject*)points);
        CHECK(fviz_points_set(points, 3u, fviz_vec3(0, 0, 1)) == FVIZ_OK);
        CHECK(fviz_object_mtime((FVizObject*)points) == mtime);
        CHECK(fviz_points_set(points, 3u, fviz_vec3(0, 0, 2)) == FVIZ_OK);
        CHECK(fviz_object_mtime((FVizObject*)points) > mtime);
        CHECK(fviz_points_bounds(points).max.z == 2.0f);
    }

    CHECK(fviz_cell_array_create(&cells) == FVIZ_OK);
    ids[0] = 0u; ids[1] = 1u; ids[2] = 2u;
    CHECK(fviz_cell_array_append(cells, FVIZ_CELL_TRIANGLE, 3u, ids) == FVIZ_OK);
    CHECK(fviz_cell_array_count(cells) == 1u);
    CHECK(fviz_cell_array_point_count(cells, 0u) == 3u);
    CHECK(fviz_cell_array_type(cells, 0u) == FVIZ_CELL_TRIANGLE);
    CHECK(fviz_cell_array_validate(cells, fviz_points_count(points)) == FVIZ_OK);
    CHECK(fviz_cell_array_point_ids(cells, 0u)[2] == 2u);
    CHECK(fviz_cell_array_append_ids(cells, FVIZ_CELL_TRIANGLE, 3u, wide_ids) == FVIZ_OK);

    ids[0] = 0u; ids[1] = 1u; ids[2] = 2u; ids[3] = 3u;
    CHECK(fviz_cell_array_append(cells, FVIZ_CELL_QUAD, 4u, ids) == FVIZ_OK);
    CHECK(fviz_cell_array_validate(cells, fviz_points_count(points)) == FVIZ_OK);

    /* Native FVizId insertion now auto-promotes the backing storage instead
       of rejecting IDs above UINT32_MAX.  Keep this in the legacy topology
       regression so the compatibility API and the native-ID API exercise
       their intentionally different contracts. */
    wide_ids[2] = (FVizId)UINT32_MAX + 1u;
    CHECK(fviz_cell_array_append_ids(cells, FVIZ_CELL_TRIANGLE, 3u, wide_ids) == FVIZ_OK);
    CHECK(fviz_cell_array_id_storage(cells) == FVIZ_ID_STORAGE_UINT64);
    CHECK(fviz_cell_array_point_id(cells, 3u, 2u, &wide_id) == FVIZ_OK);
    CHECK(wide_id == wide_ids[2]);
    CHECK(fviz_cell_array_validate(cells, fviz_points_count(points)) == FVIZ_ERROR_INVALID_STATE);

    fviz_release(cells);
    fviz_release(points);
    return 0;
}
