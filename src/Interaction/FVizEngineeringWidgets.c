#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Interaction/FVizEngineeringWidgets.h>
#include <FViz/Interaction/FVizWidgetManipulator.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Rendering/FVizMapper.h>
#include <FViz/Rendering/FVizRenderWindow.h>
#include <FViz/Spatial/FVizBVH.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizEngineeringWidgetsPrivate.h>

static FVizVec3 fviz_engineering_transform_point(FVizMat4 matrix, FVizVec3 point)
{
    return fviz_vec3(matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
                     matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
                     matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]);
}

static FVizBool fviz_engineering_ray_plane(FVizRay ray, FVizVec3 point, FVizVec3 normal, FVizVec3* out_point)
{
    const float denominator = fviz_vec3_dot(normal, ray.direction);
    float t;
    if (fabsf(denominator) < 1.0e-7f) return FVIZ_FALSE;
    t = fviz_vec3_dot(normal, fviz_vec3_sub(point, ray.origin)) / denominator;
    *out_point = fviz_ray_point_at(ray, t);
    return FVIZ_TRUE;
}

static void fviz_engineering_plane_basis(FVizVec3 normal, FVizVec3* out_u, FVizVec3* out_v)
{
    FVizVec3 reference = fabsf(normal.z) < 0.9f ? fviz_vec3(0.0f, 0.0f, 1.0f) : fviz_vec3(0.0f, 1.0f, 0.0f);
    FVizVec3 u = fviz_vec3_cross(reference, normal);
    if (fviz_vec3_length(u) < 1.0e-7f)
    {
        reference = fviz_vec3(1.0f, 0.0f, 0.0f);
        u = fviz_vec3_cross(reference, normal);
    }
    u = fviz_vec3_normalize(u);
    *out_u = u;
    *out_v = fviz_vec3_normalize(fviz_vec3_cross(normal, u));
}

static FVizResult fviz_engineering_make_poly_data(const FVizVec3* points, FVizSize point_count, const uint32_t* lines,
                                                  FVizSize line_count, const uint32_t* triangles,
                                                  FVizSize triangle_count, FVizPolyData** out_data)
{
    FVizPolyData* data = NULL;
    if (out_data == NULL || (point_count != 0u && points == NULL)) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_data = NULL;
    if (fviz_poly_data_create(&data) != FVIZ_OK ||
        fviz_poly_data_reserve(data, point_count, triangle_count) != FVIZ_OK ||
        (point_count != 0u && fviz_poly_data_add_points(data, points, point_count, NULL) != FVIZ_OK) ||
        (line_count != 0u && fviz_poly_data_add_lines(data, lines, line_count) != FVIZ_OK) ||
        (triangle_count != 0u && fviz_poly_data_add_triangles(data, triangles, triangle_count) != FVIZ_OK))
    {
        fviz_release(data);
        return fviz_last_error_code();
    }
    *out_data = data;
    return FVIZ_OK;
}

static FVizResult fviz_engineering_replace_actor_geometry(FVizActor* actor, const FVizVec3* points,
                                                          FVizSize point_count, const uint32_t* lines,
                                                          FVizSize line_count, const uint32_t* triangles,
                                                          FVizSize triangle_count)
{
    FVizPolyData* data = NULL;
    FVizResult result =
        fviz_engineering_make_poly_data(points, point_count, lines, line_count, triangles, triangle_count, &data);
    if (result == FVIZ_OK) result = fviz_actor_set_poly_data(actor, data);
    fviz_release(data);
    return result;
}

static FVizBool fviz_engineering_pick_triangle_world(FVizRenderWindow* window, FVizRenderer* renderer, int x, int y,
                                                     FVizActor** out_actor, FVizSize* out_triangle, FVizVec3* out_world)
{
    FVizHardwarePick pick;
    FVizRay ray;
    FVizActor* actor;
    const FVizPolyData* data;
    const uint32_t* triangle;
    const FVizVec3* points;
    FVizVec3 p0, p1, p2;
    FVizVec3 edge1, edge2, pvec, tvec, qvec;
    float determinant;
    float inverse;
    float u, v, t;
    int width, height;
    if (window == NULL || renderer == NULL || out_world == NULL) return FVIZ_FALSE;
    if (fviz_render_window_hardware_pick(window, x, y, &pick) != FVIZ_OK || pick.actor == NULL) return FVIZ_FALSE;
    actor = pick.actor;
    data = fviz_actor_const_poly_data(actor);
    if (data == NULL || pick.rendered_primitive_id >= fviz_poly_data_triangle_count(data)) return FVIZ_FALSE;
    triangle = fviz_poly_data_triangle_indices(data) + pick.rendered_primitive_id * 3u;
    points = fviz_poly_data_points(data);
    p0 = fviz_engineering_transform_point(fviz_actor_transform_matrix(actor), points[triangle[0]]);
    p1 = fviz_engineering_transform_point(fviz_actor_transform_matrix(actor), points[triangle[1]]);
    p2 = fviz_engineering_transform_point(fviz_actor_transform_matrix(actor), points[triangle[2]]);
    fviz_render_window_get_size(window, &width, &height);
    if (fviz_renderer_display_to_world_ray(renderer, (float)x, (float)y, width, height, &ray) != FVIZ_OK)
        return FVIZ_FALSE;
    edge1 = fviz_vec3_sub(p1, p0);
    edge2 = fviz_vec3_sub(p2, p0);
    pvec = fviz_vec3_cross(ray.direction, edge2);
    determinant = fviz_vec3_dot(edge1, pvec);
    if (fabsf(determinant) < 1.0e-8f) return FVIZ_FALSE;
    inverse = 1.0f / determinant;
    tvec = fviz_vec3_sub(ray.origin, p0);
    u = fviz_vec3_dot(tvec, pvec) * inverse;
    if (u < 0.0f || u > 1.0f) return FVIZ_FALSE;
    qvec = fviz_vec3_cross(tvec, edge1);
    v = fviz_vec3_dot(ray.direction, qvec) * inverse;
    if (v < 0.0f || u + v > 1.0f) return FVIZ_FALSE;
    t = fviz_vec3_dot(edge2, qvec) * inverse;
    if (t < 0.0f) return FVIZ_FALSE;
    *out_world = fviz_ray_point_at(ray, t);
    if (out_actor != NULL) *out_actor = actor;
    if (out_triangle != NULL) *out_triangle = pick.rendered_primitive_id;
    return FVIZ_TRUE;
}

/* ------------------------------------------------------------------------- */
/* Handle widget                                                              */
/* ------------------------------------------------------------------------- */

