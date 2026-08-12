#include <ctype.h>
#include <math.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Interaction/FVizInteractorStyle.h>
#include <FViz/Rendering/FVizActor.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizInteractorStylePrivate.h>

static void fviz_interactor_style_destroy(FVizObject* object)
{
    FVizInteractorStyle* style = (FVizInteractorStyle*)object;
    fviz_release(style->actor);
    style->actor = NULL;
}

static const FVizObjectClass g_fviz_interactor_style_class = {
    FVIZ_TYPE_INTERACTOR_STYLE,
    "FVizInteractorStyle",
    &g_fviz_object_class,
    fviz_interactor_style_destroy,
    NULL
};

static const FVizObjectClass g_fviz_interactor_style_trackball_camera_class = {
    FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_CAMERA,
    "FVizInteractorStyleTrackballCamera",
    &g_fviz_interactor_style_class,
    fviz_interactor_style_destroy,
    NULL
};

static const FVizObjectClass g_fviz_interactor_style_rubber_band_class = {
    FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND,
    "FVizInteractorStyleRubberBand",
    &g_fviz_interactor_style_class,
    fviz_interactor_style_destroy,
    NULL
};

static const FVizObjectClass g_fviz_interactor_style_trackball_actor_class = {
    FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_ACTOR,
    "FVizInteractorStyleTrackballActor",
    &g_fviz_interactor_style_class,
    fviz_interactor_style_destroy,
    NULL
};

static FVizResult fviz_interactor_style_create_with_class(
    const FVizObjectClass* object_class,
    FVizInteractorStyle** out_style)
{
    FVizInteractorStyle* style;
    if (out_style == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_style must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_style = NULL;
    style = (FVizInteractorStyle*)fviz_internal_object_allocate(
        sizeof(FVizInteractorStyle), object_class, NULL);
    if (style == NULL) return fviz_last_error_code();
    style->orbit_sensitivity = 0.008f;
    style->pan_sensitivity = 0.0015f;
    style->dolly_factor = 0.85f;
    *out_style = style;
    return FVIZ_OK;
}

FVizResult fviz_interactor_style_trackball_camera_create(FVizInteractorStyle** out_style)
{
    return fviz_interactor_style_create_with_class(
        &g_fviz_interactor_style_trackball_camera_class, out_style);
}

FVizResult fviz_interactor_style_trackball_actor_create(FVizInteractorStyle** out_style)
{
    return fviz_interactor_style_create_with_class(
        &g_fviz_interactor_style_trackball_actor_class, out_style);
}

FVizResult fviz_interactor_style_trackball_actor_set_actor(
    FVizInteractorStyle* style,
    FVizActor* actor)
{
    if (style == NULL ||
        fviz_object_is_type((const FVizObject*)style,
            FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_ACTOR) == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (actor != NULL && fviz_retain(actor) == NULL) return fviz_last_error_code();
    fviz_release(style->actor);
    style->actor = actor;
    fviz_object_modified((FVizObject*)style);
    return FVIZ_OK;
}

FVizActor* fviz_interactor_style_trackball_actor_actor(FVizInteractorStyle* style)
{
    return style != NULL && fviz_object_is_type((const FVizObject*)style,
        FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_ACTOR) != FVIZ_FALSE
        ? style->actor : NULL;
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
    style = (FVizInteractorStyle*)fviz_internal_object_allocate(
        sizeof(FVizInteractorStyle), &g_fviz_interactor_style_rubber_band_class, NULL);
    if (style == NULL) return fviz_last_error_code();
    *out_style = style;
    return FVIZ_OK;
}

FVizBool fviz_interactor_style_rubber_band_active(const FVizInteractorStyle* style)
{
    return style != NULL &&
        fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND)
        ? style->rubber_active
        : FVIZ_FALSE;
}

FVizBool fviz_interactor_style_rubber_band_completed(const FVizInteractorStyle* style)
{
    return style != NULL &&
        fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND)
        ? style->rubber_completed
        : FVIZ_FALSE;
}

