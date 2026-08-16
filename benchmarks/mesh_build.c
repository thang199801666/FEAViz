#include <FViz/FViz.h>

#include <stdio.h>
#include <time.h>

static double benchmark_wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

static int build_grid(uint32_t resolution, FVizVec3** out_points, uint32_t** out_triangles,
    FVizSize* out_point_count, FVizSize* out_triangle_count)
{
    const FVizSize edge = (FVizSize)resolution + 1u;
    const FVizSize point_count = edge * edge;
    const FVizSize triangle_count = (FVizSize)resolution * resolution * 2u;
    FVizVec3* points = (FVizVec3*)fviz_alloc(point_count * sizeof(*points));
    uint32_t* triangles = (uint32_t*)fviz_alloc(triangle_count * 3u * sizeof(*triangles));
    FVizSize i = 0u;
    uint32_t y;
    if (points == NULL || triangles == NULL)
    {
        fviz_free(triangles);
        fviz_free(points);
        return 0;
    }
    for (y = 0u; y <= resolution; ++y)
    {
        uint32_t x;
        for (x = 0u; x <= resolution; ++x)
            points[(FVizSize)y * edge + x] = fviz_vec3((float)x, (float)y, 0.0f);
    }
    for (y = 0u; y < resolution; ++y)
    {
        uint32_t x;
        for (x = 0u; x < resolution; ++x)
        {
            const uint32_t a = y * (resolution + 1u) + x;
            const uint32_t b = a + 1u;
            const uint32_t c = a + resolution + 1u;
            const uint32_t d = c + 1u;
            triangles[i++] = a; triangles[i++] = b; triangles[i++] = d;
            triangles[i++] = a; triangles[i++] = d; triangles[i++] = c;
        }
    }
    *out_points = points;
    *out_triangles = triangles;
    *out_point_count = point_count;
    *out_triangle_count = triangle_count;
    return 1;
}

static double build_scalar(const FVizVec3* points, FVizSize point_count,
    const uint32_t* triangles, FVizSize triangle_count)
{
    FVizPolyData* mesh = NULL;
    FVizSize i;
    double start;
    double finish;
    if (fviz_poly_data_create(&mesh) != FVIZ_OK) return -1.0;
    start = benchmark_wall_seconds();
    for (i = 0u; i < point_count; ++i)
        if (fviz_poly_data_add_point(mesh, points[i], NULL) != FVIZ_OK) goto fail;
    for (i = 0u; i < triangle_count; ++i)
        if (fviz_poly_data_add_triangle(mesh,
                triangles[i * 3u + 0u], triangles[i * 3u + 1u], triangles[i * 3u + 2u]) != FVIZ_OK)
            goto fail;
    finish = benchmark_wall_seconds();
    if (fviz_poly_data_validate(mesh) != FVIZ_OK) goto fail;
    fviz_release(mesh);
    return finish - start;
fail:
    fviz_release(mesh);
    return -1.0;
}

static double build_bulk(const FVizVec3* points, FVizSize point_count,
    const uint32_t* triangles, FVizSize triangle_count)
{
    FVizPolyData* mesh = NULL;
    double start;
    double finish;
    if (fviz_poly_data_create(&mesh) != FVIZ_OK) return -1.0;
    start = benchmark_wall_seconds();
    if (fviz_poly_data_reserve(mesh, point_count, triangle_count) != FVIZ_OK ||
        fviz_poly_data_add_points(mesh, points, point_count, NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangles(mesh, triangles, triangle_count) != FVIZ_OK)
        goto fail;
    finish = benchmark_wall_seconds();
    if (fviz_poly_data_validate(mesh) != FVIZ_OK) goto fail;
    fviz_release(mesh);
    return finish - start;
fail:
    fviz_release(mesh);
    return -1.0;
}

int main(void)
{
    const uint32_t resolution = 300u;
    FVizVec3* points = NULL;
    uint32_t* triangles = NULL;
    FVizSize point_count = 0u;
    FVizSize triangle_count = 0u;
    double scalar_seconds;
    double bulk_seconds;
    if (!build_grid(resolution, &points, &triangles, &point_count, &triangle_count)) return 1;
    scalar_seconds = build_scalar(points, point_count, triangles, triangle_count);
    bulk_seconds = build_bulk(points, point_count, triangles, triangle_count);
    if (scalar_seconds < 0.0 || bulk_seconds <= 0.0) return 2;
    puts("resolution,points,triangles,scalar_seconds,bulk_seconds,bulk_speedup");
    printf("%u,%llu,%llu,%.9f,%.9f,%.2f\n",
        resolution,
        (unsigned long long)point_count,
        (unsigned long long)triangle_count,
        scalar_seconds,
        bulk_seconds,
        scalar_seconds / bulk_seconds);
    fviz_free(triangles);
    fviz_free(points);
    return 0;
}