static FVizResult fviz_handle_widget_rebuild(FVizHandleWidget* widget)
{
    if (widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_engineering_replace_actor_geometry(widget->actor, &widget->position, 1u, NULL, 0u, NULL, 0u) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_actor_set_point_visibility(widget->actor, FVIZ_TRUE);
    fviz_actor_set_point_size(widget->actor, widget->size_pixels);
    (void)fviz_widget_representation_set_actor_visible(widget->representation, widget->actor, FVIZ_TRUE);
    return FVIZ_OK;
}

static FVizBool fviz_handle_widget_hit(FVizHandleWidget* widget, const FVizInteractionEvent* event)
{
    FVizVec3 display;
    float dx;
    float dy;
    float radius;
    if (widget == NULL || event == NULL || event->width <= 0 || event->height <= 0) return FVIZ_FALSE;
    if (fviz_renderer_world_to_display(fviz_widget_renderer(widget->widget), widget->position, event->width,
                                       event->height, &display) != FVIZ_OK)
        return FVIZ_FALSE;
    dx = (float)event->x - display.x;
    dy = (float)event->y - display.y;
    radius = widget->size_pixels * 0.5f + widget->pick_tolerance_pixels;
    return dx * dx + dy * dy <= radius * radius ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_handle_widget_event(FVizWidget* base_widget, const FVizInteractionEvent* event, void* user_data)
{
    FVizHandleWidget* widget = (FVizHandleWidget*)user_data;
    if (event->type == FVIZ_INTERACTION_KEY_DOWN && event->key == FVIZ_KEY_ESCAPE &&
        fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        widget->position = widget->interaction_start_position;
        fviz_widget_manipulator_end(widget->manipulator);
        (void)fviz_handle_widget_rebuild(widget);
        fviz_widget_cancel_interaction(base_widget);
        fviz_widget_value_changed(base_widget);
        (void)fviz_widget_request_render(base_widget);
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_BUTTON_DOWN && event->button == FVIZ_MOUSE_BUTTON_LEFT &&
        fviz_handle_widget_hit(widget, event) == FVIZ_TRUE)
    {
        widget->interaction_start_position = widget->position;
        fviz_widget_manipulator_set_origin(widget->manipulator, widget->position);
        if (fviz_widget_manipulator_begin(widget->manipulator, fviz_widget_renderer(base_widget), event,
                                          widget->position) != FVIZ_OK)
            return FVIZ_FALSE;
        fviz_widget_begin_interaction(base_widget);
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_MOVE && fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        FVizVec3 world;
        FVizVec3 delta;
        if (fviz_widget_manipulator_update(widget->manipulator, fviz_widget_renderer(base_widget), event, &world,
                                           &delta) == FVIZ_OK)
        {
            (void)world;
            widget->position = fviz_vec3_add(widget->position, delta);
            (void)fviz_handle_widget_rebuild(widget);
            fviz_widget_interaction(base_widget);
            fviz_widget_value_changed(base_widget);
            (void)fviz_widget_request_render(base_widget);
        }
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_BUTTON_UP && event->button == FVIZ_MOUSE_BUTTON_LEFT &&
        fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        fviz_widget_manipulator_end(widget->manipulator);
        fviz_widget_end_interaction(base_widget);
        (void)fviz_widget_request_render(base_widget);
        return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

static void fviz_handle_widget_destroy(FVizObject* object)
{
    FVizHandleWidget* widget = (FVizHandleWidget*)object;
    fviz_release(widget->widget);
    fviz_release(widget->representation);
    fviz_release(widget->manipulator);
    fviz_release(widget->actor);
}

static const FVizObjectClass g_fviz_handle_widget_class = {FVIZ_TYPE_HANDLE_WIDGET, "FVizHandleWidget",
                                                           &g_fviz_object_class, fviz_handle_widget_destroy, NULL};

FVizResult fviz_handle_widget_create(FVizRenderWindowInteractor* interactor, FVizRenderer* renderer,
                                     FVizHandleWidget** out_widget)
{
    FVizHandleWidget* widget;
    if (renderer == NULL || out_widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_widget = NULL;
    widget = (FVizHandleWidget*)fviz_internal_object_allocate(sizeof(*widget), &g_fviz_handle_widget_class, NULL);
    if (widget == NULL) return fviz_last_error_code();
    widget->size_pixels = 11.0f;
    widget->pick_tolerance_pixels = 5.0f;
    widget->color[0] = 1.0f;
    widget->color[1] = 0.65f;
    widget->color[2] = 0.1f;
    if (fviz_widget_representation_create(renderer, &widget->representation) != FVIZ_OK ||
        fviz_widget_manipulator_create(&widget->manipulator) != FVIZ_OK ||
        fviz_actor_create(&widget->actor) != FVIZ_OK ||
        fviz_widget_representation_add_actor(widget->representation, widget->actor) != FVIZ_OK ||
        fviz_widget_create(interactor, renderer, widget->representation, &widget->widget) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    fviz_actor_set_point_visibility(widget->actor, FVIZ_TRUE);
    fviz_actor_set_point_shape(widget->actor, FVIZ_POINT_SPHERE_IMPOSTOR);
    fviz_actor_set_color(widget->actor, widget->color[0], widget->color[1], widget->color[2]);
    fviz_widget_set_event_handler(widget->widget, fviz_handle_widget_event, widget);
    if (fviz_handle_widget_rebuild(widget) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    *out_widget = widget;
    return FVIZ_OK;
}

FVizWidget* fviz_handle_widget_widget(FVizHandleWidget* widget)
{
    return widget != NULL ? widget->widget : NULL;
}

void fviz_handle_widget_set_position(FVizHandleWidget* widget, FVizVec3 position)
{
    if (widget == NULL) return;
    widget->position = position;
    (void)fviz_handle_widget_rebuild(widget);
    fviz_widget_value_changed(widget->widget);
}

FVizVec3 fviz_handle_widget_position(const FVizHandleWidget* widget)
{
    return widget != NULL ? widget->position : fviz_vec3(0.0f, 0.0f, 0.0f);
}

void fviz_handle_widget_set_size(FVizHandleWidget* widget, float pixels)
{
    if (widget == NULL || !(pixels > 0.0f) || isfinite(pixels) == 0) return;
    widget->size_pixels = pixels;
    fviz_actor_set_point_size(widget->actor, pixels);
    fviz_widget_value_changed(widget->widget);
}

float fviz_handle_widget_size(const FVizHandleWidget* widget)
{
    return widget != NULL ? widget->size_pixels : 0.0f;
}

void fviz_handle_widget_set_pick_tolerance(FVizHandleWidget* widget, float pixels)
{
    if (widget == NULL || pixels < 0.0f || isfinite(pixels) == 0) return;
    widget->pick_tolerance_pixels = pixels;
}

float fviz_handle_widget_pick_tolerance(const FVizHandleWidget* widget)
{
    return widget != NULL ? widget->pick_tolerance_pixels : 0.0f;
}

void fviz_handle_widget_set_color(FVizHandleWidget* widget, float red, float green, float blue)
{
    if (widget == NULL) return;
    widget->color[0] = red;
    widget->color[1] = green;
    widget->color[2] = blue;
    fviz_actor_set_color(widget->actor, red, green, blue);
}

void fviz_handle_widget_set_manipulator_mode(FVizHandleWidget* widget, FVizWidgetManipulatorMode mode)
{
    if (widget == NULL) return;
    fviz_widget_manipulator_set_mode(widget->manipulator, mode);
}

FVizWidgetManipulatorMode fviz_handle_widget_manipulator_mode(const FVizHandleWidget* widget)
{
    return widget != NULL ? fviz_widget_manipulator_mode(widget->manipulator) : FVIZ_WIDGET_MANIPULATOR_VIEW_PLANE;
}

FVizResult fviz_handle_widget_set_axis(FVizHandleWidget* widget, FVizVec3 axis)
{
    return widget != NULL ? fviz_widget_manipulator_set_axis(widget->manipulator, axis) : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_handle_widget_set_plane_normal(FVizHandleWidget* widget, FVizVec3 normal)
{
    return widget != NULL ? fviz_widget_manipulator_set_plane_normal(widget->manipulator, normal)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

/* ------------------------------------------------------------------------- */
/* Plane widget                                                               */
/* ------------------------------------------------------------------------- */

static FVizResult fviz_plane_widget_rebuild(FVizPlaneWidget* widget)
{
    FVizVec3 u, v;
    FVizVec3 points[6];
    const uint32_t outline_lines[10] = {0u, 1u, 1u, 2u, 2u, 3u, 3u, 0u, 4u, 5u};
    const uint32_t fill_triangles[6] = {0u, 1u, 2u, 0u, 2u, 3u};
    const float half = widget->size * 0.5f;
    fviz_engineering_plane_basis(widget->normal, &u, &v);
    points[0] = fviz_vec3_add(widget->origin, fviz_vec3_add(fviz_vec3_scale(u, -half), fviz_vec3_scale(v, -half)));
    points[1] = fviz_vec3_add(widget->origin, fviz_vec3_add(fviz_vec3_scale(u, half), fviz_vec3_scale(v, -half)));
    points[2] = fviz_vec3_add(widget->origin, fviz_vec3_add(fviz_vec3_scale(u, half), fviz_vec3_scale(v, half)));
    points[3] = fviz_vec3_add(widget->origin, fviz_vec3_add(fviz_vec3_scale(u, -half), fviz_vec3_scale(v, half)));
    points[4] = widget->origin;
    points[5] = fviz_vec3_add(widget->origin, fviz_vec3_scale(widget->normal, widget->size * 0.4f));
    if (fviz_engineering_replace_actor_geometry(widget->outline_actor, points, 6u, outline_lines, 5u, NULL, 0u) !=
            FVIZ_OK ||
        fviz_engineering_replace_actor_geometry(widget->fill_actor, points, 4u, NULL, 0u, fill_triangles, 2u) !=
            FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizBool fviz_plane_widget_hit(FVizPlaneWidget* plane_widget, const FVizInteractionEvent* event)
{
    FVizRay ray;
    FVizVec3 hit, u, v, relative;
    if (fviz_renderer_display_to_world_ray(fviz_widget_renderer(plane_widget->widget), (float)event->x, (float)event->y,
                                           event->width, event->height, &ray) != FVIZ_OK ||
        fviz_engineering_ray_plane(ray, plane_widget->origin, plane_widget->normal, &hit) == FVIZ_FALSE)
        return FVIZ_FALSE;
    fviz_engineering_plane_basis(plane_widget->normal, &u, &v);
    relative = fviz_vec3_sub(hit, plane_widget->origin);
    return fabsf(fviz_vec3_dot(relative, u)) <= plane_widget->size * 0.55f &&
           fabsf(fviz_vec3_dot(relative, v)) <= plane_widget->size * 0.55f;
}

static FVizBool fviz_plane_widget_event(FVizWidget* base_widget, const FVizInteractionEvent* event, void* user_data)
{
    FVizPlaneWidget* widget = (FVizPlaneWidget*)user_data;
    if (event->type == FVIZ_INTERACTION_KEY_DOWN && event->key == FVIZ_KEY_ESCAPE &&
        fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        widget->origin = widget->interaction_start_origin;
        fviz_widget_manipulator_end(widget->manipulator);
        (void)fviz_plane_widget_rebuild(widget);
        fviz_widget_cancel_interaction(base_widget);
        if (widget->internal_changed != NULL) widget->internal_changed(widget, widget->internal_changed_data);
        (void)fviz_widget_request_render(base_widget);
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_BUTTON_DOWN && event->button == FVIZ_MOUSE_BUTTON_LEFT &&
        fviz_plane_widget_hit(widget, event) == FVIZ_TRUE)
    {
        widget->interaction_start_origin = widget->origin;
        fviz_widget_manipulator_set_origin(widget->manipulator, widget->origin);
        if (event->shift != FVIZ_FALSE)
        {
            fviz_widget_manipulator_set_mode(widget->manipulator, FVIZ_WIDGET_MANIPULATOR_AXIS);
            (void)fviz_widget_manipulator_set_axis(widget->manipulator, widget->normal);
        }
        else
            fviz_widget_manipulator_set_mode(widget->manipulator, FVIZ_WIDGET_MANIPULATOR_VIEW_PLANE);
        (void)fviz_widget_manipulator_begin(widget->manipulator, fviz_widget_renderer(base_widget), event,
                                            widget->origin);
        fviz_widget_begin_interaction(base_widget);
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_MOVE && fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        FVizVec3 world, delta;
        if (fviz_widget_manipulator_update(widget->manipulator, fviz_widget_renderer(base_widget), event, &world,
                                           &delta) == FVIZ_OK)
        {
            (void)world;
            widget->origin = fviz_vec3_add(widget->origin, delta);
            (void)fviz_plane_widget_rebuild(widget);
            fviz_widget_interaction(base_widget);
            fviz_widget_value_changed(base_widget);
            if (widget->internal_changed != NULL) widget->internal_changed(widget, widget->internal_changed_data);
            (void)fviz_widget_request_render(base_widget);
        }
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_BUTTON_UP && event->button == FVIZ_MOUSE_BUTTON_LEFT &&
        fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        fviz_widget_manipulator_end(widget->manipulator);
        fviz_widget_end_interaction(base_widget);
        (void)fviz_widget_request_render(base_widget);
        return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

static void fviz_plane_widget_destroy(FVizObject* object)
{
    FVizPlaneWidget* widget = (FVizPlaneWidget*)object;
    fviz_release(widget->widget);
    fviz_release(widget->representation);
    fviz_release(widget->manipulator);
    fviz_release(widget->fill_actor);
    fviz_release(widget->outline_actor);
}

static const FVizObjectClass g_fviz_plane_widget_class = {FVIZ_TYPE_PLANE_WIDGET, "FVizPlaneWidget",
                                                          &g_fviz_object_class, fviz_plane_widget_destroy, NULL};

FVizResult fviz_plane_widget_create(FVizRenderWindowInteractor* interactor, FVizRenderer* renderer,
                                    FVizPlaneWidget** out_widget)
{
    FVizPlaneWidget* widget;
    if (renderer == NULL || out_widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_widget = NULL;
    widget = (FVizPlaneWidget*)fviz_internal_object_allocate(sizeof(*widget), &g_fviz_plane_widget_class, NULL);
    if (widget == NULL) return fviz_last_error_code();
    widget->normal = fviz_vec3(1.0f, 0.0f, 0.0f);
    widget->size = 1.0f;
    widget->color[0] = 1.0f;
    widget->color[1] = 0.65f;
    widget->color[2] = 0.1f;
    if (fviz_widget_representation_create(renderer, &widget->representation) != FVIZ_OK ||
        fviz_widget_manipulator_create(&widget->manipulator) != FVIZ_OK ||
        fviz_actor_create(&widget->fill_actor) != FVIZ_OK || fviz_actor_create(&widget->outline_actor) != FVIZ_OK ||
        fviz_widget_representation_add_actor(widget->representation, widget->fill_actor) != FVIZ_OK ||
        fviz_widget_representation_add_actor(widget->representation, widget->outline_actor) != FVIZ_OK ||
        fviz_widget_create(interactor, renderer, widget->representation, &widget->widget) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    fviz_actor_set_color(widget->fill_actor, widget->color[0], widget->color[1], widget->color[2]);
    fviz_actor_set_opacity(widget->fill_actor, 0.12f);
    fviz_actor_set_cull_mode(widget->fill_actor, FVIZ_CULL_NONE);
    fviz_actor_set_color(widget->outline_actor, widget->color[0], widget->color[1], widget->color[2]);
    fviz_actor_set_line_width(widget->outline_actor, 2.5f);
    fviz_actor_set_line_cap(widget->outline_actor, FVIZ_LINE_CAP_ROUND);
    fviz_widget_set_event_handler(widget->widget, fviz_plane_widget_event, widget);
    if (fviz_plane_widget_rebuild(widget) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    *out_widget = widget;
    return FVIZ_OK;
}

FVizWidget* fviz_plane_widget_widget(FVizPlaneWidget* widget)
{
    return widget != NULL ? widget->widget : NULL;
}

void fviz_plane_widget_set_origin(FVizPlaneWidget* widget, FVizVec3 origin)
{
    if (widget == NULL) return;
    widget->origin = origin;
    (void)fviz_plane_widget_rebuild(widget);
    if (widget->internal_changed != NULL) widget->internal_changed(widget, widget->internal_changed_data);
    fviz_widget_value_changed(widget->widget);
}

FVizVec3 fviz_plane_widget_origin(const FVizPlaneWidget* widget)
{
    return widget != NULL ? widget->origin : fviz_vec3(0.0f, 0.0f, 0.0f);
}

FVizResult fviz_plane_widget_set_normal(FVizPlaneWidget* widget, FVizVec3 normal)
{
    if (widget == NULL || fviz_vec3_length(normal) <= 1.0e-12f) return FVIZ_ERROR_INVALID_ARGUMENT;
    widget->normal = fviz_vec3_normalize(normal);
    if (fviz_plane_widget_rebuild(widget) != FVIZ_OK) return fviz_last_error_code();
    if (widget->internal_changed != NULL) widget->internal_changed(widget, widget->internal_changed_data);
    fviz_widget_value_changed(widget->widget);
    return FVIZ_OK;
}

FVizVec3 fviz_plane_widget_normal(const FVizPlaneWidget* widget)
{
    return widget != NULL ? widget->normal : fviz_vec3(1.0f, 0.0f, 0.0f);
}

FVizPlane fviz_plane_widget_plane(const FVizPlaneWidget* widget)
{
    return widget != NULL ? fviz_plane_from_point_normal(widget->origin, widget->normal)
                          : fviz_plane_from_point_normal(fviz_vec3(0.0f, 0.0f, 0.0f), fviz_vec3(1.0f, 0.0f, 0.0f));
}

void fviz_plane_widget_set_size(FVizPlaneWidget* widget, float size)
{
    if (widget == NULL || !(size > 0.0f) || !isfinite(size)) return;
    widget->size = size;
    (void)fviz_plane_widget_rebuild(widget);
    fviz_widget_value_changed(widget->widget);
}

float fviz_plane_widget_size(const FVizPlaneWidget* widget)
{
    return widget != NULL ? widget->size : 0.0f;
}

void fviz_plane_widget_set_color(FVizPlaneWidget* widget, float red, float green, float blue)
{
    if (widget == NULL) return;
    widget->color[0] = red;
    widget->color[1] = green;
    widget->color[2] = blue;
    fviz_actor_set_color(widget->fill_actor, red, green, blue);
    fviz_actor_set_color(widget->outline_actor, red, green, blue);
}

FVizResult fviz_plane_widget_update_representation(FVizPlaneWidget* widget)
{
    return widget != NULL ? fviz_plane_widget_rebuild(widget) : FVIZ_ERROR_INVALID_ARGUMENT;
}

/* ------------------------------------------------------------------------- */
/* Box widget                                                                 */
/* ------------------------------------------------------------------------- */

static void fviz_box_widget_face_centers(const FVizBoxWidget* widget, FVizVec3 centers[6])
{
    const FVizVec3 mn = widget->bounds.min;
    const FVizVec3 mx = widget->bounds.max;
    const FVizVec3 c = fviz_bounds_center(&widget->bounds);
    centers[0] = fviz_vec3(mn.x, c.y, c.z);
    centers[1] = fviz_vec3(mx.x, c.y, c.z);
    centers[2] = fviz_vec3(c.x, mn.y, c.z);
    centers[3] = fviz_vec3(c.x, mx.y, c.z);
    centers[4] = fviz_vec3(c.x, c.y, mn.z);
    centers[5] = fviz_vec3(c.x, c.y, mx.z);
}

static FVizResult fviz_box_widget_rebuild(FVizBoxWidget* widget)
{
    FVizVec3 p[14];
    FVizVec3 centers[6];
    static const uint32_t lines[24] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
    const FVizVec3 mn = widget->bounds.min;
    const FVizVec3 mx = widget->bounds.max;
    FVizSize i;
    p[0] = fviz_vec3(mn.x, mn.y, mn.z);
    p[1] = fviz_vec3(mx.x, mn.y, mn.z);
    p[2] = fviz_vec3(mx.x, mx.y, mn.z);
    p[3] = fviz_vec3(mn.x, mx.y, mn.z);
    p[4] = fviz_vec3(mn.x, mn.y, mx.z);
    p[5] = fviz_vec3(mx.x, mn.y, mx.z);
    p[6] = fviz_vec3(mx.x, mx.y, mx.z);
    p[7] = fviz_vec3(mn.x, mx.y, mx.z);
    fviz_box_widget_face_centers(widget, centers);
    for (i = 0u; i < 6u; ++i)
        p[8u + i] = centers[i];
    if (fviz_engineering_replace_actor_geometry(widget->actor, p, 14u, lines, 12u, NULL, 0u) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_actor_set_point_visibility(widget->actor, FVIZ_TRUE);
    fviz_actor_set_point_size(widget->actor, widget->handle_size);
    return FVIZ_OK;
}

static int fviz_box_widget_face_handle_hit(FVizBoxWidget* widget, const FVizInteractionEvent* event)
{
    FVizVec3 centers[6];
    FVizSize i;
    const float radius = widget->handle_size * 0.5f + widget->pick_tolerance;
    float best_distance2 = radius * radius;
    int best = 0;
    if (event == NULL || event->width <= 0 || event->height <= 0) return 0;
    fviz_box_widget_face_centers(widget, centers);
    for (i = 0u; i < 6u; ++i)
    {
        FVizVec3 display;
        float dx;
        float dy;
        float distance2;
        if (fviz_renderer_world_to_display(fviz_widget_renderer(widget->widget), centers[i], event->width,
                                           event->height, &display) != FVIZ_OK)
            continue;
        dx = (float)event->x - display.x;
        dy = (float)event->y - display.y;
        distance2 = dx * dx + dy * dy;
        if (distance2 <= best_distance2)
        {
            best_distance2 = distance2;
            best = (int)i + 1;
        }
    }
    return best;
}

static FVizVec3 fviz_box_widget_face_axis(int face)
{
    if (face == 1 || face == 2) return fviz_vec3(1.0f, 0.0f, 0.0f);
    if (face == 3 || face == 4) return fviz_vec3(0.0f, 1.0f, 0.0f);
    return fviz_vec3(0.0f, 0.0f, 1.0f);
}

static FVizVec3 fviz_box_widget_face_center(const FVizBoxWidget* widget, int face)
{
    FVizVec3 centers[6];
    fviz_box_widget_face_centers(widget, centers);
    return face >= 1 && face <= 6 ? centers[face - 1] : fviz_bounds_center(&widget->bounds);
}

static FVizBool fviz_box_widget_ray_hit(FVizBoxWidget* widget, const FVizInteractionEvent* event)
{
    FVizRay ray;
    float tmin = -INFINITY;
    float tmax = INFINITY;
    float origin[3], direction[3], minimum[3], maximum[3];
    int axis;
    if (event->width <= 0 || event->height <= 0 ||
        fviz_renderer_display_to_world_ray(fviz_widget_renderer(widget->widget), (float)event->x, (float)event->y,
                                           event->width, event->height, &ray) != FVIZ_OK)
        return FVIZ_FALSE;
    origin[0] = ray.origin.x;
    origin[1] = ray.origin.y;
    origin[2] = ray.origin.z;
    direction[0] = ray.direction.x;
    direction[1] = ray.direction.y;
    direction[2] = ray.direction.z;
    minimum[0] = widget->bounds.min.x;
    minimum[1] = widget->bounds.min.y;
    minimum[2] = widget->bounds.min.z;
    maximum[0] = widget->bounds.max.x;
    maximum[1] = widget->bounds.max.y;
    maximum[2] = widget->bounds.max.z;
    for (axis = 0; axis < 3; ++axis)
    {
        if (fabsf(direction[axis]) < 1.0e-8f)
        {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) return FVIZ_FALSE;
        }
        else
        {
            float a = (minimum[axis] - origin[axis]) / direction[axis];
            float b = (maximum[axis] - origin[axis]) / direction[axis];
            if (a > b)
            {
                const float tmp = a;
                a = b;
                b = tmp;
            }
            if (a > tmin) tmin = a;
            if (b < tmax) tmax = b;
            if (tmin > tmax) return FVIZ_FALSE;
        }
    }
    return tmax >= 0.0f;
}

static FVizBool fviz_box_widget_event(FVizWidget* base_widget, const FVizInteractionEvent* event, void* user_data)
{
    FVizBoxWidget* widget = (FVizBoxWidget*)user_data;
    if (event->type == FVIZ_INTERACTION_KEY_DOWN && event->key == FVIZ_KEY_ESCAPE &&
        fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        widget->bounds = widget->interaction_start_bounds;
        widget->active_face = 0;
        fviz_widget_manipulator_end(widget->manipulator);
        (void)fviz_box_widget_rebuild(widget);
        fviz_widget_cancel_interaction(base_widget);
        fviz_widget_value_changed(base_widget);
        (void)fviz_widget_request_render(base_widget);
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_BUTTON_DOWN && event->button == FVIZ_MOUSE_BUTTON_LEFT)
    {
        const int face = fviz_box_widget_face_handle_hit(widget, event);
        FVizVec3 reference;
        if (face == 0 && fviz_box_widget_ray_hit(widget, event) == FVIZ_FALSE) return FVIZ_FALSE;
        widget->interaction_start_bounds = widget->bounds;
        widget->active_face = face != 0 ? face : 7;
        if (face != 0)
        {
            reference = fviz_box_widget_face_center(widget, face);
            fviz_widget_manipulator_set_mode(widget->manipulator, FVIZ_WIDGET_MANIPULATOR_AXIS);
            (void)fviz_widget_manipulator_set_axis(widget->manipulator, fviz_box_widget_face_axis(face));
        }
        else
        {
            reference = fviz_bounds_center(&widget->bounds);
            fviz_widget_manipulator_set_mode(widget->manipulator, FVIZ_WIDGET_MANIPULATOR_VIEW_PLANE);
        }
        fviz_widget_manipulator_set_origin(widget->manipulator, reference);
        if (fviz_widget_manipulator_begin(widget->manipulator, fviz_widget_renderer(base_widget), event, reference) !=
            FVIZ_OK)
        {
            widget->active_face = 0;
            return FVIZ_FALSE;
        }
        fviz_widget_begin_interaction(base_widget);
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_MOVE && fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        FVizVec3 world, delta;
        if (fviz_widget_manipulator_update(widget->manipulator, fviz_widget_renderer(base_widget), event, &world,
                                           &delta) == FVIZ_OK)
        {
            const float minimum_extent = 1.0e-6f;
            (void)world;
            if (widget->active_face == 1)
            {
                widget->bounds.min.x += delta.x;
                if (widget->bounds.min.x > widget->bounds.max.x - minimum_extent)
                    widget->bounds.min.x = widget->bounds.max.x - minimum_extent;
            }
            else if (widget->active_face == 2)
            {
                widget->bounds.max.x += delta.x;
                if (widget->bounds.max.x < widget->bounds.min.x + minimum_extent)
                    widget->bounds.max.x = widget->bounds.min.x + minimum_extent;
            }
            else if (widget->active_face == 3)
            {
                widget->bounds.min.y += delta.y;
                if (widget->bounds.min.y > widget->bounds.max.y - minimum_extent)
                    widget->bounds.min.y = widget->bounds.max.y - minimum_extent;
            }
            else if (widget->active_face == 4)
            {
                widget->bounds.max.y += delta.y;
                if (widget->bounds.max.y < widget->bounds.min.y + minimum_extent)
                    widget->bounds.max.y = widget->bounds.min.y + minimum_extent;
            }
            else if (widget->active_face == 5)
            {
                widget->bounds.min.z += delta.z;
                if (widget->bounds.min.z > widget->bounds.max.z - minimum_extent)
                    widget->bounds.min.z = widget->bounds.max.z - minimum_extent;
            }
            else if (widget->active_face == 6)
            {
                widget->bounds.max.z += delta.z;
                if (widget->bounds.max.z < widget->bounds.min.z + minimum_extent)
                    widget->bounds.max.z = widget->bounds.min.z + minimum_extent;
            }
            else if (widget->active_face == 7)
            {
                widget->bounds.min = fviz_vec3_add(widget->bounds.min, delta);
                widget->bounds.max = fviz_vec3_add(widget->bounds.max, delta);
            }
            (void)fviz_box_widget_rebuild(widget);
            fviz_widget_interaction(base_widget);
            fviz_widget_value_changed(base_widget);
            (void)fviz_widget_request_render(base_widget);
        }
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_BUTTON_UP && event->button == FVIZ_MOUSE_BUTTON_LEFT &&
        fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        widget->active_face = 0;
        fviz_widget_manipulator_end(widget->manipulator);
        fviz_widget_end_interaction(base_widget);
        (void)fviz_widget_request_render(base_widget);
        return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

static void fviz_box_widget_destroy(FVizObject* object)
{
    FVizBoxWidget* widget = (FVizBoxWidget*)object;
    fviz_release(widget->widget);
    fviz_release(widget->representation);
    fviz_release(widget->manipulator);
    fviz_release(widget->actor);
}

static const FVizObjectClass g_fviz_box_widget_class = {FVIZ_TYPE_BOX_WIDGET, "FVizBoxWidget", &g_fviz_object_class,
                                                        fviz_box_widget_destroy, NULL};

FVizResult fviz_box_widget_create(FVizRenderWindowInteractor* interactor, FVizRenderer* renderer,
                                  FVizBoxWidget** out_widget)
{
    FVizBoxWidget* widget;
    if (renderer == NULL || out_widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_widget = NULL;
    widget = (FVizBoxWidget*)fviz_internal_object_allocate(sizeof(*widget), &g_fviz_box_widget_class, NULL);
    if (widget == NULL) return fviz_last_error_code();
    widget->bounds.min = fviz_vec3(-0.5f, -0.5f, -0.5f);
    widget->bounds.max = fviz_vec3(0.5f, 0.5f, 0.5f);
    widget->bounds.valid = FVIZ_TRUE;
    widget->color[0] = 0.2f;
    widget->color[1] = 0.75f;
    widget->color[2] = 1.0f;
    widget->handle_size = 9.0f;
    widget->pick_tolerance = 5.0f;
    if (fviz_widget_representation_create(renderer, &widget->representation) != FVIZ_OK ||
        fviz_widget_manipulator_create(&widget->manipulator) != FVIZ_OK ||
        fviz_actor_create(&widget->actor) != FVIZ_OK ||
        fviz_widget_representation_add_actor(widget->representation, widget->actor) != FVIZ_OK ||
        fviz_widget_create(interactor, renderer, widget->representation, &widget->widget) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    fviz_actor_set_color(widget->actor, widget->color[0], widget->color[1], widget->color[2]);
    fviz_actor_set_line_width(widget->actor, 2.5f);
    fviz_actor_set_line_cap(widget->actor, FVIZ_LINE_CAP_ROUND);
    fviz_actor_set_point_visibility(widget->actor, FVIZ_TRUE);
    fviz_actor_set_point_shape(widget->actor, FVIZ_POINT_SPHERE_IMPOSTOR);
    fviz_actor_set_point_size(widget->actor, widget->handle_size);
    fviz_widget_set_event_handler(widget->widget, fviz_box_widget_event, widget);
    if (fviz_box_widget_rebuild(widget) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    *out_widget = widget;
    return FVIZ_OK;
}

FVizWidget* fviz_box_widget_widget(FVizBoxWidget* widget)
{
    return widget != NULL ? widget->widget : NULL;
}

FVizResult fviz_box_widget_set_bounds(FVizBoxWidget* widget, const FVizBounds* bounds)
{
    if (widget == NULL || bounds == NULL || bounds->valid == FVIZ_FALSE || bounds->min.x > bounds->max.x ||
        bounds->min.y > bounds->max.y || bounds->min.z > bounds->max.z)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    widget->bounds = *bounds;
    if (fviz_box_widget_rebuild(widget) != FVIZ_OK) return fviz_last_error_code();
    fviz_widget_value_changed(widget->widget);
    return FVIZ_OK;
}

FVizBounds fviz_box_widget_bounds(const FVizBoxWidget* widget)
{
    return widget != NULL ? widget->bounds : fviz_bounds_empty();
}

void fviz_box_widget_set_color(FVizBoxWidget* widget, float r, float g, float b)
{
    if (widget == NULL) return;
    widget->color[0] = r;
    widget->color[1] = g;
    widget->color[2] = b;
    fviz_actor_set_color(widget->actor, r, g, b);
}

void fviz_box_widget_set_handle_size(FVizBoxWidget* widget, float pixels)
{
    if (widget == NULL || !(pixels > 0.0f) || isfinite(pixels) == 0) return;
    widget->handle_size = pixels;
    fviz_actor_set_point_size(widget->actor, pixels);
}

void fviz_box_widget_set_pick_tolerance(FVizBoxWidget* widget, float pixels)
{
    if (widget == NULL || pixels < 0.0f || isfinite(pixels) == 0) return;
    widget->pick_tolerance = pixels;
}

FVizResult fviz_box_widget_update_representation(FVizBoxWidget* widget)
{
    return widget != NULL ? fviz_box_widget_rebuild(widget) : FVIZ_ERROR_INVALID_ARGUMENT;
}

/* ------------------------------------------------------------------------- */
/* Line widget                                                                */
/* ------------------------------------------------------------------------- */

static FVizResult fviz_line_widget_rebuild(FVizLineWidget* widget)
{
    const uint32_t line[2] = {0u, 1u};
    if (widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_engineering_replace_actor_geometry(widget->actor, widget->points, 2u, line, 1u, NULL, 0u) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_actor_set_line_width(widget->actor, widget->line_width);
    fviz_actor_set_point_visibility(widget->actor, FVIZ_TRUE);
    fviz_actor_set_point_size(widget->actor, widget->handle_size);
    return FVIZ_OK;
}

static float fviz_line_widget_distance_to_segment_2d(float px, float py, float ax, float ay, float bx, float by)
{
    const float abx = bx - ax;
    const float aby = by - ay;
    const float apx = px - ax;
    const float apy = py - ay;
    const float denominator = abx * abx + aby * aby;
    float t;
    float dx;
    float dy;
    if (denominator <= 1.0e-12f)
    {
        dx = px - ax;
        dy = py - ay;
        return sqrtf(dx * dx + dy * dy);
    }
    t = (apx * abx + apy * aby) / denominator;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    dx = px - (ax + t * abx);
    dy = py - (ay + t * aby);
    return sqrtf(dx * dx + dy * dy);
}

static int fviz_line_widget_hit(FVizLineWidget* widget, const FVizInteractionEvent* event)
{
    FVizVec3 a;
    FVizVec3 b;
    float dx;
    float dy;
    float endpoint_radius;
    float line_radius;
    if (widget == NULL || event == NULL || event->width <= 0 || event->height <= 0) return 0;
    if (fviz_renderer_world_to_display(fviz_widget_renderer(widget->widget), widget->points[0], event->width,
                                       event->height, &a) != FVIZ_OK ||
        fviz_renderer_world_to_display(fviz_widget_renderer(widget->widget), widget->points[1], event->width,
                                       event->height, &b) != FVIZ_OK)
        return 0;
    endpoint_radius = widget->handle_size * 0.5f + widget->pick_tolerance;
    dx = (float)event->x - a.x;
    dy = (float)event->y - a.y;
    if (dx * dx + dy * dy <= endpoint_radius * endpoint_radius) return 1;
    dx = (float)event->x - b.x;
    dy = (float)event->y - b.y;
    if (dx * dx + dy * dy <= endpoint_radius * endpoint_radius) return 2;
    line_radius = widget->line_width * 0.5f + widget->pick_tolerance;
    return fviz_line_widget_distance_to_segment_2d((float)event->x, (float)event->y, a.x, a.y, b.x, b.y) <= line_radius
               ? 3
               : 0;
}

static FVizBool fviz_line_widget_event(FVizWidget* base_widget, const FVizInteractionEvent* event, void* user_data)
{
    FVizLineWidget* widget = (FVizLineWidget*)user_data;
    if (event->type == FVIZ_INTERACTION_KEY_DOWN && event->key == FVIZ_KEY_ESCAPE &&
        fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        widget->points[0] = widget->interaction_start_points[0];
        widget->points[1] = widget->interaction_start_points[1];
        widget->active_part = 0;
        fviz_widget_manipulator_end(widget->manipulator);
        (void)fviz_line_widget_rebuild(widget);
        fviz_widget_cancel_interaction(base_widget);
        fviz_widget_value_changed(base_widget);
        (void)fviz_widget_request_render(base_widget);
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_BUTTON_DOWN && event->button == FVIZ_MOUSE_BUTTON_LEFT)
    {
        const int part = fviz_line_widget_hit(widget, event);
        FVizVec3 reference;
        if (part == 0) return FVIZ_FALSE;
        widget->active_part = part;
        widget->interaction_start_points[0] = widget->points[0];
        widget->interaction_start_points[1] = widget->points[1];
        reference = part == 1   ? widget->points[0]
                    : part == 2 ? widget->points[1]
                                : fviz_vec3_scale(fviz_vec3_add(widget->points[0], widget->points[1]), 0.5f);
        fviz_widget_manipulator_set_mode(widget->manipulator, FVIZ_WIDGET_MANIPULATOR_VIEW_PLANE);
        fviz_widget_manipulator_set_origin(widget->manipulator, reference);
        if (fviz_widget_manipulator_begin(widget->manipulator, fviz_widget_renderer(base_widget), event, reference) !=
            FVIZ_OK)
        {
            widget->active_part = 0;
            return FVIZ_FALSE;
        }
        fviz_widget_begin_interaction(base_widget);
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_MOVE && fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        FVizVec3 world;
        FVizVec3 delta;
        if (fviz_widget_manipulator_update(widget->manipulator, fviz_widget_renderer(base_widget), event, &world,
                                           &delta) == FVIZ_OK)
        {
            (void)world;
            if (widget->active_part == 1) widget->points[0] = fviz_vec3_add(widget->points[0], delta);
            else if (widget->active_part == 2)
                widget->points[1] = fviz_vec3_add(widget->points[1], delta);
            else if (widget->active_part == 3)
            {
                widget->points[0] = fviz_vec3_add(widget->points[0], delta);
                widget->points[1] = fviz_vec3_add(widget->points[1], delta);
            }
            (void)fviz_line_widget_rebuild(widget);
            fviz_widget_interaction(base_widget);
            fviz_widget_value_changed(base_widget);
            (void)fviz_widget_request_render(base_widget);
        }
        return FVIZ_TRUE;
    }
    if (event->type == FVIZ_INTERACTION_MOUSE_BUTTON_UP && event->button == FVIZ_MOUSE_BUTTON_LEFT &&
        fviz_widget_state(base_widget) == FVIZ_WIDGET_STATE_ACTIVE)
    {
        widget->active_part = 0;
        fviz_widget_manipulator_end(widget->manipulator);
        fviz_widget_end_interaction(base_widget);
        (void)fviz_widget_request_render(base_widget);
        return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

static void fviz_line_widget_destroy(FVizObject* object)
{
    FVizLineWidget* widget = (FVizLineWidget*)object;
    fviz_release(widget->widget);
    fviz_release(widget->representation);
    fviz_release(widget->manipulator);
    fviz_release(widget->actor);
}

static const FVizObjectClass g_fviz_line_widget_class = {FVIZ_TYPE_LINE_WIDGET, "FVizLineWidget", &g_fviz_object_class,
                                                         fviz_line_widget_destroy, NULL};

FVizResult fviz_line_widget_create(FVizRenderWindowInteractor* interactor, FVizRenderer* renderer,
                                   FVizLineWidget** out_widget)
{
    FVizLineWidget* widget;
    if (renderer == NULL || out_widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_widget = NULL;
    widget = (FVizLineWidget*)fviz_internal_object_allocate(sizeof(*widget), &g_fviz_line_widget_class, NULL);
    if (widget == NULL) return fviz_last_error_code();
    widget->points[0] = fviz_vec3(-0.5f, 0.0f, 0.0f);
    widget->points[1] = fviz_vec3(0.5f, 0.0f, 0.0f);
    widget->line_width = 2.5f;
    widget->handle_size = 10.0f;
    widget->pick_tolerance = 5.0f;
    widget->color[0] = 1.0f;
    widget->color[1] = 0.75f;
    widget->color[2] = 0.15f;
    if (fviz_widget_representation_create(renderer, &widget->representation) != FVIZ_OK ||
        fviz_widget_manipulator_create(&widget->manipulator) != FVIZ_OK ||
        fviz_actor_create(&widget->actor) != FVIZ_OK ||
        fviz_widget_representation_add_actor(widget->representation, widget->actor) != FVIZ_OK ||
        fviz_widget_create(interactor, renderer, widget->representation, &widget->widget) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    fviz_actor_set_color(widget->actor, widget->color[0], widget->color[1], widget->color[2]);
    fviz_actor_set_line_cap(widget->actor, FVIZ_LINE_CAP_ROUND);
    fviz_actor_set_line_join(widget->actor, FVIZ_LINE_JOIN_ROUND);
    fviz_actor_set_point_shape(widget->actor, FVIZ_POINT_SPHERE_IMPOSTOR);
    fviz_widget_set_event_handler(widget->widget, fviz_line_widget_event, widget);
    if (fviz_line_widget_rebuild(widget) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    *out_widget = widget;
    return FVIZ_OK;
}

FVizWidget* fviz_line_widget_widget(FVizLineWidget* widget)
{
    return widget != NULL ? widget->widget : NULL;
}

void fviz_line_widget_set_points(FVizLineWidget* widget, FVizVec3 point1, FVizVec3 point2)
{
    if (widget == NULL) return;
    widget->points[0] = point1;
    widget->points[1] = point2;
    (void)fviz_line_widget_rebuild(widget);
    fviz_widget_value_changed(widget->widget);
}

void fviz_line_widget_get_points(const FVizLineWidget* widget, FVizVec3* point1, FVizVec3* point2)
{
    if (widget == NULL) return;
    if (point1 != NULL) *point1 = widget->points[0];
    if (point2 != NULL) *point2 = widget->points[1];
}

float fviz_line_widget_length(const FVizLineWidget* widget)
{
    return widget != NULL ? fviz_vec3_length(fviz_vec3_sub(widget->points[1], widget->points[0])) : 0.0f;
}

void fviz_line_widget_set_color(FVizLineWidget* widget, float red, float green, float blue)
{
    if (widget == NULL) return;
    widget->color[0] = red;
    widget->color[1] = green;
    widget->color[2] = blue;
    fviz_actor_set_color(widget->actor, red, green, blue);
}

void fviz_line_widget_set_line_width(FVizLineWidget* widget, float pixels)
{
    if (widget == NULL || !(pixels > 0.0f) || isfinite(pixels) == 0) return;
    widget->line_width = pixels;
    fviz_actor_set_line_width(widget->actor, pixels);
}

void fviz_line_widget_set_handle_size(FVizLineWidget* widget, float pixels)
{
    if (widget == NULL || !(pixels > 0.0f) || isfinite(pixels) == 0) return;
    widget->handle_size = pixels;
    fviz_actor_set_point_size(widget->actor, pixels);
}

void fviz_line_widget_set_pick_tolerance(FVizLineWidget* widget, float pixels)
{
    if (widget == NULL || pixels < 0.0f || isfinite(pixels) == 0) return;
    widget->pick_tolerance = pixels;
}

FVizResult fviz_line_widget_update_representation(FVizLineWidget* widget)
{
    return widget != NULL ? fviz_line_widget_rebuild(widget) : FVIZ_ERROR_INVALID_ARGUMENT;
}

/* ------------------------------------------------------------------------- */
/* Distance and angle widgets                                                 */
/* ------------------------------------------------------------------------- */

static FVizResult fviz_distance_widget_rebuild(FVizDistanceWidget* widget)
{
    const uint32_t line[2] = {0u, 1u};
    char text[96];
    if (widget->point_count == 0u)
    {
        (void)fviz_widget_representation_set_actor_visible(widget->representation, widget->actor, FVIZ_FALSE);
        (void)fviz_widget_representation_set_billboard_text_actor_3d_visible(widget->representation, widget->label,
                                                                             FVIZ_FALSE);
        return FVIZ_OK;
    }
    if (widget->point_count == 1u)
    {
        if (fviz_engineering_replace_actor_geometry(widget->actor, widget->points, 1u, NULL, 0u, NULL, 0u) != FVIZ_OK)
            return fviz_last_error_code();
        (void)fviz_widget_representation_set_actor_visible(widget->representation, widget->actor, FVIZ_TRUE);
        fviz_actor_set_point_visibility(widget->actor, FVIZ_TRUE);
        (void)fviz_widget_representation_set_billboard_text_actor_3d_visible(widget->representation, widget->label,
                                                                             FVIZ_FALSE);
        return FVIZ_OK;
    }
    if (fviz_engineering_replace_actor_geometry(widget->actor, widget->points, 2u, line, 1u, NULL, 0u) != FVIZ_OK)
        return fviz_last_error_code();
    (void)snprintf(text, sizeof(text), "%.6g", (double)fviz_distance_widget_distance(widget));
    if (fviz_billboard_text_actor_3d_set_text(widget->label, text) != FVIZ_OK) return fviz_last_error_code();
    fviz_billboard_text_actor_3d_set_world_position(
        widget->label, fviz_vec3_scale(fviz_vec3_add(widget->points[0], widget->points[1]), 0.5f));
    (void)fviz_widget_representation_set_actor_visible(widget->representation, widget->actor, FVIZ_TRUE);
    fviz_actor_set_point_visibility(widget->actor, FVIZ_TRUE);
    (void)fviz_widget_representation_set_billboard_text_actor_3d_visible(widget->representation, widget->label,
                                                                         FVIZ_TRUE);
    return FVIZ_OK;
}

static FVizBool fviz_distance_widget_event(FVizWidget* base_widget, const FVizInteractionEvent* event, void* user_data)
{
    FVizDistanceWidget* widget = (FVizDistanceWidget*)user_data;
    FVizRenderWindow* window = fviz_render_window_interactor_window(fviz_widget_interactor(base_widget));
    FVizVec3 point;
    if (event->type != FVIZ_INTERACTION_MOUSE_BUTTON_DOWN || event->button != FVIZ_MOUSE_BUTTON_LEFT || window == NULL)
        return FVIZ_FALSE;
    if (fviz_engineering_pick_triangle_world(window, fviz_widget_renderer(base_widget), event->x, event->y, NULL, NULL,
                                             &point) == FVIZ_FALSE)
        return FVIZ_FALSE;
    if (widget->point_count >= 2u) widget->point_count = 0u;
    widget->points[widget->point_count++] = point;
    (void)fviz_distance_widget_rebuild(widget);
    fviz_widget_value_changed(base_widget);
    (void)fviz_widget_request_render(base_widget);
    return FVIZ_TRUE;
}

static void fviz_distance_widget_destroy(FVizObject* object)
{
    FVizDistanceWidget* widget = (FVizDistanceWidget*)object;
    fviz_release(widget->widget);
    fviz_release(widget->representation);
    fviz_release(widget->actor);
    fviz_release(widget->label);
}

static const FVizObjectClass g_fviz_distance_widget_class = {FVIZ_TYPE_DISTANCE_WIDGET, "FVizDistanceWidget",
                                                             &g_fviz_object_class, fviz_distance_widget_destroy, NULL};

FVizResult fviz_distance_widget_create(FVizRenderWindowInteractor* interactor, FVizRenderer* renderer,
                                       FVizDistanceWidget** out_widget)
{
    FVizDistanceWidget* widget;
    if (renderer == NULL || out_widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_widget = NULL;
    widget = (FVizDistanceWidget*)fviz_internal_object_allocate(sizeof(*widget), &g_fviz_distance_widget_class, NULL);
    if (widget == NULL) return fviz_last_error_code();
    if (fviz_widget_representation_create(renderer, &widget->representation) != FVIZ_OK ||
        fviz_actor_create(&widget->actor) != FVIZ_OK ||
        fviz_billboard_text_actor_3d_create(&widget->label) != FVIZ_OK ||
        fviz_widget_representation_add_actor(widget->representation, widget->actor) != FVIZ_OK ||
        fviz_widget_representation_add_billboard_text_actor_3d(widget->representation, widget->label) != FVIZ_OK ||
        fviz_widget_create(interactor, renderer, widget->representation, &widget->widget) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    fviz_actor_set_color(widget->actor, 1.0f, 0.85f, 0.15f);
    fviz_actor_set_line_width(widget->actor, 2.0f);
    fviz_actor_set_point_size(widget->actor, 7.0f);
    fviz_actor_set_point_shape(widget->actor, FVIZ_POINT_CIRCLE);
    fviz_widget_set_event_handler(widget->widget, fviz_distance_widget_event, widget);
    fviz_distance_widget_reset(widget);
    *out_widget = widget;
    return FVIZ_OK;
}

FVizWidget* fviz_distance_widget_widget(FVizDistanceWidget* widget)
{
    return widget != NULL ? widget->widget : NULL;
}

FVizResult fviz_distance_widget_set_points(FVizDistanceWidget* widget, FVizVec3 p1, FVizVec3 p2)
{
    if (widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    widget->points[0] = p1;
    widget->points[1] = p2;
    widget->point_count = 2u;
    if (fviz_distance_widget_rebuild(widget) != FVIZ_OK) return fviz_last_error_code();
    fviz_widget_value_changed(widget->widget);
    return FVIZ_OK;
}

void fviz_distance_widget_get_points(const FVizDistanceWidget* widget, FVizVec3* p1, FVizVec3* p2)
{
    if (widget == NULL) return;
    if (p1) *p1 = widget->points[0];
    if (p2) *p2 = widget->points[1];
}

float fviz_distance_widget_distance(const FVizDistanceWidget* widget)
{
    return widget != NULL && widget->point_count >= 2u
               ? fviz_vec3_length(fviz_vec3_sub(widget->points[1], widget->points[0]))
               : 0.0f;
}

FVizBool fviz_distance_widget_completed(const FVizDistanceWidget* widget)
{
    return widget != NULL && widget->point_count >= 2u ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_distance_widget_reset(FVizDistanceWidget* widget)
{
    if (widget == NULL) return;
    widget->point_count = 0u;
    (void)fviz_distance_widget_rebuild(widget);
    fviz_widget_value_changed(widget->widget);
}

FVizBillboardTextActor3D* fviz_distance_widget_label(FVizDistanceWidget* widget)
{
    return widget != NULL ? widget->label : NULL;
}

static float fviz_angle_widget_compute(const FVizAngleWidget* widget)
{
    FVizVec3 a, b;
    float la, lb, c;
    if (widget == NULL || widget->point_count < 3u) return 0.0f;
    a = fviz_vec3_sub(widget->points[0], widget->points[1]);
    b = fviz_vec3_sub(widget->points[2], widget->points[1]);
    la = fviz_vec3_length(a);
    lb = fviz_vec3_length(b);
    if (la <= 1.0e-12f || lb <= 1.0e-12f) return 0.0f;
    c = fviz_vec3_dot(a, b) / (la * lb);
    if (c < -1.0f) c = -1.0f;
    if (c > 1.0f) c = 1.0f;
    return acosf(c) * (180.0f / 3.14159265358979323846f);
}

static FVizResult fviz_angle_widget_rebuild(FVizAngleWidget* widget)
{
    const uint32_t lines[4] = {1u, 0u, 1u, 2u};
    char text[96];
    if (widget->point_count == 0u)
    {
        (void)fviz_widget_representation_set_actor_visible(widget->representation, widget->actor, FVIZ_FALSE);
        (void)fviz_widget_representation_set_billboard_text_actor_3d_visible(widget->representation, widget->label,
                                                                             FVIZ_FALSE);
        return FVIZ_OK;
    }
    if (fviz_engineering_replace_actor_geometry(widget->actor, widget->points, widget->point_count,
                                                widget->point_count >= 3u ? lines : NULL,
                                                widget->point_count >= 3u ? 2u : 0u, NULL, 0u) != FVIZ_OK)
        return fviz_last_error_code();
    (void)fviz_widget_representation_set_actor_visible(widget->representation, widget->actor, FVIZ_TRUE);
    fviz_actor_set_point_visibility(widget->actor, FVIZ_TRUE);
    if (widget->point_count < 3u)
    {
        (void)fviz_widget_representation_set_billboard_text_actor_3d_visible(widget->representation, widget->label,
                                                                             FVIZ_FALSE);
        return FVIZ_OK;
    }
    (void)snprintf(text, sizeof(text), "%.4g deg", (double)fviz_angle_widget_compute(widget));
    if (fviz_billboard_text_actor_3d_set_text(widget->label, text) != FVIZ_OK) return fviz_last_error_code();
    fviz_billboard_text_actor_3d_set_world_position(widget->label, widget->points[1]);
    fviz_billboard_text_actor_3d_set_pixel_offset(widget->label, 8.0f, -8.0f);
    (void)fviz_widget_representation_set_billboard_text_actor_3d_visible(widget->representation, widget->label,
                                                                         FVIZ_TRUE);
    return FVIZ_OK;
}

static FVizBool fviz_angle_widget_event(FVizWidget* base_widget, const FVizInteractionEvent* event, void* user_data)
{
    FVizAngleWidget* widget = (FVizAngleWidget*)user_data;
    FVizRenderWindow* window = fviz_render_window_interactor_window(fviz_widget_interactor(base_widget));
    FVizVec3 point;
    if (event->type != FVIZ_INTERACTION_MOUSE_BUTTON_DOWN || event->button != FVIZ_MOUSE_BUTTON_LEFT || window == NULL)
        return FVIZ_FALSE;
    if (fviz_engineering_pick_triangle_world(window, fviz_widget_renderer(base_widget), event->x, event->y, NULL, NULL,
                                             &point) == FVIZ_FALSE)
        return FVIZ_FALSE;
    if (widget->point_count >= 3u) widget->point_count = 0u;
    widget->points[widget->point_count++] = point;
    (void)fviz_angle_widget_rebuild(widget);
    fviz_widget_value_changed(base_widget);
    (void)fviz_widget_request_render(base_widget);
    return FVIZ_TRUE;
}

static void fviz_angle_widget_destroy(FVizObject* object)
{
    FVizAngleWidget* w = (FVizAngleWidget*)object;
    fviz_release(w->widget);
    fviz_release(w->representation);
    fviz_release(w->actor);
    fviz_release(w->label);
}

static const FVizObjectClass g_fviz_angle_widget_class = {FVIZ_TYPE_ANGLE_WIDGET, "FVizAngleWidget",
                                                          &g_fviz_object_class, fviz_angle_widget_destroy, NULL};

FVizResult fviz_angle_widget_create(FVizRenderWindowInteractor* interactor, FVizRenderer* renderer,
                                    FVizAngleWidget** out_widget)
{
    FVizAngleWidget* w;
    if (!renderer || !out_widget) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_widget = NULL;
    w = (FVizAngleWidget*)fviz_internal_object_allocate(sizeof(*w), &g_fviz_angle_widget_class, NULL);
    if (!w) return fviz_last_error_code();
    if (fviz_widget_representation_create(renderer, &w->representation) != FVIZ_OK ||
        fviz_actor_create(&w->actor) != FVIZ_OK || fviz_billboard_text_actor_3d_create(&w->label) != FVIZ_OK ||
        fviz_widget_representation_add_actor(w->representation, w->actor) != FVIZ_OK ||
        fviz_widget_representation_add_billboard_text_actor_3d(w->representation, w->label) != FVIZ_OK ||
        fviz_widget_create(interactor, renderer, w->representation, &w->widget) != FVIZ_OK)
    {
        fviz_release(w);
        return fviz_last_error_code();
    }
    fviz_actor_set_color(w->actor, 0.3f, 1.0f, 0.45f);
    fviz_actor_set_line_width(w->actor, 2.0f);
    fviz_actor_set_point_size(w->actor, 7.0f);
    fviz_actor_set_point_shape(w->actor, FVIZ_POINT_CIRCLE);
    fviz_widget_set_event_handler(w->widget, fviz_angle_widget_event, w);
    fviz_angle_widget_reset(w);
    *out_widget = w;
    return FVIZ_OK;
}

FVizWidget* fviz_angle_widget_widget(FVizAngleWidget* w)
{
    return w ? w->widget : NULL;
}

FVizResult fviz_angle_widget_set_points(FVizAngleWidget* w, FVizVec3 p1, FVizVec3 vertex, FVizVec3 p2)
{
    if (!w) return FVIZ_ERROR_INVALID_ARGUMENT;
    w->points[0] = p1;
    w->points[1] = vertex;
    w->points[2] = p2;
    w->point_count = 3u;
    if (fviz_angle_widget_rebuild(w) != FVIZ_OK) return fviz_last_error_code();
    fviz_widget_value_changed(w->widget);
    return FVIZ_OK;
}

float fviz_angle_widget_angle_degrees(const FVizAngleWidget* w)
{
    return fviz_angle_widget_compute(w);
}

FVizBool fviz_angle_widget_completed(const FVizAngleWidget* w)
{
    return w && w->point_count >= 3u ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_angle_widget_reset(FVizAngleWidget* w)
{
    if (!w) return;
    w->point_count = 0u;
    (void)fviz_angle_widget_rebuild(w);
    fviz_widget_value_changed(w->widget);
}

FVizBillboardTextActor3D* fviz_angle_widget_label(FVizAngleWidget* w)
{
    return w ? w->label : NULL;
}

/* ------------------------------------------------------------------------- */
/* Section cut widget                                                         */
/* ------------------------------------------------------------------------- */

static FVizResult fviz_section_cut_targets_reserve(FVizSectionCutWidget* widget, FVizSize required)
{
    FVizSize capacity, bytes;
    FVizSectionCutTarget* targets;
    if (required <= widget->target_capacity) return FVIZ_OK;
    capacity = widget->target_capacity ? widget->target_capacity : 4u;
    while (capacity < required)
    {
        if (capacity > SIZE_MAX / 2u) return FVIZ_ERROR_OVERFLOW;
        capacity *= 2u;
    }
    if (fviz_size_multiply(capacity, sizeof(*targets), &bytes) != FVIZ_OK) return fviz_last_error_code();
    targets = (FVizSectionCutTarget*)fviz_realloc(widget->targets, bytes);
    if (!targets) return fviz_last_error_code();
    widget->targets = targets;
    widget->target_capacity = capacity;
    return FVIZ_OK;
}

static FVizPlane fviz_section_cut_effective_plane(FVizSectionCutWidget* widget)
{
    FVizPlane p = fviz_plane_widget_plane(widget->plane_widget);
    if (widget->inside_out != FVIZ_FALSE)
    {
        p.normal = fviz_vec3_scale(p.normal, -1.0f);
        p.distance = -p.distance;
    }
    return p;
}

static void fviz_section_cut_plane_changed(FVizPlaneWidget* plane_widget, void* user_data)
{
    (void)plane_widget;
    (void)fviz_section_cut_widget_update((FVizSectionCutWidget*)user_data);
}

static void fviz_section_cut_widget_destroy(FVizObject* object)
{
    FVizSectionCutWidget* w = (FVizSectionCutWidget*)object;
    fviz_section_cut_widget_remove_all_actors(w);
    fviz_free(w->targets);
    w->targets = NULL;
    w->target_capacity = 0u;
    if (w->plane_widget)
    {
        w->plane_widget->internal_changed = NULL;
        w->plane_widget->internal_changed_data = NULL;
    }
    fviz_release(w->plane_widget);
}

static const FVizObjectClass g_fviz_section_cut_widget_class = {
    FVIZ_TYPE_SECTION_CUT_WIDGET, "FVizSectionCutWidget", &g_fviz_object_class, fviz_section_cut_widget_destroy, NULL};

FVizResult fviz_section_cut_widget_create(FVizRenderWindowInteractor* interactor, FVizRenderer* renderer,
                                          FVizSectionCutWidget** out_widget)
{
    FVizSectionCutWidget* w;
    if (!renderer || !out_widget) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_widget = NULL;
    w = (FVizSectionCutWidget*)fviz_internal_object_allocate(sizeof(*w), &g_fviz_section_cut_widget_class, NULL);
    if (!w) return fviz_last_error_code();
    if (fviz_plane_widget_create(interactor, renderer, &w->plane_widget) != FVIZ_OK)
    {
        fviz_release(w);
        return fviz_last_error_code();
    }
    w->plane_widget->internal_changed = fviz_section_cut_plane_changed;
    w->plane_widget->internal_changed_data = w;
    *out_widget = w;
    return FVIZ_OK;
}

FVizPlaneWidget* fviz_section_cut_widget_plane_widget(FVizSectionCutWidget* w)
{
    return w ? w->plane_widget : NULL;
}

FVizResult fviz_section_cut_widget_add_actor(FVizSectionCutWidget* w, FVizActor* actor)
{
    FVizSize i;
    FVizClipPlaneId id;
    FVizMapper* mapper;
    if (!w || !actor) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0; i < w->target_count; ++i)
        if (w->targets[i].actor == actor) return FVIZ_OK;
    mapper = fviz_actor_mapper(actor);
    if (!mapper) return FVIZ_ERROR_INVALID_STATE;
    if (fviz_section_cut_targets_reserve(w, w->target_count + 1u) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_mapper_add_clipping_plane_with_id(mapper, fviz_section_cut_effective_plane(w), &id) != FVIZ_OK)
        return fviz_last_error_code();
    if (!fviz_retain(actor))
    {
        (void)fviz_mapper_remove_clipping_plane(mapper, id);
        return fviz_last_error_code();
    }
    w->targets[w->target_count].actor = actor;
    w->targets[w->target_count].plane_id = id;
    ++w->target_count;
    return FVIZ_OK;
}

FVizResult fviz_section_cut_widget_remove_actor(FVizSectionCutWidget* w, FVizActor* actor)
{
    FVizSize i;
    if (!w || !actor) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0; i < w->target_count; ++i)
        if (w->targets[i].actor == actor)
        {
            FVizMapper* mapper = fviz_actor_mapper(actor);
            if (mapper) (void)fviz_mapper_remove_clipping_plane(mapper, w->targets[i].plane_id);
            fviz_release(actor);
            if (i + 1u < w->target_count)
                (void)memmove(&w->targets[i], &w->targets[i + 1u],
                              (size_t)(w->target_count - i - 1u) * sizeof(w->targets[0]));
            --w->target_count;
            return FVIZ_OK;
        }
    return FVIZ_ERROR_NOT_FOUND;
}

void fviz_section_cut_widget_remove_all_actors(FVizSectionCutWidget* w)
{
    if (!w) return;
    while (w->target_count > 0u)
        (void)fviz_section_cut_widget_remove_actor(w, w->targets[w->target_count - 1u].actor);
}

FVizSize fviz_section_cut_widget_actor_count(const FVizSectionCutWidget* w)
{
    return w ? w->target_count : 0u;
}

void fviz_section_cut_widget_set_inside_out(FVizSectionCutWidget* w, FVizBool inside_out)
{
    if (!w) return;
    w->inside_out = inside_out != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    (void)fviz_section_cut_widget_update(w);
}

FVizBool fviz_section_cut_widget_inside_out(const FVizSectionCutWidget* w)
{
    return w ? w->inside_out : FVIZ_FALSE;
}

FVizResult fviz_section_cut_widget_update(FVizSectionCutWidget* w)
{
    FVizSize i;
    FVizPlane p;
    if (!w) return FVIZ_ERROR_INVALID_ARGUMENT;
    p = fviz_section_cut_effective_plane(w);
    for (i = 0; i < w->target_count; ++i)
    {
        FVizMapper* mapper = fviz_actor_mapper(w->targets[i].actor);
        if (!mapper) return FVIZ_ERROR_INVALID_STATE;
        if (fviz_mapper_update_clipping_plane(mapper, w->targets[i].plane_id, p) != FVIZ_OK)
            return fviz_last_error_code();
    }
    return FVIZ_OK;
}

/* ------------------------------------------------------------------------- */
/* Probe widget                                                               */
/* ------------------------------------------------------------------------- */

static FVizBool fviz_probe_widget_event(FVizWidget* base_widget, const FVizInteractionEvent* event, void* user_data)
{
    FVizProbeWidget* w = (FVizProbeWidget*)user_data;
    if (event->type == FVIZ_INTERACTION_MOUSE_BUTTON_DOWN && event->button == FVIZ_MOUSE_BUTTON_LEFT)
    {
        if (fviz_probe_widget_probe_at(w, event->x, event->y) == FVIZ_OK)
        {
            fviz_widget_value_changed(base_widget);
            (void)fviz_widget_request_render(base_widget);
            return FVIZ_TRUE;
        }
    }
    return FVIZ_FALSE;
}

static void fviz_probe_widget_destroy(FVizObject* object)
{
    FVizProbeWidget* w = (FVizProbeWidget*)object;
    fviz_release(w->selection);
    fviz_release(w->label);
    fviz_release(w->widget);
    fviz_release(w->representation);
}

static const FVizObjectClass g_fviz_probe_widget_class = {FVIZ_TYPE_PROBE_WIDGET, "FVizProbeWidget",
                                                          &g_fviz_object_class, fviz_probe_widget_destroy, NULL};

FVizResult fviz_probe_widget_create(FVizRenderWindowInteractor* interactor, FVizRenderer* renderer,
                                    FVizProbeWidget** out_widget)
{
    FVizProbeWidget* w;
    if (!renderer || !out_widget) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_widget = NULL;
    w = (FVizProbeWidget*)fviz_internal_object_allocate(sizeof(*w), &g_fviz_probe_widget_class, NULL);
    if (!w) return fviz_last_error_code();
    if (fviz_widget_representation_create(renderer, &w->representation) != FVIZ_OK ||
        fviz_billboard_text_actor_3d_create(&w->label) != FVIZ_OK ||
        fviz_widget_representation_add_billboard_text_actor_3d(w->representation, w->label) != FVIZ_OK ||
        fviz_widget_create(interactor, renderer, w->representation, &w->widget) != FVIZ_OK)
    {
        fviz_release(w);
        return fviz_last_error_code();
    }
    (void)fviz_widget_representation_set_billboard_text_actor_3d_visible(w->representation, w->label, FVIZ_FALSE);
    fviz_widget_set_event_handler(w->widget, fviz_probe_widget_event, w);
    *out_widget = w;
    return FVIZ_OK;
}

FVizWidget* fviz_probe_widget_widget(FVizProbeWidget* w)
{
    return w ? w->widget : NULL;
}

FVizResult fviz_probe_widget_set_array_name(FVizProbeWidget* w, const char* name)
{
    size_t n;
    if (!w) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (!name || !*name)
    {
        w->array_name[0] = '\0';
        return FVIZ_OK;
    }
    n = strlen(name);
    if (n >= sizeof(w->array_name)) return FVIZ_ERROR_OVERFLOW;
    memcpy(w->array_name, name, n + 1u);
    return FVIZ_OK;
}

const char* fviz_probe_widget_array_name(const FVizProbeWidget* w)
{
    return w && w->array_name[0] ? w->array_name : NULL;
}

FVizResult fviz_probe_widget_probe_at(FVizProbeWidget* w, int x, int y)
{
    FVizRenderWindow* window;
    FVizRenderer* renderer;
    FVizSelection* selection = NULL;
    FVizSelectionRecord record;
    FVizVec3 world;
    FVizActor* actor = NULL;
    FVizSize triangle = 0u;
    char text[256];
    int written;
    if (!w || !w->widget) return FVIZ_ERROR_INVALID_ARGUMENT;
    window = fviz_render_window_interactor_window(fviz_widget_interactor(w->widget));
    renderer = fviz_widget_renderer(w->widget);
    if (!window || !renderer) return FVIZ_ERROR_INVALID_STATE;
    if (fviz_render_window_select_at(window, x, y, FVIZ_SELECTION_CELL, &selection) != FVIZ_OK)
        return fviz_last_error_code();
    if (w->array_name[0] && fviz_selection_probe(selection, 0u, w->array_name) != FVIZ_OK)
    {
        fviz_release(selection);
        return fviz_last_error_code();
    }
    fviz_release(w->selection);
    w->selection = selection;
    if (fviz_selection_get_record(selection, 0u, &record) != FVIZ_OK) return fviz_last_error_code();
    if (!fviz_engineering_pick_triangle_world(window, renderer, x, y, &actor, &triangle, &world))
    {
        (void)actor;
        (void)triangle;
        world = fviz_vec3(0.0f, 0.0f, 0.0f);
    }
    if (w->array_name[0] && record.scalar_component_count > 0u)
        written = snprintf(text, sizeof(text), "%s: %.6g", w->array_name, record.scalar_tuple[0]);
    else
        written = snprintf(text, sizeof(text), "Cell %llu", (unsigned long long)record.original_cell_id);
    if (written < 0) return FVIZ_ERROR_INTERNAL;
    if (fviz_billboard_text_actor_3d_set_text(w->label, text) != FVIZ_OK) return fviz_last_error_code();
    fviz_billboard_text_actor_3d_set_world_position(w->label, world);
    fviz_billboard_text_actor_3d_set_pixel_offset(w->label, 8.0f, -8.0f);
    (void)fviz_widget_representation_set_billboard_text_actor_3d_visible(w->representation, w->label, FVIZ_TRUE);
    return FVIZ_OK;
}

FVizSelection* fviz_probe_widget_selection(FVizProbeWidget* w)
{
    return w ? w->selection : NULL;
}

FVizBillboardTextActor3D* fviz_probe_widget_label(FVizProbeWidget* w)
{
    return w ? w->label : NULL;
}

void fviz_probe_widget_clear(FVizProbeWidget* w)
{
    if (!w) return;
    fviz_release(w->selection);
    w->selection = NULL;
    (void)fviz_widget_representation_set_billboard_text_actor_3d_visible(w->representation, w->label, FVIZ_FALSE);
    if (w->widget) fviz_widget_value_changed(w->widget);
}
