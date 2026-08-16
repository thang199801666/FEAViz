#include <ctype.h>
#include <math.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Interaction/FVizInteractorStyle.h>
#include <FViz/Math/FVizMath.h>
#include <FViz/Rendering/FVizActor.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizInteractorStylePrivate.h>

static void fviz_interactor_style_destroy(FVizObject* object)
{
    FVizInteractorStyle* style = (FVizInteractorStyle*)object;
    fviz_release(style->snapshot_camera);
    fviz_release(style->actor);
    style->snapshot_camera = NULL;
    style->actor = NULL;
}

static void fviz_interactor_style_finish_transaction(FVizInteractorStyle* style)
{
    if (style == NULL) return;
    fviz_release(style->snapshot_camera);
    style->snapshot_camera = NULL;
    style->transaction_active = FVIZ_FALSE;
}

static void fviz_interactor_style_begin_camera_transaction(FVizInteractorStyle* style, FVizCamera* camera)
{
    if (style == NULL || camera == NULL || style->transaction_active != FVIZ_FALSE) return;
    style->snapshot_camera = (FVizCamera*)fviz_retain(camera);
    if (style->snapshot_camera == NULL) return;
    style->snapshot_camera_position = fviz_camera_position(camera);
    style->snapshot_camera_target = fviz_camera_target(camera);
    style->snapshot_camera_up = fviz_camera_up(camera);
    style->snapshot_camera_fov = fviz_camera_fov_degrees(camera);
    style->snapshot_camera_near = fviz_camera_near_plane(camera);
    style->snapshot_camera_far = fviz_camera_far_plane(camera);
    style->snapshot_camera_parallel_scale = fviz_camera_parallel_scale(camera);
    style->snapshot_camera_projection = fviz_camera_projection_mode(camera);
    style->transaction_active = FVIZ_TRUE;
}

static void fviz_interactor_style_begin_actor_transaction(FVizInteractorStyle* style)
{
    if (style == NULL || style->actor == NULL || style->transaction_active != FVIZ_FALSE) return;
    style->snapshot_actor_position = fviz_actor_position(style->actor);
    style->snapshot_actor_orientation = fviz_actor_orientation(style->actor);
    style->snapshot_actor_scale = fviz_actor_scale(style->actor);
    style->transaction_active = FVIZ_TRUE;
}

static const FVizObjectClass g_fviz_interactor_style_class = {
    FVIZ_TYPE_INTERACTOR_STYLE, "FVizInteractorStyle", &g_fviz_object_class, fviz_interactor_style_destroy, NULL};

static const FVizObjectClass g_fviz_interactor_style_trackball_camera_class = {
    FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_CAMERA, "FVizInteractorStyleTrackballCamera", &g_fviz_interactor_style_class,
    fviz_interactor_style_destroy, NULL};

static const FVizObjectClass g_fviz_interactor_style_rubber_band_class = {
    FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND, "FVizInteractorStyleRubberBand", &g_fviz_interactor_style_class,
    fviz_interactor_style_destroy, NULL};

static const FVizObjectClass g_fviz_interactor_style_trackball_actor_class = {
    FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_ACTOR, "FVizInteractorStyleTrackballActor", &g_fviz_interactor_style_class,
    fviz_interactor_style_destroy, NULL};

