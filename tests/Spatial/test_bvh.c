#include <math.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizResult build_surface(FVizPolyData** out_data)
{
    FVizPolyData* data = NULL;
    uint32_t a, b, c, d;
    if (fviz_poly_data_create(&data) != FVIZ_OK) return fviz_last_error_code();
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(-1,-1,0), &a) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(1,-1,0), &b) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(1,1,0), &c) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(-1,1,0), &d) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, a, b, c) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, a, c, d) == FVIZ_OK);
    CHECK(fviz_poly_data_compute_normals(data) == FVIZ_OK);
    *out_data = data;
    return FVIZ_OK;
}

static int test_bvh_basic(void)
{
    FVizPolyData* data = NULL;
    FVizBVH* bvh = NULL;
    FVizRayHit hit;
    FVizRayHit hit2;
    CHECK(build_surface(&data) == FVIZ_OK);
    CHECK(fviz_bvh_create(&bvh) == FVIZ_OK);
    CHECK(fviz_bvh_valid(bvh) == FVIZ_FALSE);
    CHECK(fviz_bvh_build(bvh, data) == FVIZ_OK);
    CHECK(fviz_bvh_valid(bvh) == FVIZ_TRUE);
    CHECK(fviz_bvh_triangle_count(bvh) == 2u);
    CHECK(fviz_bvh_ray_cast_any(bvh, fviz_ray(fviz_vec3(0,0,3), fviz_vec3(0,0,-1))) == FVIZ_TRUE);
    CHECK(fviz_bvh_ray_cast(bvh, fviz_ray(fviz_vec3(0,0,3), fviz_vec3(0,0,-1)), &hit) == FVIZ_TRUE);
    CHECK(fabsf(hit.point.z) < 1.0e-3f);
    CHECK(hit.distance > 2.9f && hit.distance < 3.1f);
    CHECK(fviz_bvh_ray_cast(bvh, fviz_ray(fviz_vec3(0,0,-3), fviz_vec3(0,0,1)), &hit2) == FVIZ_TRUE);
    CHECK(fabsf(hit2.point.z) < 1.0e-3f);
    CHECK(hit2.distance > 2.9f && hit2.distance < 3.1f);
    CHECK(fviz_bvh_ray_cast_any(bvh, fviz_ray(fviz_vec3(0,0,3), fviz_vec3(0,0,1))) == FVIZ_FALSE);
    {
        FVizClosestPoint closest;
        CHECK(fviz_bvh_closest_point(
            bvh, fviz_vec3(0.25f, 0.5f, 2.0f), -1.0f, &closest) == FVIZ_OK);
        CHECK(fabsf(closest.point.x - 0.25f) < 1.0e-5f);
        CHECK(fabsf(closest.point.y - 0.5f) < 1.0e-5f);
        CHECK(fabsf(closest.point.z) < 1.0e-5f);
        CHECK(fabsf(closest.distance_squared - 4.0f) < 1.0e-5f);
        CHECK(fabsf(closest.barycentric.x + closest.barycentric.y +
            closest.barycentric.z - 1.0f) < 1.0e-5f);
        CHECK(fviz_bvh_closest_point(
            bvh, fviz_vec3(0.25f, 0.5f, 2.0f), 1.0f, &closest) ==
            FVIZ_ERROR_NOT_FOUND);
    }
    {
        FVizBounds query = fviz_bounds_empty();
        FVizBounds miss = fviz_bounds_empty();
        fviz_bounds_include_point(&query, fviz_vec3(-0.1f, -0.1f, -0.01f));
        fviz_bounds_include_point(&query, fviz_vec3(0.1f, 0.1f, 0.01f));
        fviz_bounds_include_point(&miss, fviz_vec3(4.0f, 4.0f, -0.1f));
        fviz_bounds_include_point(&miss, fviz_vec3(5.0f, 5.0f, 0.1f));
        /* A coplanar surface used to fail because intersects_bounds cast a ray
           inside the triangle plane instead of traversing the BVH AABBs. */
        CHECK(fviz_bvh_intersects_bounds(bvh, &query) == FVIZ_TRUE);
        CHECK(fviz_bvh_intersects_bounds(bvh, &miss) == FVIZ_FALSE);
    }
    fviz_release(bvh);
    fviz_release(data);
    return 0;
}

