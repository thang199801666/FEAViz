#include <math.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizInteractionEvent event_at(FVizInteractionEventType type, FVizMouseButton button, int x, int y)
{
    FVizInteractionEvent event;
    (void)memset(&event, 0, sizeof(event));
    event.type = type;
    event.button = button;
    event.x = x;
    event.y = y;
    return event;
}

static int test_trackball_camera(void)
{
    FVizRenderer* renderer = NULL;
    FVizInteractorStyle* style = NULL;
    FVizCamera* camera;
    FVizInteractionEvent event;
    FVizVec3 before;
    FVizVec3 after;
    float distance_before;
    float distance_after;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_interactor_style_trackball_camera_create(&style) == FVIZ_OK);
    CHECK(fviz_object_type_id((const FVizObject*)style) == FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_CAMERA);
    fviz_interactor_style_set_orbit_sensitivity(style, 0.01f);
    fviz_interactor_style_set_pan_sensitivity(style, 0.002f);
    fviz_interactor_style_set_dolly_factor(style, 0.8f);
    CHECK(fabsf(fviz_interactor_style_orbit_sensitivity(style) - 0.01f) < 1.0e-6f);
    CHECK(fabsf(fviz_interactor_style_pan_sensitivity(style) - 0.002f) < 1.0e-6f);
    CHECK(fabsf(fviz_interactor_style_dolly_factor(style) - 0.8f) < 1.0e-6f);

    camera = fviz_renderer_camera(renderer);
    before = fviz_camera_position(camera);
    event = event_at(FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_LEFT, 10, 10);
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    event = event_at(FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE, 30, 20);
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    event = event_at(FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_LEFT, 30, 20);
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    after = fviz_camera_position(camera);
    CHECK(fviz_vec3_length(fviz_vec3_sub(after, before)) > 0.01f);

    distance_before = fviz_vec3_length(fviz_vec3_sub(after, fviz_camera_target(camera)));
    event = event_at(FVIZ_INTERACTION_MOUSE_WHEEL, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
    event.wheel_delta = 1.0f;
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    distance_after = fviz_vec3_length(
        fviz_vec3_sub(fviz_camera_position(camera), fviz_camera_target(camera)));
    CHECK(distance_after < distance_before);

    distance_before = distance_after;
    event = event_at(FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_RIGHT, 30, 20);
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    event = event_at(FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE, 30, 30);
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    event = event_at(FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_RIGHT, 30, 30);
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    distance_after = fviz_vec3_length(
        fviz_vec3_sub(fviz_camera_position(camera), fviz_camera_target(camera)));
    CHECK(distance_after > distance_before);

    fviz_release(style);
    fviz_release(renderer);
    return 0;
}

static int test_keyboard_style(void)
{
    FVizRenderer* renderer = NULL;
    FVizInteractorStyle* style = NULL;
    FVizActor* actor = NULL;
    FVizInteractionEvent event;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_interactor_style_trackball_camera_create(&style) == FVIZ_OK);
    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) == FVIZ_OK);
    event = event_at(FVIZ_INTERACTION_KEY_DOWN, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
    event.key = 'w';
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    CHECK(fviz_actor_wireframe(actor) == FVIZ_TRUE);
    event.key = 's';
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    CHECK(fviz_actor_wireframe(actor) == FVIZ_FALSE);
    event.key = 'X';
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_FALSE);
    fviz_release(actor);
    fviz_release(style);
    fviz_release(renderer);
    return 0;
}

static int test_rubber_band_style(void)
{
    FVizRenderer* renderer = NULL;
    FVizInteractorStyle* style = NULL;
    FVizInteractionEvent event;
    int minimum_x;
    int minimum_y;
    int maximum_x;
    int maximum_y;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_interactor_style_rubber_band_create(&style) == FVIZ_OK);
    CHECK(fviz_object_is_type(
        (const FVizObject*)style, FVIZ_TYPE_INTERACTOR_STYLE) == FVIZ_TRUE);
    event = event_at(FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_LEFT, 40, 30);
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    CHECK(fviz_interactor_style_rubber_band_active(style) == FVIZ_TRUE);
    event = event_at(FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE, 10, 60);
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    event = event_at(FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_LEFT, 10, 60);
    CHECK(fviz_interactor_style_process_event(style, renderer, &event) == FVIZ_TRUE);
    CHECK(fviz_interactor_style_rubber_band_active(style) == FVIZ_FALSE);
    CHECK(fviz_interactor_style_rubber_band_completed(style) == FVIZ_TRUE);
    CHECK(fviz_interactor_style_rubber_band_rectangle(
        style, &minimum_x, &minimum_y, &maximum_x, &maximum_y) == FVIZ_OK);
    CHECK(minimum_x == 10 && minimum_y == 30 && maximum_x == 40 && maximum_y == 60);
    fviz_interactor_style_rubber_band_reset(style);
    CHECK(fviz_interactor_style_rubber_band_completed(style) == FVIZ_FALSE);
    fviz_release(style);
    fviz_release(renderer);
    return 0;
}

int main(void)
{
    CHECK(test_trackball_camera() == 0);
    CHECK(test_keyboard_style() == 0);
    CHECK(test_rubber_band_style() == 0);
    return 0;
}