static FVizResult fviz_interactor_style_create_with_class(const FVizObjectClass* object_class,
                                                          FVizInteractorStyle** out_style)
{
    FVizInteractorStyle* style;
    if (out_style == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_style must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_style = NULL;
    style = (FVizInteractorStyle*)fviz_internal_object_allocate(sizeof(FVizInteractorStyle), object_class, NULL);
    if (style == NULL) return fviz_last_error_code();
    style->orbit_sensitivity = 0.008f;
    style->pan_sensitivity = 0.0015f;
    style->dolly_factor = 0.85f;
    *out_style = style;
    return FVIZ_OK;
}

FVizResult fviz_interactor_style_trackball_camera_create(FVizInteractorStyle** out_style)
{
    return fviz_interactor_style_create_with_class(&g_fviz_interactor_style_trackball_camera_class, out_style);
}

FVizResult fviz_interactor_style_trackball_actor_create(FVizInteractorStyle** out_style)
{
    return fviz_interactor_style_create_with_class(&g_fviz_interactor_style_trackball_actor_class, out_style);
}

FVizResult fviz_interactor_style_trackball_actor_set_actor(FVizInteractorStyle* style, FVizActor* actor)
{
    if (style == NULL ||
        fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_ACTOR) == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (actor != NULL && fviz_retain(actor) == NULL) return fviz_last_error_code();
    if (style->transaction_active != FVIZ_FALSE) fviz_interactor_style_cancel_interaction(style);
    fviz_release(style->actor);
    style->actor = actor;
    fviz_object_modified((FVizObject*)style);
    return FVIZ_OK;
}

FVizActor* fviz_interactor_style_trackball_actor_actor(FVizInteractorStyle* style)
{
    return style != NULL && fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_ACTOR) !=
                                FVIZ_FALSE
               ? style->actor
               : NULL;
}

FVizResult fviz_interactor_style_rubber_band_create(FVizInteractorStyle** out_style)
{
    FVizInteractorStyle* style;
    if (out_style == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_style must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_style = NULL;
    style = (FVizInteractorStyle*)fviz_internal_object_allocate(sizeof(FVizInteractorStyle),
                                                                &g_fviz_interactor_style_rubber_band_class, NULL);
    if (style == NULL) return fviz_last_error_code();
    *out_style = style;
    return FVIZ_OK;
}

FVizBool fviz_interactor_style_rubber_band_active(const FVizInteractorStyle* style)
{
    return style != NULL && fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND)
               ? style->rubber_active
               : FVIZ_FALSE;
}

FVizBool fviz_interactor_style_rubber_band_completed(const FVizInteractorStyle* style)
{
    return style != NULL && fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND)
               ? style->rubber_completed
               : FVIZ_FALSE;
}

FVizResult fviz_interactor_style_rubber_band_rectangle(const FVizInteractorStyle* style, int* minimum_x, int* minimum_y,
                                                       int* maximum_x, int* maximum_y)
{
    if (style == NULL ||
        fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND) == FVIZ_FALSE ||
        minimum_x == NULL || minimum_y == NULL || maximum_x == NULL || maximum_y == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rubber-band rectangle arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *minimum_x = style->rubber_start_x < style->rubber_end_x ? style->rubber_start_x : style->rubber_end_x;
    *minimum_y = style->rubber_start_y < style->rubber_end_y ? style->rubber_start_y : style->rubber_end_y;
    *maximum_x = style->rubber_start_x > style->rubber_end_x ? style->rubber_start_x : style->rubber_end_x;
    *maximum_y = style->rubber_start_y > style->rubber_end_y ? style->rubber_start_y : style->rubber_end_y;
    return FVIZ_OK;
}

void fviz_interactor_style_rubber_band_reset(FVizInteractorStyle* style)
{
    if (style == NULL ||
        fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND) == FVIZ_FALSE)
        return;
    style->rubber_active = FVIZ_FALSE;
    style->rubber_completed = FVIZ_FALSE;
    style->state = FVIZ_INTERACTION_STATE_NONE;
}

void fviz_interactor_style_set_orbit_sensitivity(FVizInteractorStyle* style, float radians_per_pixel)
{
    if (style != NULL && radians_per_pixel > 0.0f) style->orbit_sensitivity = radians_per_pixel;
}

float fviz_interactor_style_orbit_sensitivity(const FVizInteractorStyle* style)
{
    return style != NULL ? style->orbit_sensitivity : 0.0f;
}

void fviz_interactor_style_set_pan_sensitivity(FVizInteractorStyle* style, float fraction_per_pixel)
{
    if (style != NULL && fraction_per_pixel > 0.0f) style->pan_sensitivity = fraction_per_pixel;
}

float fviz_interactor_style_pan_sensitivity(const FVizInteractorStyle* style)
{
    return style != NULL ? style->pan_sensitivity : 0.0f;
}

void fviz_interactor_style_set_dolly_factor(FVizInteractorStyle* style, float factor)
{
    if (style != NULL && factor > 0.0f && factor < 1.0f) style->dolly_factor = factor;
}

