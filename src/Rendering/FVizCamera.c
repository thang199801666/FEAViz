#include <math.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Math/FVizMath.h>
#include <FViz/Rendering/FVizCamera.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizCameraPrivate.h>

static const FVizObjectClass g_fviz_camera_class = {
    FVIZ_TYPE_CAMERA,
    "FVizCamera",
    &g_fviz_object_class,
    NULL,
    NULL
};

static void fviz_camera_modified(FVizCamera* camera)
{
    if (camera != NULL) fviz_object_modified((FVizObject*)camera);
}

static FVizBool fviz_camera_vec3_equal(FVizVec3 a, FVizVec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizResult fviz_camera_create(FVizCamera** out_camera)
{
    FVizCamera* camera;
    if (out_camera == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_camera must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_camera = NULL;
    camera = (FVizCamera*)fviz_internal_object_allocate(sizeof(FVizCamera), &g_fviz_camera_class, NULL);
    if (camera == NULL) return fviz_last_error_code();
    camera->position = fviz_vec3(3.0f, 2.0f, 3.0f);
    camera->target = fviz_vec3(0.0f, 0.0f, 0.0f);
    camera->up = fviz_vec3(0.0f, 1.0f, 0.0f);
    camera->fov_degrees = 45.0f;
    camera->near_plane = 0.01f;
    camera->far_plane = 10000.0f;
    camera->parallel_scale = 1.0f;
    camera->projection_mode = FVIZ_CAMERA_PERSPECTIVE;
    *out_camera = camera;
    return FVIZ_OK;
}

void fviz_camera_set_position(FVizCamera* camera, FVizVec3 position)
{
    if (camera == NULL || fviz_camera_vec3_equal(camera->position, position) != FVIZ_FALSE) return;
    camera->position = position;
    fviz_camera_modified(camera);
}

void fviz_camera_set_target(FVizCamera* camera, FVizVec3 target)
{
    if (camera == NULL || fviz_camera_vec3_equal(camera->target, target) != FVIZ_FALSE) return;
    camera->target = target;
    fviz_camera_modified(camera);
}

void fviz_camera_set_up(FVizCamera* camera, FVizVec3 up)
{
    FVizVec3 normalized;
    if (camera == NULL) return;
    normalized = fviz_vec3_normalize(up);
    if (fviz_camera_vec3_equal(camera->up, normalized) != FVIZ_FALSE) return;
    camera->up = normalized;
    fviz_camera_modified(camera);
}

FVizVec3 fviz_camera_position(const FVizCamera* camera) { return camera != NULL ? camera->position : fviz_vec3(0,0,0); }
FVizVec3 fviz_camera_target(const FVizCamera* camera) { return camera != NULL ? camera->target : fviz_vec3(0,0,0); }
FVizVec3 fviz_camera_up(const FVizCamera* camera) { return camera != NULL ? camera->up : fviz_vec3(0,1,0); }

void fviz_camera_set_perspective(FVizCamera* camera, float vertical_fov_degrees, float near_plane, float far_plane)
{
    float new_fov;
    float new_near;
    float new_far;
    if (camera == NULL) return;
    new_fov = camera->fov_degrees;
    new_near = camera->near_plane;
    new_far = camera->far_plane;
    if (vertical_fov_degrees > 1.0f && vertical_fov_degrees < 179.0f) new_fov = vertical_fov_degrees;
    if (near_plane > 0.0f) new_near = near_plane;
    if (far_plane > new_near) new_far = far_plane;
    if (camera->fov_degrees == new_fov && camera->near_plane == new_near &&
        camera->far_plane == new_far && camera->projection_mode == FVIZ_CAMERA_PERSPECTIVE)
        return;
    camera->fov_degrees = new_fov;
    camera->near_plane = new_near;
    camera->far_plane = new_far;
    camera->projection_mode = FVIZ_CAMERA_PERSPECTIVE;
    fviz_camera_modified(camera);
}

void fviz_camera_set_clipping_range(FVizCamera* camera, float near_plane, float far_plane)
{
    if (camera == NULL) return;
    if (near_plane <= 0.0f || far_plane <= near_plane) return;
    if (camera->near_plane == near_plane && camera->far_plane == far_plane) return;
    camera->near_plane = near_plane;
    camera->far_plane = far_plane;
    fviz_camera_modified(camera);
}

float fviz_camera_fov_degrees(const FVizCamera* camera) { return camera != NULL ? camera->fov_degrees : 45.0f; }

float fviz_camera_near_plane(const FVizCamera* camera) { return camera != NULL ? camera->near_plane : 0.0f; }

float fviz_camera_far_plane(const FVizCamera* camera) { return camera != NULL ? camera->far_plane : 0.0f; }

void fviz_camera_set_projection_mode(FVizCamera* camera, FVizCameraProjectionMode mode)
{
    FVizCameraProjectionMode normalized;
    if (camera == NULL) return;
    normalized = mode == FVIZ_CAMERA_PARALLEL ? FVIZ_CAMERA_PARALLEL : FVIZ_CAMERA_PERSPECTIVE;
    if (camera->projection_mode == normalized) return;
    camera->projection_mode = normalized;
    fviz_camera_modified(camera);
}

FVizCameraProjectionMode fviz_camera_projection_mode(const FVizCamera* camera)
{
    return camera != NULL ? camera->projection_mode : FVIZ_CAMERA_PERSPECTIVE;
}

void fviz_camera_set_parallel_scale(FVizCamera* camera, float scale)
{
    if (camera == NULL || scale <= 0.0f || isfinite(scale) == 0 || camera->parallel_scale == scale) return;
    camera->parallel_scale = scale;
    fviz_camera_modified(camera);
}

float fviz_camera_parallel_scale(const FVizCamera* camera)
{
    return camera != NULL ? camera->parallel_scale : 1.0f;
}

FVizMat4 fviz_camera_view_matrix(const FVizCamera* camera)
{
    return camera != NULL ? fviz_mat4_look_at(camera->position, camera->target, camera->up) : fviz_mat4_identity();
}

FVizMat4 fviz_camera_projection_matrix(const FVizCamera* camera, float aspect_ratio)
{
    if (camera == NULL || aspect_ratio <= 0.0f) return fviz_mat4_identity();
    if (camera->projection_mode == FVIZ_CAMERA_PARALLEL)
    {
        const float half_height = camera->parallel_scale;
        const float half_width = half_height * aspect_ratio;
        return fviz_mat4_orthographic(
            -half_width, half_width, -half_height, half_height,
            camera->near_plane, camera->far_plane);
    }
    return fviz_mat4_perspective(
        FVIZ_DEG_TO_RAD_F(camera->fov_degrees), aspect_ratio,
        camera->near_plane, camera->far_plane);
}

void fviz_camera_fit_bounds(FVizCamera* camera, const FVizBounds* bounds, float padding)
{
    FVizVec3 direction;
    float radius;
    float distance;
    float half_fov;
    if (camera == NULL || bounds == NULL || bounds->valid == FVIZ_FALSE) return;
    if (padding < 1.0f) padding = 1.0f;
    radius = fviz_bounds_radius(bounds);
    if (radius < 1.0e-4f) radius = 1.0f;
    direction = fviz_vec3_normalize(fviz_vec3_sub(camera->position, camera->target));
    if (fviz_vec3_length(direction) < 1.0e-4f)
        direction = fviz_vec3_normalize(fviz_vec3(1.0f, 0.7f, 1.0f));
    camera->target = fviz_bounds_center(bounds);
    if (camera->projection_mode == FVIZ_CAMERA_PARALLEL)
    {
        camera->parallel_scale = radius * padding;
        distance = radius * 3.0f;
    }
    else
    {
        half_fov = FVIZ_DEG_TO_RAD_F(camera->fov_degrees) * 0.5f;
        distance = (radius * padding) / sinf(half_fov);
    }
    camera->position = fviz_vec3_add(camera->target, fviz_vec3_scale(direction, distance));
    camera->near_plane = distance > radius * 2.0f ? distance - radius * 2.0f : 0.001f;
    if (camera->near_plane < radius * 0.001f) camera->near_plane = radius * 0.001f;
    camera->far_plane = distance + radius * 4.0f;
    fviz_camera_modified(camera);
}

void fviz_camera_orbit(FVizCamera* camera, float yaw_radians, float pitch_radians)
{
    FVizVec3 offset;
    float radius;
    float yaw;
    float pitch;
    const float limit = FVIZ_PI_F * 0.495f;
    if (camera == NULL || (yaw_radians == 0.0f && pitch_radians == 0.0f)) return;
    offset = fviz_vec3_sub(camera->position, camera->target);
    radius = fviz_vec3_length(offset);
    if (radius <= 1.0e-6f) return;
    yaw = atan2f(offset.x, offset.z) + yaw_radians;
    pitch = asinf(offset.y / radius) + pitch_radians;
    if (pitch > limit) pitch = limit;
    if (pitch < -limit) pitch = -limit;
    offset.x = radius * cosf(pitch) * sinf(yaw);
    offset.y = radius * sinf(pitch);
    offset.z = radius * cosf(pitch) * cosf(yaw);
    camera->position = fviz_vec3_add(camera->target, offset);
    fviz_camera_modified(camera);
}

void fviz_camera_dolly(FVizCamera* camera, float factor)
{
    FVizVec3 offset;
    if (camera == NULL || factor <= 0.0f || isfinite(factor) == 0 || factor == 1.0f) return;
    if (camera->projection_mode == FVIZ_CAMERA_PARALLEL)
    {
        camera->parallel_scale *= factor;
        if (camera->parallel_scale < 1.0e-9f) camera->parallel_scale = 1.0e-9f;
        fviz_camera_modified(camera);
        return;
    }
    offset = fviz_vec3_sub(camera->position, camera->target);
    camera->position = fviz_vec3_add(camera->target, fviz_vec3_scale(offset, factor));
    fviz_camera_modified(camera);
}

void fviz_camera_pan(FVizCamera* camera, float right_amount, float up_amount)
{
    FVizVec3 forward;
    FVizVec3 right;
    FVizVec3 up;
    FVizVec3 delta;
    if (camera == NULL || (right_amount == 0.0f && up_amount == 0.0f)) return;
    forward = fviz_vec3_normalize(fviz_vec3_sub(camera->target, camera->position));
    right = fviz_vec3_normalize(fviz_vec3_cross(forward, camera->up));
    up = fviz_vec3_normalize(fviz_vec3_cross(right, forward));
    delta = fviz_vec3_add(fviz_vec3_scale(right, right_amount), fviz_vec3_scale(up, up_amount));
    camera->position = fviz_vec3_add(camera->position, delta);
    camera->target = fviz_vec3_add(camera->target, delta);
    fviz_camera_modified(camera);
}

FVizRay fviz_camera_pick_ray(const FVizCamera* camera, int width, int height, int x, int y)
{
    FVizVec3 forward;
    FVizVec3 right;
    FVizVec3 up;
    FVizVec3 direction;
    float half_fov;
    float aspect;
    float ndc_x;
    float ndc_y;
    FVizRay ray;
    if (camera == NULL || width <= 0 || height <= 0)
    {
        ray.origin = fviz_vec3(0.0f, 0.0f, 0.0f);
        ray.direction = fviz_vec3(0.0f, 0.0f, -1.0f);
        return ray;
    }
    forward = fviz_vec3_normalize(fviz_vec3_sub(camera->target, camera->position));
    right = fviz_vec3_normalize(fviz_vec3_cross(forward, camera->up));
    up = fviz_vec3_normalize(fviz_vec3_cross(right, forward));
    aspect = (float)width / (float)height;
    ndc_x = 2.0f * ((float)x + 0.5f) / (float)width - 1.0f;
    ndc_y = 1.0f - 2.0f * ((float)y + 0.5f) / (float)height;
    if (camera->projection_mode == FVIZ_CAMERA_PARALLEL)
    {
        ray.origin = fviz_vec3_add(camera->position,
            fviz_vec3_add(
                fviz_vec3_scale(right, ndc_x * camera->parallel_scale * aspect),
                fviz_vec3_scale(up, ndc_y * camera->parallel_scale)));
        ray.direction = forward;
        return ray;
    }
    half_fov = tanf(FVIZ_DEG_TO_RAD_F(camera->fov_degrees) * 0.5f);
    direction = fviz_vec3_add(
        forward,
        fviz_vec3_add(
            fviz_vec3_scale(right, ndc_x * half_fov * aspect),
            fviz_vec3_scale(up, ndc_y * half_fov)));
    ray.origin = camera->position;
    ray.direction = fviz_vec3_normalize(direction);
    return ray;
}
