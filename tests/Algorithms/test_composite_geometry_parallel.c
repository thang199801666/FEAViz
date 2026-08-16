#include <FViz/FViz.h>

#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)

int main(void)
{
    enum { LEAF_COUNT = 12 };
    const int64_t extent[6] = {0, 4, 0, 4, 0, 4};
    FVizMultiBlockDataSet* root = NULL;
    FVizCompositeGeometryFilter* filter = NULL;
    FVizImageData* leaves[LEAF_COUNT];
    FVizCompositeGeometryCacheStatistics stats;
    FVizSize initial_bytes;
    uint32_t i;

    for (i = 0u; i < LEAF_COUNT; ++i) leaves[i] = NULL;
    CHECK(fviz_multi_block_data_set_create(&root) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_reserve(root, LEAF_COUNT) == FVIZ_OK);
    for (i = 0u; i < LEAF_COUNT; ++i)
    {
        const double origin[3] = {(double)i * 6.0, 0.0, 0.0};
        CHECK(fviz_image_data_create(&leaves[i]) == FVIZ_OK);
        CHECK(fviz_image_data_set_extent(leaves[i], extent) == FVIZ_OK);
        CHECK(fviz_image_data_set_origin(leaves[i], origin) == FVIZ_OK);
        CHECK(fviz_multi_block_data_set_add_block(
            root, (FVizDataObject*)leaves[i], NULL, NULL) == FVIZ_OK);
    }

    CHECK(fviz_composite_geometry_filter_create(&filter) == FVIZ_OK);
    CHECK(fviz_composite_geometry_filter_set_parallel_threshold(filter, 1u) == FVIZ_OK);
    CHECK(fviz_composite_geometry_filter_set_input_data(filter, root) == FVIZ_OK);
    CHECK(fviz_composite_geometry_filter_update(filter) == FVIZ_OK);
    stats = fviz_composite_geometry_filter_cache_statistics(filter);
    CHECK(stats.entries == LEAF_COUNT);
    CHECK(stats.misses == LEAF_COUNT);
    CHECK(stats.bytes > 0u);
    initial_bytes = stats.bytes;
    if (fviz_parallel_context_thread_count(fviz_parallel_default_context()) > 1u)
    {
        CHECK(stats.parallel_batches >= 1u);
        CHECK(stats.parallel_leaf_conversions >= LEAF_COUNT);
    }

    /* Limit the cache below its current footprint. The output tree remains
     * valid because it owns its assigned PolyData independently of cache LRU. */
    CHECK(fviz_composite_geometry_filter_set_cache_byte_capacity(
        filter, initial_bytes / 2u) == FVIZ_OK);
    stats = fviz_composite_geometry_filter_cache_statistics(filter);
    CHECK(stats.bytes <= stats.byte_capacity);
    CHECK(stats.entries < LEAF_COUNT);
    CHECK(stats.evictions > 0u);
    CHECK(fviz_composite_geometry_filter_output(filter) != NULL);

    /* A one-byte budget makes every converted leaf oversize. Execution must
     * still succeed, but no converted geometry may remain cached. */
    CHECK(fviz_composite_geometry_filter_set_cache_byte_capacity(filter, 1u) == FVIZ_OK);
    CHECK(fviz_multi_block_data_set_set_block_name(root, 0u, "touch") == FVIZ_OK);
    CHECK(fviz_composite_geometry_filter_update(filter) == FVIZ_OK);
    stats = fviz_composite_geometry_filter_cache_statistics(filter);
    CHECK(stats.entries == 0u);
    CHECK(stats.bytes == 0u);
    CHECK(stats.oversize_skips >= LEAF_COUNT);

    CHECK(fviz_composite_geometry_filter_set_parallel_enabled(filter, FVIZ_FALSE) == FVIZ_OK);
    CHECK(fviz_composite_geometry_filter_parallel_enabled(filter) == FVIZ_FALSE);
    CHECK(fviz_composite_geometry_filter_set_parallel_threshold(filter, 3u) == FVIZ_OK);
    CHECK(fviz_composite_geometry_filter_parallel_threshold(filter) == 3u);

    fviz_release(filter);
    fviz_release(root);
    for (i = 0u; i < LEAF_COUNT; ++i) fviz_release(leaves[i]);
    return 0;
}