float fviz_interactor_style_dolly_factor(const FVizInteractorStyle* style)
{
    return style != NULL ? style->dolly_factor : 1.0f;
}

FVizInteractionState fviz_interactor_style_state(const FVizInteractorStyle* style)
{
    return style != NULL ? style->state : FVIZ_INTERACTION_STATE_NONE;
}

void fviz_interactor_style_cancel_interaction(FVizInteractorStyle* style)
{
    if (style == NULL) return;
    if (style->transaction_active != FVIZ_FALSE)
    {
        if (style->snapshot_camera != NULL)
        {
            fviz_camera_set_position(style->snapshot_camera, style->snapshot_camera_position);
            fviz_camera_set_target(style->snapshot_camera, style->snapshot_camera_target);
            fviz_camera_set_up(style->snapshot_camera, style->snapshot_camera_up);
            fviz_camera_set_perspective(style->snapshot_camera, style->snapshot_camera_fov, style->snapshot_camera_near,
                                        style->snapshot_camera_far);
            fviz_camera_set_parallel_scale(style->snapshot_camera, style->snapshot_camera_parallel_scale);
            fviz_camera_set_projection_mode(style->snapshot_camera, style->snapshot_camera_projection);
        }
        else if (style->actor != NULL)
        {
            fviz_actor_set_position(style->actor, style->snapshot_actor_position);
            fviz_actor_set_orientation(style->actor, style->snapshot_actor_orientation);
            fviz_actor_set_scale(style->actor, style->snapshot_actor_scale);
        }
    }
    fviz_interactor_style_finish_transaction(style);
    style->left_down = FVIZ_FALSE;
    style->middle_down = FVIZ_FALSE;
    style->right_down = FVIZ_FALSE;
    style->rubber_active = FVIZ_FALSE;
    style->state = FVIZ_INTERACTION_STATE_NONE;
}

static void fviz_interactor_style_toggle_wireframe(FVizRenderer* renderer)
{
    FVizScene* scene = fviz_renderer_scene(renderer);
    FVizSize i;
    for (i = 0u; scene != NULL && i < fviz_scene_actor_count(scene); ++i)
    {
        FVizActor* actor = fviz_scene_actor(scene, i);
        fviz_actor_set_wireframe(actor, fviz_actor_wireframe(actor) == FVIZ_TRUE ? FVIZ_FALSE : FVIZ_TRUE);
    }
}

