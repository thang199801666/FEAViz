#include <math.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int test_vec2(void)
{
    FVizVec2 a = fviz_vec2(3, 4);
    FVizVec2 n = fviz_vec2_normalize(a);
    CHECK(fabsf(fviz_vec2_length(a) - 5.0f) < 1.0e-5f);
    CHECK(fabsf(fviz_vec2_length(n) - 1.0f) < 1.0e-5f);
    CHECK(fabsf(fviz_vec2_dot(a, fviz_vec2(2, 1)) - 10.0f) < 1.0e-5f);
    return 0;
}

static int test_vec4(void)
{
    FVizVec4 a = fviz_vec4(1, 2, 3, 4);
    FVizVec4 b = fviz_vec4(5, 6, 7, 8);
    FVizVec4 sum = fviz_vec4_add(a, b);
    CHECK(sum.x == 6.0f && sum.y == 8.0f && sum.z == 10.0f && sum.w == 12.0f);
    CHECK(fabsf(fviz_vec4_dot(a, b) - 70.0f) < 1.0e-5f);
    return 0;
}

static int test_quat(void)
{
    const FVizQuat identity = fviz_quat_identity();
    FVizQuat rot = fviz_quat_from_axis_angle(fviz_vec3(0, 1, 0), FVIZ_PI_F * 0.5f);
    FVizVec3 x = fviz_vec3(1, 0, 0);
    FVizVec3 z_axis = fviz_quat_rotate_vec3(rot, x);
    FVizVec3 forward = fviz_quat_rotate_vec3(identity, x);
    CHECK(fabsf(z_axis.x) < 1.0e-4f && fabsf(z_axis.z + 1.0f) < 1.0e-4f);
    CHECK(fabsf(forward.x - 1.0f) < 1.0e-6f);
    rot = fviz_quat_normalize(rot);
    CHECK(fabsf(fviz_quat_dot(rot, rot) - 1.0f) < 1.0e-5f);
    return 0;
}

static int test_mat3(void)
{
    const FVizQuat rot = fviz_quat_from_axis_angle(fviz_vec3(0, 0, 1), FVIZ_PI_F * 0.5f);
    FVizMat3 m = fviz_mat3_from_quaternion(rot);
    FVizVec3 rotated = fviz_mat3_transform_vec3(m, fviz_vec3(1, 0, 0));
    FVizMat3 inv = fviz_mat3_inverse(m);
    FVizVec3 round_trip = fviz_mat3_transform_vec3(inv, rotated);
    FVizMat3 t = fviz_mat3_transpose(m);
    CHECK(fabsf(rotated.y - 1.0f) < 1.0e-4f);
    CHECK(fabsf(round_trip.x - 1.0f) < 1.0e-4f && fabsf(round_trip.y) < 1.0e-4f);
    CHECK(fabsf(t.m[1] - m.m[3]) < 1.0e-6f);
    return 0;
}

static int test_ray(void)
{
    FVizRay ray = fviz_ray(fviz_vec3(0, 0, 0), fviz_vec3(0, 0, 1));
    FVizVec3 at = fviz_ray_point_at(ray, 2.0f);
    float t = 0.0f;
    CHECK(fabsf(at.z - 2.0f) < 1.0e-6f);
    CHECK(fabsf(fviz_ray_distance_to_point(ray, fviz_vec3(1, 0, 0)) - 1.0f) < 1.0e-6f);
    CHECK(fviz_ray_intersects_sphere(ray, fviz_vec3(0, 0, 5), 1.0f, &t) == FVIZ_TRUE);
    CHECK(fabsf(t - 4.0f) < 1.0e-5f);
    CHECK(fviz_ray_intersects_sphere(ray, fviz_vec3(5, 0, 5), 1.0f, NULL) == FVIZ_FALSE);
    return 0;
}

static int test_plane(void)
{
    FVizPlane plane = fviz_plane_from_point_normal(fviz_vec3(0, 0, 3), fviz_vec3(0, 0, 1));
    CHECK(fabsf(fviz_plane_distance_to_point(plane, fviz_vec3(0, 0, 0)) + 3.0f) < 1.0e-5f);
    CHECK(fabsf(fviz_plane_distance_to_point(plane, fviz_vec3(0, 0, 3))) < 1.0e-5f);
    FVizVec3 projected = fviz_plane_project_point(plane, fviz_vec3(1, 2, 0));
    CHECK(fabsf(projected.z - 3.0f) < 1.0e-5f);
    return 0;
}

