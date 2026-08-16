#include <math.h>
#include <string.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGridPartitionFilter* filter = NULL;
    FVizPartitionedDataSet* output;
    FVizDataArray* node_values = NULL;
    FVizFieldStatisticsOptions stats_options;
    FVizFieldStatistics whole_stats;
    FVizFieldStatistics partition_stats;
    FVizFieldMoments whole_moments;
    FVizFieldMoments partition_moments;
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
    const double nodal_values[7] = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    FVizSize i;
    CHECK(fviz_unstructured_grid_create(&grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(grid, points, 7u, NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cells_fixed_ids(grid, FVIZ_CELL_TETRA, 4u, 4u, cells) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &node_values) == FVIZ_OK);
    CHECK(fviz_data_array_resize(node_values, 7u) == FVIZ_OK);
    CHECK(fviz_data_array_set_tuples(node_values, 0u, nodal_values, 7u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(
        fviz_unstructured_grid_point_data(grid), "NodeValue", node_values) == FVIZ_OK);
    fviz_release(node_values);
    node_values = NULL;
    CHECK(fviz_unstructured_grid_partition_filter_create(&filter) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_partition_filter_set_input_data(filter, grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_partition_filter_set_partition_count(filter, 2u) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_partition_filter_set_ghost_levels(filter, 1u) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_partition_filter_update(filter) == FVIZ_OK);
    output = fviz_unstructured_grid_partition_filter_output(filter);
    CHECK(output != NULL && fviz_partitioned_data_set_count(output) == 2u);
    for (i = 0u; i < 2u; ++i)
    {
        FVizUnstructuredGrid* piece = (FVizUnstructuredGrid*)fviz_partitioned_data_set_partition(output, i);
        const FVizDataArray* ghosts;
        const FVizDataArray* point_ghosts;
        const FVizDataArray* original_point_ids;
        FVizSize point_index;
        CHECK(piece != NULL && fviz_object_is_type((FVizObject*)piece, FVIZ_TYPE_UNSTRUCTURED_GRID));
        CHECK(fviz_unstructured_grid_cell_count(piece) == 3u);
        CHECK(fviz_partitioned_data_set_partition_name(output, i) != NULL);
        ghosts = fviz_attribute_set_const_get(
            fviz_unstructured_grid_cell_data(piece), FVIZ_GHOST_ARRAY_NAME);
        CHECK(ghosts != NULL && fviz_data_array_tuple_count(ghosts) == 3u);
        point_ghosts = fviz_attribute_set_const_get(
            fviz_unstructured_grid_point_data(piece), FVIZ_GHOST_ARRAY_NAME);
        original_point_ids = fviz_attribute_set_const_get(
            fviz_unstructured_grid_point_data(piece), "FVizOriginalPointIds");
        CHECK(point_ghosts != NULL && original_point_ids != NULL);
        CHECK(fviz_data_array_tuple_count(point_ghosts) == fviz_unstructured_grid_point_count(piece));
        for (point_index = 0u; point_index < fviz_unstructured_grid_point_count(piece); ++point_index)
        {
            const uint8_t ghost = *(const uint8_t*)fviz_data_array_const_tuple(point_ghosts, point_index);
            const uint64_t original = *(const uint64_t*)fviz_data_array_const_tuple(original_point_ids, point_index);
            if (i == 0u && original == 5u)
                CHECK((ghost & FVIZ_GHOST_DUPLICATE) != 0u);
            if (i == 1u && (original == 1u || original == 2u || original == 3u || original == 4u))
                CHECK((ghost & FVIZ_GHOST_DUPLICATE) != 0u);
            if ((i == 0u && original <= 4u) || (i == 1u && (original == 5u || original == 6u)))
                CHECK((ghost & FVIZ_GHOST_DUPLICATE) == 0u);
        }
    }
    CHECK(strcmp(fviz_partitioned_data_set_partition_name(output, 0u), "Piece 0") == 0);
    CHECK(strcmp(fviz_partitioned_data_set_partition_name(output, 1u), "Piece 1") == 0);

    fviz_field_statistics_options_initialize(&stats_options);
    stats_options.association = FVIZ_FIELD_POINT_DATA;
    CHECK(fviz_field_statistics_compute(
        (FVizDataObject*)grid, "NodeValue", &stats_options, &whole_stats) == FVIZ_OK);
    CHECK(fviz_field_statistics_compute(
        (FVizDataObject*)output, "NodeValue", &stats_options, &partition_stats) == FVIZ_OK);
    CHECK(whole_stats.valid != FVIZ_FALSE && partition_stats.valid != FVIZ_FALSE);
    CHECK(whole_stats.finite_tuple_count == 7u);
    CHECK(partition_stats.finite_tuple_count == whole_stats.finite_tuple_count);
    CHECK(whole_stats.minimum.value == partition_stats.minimum.value);
    CHECK(whole_stats.maximum.value == partition_stats.maximum.value);
    CHECK(fviz_field_statistics_compute_moments(
        (FVizDataObject*)grid, "NodeValue", &stats_options, &whole_moments) == FVIZ_OK);
    CHECK(fviz_field_statistics_compute_moments(
        (FVizDataObject*)output, "NodeValue", &stats_options, &partition_moments) == FVIZ_OK);
    CHECK(partition_moments.finite_tuple_count == whole_moments.finite_tuple_count);
    CHECK(fabs(partition_moments.mean - whole_moments.mean) < 1.0e-12);
    CHECK(fabs(partition_moments.root_mean_square - whole_moments.root_mean_square) < 1.0e-12);
    CHECK(fabs(partition_moments.variance - whole_moments.variance) < 1.0e-12);

    fviz_release(filter);
    fviz_release(grid);
    return 0;
}
