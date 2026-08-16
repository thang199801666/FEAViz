#include <math.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Interaction/FVizWidgetManipulator.h>
#include <FViz/Math/FVizPlane.h>
#include <FViz/Math/FVizRay.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Interaction/FVizWidgetManipulatorPrivate.h>

static const FVizObjectClass g_fviz_widget_manipulator_class = {
    FVIZ_TYPE_WIDGET_MANIPULATOR,
    "FVizWidgetManipulator",
    &g_fviz_object_class,
    NULL,
    NULL
};

static FVizBool fviz_widget_ray_plane_intersection(
    FVizRay ray,
    FVizVec3 point,
    FVizVec3 normal,
    FVizVec3* out_point)
{
    const float denominator = fviz_vec3_dot(normal, ray.direction);
    float t;
    if (fabsf(denominator) < 1.0e-7f) return FVIZ_FALSE;
    t = fviz_vec3_dot(normal, fviz_vec3_sub(point, ray.origin)) / denominator;
    *out_point = fviz_ray_point_at(ray, t);
    return FVIZ_TRUE;
}

static FVizBool fviz_widget_axis_closest_point(
    FVizRay ray,
    FVizVec3 origin,
    FVizVec3 axis,
    FVizVec3* out_point)
{
    const FVizVec3 w = fviz_vec3_sub(origin, ray.origin);
    const float b = fviz_vec3_dot(axis, ray.direction);
    const float d = fviz_vec3_dot(axis, w);
    const float e = fviz_vec3_dot(ray.direction, w);
    const float denominator = 1.0f - b * b;
    float s;
    if (fabsf(denominator) < 1.0e-6f) return FVIZ_FALSE;
    s = (b * e - d) / denominator;
    *out_point = fviz_vec3_add(origin, fviz_vec3_scale(axis, s));
    return FVIZ_TRUE;
}

static FVizResult fviz_widget_manipulator_world(
    FVizWidgetManipulator* manipulator,
    FVizRenderer* renderer,
    const FVizInteractionEvent* event,
    FVizVec3 reference_world,
    FVizVec3* out_world)
{
    FVizRay ray;
    if (event->width <= 0 || event->height <= 0 ||
        fviz_renderer_display_to_world_ray(renderer, (float)event->x, (float)event->y,
            event->width, event->height, &ray) != FVIZ_OK)
        return FVIZ_ERROR_NOT_FOUND;
    if (manipulator->mode == FVIZ_WIDGET_MANIPULATOR_AXIS)
    {
        if (fviz_widget_axis_closest_point(ray, manipulator->origin, manipulator->axis, out_world) == FVIZ_TRUE)
            return FVIZ_OK;
        if (fviz_widget_ray_plane_intersection(
                ray, reference_world, manipulator->active_plane_normal, out_world) == FVIZ_FALSE)
            return FVIZ_ERROR_NOT_FOUND;
        *out_world = fviz_vec3_add(reference_world,
            fviz_vec3_scale(manipulator->axis,
                fviz_vec3_dot(fviz_vec3_sub(*out_world, reference_world), manipulator->axis)));
        return FVIZ_OK;
    }
    if (fviz_widget_ray_plane_intersection(
            ray,
            manipulator->mode == FVIZ_WIDGET_MANIPULATOR_PLANE ? manipulator->origin : reference_world,
            manipulator->active_plane_normal,
            out_world) == FVIZ_FALSE)
        return FVIZ_ERROR_NOT_FOUND;
    return FVIZ_OK;
}

