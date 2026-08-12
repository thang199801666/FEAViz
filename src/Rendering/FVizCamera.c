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
    *out_camera = camera;
    return FVIZ_OK;
}

void fviz_camera_set_position(FVizCamera* camera, FVizVec3 position) { if (camera != NULL) camera->position = position; }
void fviz_camera_set_target(FVizCamera* camera, FVizVec3 target) { if (camera != NULL) camera->target = target; }
void fviz_camera_set_up(FVizCamera* camera, FVizVec3 up) { if (camera != NULL) camera->up = fviz_vec3_normalize(up); }
FVizVec3 fviz_camera_position(const FVizCamera* camera) { return camera != NULL ? camera->position : fviz_vec3(0,0,0); }
FVizVec3 fviz_camera_target(const FVizCamera* camera) { return camera != NULL ? camera->target : fviz_vec3(0,0,0); }
FVizVec3 fviz_camera_up(const FVizCamera* camera) { return camera != NULL ? camera->up : fviz_vec3(0,1,0); }

void fviz_camera_set_perspective(FVizCamera* camera, float vertical_fov_degrees, float near_plane, float far_plane)
{
    if (camera == NULL) return;
    if (vertical_fov_degrees > 1.0f && vertical_fov_degrees < 179.0f) camera->fov_degrees = vertical_fov_degrees;
    if (near_plane > 0.0f) camera->near_plane = near_plane;
    if (far_plane > camera->near_plane) camera->far_plane = far_plane;
}

float fviz_camera_fov_degrees(const FVizCamera* camera) { return camera != NULL ? camera->fov_degrees : 45.0f; }
FVizMat4 fviz_camera_view_matrix(const FVizCamera* camera)
{
    return camera != NULL ? fviz_mat4_look_at(camera->position, camera->target, camera->up) : fviz_mat4_identity();
}
FVizMat4 fviz_camera_projection_matrix(const FVizCamera* camera, float aspect_ratio)
{
    return camera != NULL ? fviz_mat4_perspective(FVIZ_DEG_TO_RAD_F(camera->fov_degrees), aspect_ratio, camera->near_plane, camera->far_plane) : fviz_mat4_identity();
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
    half_fov = FVIZ_DEG_TO_RAD_F(camera->fov_degrees) * 0.5f;
    distance = (radius * padding) / sinf(half_fov);
    direction = fviz_vec3_normalize(fviz_vec3_sub(camera->position, camera->target));
    if (fviz_vec3_length(direction) < 1.0e-4f) direction = fviz_vec3_normalize(fviz_vec3(1.0f, 0.7f, 1.0f));
    camera->target = fviz_bounds_center(bounds);
    camera->position = fviz_vec3_add(camera->target, fviz_vec3_scale(direction, distance));
    camera->near_plane = distance > radius * 2.0f ? distance - radius * 2.0f : 0.001f;
    if (camera->near_plane < radius * 0.001f) camera->near_plane = radius * 0.001f;
    camera->far_plane = distance + radius * 4.0f;
}

void fviz_camera_orbit(FVizCamera* camera, float yaw_radians, float pitch_radians)
{
    FVizVec3 offset;
    float radius;
    float yaw;
    float pitch;
    const float limit = FVIZ_PI_F * 0.495f;
    if (camera == NULL) return;
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
}

void fviz_camera_dolly(FVizCamera* camera, float factor)
{
    FVizVec3 offset;
    if (camera == NULL || factor <= 0.0f) return;
    offset = fviz_vec3_sub(camera->position, camera->target);
    camera->position = fviz_vec3_add(camera->target, fviz_vec3_scale(offset, factor));
}

void fviz_camera_pan(FVizCamera* camera, float right_amount, float up_amount)
{
    FVizVec3 forward;
    FVizVec3 right;
    FVizVec3 up;
    FVizVec3 delta;
    if (camera == NULL) return;
    forward = fviz_vec3_normalize(fviz_vec3_sub(camera->target, camera->position));
    right = fviz_vec3_normalize(fviz_vec3_cross(forward, camera->up));
    up = fviz_vec3_normalize(fviz_vec3_cross(right, forward));
    delta = fviz_vec3_add(fviz_vec3_scale(right, right_amount), fviz_vec3_scale(up, up_amount));
    camera->position = fviz_vec3_add(camera->position, delta);
    camera->target = fviz_vec3_add(camera->target, delta);
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
    half_fov = tanf(FVIZ_DEG_TO_RAD_F(camera->fov_degrees) * 0.5f);
    aspect = (float)width / (float)height;
    ndc_x = 2.0f * (float)x / (float)width - 1.0f;
    ndc_y = 1.0f - 2.0f * (float)y / (float)height;
    direction = fviz_vec3_add(
        forward,
        fviz_vec3_add(
            fviz_vec3_scale(right, ndc_x * half_fov * aspect),
            fviz_vec3_scale(up, ndc_y * half_fov)));
    ray.origin = camera->position;
    ray.direction = fviz_vec3_normalize(direction);
    return ray;
}
