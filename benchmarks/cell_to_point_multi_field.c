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
    enum { FIELD_COUNT = 4 };
    const uint32_t n = 40u;
    const FVizSize edge = (FVizSize)n + 1u;
    const FVizSize point_count = edge * edge * edge;
    const FVizSize cell_count = (FVizSize)n * n * n;
    FVizVec3* points = NULL;
    FVizId* cells = NULL;
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* result = NULL;
    FVizSize p = 0u;
    FVizSize c = 0u;
    uint32_t z;
    uint32_t field_index;
    double start;
    double finish;

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
        fviz_unstructured_grid_add_cells_fixed_ids(
            grid, FVIZ_CELL_HEXAHEDRON, 8u, cell_count, cells) != FVIZ_OK)
        goto fail;

    for (field_index = 0u; field_index < FIELD_COUNT; ++field_index)
    {
        const uint32_t components = (field_index & 1u) != 0u ? 3u : 1u;
        FVizDataArray* field = NULL;
        char name[32];
        FVizSize value_index;
        float* values;
        (void)snprintf(name, sizeof(name), "CellField%u", (unsigned)field_index);
        if (fviz_data_array_create(FVIZ_DATA_FLOAT32, components, &field) != FVIZ_OK ||
            fviz_data_array_resize(field, cell_count) != FVIZ_OK)
        {
            fviz_release(field);
            goto fail;
        }
        values = (float*)fviz_data_array_data(field);
        for (value_index = 0u; value_index < cell_count * (FVizSize)components; ++value_index)
            values[value_index] = (float)((value_index + (FVizSize)field_index * 17u) % 101u);
        if (fviz_attribute_set_add(
                fviz_unstructured_grid_cell_data(grid), name, field) != FVIZ_OK)
        {
            fviz_release(field);
            goto fail;
        }
        fviz_release(field);
    }

    start = wall_seconds();
    if (fviz_unstructured_grid_cell_data_to_point_data(grid, &result) != FVIZ_OK) goto fail;
    finish = wall_seconds();
    if (fviz_unstructured_grid_point_count(result) != point_count) goto fail;
    for (field_index = 0u; field_index < FIELD_COUNT; ++field_index)
    {
        char name[32];
        (void)snprintf(name, sizeof(name), "CellField%u", (unsigned)field_index);
        if (fviz_attribute_set_const_get(
                fviz_unstructured_grid_point_data(result), name) == NULL)
            goto fail;
    }

    puts("cells,points,fields,seconds,cells_per_second");
    printf("%llu,%llu,%u,%.9f,%.2f\n",
        (unsigned long long)cell_count,
        (unsigned long long)point_count,
        (unsigned)FIELD_COUNT,
        finish - start,
        (double)cell_count / (finish - start));

    fviz_release(result);
    fviz_release(grid);
    fviz_free(cells);
    fviz_free(points);
    return 0;
fail:
    fviz_release(result);
    fviz_release(grid);
    fviz_free(cells);
    fviz_free(points);
    return 1;
}
