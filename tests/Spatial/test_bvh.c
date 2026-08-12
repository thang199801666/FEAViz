#include <math.h>

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

static int test_camera_pick_ray(void)
{
    FVizCamera* camera = NULL;
    FVizRay center;
    FVizRay offset;
    CHECK(fviz_camera_create(&camera) == FVIZ_OK);
    fviz_camera_set_position(camera, fviz_vec3(0, 0, 10));
    fviz_camera_set_target(camera, fviz_vec3(0, 0, 0));
    center = fviz_camera_pick_ray(camera, 800, 600, 400, 300);
    CHECK(fabsf(center.direction.x) < 1.0e-4f);
    CHECK(fabsf(center.direction.y) < 1.0e-4f);
    CHECK(fabsf(center.direction.z + 1.0f) < 1.0e-4f);
    offset = fviz_camera_pick_ray(camera, 800, 600, 0, 0);
    CHECK(offset.direction.x < 0.0f);
    CHECK(offset.direction.y > 0.0f);
    fviz_release(camera);
    return 0;
}

int main(void)
{
    CHECK(test_bvh_basic() == 0);
    CHECK(test_bvh_many_triangles() == 0);
    CHECK(test_camera_pick_ray() == 0);
    return 0;
}
