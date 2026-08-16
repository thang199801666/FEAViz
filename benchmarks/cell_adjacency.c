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
    const uint32_t n = 30u;
    const uint32_t edge = n + 1u;
    const FVizSize point_count = (FVizSize)edge * edge * edge;
    const FVizSize cell_count = (FVizSize)n * n * n;
    FVizCellArray* cells = NULL;
    FVizCellAdjacency* adjacency = NULL;
    uint32_t* ids = NULL;
    FVizSize c = 0u;
    uint32_t x, y, z;
    double start, seconds;

    ids = (uint32_t*)fviz_alloc(cell_count * 8u * sizeof(*ids));
    if (ids == NULL || fviz_cell_array_create(&cells) != FVIZ_OK) return 1;
    for (z = 0u; z < n; ++z)
        for (y = 0u; y < n; ++y)
            for (x = 0u; x < n; ++x)
            {
                ids[c++] = (uint32_t)point_id(x, y, z, edge);
                ids[c++] = (uint32_t)point_id(x + 1u, y, z, edge);
                ids[c++] = (uint32_t)point_id(x + 1u, y + 1u, z, edge);
                ids[c++] = (uint32_t)point_id(x, y + 1u, z, edge);
                ids[c++] = (uint32_t)point_id(x, y, z + 1u, edge);
                ids[c++] = (uint32_t)point_id(x + 1u, y, z + 1u, edge);
                ids[c++] = (uint32_t)point_id(x + 1u, y + 1u, z + 1u, edge);
                ids[c++] = (uint32_t)point_id(x, y + 1u, z + 1u, edge);
            }
    if (fviz_cell_array_append_fixed(cells, FVIZ_CELL_HEXAHEDRON, 8u, cell_count, ids) != FVIZ_OK)
        return 2;
    start = wall_seconds();
    if (fviz_cell_adjacency_build(cells, point_count, &adjacency) != FVIZ_OK) return 3;
    seconds = wall_seconds() - start;
    puts("cells,points,adjacency_edges,seconds,cells_per_second");
    printf("%llu,%llu,%llu,%.6f,%.3f\n",
        (unsigned long long)cell_count,
        (unsigned long long)point_count,
        (unsigned long long)fviz_cell_adjacency_edge_count(adjacency),
        seconds,
        seconds > 0.0 ? (double)cell_count / seconds : 0.0);
    fviz_release(adjacency);
    fviz_release(cells);
    fviz_free(ids);
    return 0;
}