static int test_transform(void)
{
    FVizTransform* transform = NULL;
    FVizVec3 point;
    FVizVec3 vector;
    FVizMTime before;
    CHECK(fviz_transform_create(&transform) == FVIZ_OK);
    CHECK(fviz_object_type_id((const FVizObject*)transform) == FVIZ_TYPE_TRANSFORM);
    before = fviz_object_mtime((const FVizObject*)transform);
    fviz_transform_translate(transform, fviz_vec3(2.0f, 3.0f, 4.0f));
    fviz_transform_scale(transform, fviz_vec3(2.0f, 2.0f, 2.0f));
    CHECK(fviz_object_mtime((const FVizObject*)transform) > before);
    point = fviz_transform_point(transform, fviz_vec3(1.0f, 0.0f, 0.0f));
    vector = fviz_transform_vector(transform, fviz_vec3(1.0f, 0.0f, 0.0f));
    CHECK(fabsf(point.x - 4.0f) < 1.0e-5f && fabsf(point.y - 3.0f) < 1.0e-5f);
    CHECK(fabsf(vector.x - 2.0f) < 1.0e-5f && fabsf(vector.y) < 1.0e-5f);
    fviz_transform_identity(transform);
    point = fviz_transform_point(transform, fviz_vec3(1.0f, 2.0f, 3.0f));
    CHECK(point.x == 1.0f && point.y == 2.0f && point.z == 3.0f);
    fviz_release(transform);
    return 0;
}

static int test_tensor(void)
{
    const double components[6] = {4.0, 2.0, 1.0, 0.0, 0.0, 0.0};
    FVizSymmetricTensor3d tensor;
    FVizSymmetricTensor3d deviatoric;
    double values[3];
    double vectors[9];
    CHECK(fviz_symmetric_tensor3d_from_components(components, 6u, &tensor) == FVIZ_OK);
    CHECK(fabs(fviz_symmetric_tensor3d_trace(&tensor) - 7.0) < 1.0e-12);
    CHECK(fviz_symmetric_tensor3d_eigensystem(&tensor, values, vectors) == FVIZ_OK);
    CHECK(fabs(values[0] - 4.0) < 1.0e-12);
    CHECK(fabs(values[1] - 2.0) < 1.0e-12);
    CHECK(fabs(values[2] - 1.0) < 1.0e-12);
    CHECK(fabs(vectors[0] - 1.0) < 1.0e-12);
    CHECK(fviz_symmetric_tensor3d_deviatoric(&tensor, &deviatoric) == FVIZ_OK);
    CHECK(fabs(fviz_symmetric_tensor3d_trace(&deviatoric)) < 1.0e-12);
    return 0;
}

int main(void)
{
    FVizVec3 x = fviz_vec3(1,0,0);
    FVizVec3 y = fviz_vec3(0,1,0);
    FVizVec3 z = fviz_vec3_cross(x, y);
    FVizBounds bounds = fviz_bounds_empty();
    FVizMat4 identity = fviz_mat4_identity();
    CHECK(fabsf(z.z - 1.0f) < 1.0e-6f);
    fviz_bounds_include_point(&bounds, fviz_vec3(-1,-2,-3));
    fviz_bounds_include_point(&bounds, fviz_vec3(3,2,1));
    CHECK(bounds.valid == FVIZ_TRUE);
    CHECK(fabsf(fviz_bounds_center(&bounds).x - 1.0f) < 1.0e-6f);
    CHECK(identity.m[0] == 1.0f && identity.m[15] == 1.0f);
    CHECK(test_vec2() == 0);
    CHECK(test_vec4() == 0);
    CHECK(test_quat() == 0);
    CHECK(test_mat3() == 0);
    CHECK(test_ray() == 0);
    CHECK(test_plane() == 0);
    CHECK(test_transform() == 0);
    CHECK(test_tensor() == 0);
    return 0;
}
