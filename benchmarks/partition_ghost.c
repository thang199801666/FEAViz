#include <FViz/FViz.h>

#include <stdio.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    return timespec_get(&value, TIME_UTC) == TIME_UTC
        ? (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9 : 0.0;
}

static FVizSize point_id(uint32_t x, uint32_t y, uint32_t z, uint32_t edge)
{
    return (FVizSize)x + (FVizSize)edge * ((FVizSize)y + (FVizSize)edge * (FVizSize)z);
}

int main(void)
{
    const uint32_t n = 24u;
    const uint32_t edge = n + 1u;
    const FVizSize point_count = (FVizSize)edge * edge * edge;
    const FVizSize cell_count = (FVizSize)n * n * n;
    FVizVec3* points = NULL;
    uint32_t* ids = NULL;
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGridPartitionFilter* filter = NULL;
    FVizPartitionedDataSet* output = NULL;
    FVizSize point_index = 0u;
    FVizSize id_index = 0u;
    FVizSize total_output_cells = 0u;
    FVizSize ghost_cells = 0u;
    FVizSize partition;
    uint32_t x, y, z;
    double first_started, first_seconds;
    double update_started, update_seconds;

    points = (FVizVec3*)fviz_alloc(point_count * sizeof(*points));
    ids = (uint32_t*)fviz_alloc(cell_count * 8u * sizeof(*ids));
    if (points == NULL || ids == NULL) return 1;

    for (z = 0u; z < edge; ++z)
        for (y = 0u; y < edge; ++y)
            for (x = 0u; x < edge; ++x)
                points[point_index++] = fviz_vec3((float)x, (float)y, (float)z);

    for (z = 0u; z < n; ++z)
        for (y = 0u; y < n; ++y)
            for (x = 0u; x < n; ++x)
            {
                ids[id_index++] = (uint32_t)point_id(x, y, z, edge);
                ids[id_index++] = (uint32_t)point_id(x + 1u, y, z, edge);
                ids[id_index++] = (uint32_t)point_id(x + 1u, y + 1u, z, edge);
                ids[id_index++] = (uint32_t)point_id(x, y + 1u, z, edge);
                ids[id_index++] = (uint32_t)point_id(x, y, z + 1u, edge);
                ids[id_index++] = (uint32_t)point_id(x + 1u, y, z + 1u, edge);
                ids[id_index++] = (uint32_t)point_id(x + 1u, y + 1u, z + 1u, edge);
                ids[id_index++] = (uint32_t)point_id(x, y + 1u, z + 1u, edge);
            }

    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return 2;
    if (fviz_unstructured_grid_reserve(grid, point_count, cell_count, cell_count * 8u) != FVIZ_OK) return 3;
    if (fviz_unstructured_grid_add_points(grid, points, point_count, NULL) != FVIZ_OK) return 4;
    if (fviz_unstructured_grid_add_cells_fixed(
            grid, FVIZ_CELL_HEXAHEDRON, 8u, cell_count, ids) != FVIZ_OK) return 5;

    if (fviz_unstructured_grid_partition_filter_create(&filter) != FVIZ_OK) return 6;
    if (fviz_unstructured_grid_partition_filter_set_input_data(filter, grid) != FVIZ_OK) return 7;
    if (fviz_unstructured_grid_partition_filter_set_partition_count(filter, 8u) != FVIZ_OK) return 8;
    if (fviz_unstructured_grid_partition_filter_set_ghost_levels(filter, 1u) != FVIZ_OK) return 9;

    first_started = wall_seconds();
    if (fviz_unstructured_grid_partition_filter_update(filter) != FVIZ_OK) return 10;
    first_seconds = wall_seconds() - first_started;
    output = fviz_unstructured_grid_partition_filter_output(filter);
    if (output == NULL || fviz_partitioned_data_set_count(output) != 8u) return 11;

    for (partition = 0u; partition < fviz_partitioned_data_set_count(output); ++partition)
    {
        const FVizDataObject* data = fviz_partitioned_data_set_const_partition(output, partition);
        const FVizUnstructuredGrid* piece;
        const FVizDataArray* ghosts;
        const uint8_t* flags;
        FVizSize cell;
        if (data == NULL || !fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_UNSTRUCTURED_GRID)) return 12;
        piece = (const FVizUnstructuredGrid*)data;
        total_output_cells += fviz_unstructured_grid_cell_count(piece);
        ghosts = fviz_attribute_set_const_get(
            fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)piece), FVIZ_GHOST_ARRAY_NAME);
        if (ghosts == NULL) return 13;
        flags = (const uint8_t*)fviz_data_array_const_data(ghosts);
        for (cell = 0u; cell < fviz_data_array_tuple_count(ghosts); ++cell)
            if ((flags[cell] & FVIZ_GHOST_DUPLICATE) != 0u) ++ghost_cells;
    }

    /* Coordinate-only deformation keeps topology and therefore reuses the
     * cached cell adjacency while rematerializing point positions. */
    for (point_index = 0u; point_index < point_count; ++point_index)
        points[point_index].z += 0.01f * points[point_index].x;
    if (fviz_points_set_many(fviz_unstructured_grid_points(grid), 0u, points, point_count) != FVIZ_OK) return 14;
    update_started = wall_seconds();
    if (fviz_unstructured_grid_partition_filter_update(filter) != FVIZ_OK) return 15;
    update_seconds = wall_seconds() - update_started;

    puts("input_cells,partitions,ghost_levels,total_output_cells,ghost_cells,first_seconds,deformed_seconds");
    printf("%llu,8,1,%llu,%llu,%.6f,%.6f\n",
        (unsigned long long)cell_count,
        (unsigned long long)total_output_cells,
        (unsigned long long)ghost_cells,
        first_seconds,
        update_seconds);

    fviz_release(filter);
    fviz_release(grid);
    fviz_free(ids);
    fviz_free(points);
    return 0;
}
