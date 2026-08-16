#include <FViz/FViz.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

static FVizResult build_grid(uint32_t n, FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* grid = NULL;
    uint32_t z, y, x;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return fviz_last_error_code();
    for (z = 0u; z <= n; ++z)
        for (y = 0u; y <= n; ++y)
            for (x = 0u; x <= n; ++x)
                if (fviz_unstructured_grid_add_point(grid, fviz_vec3((float)x, (float)y, (float)z), NULL) != FVIZ_OK)
                    goto fail;
    for (z = 0u; z < n; ++z)
        for (y = 0u; y < n; ++y)
            for (x = 0u; x < n; ++x)
            {
                const uint32_t edge = n + 1u;
                const uint32_t layer = edge * edge;
                const uint32_t base = z * layer + y * edge + x;
                const uint32_t ids[8] = {
                    base, base + 1u, base + edge + 1u, base + edge,
                    base + layer, base + layer + 1u, base + layer + edge + 1u, base + layer + edge};
                if (fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, ids) != FVIZ_OK)
                    goto fail;
            }
    *out_grid = grid;
    return FVIZ_OK;
fail:
    fviz_release(grid);
    return fviz_last_error_code();
}

static double run_queries(FVizPointLocator* locator, FVizVec3 query, uint32_t iterations)
{
    FVizLocatedCell location;
    uint32_t i;
    const double start = wall_seconds();
    for (i = 0u; i < iterations; ++i)
        if (fviz_point_locator_locate_point(locator, query, &location) == FVIZ_FALSE) return -1.0;
    return wall_seconds() - start;
}

int main(void)
{
    const uint32_t n = 20u;
    const uint32_t accelerated_iterations = 2000u;
    const uint32_t brute_iterations = 20u;
    FVizUnstructuredGrid* grid = NULL;
    FVizPointLocator* locator = NULL;
    const FVizVec3 query = {19.5f, 19.5f, 19.5f};
    double build_started;
    double build_seconds;
    double accelerated;
    double refit_seconds;
    double refit_queries;
    double brute;
    if (build_grid(n, &grid) != FVIZ_OK || fviz_point_locator_create(&locator) != FVIZ_OK) return 1;
    build_started = wall_seconds();
    if (fviz_point_locator_set_grid(locator, grid) != FVIZ_OK) return 2;
    build_seconds = wall_seconds() - build_started;
    accelerated = run_queries(locator, query, accelerated_iterations);
    {
        FVizPoints* points = fviz_unstructured_grid_points(grid);
        const FVizSize count = fviz_points_count(points);
        const FVizVec3* source = fviz_points_data(points);
        FVizVec3* deformed = (FVizVec3*)malloc((size_t)count * sizeof(*deformed));
        FVizSize i;
        double started;
        if (deformed == NULL) return 3;
        for (i = 0u; i < count; ++i)
        {
            deformed[i] = source[i];
            deformed[i].z += 0.05f;
        }
        if (fviz_points_set_many(points, 0u, deformed, count) != FVIZ_OK) return 3;
        free(deformed);
        started = wall_seconds();
        if (fviz_point_locator_refit(locator) != FVIZ_OK) return 3;
        refit_seconds = wall_seconds() - started;
    }
    refit_queries = run_queries(locator, fviz_vec3(19.5f, 19.5f, 19.55f), accelerated_iterations);
    if (fviz_unstructured_grid_add_point(grid, fviz_vec3(-10.0f, -10.0f, -10.0f), NULL) != FVIZ_OK) return 3;
    brute = run_queries(locator, query, brute_iterations);
    if (accelerated <= 0.0 || refit_queries <= 0.0 || brute <= 0.0) return 4;
    puts("cells,build_seconds,refit_seconds,accelerated_us_per_query,refit_us_per_query,brute_us_per_query,speedup");
    printf("%llu,%.9f,%.9f,%.3f,%.3f,%.3f,%.2f\n",
        (unsigned long long)fviz_unstructured_grid_cell_count(grid),
        build_seconds, refit_seconds,
        accelerated * 1.0e6 / accelerated_iterations,
        refit_queries * 1.0e6 / accelerated_iterations,
        brute * 1.0e6 / brute_iterations,
        (brute / brute_iterations) / (accelerated / accelerated_iterations));
    fviz_release(locator);
    fviz_release(grid);
    return 0;
}