static int test_bvh_many_triangles(void)
{
    FVizPolyData* data = NULL;
    FVizBVH* bvh = NULL;
    FVizRayHit hit;
    const FVizSize n = 40u;
    FVizSize i;
    if (fviz_poly_data_create(&data) != FVIZ_OK) return 1;
    for (i = 0u; i < n; ++i)
    {
        const float y = (float)i;
        CHECK(fviz_poly_data_add_point(data, fviz_vec3(-1.0f, y, 0.0f), NULL) == FVIZ_OK);
        CHECK(fviz_poly_data_add_point(data, fviz_vec3(1.0f, y, 0.0f), NULL) == FVIZ_OK);
        if (i > 0u)
        {
            const uint32_t base = (uint32_t)(i * 2u);
            CHECK(fviz_poly_data_add_triangle(data, base - 2u, base - 1u, base) == FVIZ_OK);
            CHECK(fviz_poly_data_add_triangle(data, base - 1u, base + 1u, base) == FVIZ_OK);
        }
    }
    CHECK(fviz_bvh_create(&bvh) == FVIZ_OK);
    CHECK(fviz_bvh_build(bvh, data) == FVIZ_OK);
    CHECK(fviz_bvh_triangle_count(bvh) == (n - 1u) * 2u);
    CHECK(fviz_bvh_ray_cast(bvh, fviz_ray(fviz_vec3(0.0f, 20.0f, 5.0f), fviz_vec3(0.0f, 0.0f, -1.0f)), &hit) == FVIZ_TRUE);
    CHECK(fabsf(hit.point.z) < 1.0e-3f);
    CHECK(hit.distance > 4.9f && hit.distance < 5.1f);
    fviz_release(bvh);
    fviz_release(data);
    return 0;
}


static int test_bvh_refit_deformed_geometry(void)
{
    FVizPolyData* data = NULL;
    FVizBVH* bvh = NULL;
    FVizRayHit hit;
    FVizVec3 points[4] = {
        {-1.0f, -1.0f, 2.0f}, {1.0f, -1.0f, 2.0f},
        {1.0f, 1.0f, 2.0f}, {-1.0f, 1.0f, 2.0f}
    };
    CHECK(build_surface(&data) == FVIZ_OK);
    CHECK(fviz_bvh_create(&bvh) == FVIZ_OK);
    CHECK(fviz_bvh_build(bvh, data) == FVIZ_OK);
    CHECK(fviz_bvh_current(bvh) == FVIZ_TRUE);
    CHECK(fviz_bvh_refit_required(bvh) == FVIZ_FALSE);
    CHECK(fviz_poly_data_set_points(data, points, 4u) == FVIZ_OK);
    CHECK(fviz_bvh_current(bvh) == FVIZ_FALSE);
    CHECK(fviz_bvh_refit_required(bvh) == FVIZ_TRUE);
    CHECK(fviz_bvh_update(bvh) == FVIZ_OK);
    CHECK(fviz_bvh_current(bvh) == FVIZ_TRUE);
    CHECK(fviz_bvh_ray_cast(
        bvh, fviz_ray(fviz_vec3(0.0f, 0.0f, 3.0f), fviz_vec3(0.0f, 0.0f, -1.0f)), &hit) == FVIZ_TRUE);
    CHECK(fabsf(hit.point.z - 2.0f) < 1.0e-4f);
    CHECK(fabsf(hit.distance - 1.0f) < 1.0e-4f);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0.0f, 0.0f, 2.0f), NULL) == FVIZ_OK);
    /* Point-only growth changes geometry but not connectivity and remains refittable. */
    CHECK(fviz_bvh_refit_required(bvh) == FVIZ_TRUE);
    CHECK(fviz_bvh_update(bvh) == FVIZ_OK);
    CHECK(fviz_bvh_current(bvh) == FVIZ_TRUE);
    /* Attribute-only changes do not invalidate spatial acceleration. */
    {
        FVizDataArray* field = NULL;
        float values[5] = {0,1,2,3,4};
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &field) == FVIZ_OK);
        CHECK(fviz_data_array_append_tuples(field, values, 5u) == FVIZ_OK);
        CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(data), "S", field) == FVIZ_OK);
        fviz_release(field);
        CHECK(fviz_bvh_current(bvh) == FVIZ_TRUE);
    }
    /* Connectivity changes require rebuild rather than refit. */
    CHECK(fviz_poly_data_add_triangle(data, 0u, 1u, 4u) == FVIZ_OK);
    CHECK(fviz_bvh_refit_required(bvh) == FVIZ_FALSE);
    CHECK(fviz_bvh_current(bvh) == FVIZ_FALSE);
    CHECK(fviz_bvh_update(bvh) == FVIZ_OK);
    CHECK(fviz_bvh_triangle_count(bvh) == 3u);
    CHECK(fviz_bvh_current(bvh) == FVIZ_TRUE);
    fviz_release(bvh);
    fviz_release(data);
    return 0;
}

