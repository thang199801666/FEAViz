#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

static FVizResult make_poly(FVizPolyData** out_poly)
{
    FVizPolyData* poly = NULL;
    FVizDataArray* scalars = NULL;
    const FVizVec3 points[4] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}
    };
    const uint32_t triangles[6] = {0u, 1u, 2u, 0u, 2u, 3u};
    const float values[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    *out_poly = NULL;
    if (fviz_poly_data_create(&poly) != FVIZ_OK ||
        fviz_poly_data_add_points(poly, points, 4u, NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangles(poly, triangles, 2u) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalars) != FVIZ_OK ||
        fviz_data_array_append_tuples(scalars, values, 4u) != FVIZ_OK ||
        fviz_attribute_set_add(fviz_poly_data_point_data(poly), "Stress", scalars) != FVIZ_OK)
    {
        fviz_release(scalars);
        fviz_release(poly);
        return fviz_last_error_code();
    }
    fviz_release(scalars);
    *out_poly = poly;
    return FVIZ_OK;
}

int main(void)
{
    FVizPolyData* poly = NULL;
    FVizMultiBlockDataSet* root = NULL;
    FVizPartitionedDataSet* partitions = NULL;
    FVizDataObjectMemoryInfo leaf_info;
    FVizDataObjectMemoryInfo tree_info;
    FVizSize size;

    CHECK(make_poly(&poly) == FVIZ_OK);
    CHECK(fviz_data_object_memory_info((const FVizDataObject*)poly, &leaf_info) == FVIZ_OK);
    CHECK(leaf_info.total_bytes > sizeof(FVizVec3) * 4u);
    CHECK(leaf_info.geometry_bytes >= sizeof(FVizVec3) * 4u);
    CHECK(leaf_info.topology_bytes > 0u);
    CHECK(leaf_info.attribute_bytes >= sizeof(float) * 4u);
    CHECK(leaf_info.unique_object_count >= 1u);
    CHECK(fviz_data_object_memory_size((const FVizDataObject*)poly) == leaf_info.total_bytes);

    CHECK(fviz_partitioned_data_set_create(&partitions) == FVIZ_OK);
    CHECK(fviz_partitioned_data_set_add_partition(partitions, (FVizDataObject*)poly, "A", NULL) == FVIZ_OK);
    /* Same leaf twice must only count its payload once. */
    CHECK(fviz_partitioned_data_set_add_partition(partitions, (FVizDataObject*)poly, "B", NULL) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_create(&root) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_add_block(root, (FVizDataObject*)partitions, "Pieces", NULL) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_add_block(root, (FVizDataObject*)poly, "SharedLeaf", NULL) == FVIZ_OK);
    CHECK(fviz_data_object_memory_info((const FVizDataObject*)root, &tree_info) == FVIZ_OK);
    CHECK(tree_info.total_bytes > leaf_info.total_bytes);
    CHECK(tree_info.composite_bytes > 0u);
    CHECK(tree_info.attribute_bytes == leaf_info.attribute_bytes);
    CHECK(tree_info.geometry_bytes == leaf_info.geometry_bytes);
    size = fviz_data_object_memory_size((const FVizDataObject*)root);
    CHECK(size == tree_info.total_bytes);

    fviz_release(root);
    fviz_release(partitions);
    fviz_release(poly);
    return 0;
}
