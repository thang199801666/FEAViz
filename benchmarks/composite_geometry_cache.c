#include <FViz/FViz.h>

#include <stdio.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    return timespec_get(&value, TIME_UTC) == TIME_UTC
        ? (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9 : 0.0;
}

int main(void)
{
    enum { LEAF_COUNT = 256, WARM_RUNS = 8 };
    const int64_t extent[6] = {0, 8, 0, 8, 0, 8};
    FVizMultiBlockDataSet* root = NULL;
    FVizCompositeGeometryFilter* filter = NULL;
    FVizImageData* leaves[LEAF_COUNT];
    uint32_t i;
    double start;
    double serial_cold_seconds;
    double parallel_cold_seconds;
    double warm_seconds;
    double single_leaf_seconds;
    for (i = 0u; i < LEAF_COUNT; ++i) leaves[i] = NULL;

    if (fviz_multi_block_data_set_create(&root) != FVIZ_OK ||
        fviz_multi_block_data_set_reserve(root, LEAF_COUNT) != FVIZ_OK)
        goto fail;
    for (i = 0u; i < LEAF_COUNT; ++i)
    {
        double origin[3] = {(double)(i % 16u) * 10.0, (double)(i / 16u) * 10.0, 0.0};
        char name[32];
        if (fviz_image_data_create(&leaves[i]) != FVIZ_OK ||
            fviz_image_data_set_extent(leaves[i], extent) != FVIZ_OK ||
            fviz_image_data_set_origin(leaves[i], origin) != FVIZ_OK)
            goto fail;
        (void)snprintf(name, sizeof(name), "Part-%u", (unsigned)i);
        if (fviz_multi_block_data_set_add_block(root, (FVizDataObject*)leaves[i], name, NULL) != FVIZ_OK)
            goto fail;
    }
    if (fviz_composite_geometry_filter_create(&filter) != FVIZ_OK ||
        fviz_composite_geometry_filter_set_parallel_enabled(filter, FVIZ_FALSE) != FVIZ_OK ||
        fviz_composite_geometry_filter_set_input_data(filter, root) != FVIZ_OK)
        goto fail;
    start = wall_seconds();
    if (fviz_composite_geometry_filter_update(filter) != FVIZ_OK) goto fail;
    serial_cold_seconds = wall_seconds() - start;
    fviz_release(filter);
    filter = NULL;

    if (fviz_composite_geometry_filter_create(&filter) != FVIZ_OK ||
        fviz_composite_geometry_filter_set_parallel_threshold(filter, 1u) != FVIZ_OK ||
        fviz_composite_geometry_filter_set_input_data(filter, root) != FVIZ_OK)
        goto fail;
    start = wall_seconds();
    if (fviz_composite_geometry_filter_update(filter) != FVIZ_OK) goto fail;
    parallel_cold_seconds = wall_seconds() - start;

    start = wall_seconds();
    for (i = 0u; i < WARM_RUNS; ++i)
    {
        char name[32];
        (void)snprintf(name, sizeof(name), "Part-0-%u", (unsigned)i);
        if (fviz_multi_block_data_set_set_block_name(root, 0u, name) != FVIZ_OK ||
            fviz_composite_geometry_filter_update(filter) != FVIZ_OK)
            goto fail;
    }
    warm_seconds = (wall_seconds() - start) / (double)WARM_RUNS;

    {
        const double spacing[3] = {1.25, 1.0, 1.0};
        if (fviz_image_data_set_spacing(leaves[LEAF_COUNT / 2u], spacing) != FVIZ_OK) goto fail;
        start = wall_seconds();
        if (fviz_composite_geometry_filter_update(filter) != FVIZ_OK) goto fail;
        single_leaf_seconds = wall_seconds() - start;
    }

    {
        const FVizCompositeGeometryCacheStatistics stats =
            fviz_composite_geometry_filter_cache_statistics(filter);
        puts("leaves,serial_cold_seconds,parallel_cold_seconds,warm_hierarchy_seconds,single_leaf_seconds,parallel_batches,parallel_leaf_conversions");
        printf("%u,%.9f,%.9f,%.9f,%.9f,%llu,%llu\n",
            (unsigned)LEAF_COUNT, serial_cold_seconds, parallel_cold_seconds,
            warm_seconds, single_leaf_seconds,
            (unsigned long long)stats.parallel_batches,
            (unsigned long long)stats.parallel_leaf_conversions);
    }

    fviz_release(filter);
    fviz_release(root);
    for (i = 0u; i < LEAF_COUNT; ++i) fviz_release(leaves[i]);
    return 0;

fail:
    fviz_release(filter);
    fviz_release(root);
    for (i = 0u; i < LEAF_COUNT; ++i) fviz_release(leaves[i]);
    return 1;
}