FVizResult fviz_interactor_style_rubber_band_rectangle(
    const FVizInteractorStyle* style,
    int* minimum_x,
    int* minimum_y,
    int* maximum_x,
    int* maximum_y)
{
    if (style == NULL ||
        fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND) == FVIZ_FALSE ||
        minimum_x == NULL || minimum_y == NULL || maximum_x == NULL || maximum_y == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "rubber-band rectangle arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *minimum_x = style->rubber_start_x < style->rubber_end_x
        ? style->rubber_start_x : style->rubber_end_x;
    *minimum_y = style->rubber_start_y < style->rubber_end_y
        ? style->rubber_start_y : style->rubber_end_y;
    *maximum_x = style->rubber_start_x > style->rubber_end_x
        ? style->rubber_start_x : style->rubber_end_x;
    *maximum_y = style->rubber_start_y > style->rubber_end_y
        ? style->rubber_start_y : style->rubber_end_y;
    return FVIZ_OK;
}

void fviz_interactor_style_rubber_band_reset(FVizInteractorStyle* style)
{
    if (style == NULL ||
        fviz_object_is_type((const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND) == FVIZ_FALSE)
        return;
    style->rubber_active = FVIZ_FALSE;
    style->rubber_completed = FVIZ_FALSE;
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

FVizBool fviz_interactor_style_process_event(
    FVizInteractorStyle* style,
    FVizRenderer* renderer,
    const FVizInteractionEvent* event)
{
    FVizCamera* camera;
    if (style == NULL || renderer == NULL || event == NULL) return FVIZ_FALSE;
    if (fviz_object_is_type(
            (const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_ACTOR) == FVIZ_TRUE)
    {
        FVizActor* actor = style->actor;
        if (actor == NULL) return FVIZ_FALSE;
        switch (event->type)
        {
            case FVIZ_INTERACTION_MOUSE_BUTTON_DOWN:
                if (event->button == FVIZ_MOUSE_BUTTON_LEFT) style->left_down = FVIZ_TRUE;
                else if (event->button == FVIZ_MOUSE_BUTTON_MIDDLE) style->middle_down = FVIZ_TRUE;
                else if (event->button == FVIZ_MOUSE_BUTTON_RIGHT) style->right_down = FVIZ_TRUE;
                else return FVIZ_FALSE;
                style->last_x = event->x;
                style->last_y = event->y;
                return FVIZ_TRUE;
            case FVIZ_INTERACTION_MOUSE_BUTTON_UP:
                if (event->button == FVIZ_MOUSE_BUTTON_LEFT) style->left_down = FVIZ_FALSE;
                else if (event->button == FVIZ_MOUSE_BUTTON_MIDDLE) style->middle_down = FVIZ_FALSE;
                else if (event->button == FVIZ_MOUSE_BUTTON_RIGHT) style->right_down = FVIZ_FALSE;
                else return FVIZ_FALSE;
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
                    const FVizQuat yaw = fviz_quat_from_axis_angle(
                        fviz_vec3(0.0f, 1.0f, 0.0f), -(float)dx * style->orbit_sensitivity);
                    const FVizQuat pitch = fviz_quat_from_axis_angle(
                        fviz_vec3(1.0f, 0.0f, 0.0f), -(float)dy * style->orbit_sensitivity);
                    fviz_actor_set_orientation(actor, fviz_quat_normalize(fviz_quat_multiply(
                        yaw, fviz_quat_multiply(pitch, fviz_actor_orientation(actor)))));
                    handled = FVIZ_TRUE;
                }
                else if (style->middle_down != FVIZ_FALSE)
                {
                    const FVizVec3 position = fviz_actor_position(actor);
                    fviz_actor_set_position(actor, fviz_vec3(
                        position.x + (float)dx * style->pan_sensitivity,
                        position.y - (float)dy * style->pan_sensitivity,
                        position.z));
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
                const float factor = event->wheel_delta > 0.0f
                    ? 1.0f / style->dolly_factor : style->dolly_factor;
                if (event->wheel_delta == 0.0f) return FVIZ_FALSE;
                fviz_actor_set_scale(actor, fviz_vec3_scale(fviz_actor_scale(actor), factor));
                return FVIZ_TRUE;
            }
            default:
                return FVIZ_FALSE;
        }
    }
    if (fviz_object_is_type(
            (const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND) == FVIZ_TRUE)
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
                style->rubber_completed = FVIZ_FALSE;
                return FVIZ_TRUE;
            case FVIZ_INTERACTION_MOUSE_MOVE:
                if (style->rubber_active == FVIZ_FALSE) return FVIZ_FALSE;
                style->rubber_end_x = event->x;
                style->rubber_end_y = event->y;
                return FVIZ_TRUE;
            case FVIZ_INTERACTION_MOUSE_BUTTON_UP:
                if (event->button != FVIZ_MOUSE_BUTTON_LEFT || style->rubber_active == FVIZ_FALSE)
                    return FVIZ_FALSE;
                style->rubber_end_x = event->x;
                style->rubber_end_y = event->y;
                style->rubber_active = FVIZ_FALSE;
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
        case FVIZ_INTERACTION_MOUSE_BUTTON_DOWN:
            if (event->button == FVIZ_MOUSE_BUTTON_LEFT) style->left_down = FVIZ_TRUE;
            else if (event->button == FVIZ_MOUSE_BUTTON_MIDDLE) style->middle_down = FVIZ_TRUE;
            else if (event->button == FVIZ_MOUSE_BUTTON_RIGHT) style->right_down = FVIZ_TRUE;
            else return FVIZ_FALSE;
            style->last_x = event->x;
            style->last_y = event->y;
            return FVIZ_TRUE;
        case FVIZ_INTERACTION_MOUSE_BUTTON_UP:
            if (event->button == FVIZ_MOUSE_BUTTON_LEFT) style->left_down = FVIZ_FALSE;
            else if (event->button == FVIZ_MOUSE_BUTTON_MIDDLE) style->middle_down = FVIZ_FALSE;
            else if (event->button == FVIZ_MOUSE_BUTTON_RIGHT) style->right_down = FVIZ_FALSE;
            else return FVIZ_FALSE;
            style->last_x = event->x;
            style->last_y = event->y;
            return FVIZ_TRUE;
        case FVIZ_INTERACTION_MOUSE_MOVE:
        {
            const int dx = event->x - style->last_x;
            const int dy = event->y - style->last_y;
            FVizBool handled = FVIZ_FALSE;
            if (style->left_down == FVIZ_TRUE)
            {
                fviz_camera_orbit(camera,
                    -(float)dx * style->orbit_sensitivity,
                    -(float)dy * style->orbit_sensitivity);
                handled = FVIZ_TRUE;
            }
            else if (style->middle_down == FVIZ_TRUE)
            {
                const float distance = fviz_vec3_length(
                    fviz_vec3_sub(fviz_camera_position(camera), fviz_camera_target(camera)));
                const float scale = distance * style->pan_sensitivity;
                fviz_camera_pan(camera, -(float)dx * scale, (float)dy * scale);
                handled = FVIZ_TRUE;
            }
            else if (style->right_down == FVIZ_TRUE)
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
            fviz_camera_dolly(camera,
                event->wheel_delta > 0.0f ? style->dolly_factor : 1.0f / style->dolly_factor);
            return FVIZ_TRUE;
        case FVIZ_INTERACTION_KEY_DOWN:
        {
            const int key = event->key >= 0 && event->key <= 255
                ? toupper((unsigned char)event->key) : event->key;
            if (key == 'F' || key == 'R')
            {
                fviz_renderer_fit_camera(renderer, 1.2f);
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
