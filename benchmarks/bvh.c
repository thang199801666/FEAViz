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

static uint32_t next_random(uint32_t* state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

int main(void)
{
    const uint32_t resolution = 300u;
    const uint32_t query_count = 10000u;
    FVizPlaneSource* source = NULL;
    FVizPolyData* mesh;
    FVizBVH* bvh = NULL;
    FVizBounds bounds;
    uint32_t state = 0x12345678u;
    uint32_t i;
    uint32_t hits = 0u;
    double start;
    double build_seconds;
    double query_seconds;
    double refit_seconds;
    double rebuild_seconds;
    FVizVec3* deformed = NULL;
    FVizSize point_count;
    if (fviz_plane_source_create(&source) != FVIZ_OK ||
        fviz_plane_source_set_resolution(source, resolution, resolution) != FVIZ_OK ||
        fviz_plane_source_update(source) != FVIZ_OK)
        return 1;
    mesh = fviz_plane_source_output(source);
    if (mesh == NULL || fviz_bvh_create(&bvh) != FVIZ_OK) return 2;
    start = wall_seconds();
    if (fviz_bvh_build(bvh, mesh) != FVIZ_OK) return 3;
    build_seconds = wall_seconds() - start;
    point_count = fviz_poly_data_point_count(mesh);
    deformed = (FVizVec3*)malloc(point_count * sizeof(FVizVec3));
    if (deformed == NULL) return 5;
    {
        const FVizVec3* source_points = fviz_poly_data_points(mesh);
        FVizSize point_index;
        for (point_index = 0u; point_index < point_count; ++point_index)
        {
            deformed[point_index] = source_points[point_index];
            deformed[point_index].z += 0.25f + 0.05f * deformed[point_index].x * deformed[point_index].y;
        }
    }
    if (fviz_poly_data_set_points(mesh, deformed, point_count) != FVIZ_OK) return 6;
    start = wall_seconds();
    if (fviz_bvh_refit(bvh) != FVIZ_OK) return 7;
    refit_seconds = wall_seconds() - start;
    start = wall_seconds();
    if (fviz_bvh_build(bvh, mesh) != FVIZ_OK) return 8;
    rebuild_seconds = wall_seconds() - start;
    bounds = fviz_poly_data_bounds(mesh);
    start = wall_seconds();
    for (i = 0u; i < query_count; ++i)
    {
        const float ux = (float)(next_random(&state) & 0x00ffffffu) / 16777215.0f;
        const float uy = (float)(next_random(&state) & 0x00ffffffu) / 16777215.0f;
        FVizRay ray;
        FVizRayHit hit;
        ray.origin = fviz_vec3(
            bounds.min.x + (bounds.max.x - bounds.min.x) * ux,
            bounds.min.y + (bounds.max.y - bounds.min.y) * uy,
            bounds.max.z + 1.0f);
        ray.direction = fviz_vec3(0.0f, 0.0f, -1.0f);
        if (fviz_bvh_ray_cast(bvh, ray, &hit) != FVIZ_FALSE) ++hits;
    }
    query_seconds = wall_seconds() - start;
    puts("triangles,build_seconds,refit_seconds,rebuild_seconds,refit_speedup,queries,hits,query_seconds,us_per_query");
    printf("%llu,%.9f,%.9f,%.9f,%.2f,%u,%u,%.9f,%.3f\n",
        (unsigned long long)fviz_poly_data_triangle_count(mesh),
        build_seconds, refit_seconds, rebuild_seconds,
        refit_seconds > 0.0 ? rebuild_seconds / refit_seconds : 0.0,
        query_count, hits, query_seconds,
        query_seconds * 1.0e6 / (double)query_count);
    free(deformed);
    fviz_release(bvh);
    fviz_release(source);
    return hits == query_count ? 0 : 4;
}