static int test_bvh_rebuild_empty_clears_state(void)
{
    FVizPolyData* data = NULL;
    FVizPolyData* empty = NULL;
    FVizBVH* bvh = NULL;
    CHECK(build_surface(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_create(&empty) == FVIZ_OK);
    CHECK(fviz_bvh_create(&bvh) == FVIZ_OK);
    CHECK(fviz_bvh_build(bvh, data) == FVIZ_OK);
    CHECK(fviz_bvh_valid(bvh) == FVIZ_TRUE);
    CHECK(fviz_bvh_triangle_count(bvh) == 2u);
    CHECK(fviz_bvh_build(bvh, empty) == FVIZ_OK);
    CHECK(fviz_bvh_valid(bvh) == FVIZ_FALSE);
    CHECK(fviz_bvh_triangle_count(bvh) == 0u);
    CHECK(fviz_bvh_ray_cast_any(
        bvh, fviz_ray(fviz_vec3(0.0f, 0.0f, 1.0f), fviz_vec3(0.0f, 0.0f, -1.0f))) == FVIZ_FALSE);
    fviz_release(bvh);
    fviz_release(empty);
    fviz_release(data);
    return 0;
}

static int test_camera_pick_ray(void)
{
    FVizCamera* camera = NULL;
    FVizRay center;
    FVizRay offset;
    CHECK(fviz_camera_create(&camera) == FVIZ_OK);
    fviz_camera_set_position(camera, fviz_vec3(0, 0, 10));
    fviz_camera_set_target(camera, fviz_vec3(0, 0, 0));
    center = fviz_camera_pick_ray(camera, 800, 600, 400, 300);
    /* Mouse coordinates address raster pixels.  For an even-sized viewport the
       optical axis lies between the four center pixels, so a pixel-center ray
       is expected to be very slightly off-axis. */
    CHECK(fabsf(center.direction.x) < 1.0e-3f);
    CHECK(fabsf(center.direction.y) < 1.0e-3f);
    CHECK(fabsf(center.direction.z + 1.0f) < 1.0e-4f);
    offset = fviz_camera_pick_ray(camera, 800, 600, 0, 0);
    CHECK(offset.direction.x < 0.0f);
    CHECK(offset.direction.y > 0.0f);
    fviz_release(camera);
    return 0;
}

static int test_bvh_thread_equivalence(void)
{
    FVizPolyData* data = NULL;
    FVizBVH* serial = NULL;
    FVizBVH* parallel = NULL;
    FVizRayHit serial_hit;
    FVizRayHit parallel_hit;
    const FVizRay ray = fviz_ray(fviz_vec3(0.0f, 8.0f, 5.0f), fviz_vec3(0.0f, 0.0f, -1.0f));
    uint32_t i;
    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    for (i = 0u; i < 64u; ++i)
    {
        const float y = (float)i * 0.25f;
        CHECK(fviz_poly_data_add_point(data, fviz_vec3(-1.0f, y, 0.0f), NULL) == FVIZ_OK);
        CHECK(fviz_poly_data_add_point(data, fviz_vec3(1.0f, y, 0.0f), NULL) == FVIZ_OK);
        if (i > 0u)
        {
            const uint32_t base = i * 2u;
            CHECK(fviz_poly_data_add_triangle(data, base - 2u, base - 1u, base) == FVIZ_OK);
            CHECK(fviz_poly_data_add_triangle(data, base - 1u, base + 1u, base) == FVIZ_OK);
        }
    }
    CHECK(fviz_bvh_create(&serial) == FVIZ_OK);
    CHECK(fviz_bvh_create(&parallel) == FVIZ_OK);
    fviz_parallel_set_thread_limit(1u);
    CHECK(fviz_bvh_build(serial, data) == FVIZ_OK);
    fviz_parallel_set_thread_limit(4u);
    CHECK(fviz_bvh_build(parallel, data) == FVIZ_OK);
    CHECK(fviz_bvh_ray_cast(serial, ray, &serial_hit) == FVIZ_TRUE);
    CHECK(fviz_bvh_ray_cast(parallel, ray, &parallel_hit) == FVIZ_TRUE);
    CHECK(serial_hit.triangle_index == parallel_hit.triangle_index);
    CHECK(fabsf(serial_hit.distance - parallel_hit.distance) < 1.0e-6f);
    fviz_parallel_set_thread_limit(0u);
    fviz_release(parallel);
    fviz_release(serial);
    fviz_release(data);
    return 0;
}

static int test_bvh_batch_queries_and_memory(void)
{
    FVizPolyData* data = NULL;
    FVizBVH* bvh = NULL;
    FVizRay rays[3];
    FVizRayHit hits[3];
    FVizBool hit_flags[3] = { FVIZ_FALSE, FVIZ_FALSE, FVIZ_FALSE };
    FVizVec3 queries[3];
    FVizClosestPoint closest[3];
    FVizBool found[3] = { FVIZ_FALSE, FVIZ_FALSE, FVIZ_FALSE };
    CHECK(build_surface(&data) == FVIZ_OK);
    CHECK(fviz_bvh_create(&bvh) == FVIZ_OK);
    CHECK(fviz_bvh_build(bvh, data) == FVIZ_OK);
    rays[0] = fviz_ray(fviz_vec3(0, 0, 2), fviz_vec3(0, 0, -1));
    rays[1] = fviz_ray(fviz_vec3(4, 4, 2), fviz_vec3(0, 0, -1));
    rays[2] = fviz_ray(fviz_vec3(-0.5f, 0.5f, -2), fviz_vec3(0, 0, 1));
    CHECK(fviz_bvh_ray_cast_batch(bvh, rays, 3u, hits, hit_flags, NULL) == FVIZ_OK);
    CHECK(hit_flags[0] == FVIZ_TRUE);
    CHECK(hit_flags[1] == FVIZ_FALSE);
    CHECK(hit_flags[2] == FVIZ_TRUE);
    queries[0] = fviz_vec3(0, 0, 1);
    queries[1] = fviz_vec3(4, 4, 1);
    queries[2] = fviz_vec3(-0.5f, 0.5f, -3);
    CHECK(fviz_bvh_closest_point_batch(
        bvh, queries, 3u, 2.0f, closest, found, NULL) == FVIZ_OK);
    CHECK(found[0] == FVIZ_TRUE);
    CHECK(found[1] == FVIZ_FALSE);
    CHECK(found[2] == FVIZ_FALSE);
    CHECK(fviz_bvh_memory_size(bvh) > sizeof(FVizBVH*));
    fviz_release(bvh);
    fviz_release(data);
    return 0;
}

int main(void)
{
    CHECK(test_bvh_basic() == 0);
    CHECK(test_bvh_many_triangles() == 0);
    CHECK(test_bvh_thread_equivalence() == 0);
    CHECK(test_bvh_batch_queries_and_memory() == 0);
    CHECK(test_bvh_refit_deformed_geometry() == 0);
    CHECK(test_bvh_rebuild_empty_clears_state() == 0);
    CHECK(test_camera_pick_ray() == 0);
    return 0;
}