FVizResult fviz_widget_manipulator_create(FVizWidgetManipulator** out_manipulator)
{
    FVizWidgetManipulator* manipulator;
    if (out_manipulator == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_manipulator = NULL;
    manipulator = (FVizWidgetManipulator*)fviz_internal_object_allocate(
        sizeof(*manipulator), &g_fviz_widget_manipulator_class, NULL);
    if (manipulator == NULL) return fviz_last_error_code();
    manipulator->mode = FVIZ_WIDGET_MANIPULATOR_VIEW_PLANE;
    manipulator->axis = fviz_vec3(1.0f, 0.0f, 0.0f);
    manipulator->plane_normal = fviz_vec3(0.0f, 0.0f, 1.0f);
    manipulator->active_plane_normal = manipulator->plane_normal;
    *out_manipulator = manipulator;
    return FVIZ_OK;
}

void fviz_widget_manipulator_set_mode(
    FVizWidgetManipulator* manipulator, FVizWidgetManipulatorMode mode)
{
    if (manipulator == NULL || mode < FVIZ_WIDGET_MANIPULATOR_VIEW_PLANE ||
        mode > FVIZ_WIDGET_MANIPULATOR_AXIS) return;
    manipulator->mode = mode;
    fviz_object_modified((FVizObject*)manipulator);
}

FVizWidgetManipulatorMode fviz_widget_manipulator_mode(const FVizWidgetManipulator* manipulator)
{
    return manipulator != NULL ? manipulator->mode : FVIZ_WIDGET_MANIPULATOR_VIEW_PLANE;
}

void fviz_widget_manipulator_set_origin(FVizWidgetManipulator* manipulator, FVizVec3 origin)
{
    if (manipulator == NULL) return;
    manipulator->origin = origin;
    fviz_object_modified((FVizObject*)manipulator);
}

FVizVec3 fviz_widget_manipulator_origin(const FVizWidgetManipulator* manipulator)
{
    return manipulator != NULL ? manipulator->origin : fviz_vec3(0.0f, 0.0f, 0.0f);
}

FVizResult fviz_widget_manipulator_set_axis(FVizWidgetManipulator* manipulator, FVizVec3 axis)
{
    if (manipulator == NULL || fviz_vec3_length(axis) <= 1.0e-12f) return FVIZ_ERROR_INVALID_ARGUMENT;
    manipulator->axis = fviz_vec3_normalize(axis);
    fviz_object_modified((FVizObject*)manipulator);
    return FVIZ_OK;
}

FVizVec3 fviz_widget_manipulator_axis(const FVizWidgetManipulator* manipulator)
{
    return manipulator != NULL ? manipulator->axis : fviz_vec3(1.0f, 0.0f, 0.0f);
}

FVizResult fviz_widget_manipulator_set_plane_normal(
    FVizWidgetManipulator* manipulator, FVizVec3 normal)
{
    if (manipulator == NULL || fviz_vec3_length(normal) <= 1.0e-12f) return FVIZ_ERROR_INVALID_ARGUMENT;
    manipulator->plane_normal = fviz_vec3_normalize(normal);
    fviz_object_modified((FVizObject*)manipulator);
    return FVIZ_OK;
}

FVizVec3 fviz_widget_manipulator_plane_normal(const FVizWidgetManipulator* manipulator)
{
    return manipulator != NULL ? manipulator->plane_normal : fviz_vec3(0.0f, 0.0f, 1.0f);
}

FVizResult fviz_widget_manipulator_begin(
    FVizWidgetManipulator* manipulator,
    FVizRenderer* renderer,
    const FVizInteractionEvent* event,
    FVizVec3 reference_world)
{
    FVizVec3 world;
    if (manipulator == NULL || renderer == NULL || event == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (manipulator->mode == FVIZ_WIDGET_MANIPULATOR_VIEW_PLANE ||
        manipulator->mode == FVIZ_WIDGET_MANIPULATOR_AXIS)
    {
        FVizCamera* camera = fviz_renderer_camera(renderer);
        FVizVec3 direction = camera != NULL
            ? fviz_vec3_sub(fviz_camera_target(camera), fviz_camera_position(camera))
            : fviz_vec3(0.0f, 0.0f, -1.0f);
        if (fviz_vec3_length(direction) <= 1.0e-12f) direction = fviz_vec3(0.0f, 0.0f, -1.0f);
        manipulator->active_plane_normal = fviz_vec3_normalize(direction);
    }
    else
        manipulator->active_plane_normal = manipulator->plane_normal;
    if (fviz_widget_manipulator_world(manipulator, renderer, event, reference_world, &world) != FVIZ_OK)
        world = reference_world;
    manipulator->last_world = world;
    manipulator->active = FVIZ_TRUE;
    fviz_object_modified((FVizObject*)manipulator);
    return FVIZ_OK;
}

FVizResult fviz_widget_manipulator_update(
    FVizWidgetManipulator* manipulator,
    FVizRenderer* renderer,
    const FVizInteractionEvent* event,
    FVizVec3* out_world,
    FVizVec3* out_delta)
{
    FVizVec3 world;
    if (manipulator == NULL || renderer == NULL || event == NULL ||
        out_world == NULL || out_delta == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (manipulator->active == FVIZ_FALSE) return FVIZ_ERROR_INVALID_STATE;
    if (fviz_widget_manipulator_world(
            manipulator, renderer, event, manipulator->last_world, &world) != FVIZ_OK)
        return fviz_last_error_code();
    *out_delta = fviz_vec3_sub(world, manipulator->last_world);
    *out_world = world;
    manipulator->last_world = world;
    return FVIZ_OK;
}

void fviz_widget_manipulator_end(FVizWidgetManipulator* manipulator)
{
    if (manipulator == NULL || manipulator->active == FVIZ_FALSE) return;
    manipulator->active = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)manipulator);
}

FVizBool fviz_widget_manipulator_active(const FVizWidgetManipulator* manipulator)
{
    return manipulator != NULL ? manipulator->active : FVIZ_FALSE;
}
