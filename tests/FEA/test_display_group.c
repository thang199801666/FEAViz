#include <stdio.h>
#include <string.h>

#include <FViz/FEA/FVizFEA.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#expr); return __LINE__; } } while(0)

/* Builds a 2x2x2 hex beam with provenance label arrays (identity). */
static FVizResult build_grid(FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* point_ids = NULL;
    FVizDataArray* cell_ids = NULL;
    FVizSize x, y, z, i;
    const FVizSize n = 3u;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return fviz_last_error_code();
    for (z = 0u; z < n; ++z)
        for (y = 0u; y < n; ++y)
            for (x = 0u; x < n; ++x)
            {
                const FVizVec3 p = fviz_vec3((float)x, (float)y, (float)z);
                if (fviz_unstructured_grid_add_point(grid, p, NULL) != FVIZ_OK) return fviz_last_error_code();
            }
    for (z = 0u; z + 1u < n; ++z)
        for (y = 0u; y + 1u < n; ++y)
            for (x = 0u; x + 1u < n; ++x)
            {
                const uint32_t base = (uint32_t)(z * n * n + y * n + x);
                const uint32_t n32 = (uint32_t)n;
                const uint32_t ids[8] = {
                    base, base + 1u, base + n32 + 1u, base + n32,
                    base + n32 * n32, base + n32 * n32 + 1u, base + n32 * n32 + n32 + 1u, base + n32 * n32 + n32};
                if (fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, ids) != FVIZ_OK)
                    return fviz_last_error_code();
            }
    if (fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &point_ids) != FVIZ_OK ||
        fviz_data_array_resize(point_ids, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &cell_ids) != FVIZ_OK ||
        fviz_data_array_resize(cell_ids, fviz_unstructured_grid_cell_count(grid)) != FVIZ_OK)
        return fviz_last_error_code();
    for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        if (fviz_data_array_set_component(point_ids, i, 0u, (double)i) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < fviz_unstructured_grid_cell_count(grid); ++i)
        if (fviz_data_array_set_component(cell_ids, i, 0u, (double)i) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid),
            FVIZ_ORIGINAL_POINT_IDS_ARRAY_NAME, point_ids) != FVIZ_OK ||
        fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid),
            FVIZ_ORIGINAL_CELL_IDS_ARRAY_NAME, cell_ids) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_release(point_ids);
    fviz_release(cell_ids);
    *out_grid = grid;
    return FVIZ_OK;
}

static int test_create_and_sets(void)
{
    FVizFEADisplayGroup* group = NULL;
    const uint64_t nodes[2] = {0u, 1u};
    const uint64_t elements[2] = {1u, 2u};
    FVizFEADisplayGroupStatistics stats;
    FVizUnstructuredGrid* grid = NULL;
    CHECK(fviz_fea_display_group_create("GroupA", &group) == FVIZ_OK);
    CHECK(strcmp(fviz_fea_display_group_name(group), "GroupA") == 0);
    CHECK(fviz_fea_display_group_visible(group) == FVIZ_TRUE);
    CHECK(fviz_fea_display_group_set_nodes(group, nodes, 2u) == FVIZ_OK);
    CHECK(fviz_fea_display_group_set_elements(group, elements, 2u) == FVIZ_OK);
    CHECK(fviz_fea_display_group_set_faces(group, NULL, 0u) == FVIZ_OK);
    CHECK(build_grid(&grid) == FVIZ_OK);
    fviz_fea_display_group_get_statistics(group, grid, &stats);
    CHECK(stats.node_count == 2u);
    CHECK(stats.element_count == 2u);
    CHECK(stats.visible_points == 2u);
    CHECK(stats.visible_cells == 2u);
    fviz_release(grid);
    fviz_release(group);
    return 0;
}

static int test_masks(void)
{
    FVizFEADisplayGroup* group = NULL;
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* point_mask = NULL;
    FVizDataArray* cell_mask = NULL;
    const uint64_t elements[1] = {1u};
    double v;
    CHECK(build_grid(&grid) == FVIZ_OK);
    CHECK(fviz_fea_display_group_create("Mask", &group) == FVIZ_OK);
    CHECK(fviz_fea_display_group_set_elements(group, elements, 1u) == FVIZ_OK);
    CHECK(fviz_fea_display_group_create_masks(group, grid, &point_mask, &cell_mask) == FVIZ_OK);
    CHECK(fviz_data_array_tuple_count(point_mask) == fviz_unstructured_grid_point_count(grid));
    CHECK(fviz_data_array_tuple_count(cell_mask) == fviz_unstructured_grid_cell_count(grid));
    /* Cell 1 in, cell 0 out. */
    CHECK(fviz_data_array_get_component(cell_mask, 0u, 0u, &v) == FVIZ_OK && v == 0.0);
    CHECK(fviz_data_array_get_component(cell_mask, 1u, 0u, &v) == FVIZ_OK && v == 1.0);
    fviz_release(cell_mask);
    fviz_release(point_mask);
    fviz_release(group);
    fviz_release(grid);
    return 0;
}

static int test_combine(void)
{
    FVizFEADisplayGroup* a = NULL;
    FVizFEADisplayGroup* b = NULL;
    const uint64_t ea[2] = {0u, 1u};
    const uint64_t eb[2] = {1u, 2u};
    FVizUnstructuredGrid* grid = NULL;
    FVizFEADisplayGroupStatistics stats;
    CHECK(fviz_fea_display_group_create("A", &a) == FVIZ_OK);
    CHECK(fviz_fea_display_group_create("B", &b) == FVIZ_OK);
    CHECK(fviz_fea_display_group_set_elements(a, ea, 2u) == FVIZ_OK);
    CHECK(fviz_fea_display_group_set_elements(b, eb, 2u) == FVIZ_OK);
    CHECK(build_grid(&grid) == FVIZ_OK);
    /* Union: {0,1,2}. */
    CHECK(fviz_fea_display_group_combine(a, b, FVIZ_FEA_DISPLAY_GROUP_ADD) == FVIZ_OK);
    fviz_fea_display_group_get_statistics(a, grid, &stats);
    CHECK(stats.element_count == 3u);
    /* Intersect with b {1,2} -> {1,2}. */
    CHECK(fviz_fea_display_group_combine(a, b, FVIZ_FEA_DISPLAY_GROUP_INTERSECT) == FVIZ_OK);
    fviz_fea_display_group_get_statistics(a, grid, &stats);
    CHECK(stats.element_count == 2u);
    /* Remove b from a -> empty. */
    CHECK(fviz_fea_display_group_combine(a, b, FVIZ_FEA_DISPLAY_GROUP_REMOVE) == FVIZ_OK);
    fviz_fea_display_group_get_statistics(a, grid, &stats);
    CHECK(stats.element_count == 0u);
    fviz_release(grid);
    fviz_release(b);
    fviz_release(a);
    return 0;
}

int main(void)
{
    int result = 0;
    if ((result = test_create_and_sets()) != 0)
    { fprintf(stderr, "test_create_and_sets failed at line %d\n", result); return result; }
    if ((result = test_masks()) != 0)
    { fprintf(stderr, "test_masks failed at line %d\n", result); return result; }
    if ((result = test_combine()) != 0)
    { fprintf(stderr, "test_combine failed at line %d\n", result); return result; }
    printf("FVizTestFEADisplayGroup passed\n");
    return 0;
}