FVizBool fviz_interactor_style_process_event(FVizInteractorStyle* style, FVizRenderer* renderer,
                                             const FVizInteractionEvent* event)
{
    FVizCamera* camera;
    if (style == NULL || renderer == NULL || event == NULL) return FVIZ_FALSE;
    if (fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_ACTOR) == FVIZ_TRUE)
    {
        FVizActor* actor = style->actor;
        if (actor == NULL) return FVIZ_FALSE;
        switch (event->type)
        {
            case FVIZ_INTERACTION_MOUSE_BUTTON_DOWN:
                if (event->button != FVIZ_MOUSE_BUTTON_LEFT && event->button != FVIZ_MOUSE_BUTTON_MIDDLE &&
                    event->button != FVIZ_MOUSE_BUTTON_RIGHT)
                    return FVIZ_FALSE;
                fviz_interactor_style_begin_actor_transaction(style);
                if (event->button == FVIZ_MOUSE_BUTTON_LEFT)
                {
                    style->left_down = FVIZ_TRUE;
                    style->state = FVIZ_INTERACTION_STATE_ACTOR_ROTATE;
                }
                else if (event->button == FVIZ_MOUSE_BUTTON_MIDDLE)
                {
                    style->middle_down = FVIZ_TRUE;
                    style->state = FVIZ_INTERACTION_STATE_ACTOR_PAN;
                }
                else if (event->button == FVIZ_MOUSE_BUTTON_RIGHT)
                {
                    style->right_down = FVIZ_TRUE;
                    style->state = FVIZ_INTERACTION_STATE_ACTOR_DOLLY;
                }
                else
                    return FVIZ_FALSE;
                style->last_x = event->x;
                style->last_y = event->y;
                return FVIZ_TRUE;
            case FVIZ_INTERACTION_MOUSE_BUTTON_UP:
                if (event->button == FVIZ_MOUSE_BUTTON_LEFT) style->left_down = FVIZ_FALSE;
                else if (event->button == FVIZ_MOUSE_BUTTON_MIDDLE)
                    style->middle_down = FVIZ_FALSE;
                else if (event->button == FVIZ_MOUSE_BUTTON_RIGHT)
                    style->right_down = FVIZ_FALSE;
                else
                    return FVIZ_FALSE;
                if (style->left_down == FVIZ_FALSE && style->middle_down == FVIZ_FALSE &&
                    style->right_down == FVIZ_FALSE)
                {
                    style->state = FVIZ_INTERACTION_STATE_NONE;
                    fviz_interactor_style_finish_transaction(style);
                }
                style->last_x = event->x;
                style->last_y = event->y;
                return FVIZ_TRUE;
            case FVIZ_INTERACTION_MOUSE_MOVE:
                {
                    const int dx = event->x - style->last_x;
                    const int dy = event->y - style->last_y;
                    FVizBool handled = FVIZ_FALSE;
                    if (style->left_down != FVIZ_FALSE)
                    {
                        const FVizQuat yaw = fviz_quat_from_axis_angle(fviz_vec3(0.0f, 1.0f, 0.0f),
                                                                       -(float)dx * style->orbit_sensitivity);
                        const FVizQuat pitch = fviz_quat_from_axis_angle(fviz_vec3(1.0f, 0.0f, 0.0f),
                                                                         -(float)dy * style->orbit_sensitivity);
                        fviz_actor_set_orientation(actor,
                                                   fviz_quat_normalize(fviz_quat_multiply(
                                                       yaw, fviz_quat_multiply(pitch, fviz_actor_orientation(actor)))));
                        handled = FVIZ_TRUE;
                    }
                    else if (style->middle_down != FVIZ_FALSE)
                    {
                        const FVizVec3 position = fviz_actor_position(actor);
                        fviz_actor_set_position(actor,
                                                fviz_vec3(position.x + (float)dx * style->pan_sensitivity,
                                                          position.y - (float)dy * style->pan_sensitivity, position.z));
                        handled = FVIZ_TRUE;
                    }
                    else if (style->right_down != FVIZ_FALSE)
                    {
                        const float factor = powf(style->dolly_factor, (float)dy * 0.1f);
                        fviz_actor_set_scale(actor, fviz_vec3_scale(fviz_actor_scale(actor), factor));
                        handled = FVIZ_TRUE;
                    }
                    style->last_x = event->x;
                    style->last_y = event->y;
                    return handled;
                }
            case FVIZ_INTERACTION_MOUSE_WHEEL:
                {
                    float factor;
                    if (event->wheel_delta == 0.0f) return FVIZ_FALSE;
                    factor = powf(style->dolly_factor, -event->wheel_delta);
                    fviz_actor_set_scale(actor, fviz_vec3_scale(fviz_actor_scale(actor), factor));
                    return FVIZ_TRUE;
                }
            default:
                return FVIZ_FALSE;
        }
    }
    if (fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND) == FVIZ_TRUE)
    {
        switch (event->type)
        {
            case FVIZ_INTERACTION_MOUSE_BUTTON_DOWN:
                if (event->button != FVIZ_MOUSE_BUTTON_LEFT) return FVIZ_FALSE;
                style->rubber_start_x = event->x;
                style->rubber_start_y = event->y;
                style->rubber_end_x = event->x;
                style->rubber_end_y = event->y;
                style->rubber_active = FVIZ_TRUE;
                style->state = FVIZ_INTERACTION_STATE_RUBBER_BAND;
                style->rubber_completed = FVIZ_FALSE;
                return FVIZ_TRUE;
            case FVIZ_INTERACTION_MOUSE_MOVE:
                if (style->rubber_active == FVIZ_FALSE) return FVIZ_FALSE;
                style->rubber_end_x = event->x;
                style->rubber_end_y = event->y;
                return FVIZ_TRUE;
            case FVIZ_INTERACTION_MOUSE_BUTTON_UP:
                if (event->button != FVIZ_MOUSE_BUTTON_LEFT || style->rubber_active == FVIZ_FALSE) return FVIZ_FALSE;
                style->rubber_end_x = event->x;
                style->rubber_end_y = event->y;
                style->rubber_active = FVIZ_FALSE;
                style->state = FVIZ_INTERACTION_STATE_NONE;
                style->rubber_completed = FVIZ_TRUE;
                return FVIZ_TRUE;
            case FVIZ_INTERACTION_KEY_DOWN:
                if (event->key == FVIZ_KEY_ESCAPE)
                {
                    fviz_interactor_style_rubber_band_reset(style);
                    return FVIZ_TRUE;
                }
                return FVIZ_FALSE;
            default:
                return FVIZ_FALSE;
        }
    }
    camera = fviz_renderer_camera(renderer);
    if (camera == NULL) return FVIZ_FALSE;
    switch (event->type)
    {
        case FVIZ_INTERACTION_DOUBLE_CLICK:
            if (event->button != FVIZ_MOUSE_BUTTON_LEFT) return FVIZ_FALSE;
            fviz_renderer_fit_camera(renderer, 1.2f);
            return FVIZ_TRUE;
        case FVIZ_INTERACTION_MOUSE_BUTTON_DOWN:
            if (event->button != FVIZ_MOUSE_BUTTON_LEFT && event->button != FVIZ_MOUSE_BUTTON_MIDDLE &&
                event->button != FVIZ_MOUSE_BUTTON_RIGHT)
                return FVIZ_FALSE;
            fviz_interactor_style_begin_camera_transaction(style, camera);
            if (event->button == FVIZ_MOUSE_BUTTON_LEFT)
            {
                style->left_down = FVIZ_TRUE;
                style->state =
                    event->shift != FVIZ_FALSE
                        ? FVIZ_INTERACTION_STATE_PAN
                        : (event->control != FVIZ_FALSE ? FVIZ_INTERACTION_STATE_DOLLY : FVIZ_INTERACTION_STATE_ROTATE);
            }
            else if (event->button == FVIZ_MOUSE_BUTTON_MIDDLE)
            {
                style->middle_down = FVIZ_TRUE;
                style->state = FVIZ_INTERACTION_STATE_PAN;
            }
            else if (event->button == FVIZ_MOUSE_BUTTON_RIGHT)
            {
                style->right_down = FVIZ_TRUE;
                style->state = FVIZ_INTERACTION_STATE_DOLLY;
            }
            else
                return FVIZ_FALSE;
            style->last_x = event->x;
            style->last_y = event->y;
            return FVIZ_TRUE;
        case FVIZ_INTERACTION_MOUSE_BUTTON_UP:
            if (event->button == FVIZ_MOUSE_BUTTON_LEFT) style->left_down = FVIZ_FALSE;
            else if (event->button == FVIZ_MOUSE_BUTTON_MIDDLE)
                style->middle_down = FVIZ_FALSE;
            else if (event->button == FVIZ_MOUSE_BUTTON_RIGHT)
                style->right_down = FVIZ_FALSE;
            else
                return FVIZ_FALSE;
            if (style->left_down == FVIZ_FALSE && style->middle_down == FVIZ_FALSE && style->right_down == FVIZ_FALSE)
            {
                style->state = FVIZ_INTERACTION_STATE_NONE;
                fviz_interactor_style_finish_transaction(style);
            }
            style->last_x = event->x;
            style->last_y = event->y;
            return FVIZ_TRUE;
        case FVIZ_INTERACTION_MOUSE_MOVE:
            {
                const int dx = event->x - style->last_x;
                const int dy = event->y - style->last_y;
                FVizBool handled = FVIZ_FALSE;
                if (style->state == FVIZ_INTERACTION_STATE_ROTATE)
                {
                    fviz_camera_orbit(camera, -(float)dx * style->orbit_sensitivity,
                                      -(float)dy * style->orbit_sensitivity);
                    handled = FVIZ_TRUE;
                }
                else if (style->state == FVIZ_INTERACTION_STATE_PAN)
                {
                    float viewport_height = event->height > 0 ? (float)event->height : 600.0f;
                    float minimum_x;
                    float minimum_y;
                    float maximum_x;
                    float maximum_y;
                    float half_height;
                    float world_per_pixel;
                    const float sensitivity_scale = style->pan_sensitivity / 0.0015f;
                    fviz_renderer_get_viewport(renderer, &minimum_x, &minimum_y, &maximum_x, &maximum_y);
                    (void)minimum_x;
                    (void)maximum_x;
                    viewport_height *= maximum_y - minimum_y;
                    if (viewport_height < 1.0f) viewport_height = 1.0f;
                    if (fviz_camera_projection_mode(camera) == FVIZ_CAMERA_PARALLEL)
                        half_height = fviz_camera_parallel_scale(camera);
                    else
                    {
                        const float distance =
                            fviz_vec3_length(fviz_vec3_sub(fviz_camera_position(camera), fviz_camera_target(camera)));
                        const float half_fov = fviz_camera_fov_degrees(camera) * FVIZ_PI_F / 360.0f;
                        half_height = distance * tanf(half_fov);
                    }
                    world_per_pixel = 2.0f * half_height / viewport_height * sensitivity_scale;
                    fviz_camera_pan(camera, -(float)dx * world_per_pixel, (float)dy * world_per_pixel);
                    handled = FVIZ_TRUE;
                }
                else if (style->state == FVIZ_INTERACTION_STATE_DOLLY)
                {
                    fviz_camera_dolly(camera, powf(style->dolly_factor, -(float)dy * 0.1f));
                    handled = FVIZ_TRUE;
                }
                style->last_x = event->x;
                style->last_y = event->y;
                return handled;
            }
        case FVIZ_INTERACTION_MOUSE_WHEEL:
            if (event->wheel_delta == 0.0f) return FVIZ_FALSE;
            fviz_camera_dolly(camera, powf(style->dolly_factor, event->wheel_delta));
            return FVIZ_TRUE;
        case FVIZ_INTERACTION_KEY_DOWN:
            {
                const int key = event->key >= 0 && event->key <= 255 ? toupper((unsigned char)event->key) : event->key;
                if (key == 'F' || key == 'R')
                {
                    fviz_renderer_fit_camera(renderer, 1.2f);
                    return FVIZ_TRUE;
                }
                if (key == 'P')
                {
                    const float distance =
                        fviz_vec3_length(fviz_vec3_sub(fviz_camera_position(camera), fviz_camera_target(camera)));
                    const float half_fov = fviz_camera_fov_degrees(camera) * FVIZ_PI_F / 360.0f;
                    if (fviz_camera_projection_mode(camera) == FVIZ_CAMERA_PERSPECTIVE)
                    {
                        fviz_camera_set_parallel_scale(camera, distance * tanf(half_fov));
                        fviz_camera_set_projection_mode(camera, FVIZ_CAMERA_PARALLEL);
                    }
                    else
                    {
                        const float tangent = tanf(half_fov);
                        if (tangent > 1.0e-6f)
                        {
                            const FVizVec3 target = fviz_camera_target(camera);
                            const FVizVec3 direction =
                                fviz_vec3_normalize(fviz_vec3_sub(fviz_camera_position(camera), target));
                            const float new_distance = fviz_camera_parallel_scale(camera) / tangent;
                            fviz_camera_set_position(camera,
                                                     fviz_vec3_add(target, fviz_vec3_scale(direction, new_distance)));
                        }
                        fviz_camera_set_projection_mode(camera, FVIZ_CAMERA_PERSPECTIVE);
                    }
                    return FVIZ_TRUE;
                }
                if (key == 'W')
                {
                    fviz_interactor_style_toggle_wireframe(renderer);
                    return FVIZ_TRUE;
                }
                if (key == 'S')
                {
                    FVizScene* scene = fviz_renderer_scene(renderer);
                    FVizSize i;
                    for (i = 0u; scene != NULL && i < fviz_scene_actor_count(scene); ++i)
                        fviz_actor_set_wireframe(fviz_scene_actor(scene, i), FVIZ_FALSE);
                    return FVIZ_TRUE;
                }
                return FVIZ_FALSE;
            }
        default:
            return FVIZ_FALSE;
    }
}
