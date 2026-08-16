#include <FViz/FViz.h>

#include <stdio.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

int main(void)
{
    const uint32_t n = 24u;
    const FVizSize edge = (FVizSize)n + 1u;
    const FVizSize point_count = edge * edge * edge;
    const FVizSize cell_count = (FVizSize)n * n * n;
    FVizVec3* points = NULL;
    FVizId* cells = NULL;
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* result = NULL;
    FVizDataArray* scalar = NULL;
    FVizSize p = 0u;
    FVizSize c = 0u;
    double start;
    double finish;
    uint32_t z;

    points = (FVizVec3*)fviz_alloc(point_count * sizeof(*points));
    cells = (FVizId*)fviz_alloc(cell_count * 8u * sizeof(*cells));
    if (points == NULL || cells == NULL) goto fail;

    for (z = 0u; z <= n; ++z)
    {
        uint32_t y;
        for (y = 0u; y <= n; ++y)
        {
            uint32_t x;
            for (x = 0u; x <= n; ++x)
                points[p++] = fviz_vec3((float)x, (float)y, (float)z);
        }
    }
    for (z = 0u; z < n; ++z)
    {
        uint32_t y;
        for (y = 0u; y < n; ++y)
        {
            uint32_t x;
            for (x = 0u; x < n; ++x)
            {
                const FVizId a = ((FVizId)z * edge + y) * edge + x;
                const FVizId layer = (FVizId)edge * edge;
                cells[c++] = a;
                cells[c++] = a + 1u;
                cells[c++] = a + 1u + edge;
                cells[c++] = a + edge;
                cells[c++] = a + layer;
                cells[c++] = a + layer + 1u;
                cells[c++] = a + layer + 1u + edge;
                cells[c++] = a + layer + edge;
            }
        }
    }
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK ||
        fviz_unstructured_grid_reserve(grid, point_count, cell_count, cell_count * 8u) != FVIZ_OK ||
        fviz_unstructured_grid_add_points_ids(grid, points, point_count, NULL) != FVIZ_OK ||
        fviz_unstructured_grid_add_cells_fixed_ids(grid, FVIZ_CELL_HEXAHEDRON, 8u, cell_count, cells) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalar) != FVIZ_OK ||
        fviz_data_array_resize(scalar, cell_count) != FVIZ_OK)
        goto fail;
    {
        float* values = (float*)fviz_data_array_data(scalar);
        FVizSize i;
        for (i = 0u; i < cell_count; ++i) values[i] = (float)(i % 101u);
    }
    if (fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid), "CellValue", scalar) != FVIZ_OK)
        goto fail;

    start = wall_seconds();
    if (fviz_unstructured_grid_cell_data_to_point_data(grid, &result) != FVIZ_OK) goto fail;
    finish = wall_seconds();
    if (fviz_unstructured_grid_point_count(result) != point_count ||
        fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(result), "CellValue") == NULL)
        goto fail;

    puts("cells,points,seconds,cells_per_second");
    printf("%llu,%llu,%.9f,%.2f\n",
        (unsigned long long)cell_count,
        (unsigned long long)point_count,
        finish - start,
        (double)cell_count / (finish - start));

    fviz_release(result);
    fviz_release(scalar);
    fviz_release(grid);
    fviz_free(cells);
    fviz_free(points);
    return 0;
fail:
    fviz_release(result);
    fviz_release(scalar);
    fviz_release(grid);
    fviz_free(cells);
    fviz_free(points);
    return 1;
}
