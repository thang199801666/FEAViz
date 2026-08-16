#include <math.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizInteractionEvent make_event(FVizInteractionEventType type, FVizMouseButton button, int x, int y)
{
    FVizInteractionEvent event;
    (void)memset(&event, 0, sizeof(event));
    event.type = type;
    event.button = button;
    event.x = x;
    event.y = y;
    event.width = 800;
    event.height = 600;
    event.content_scale = 1.0f;
    return event;
}

static int test_mapper_clip_plane_ids(void)
{
    FVizMapper* mapper = NULL;
    FVizClipPlaneId first = FVIZ_CLIP_PLANE_ID_INVALID;
    FVizClipPlaneId second = FVIZ_CLIP_PLANE_ID_INVALID;
    FVizClipPlaneId third = FVIZ_CLIP_PLANE_ID_INVALID;
    FVizPlane plane;
    CHECK(fviz_mapper_create(&mapper) == FVIZ_OK);
    CHECK(fviz_mapper_add_clipping_plane_with_id(mapper,
        fviz_plane_from_point_normal(fviz_vec3(0,0,0), fviz_vec3(1,0,0)), &first) == FVIZ_OK);
    CHECK(fviz_mapper_add_clipping_plane_with_id(mapper,
        fviz_plane_from_point_normal(fviz_vec3(0,1,0), fviz_vec3(0,1,0)), &second) == FVIZ_OK);
    CHECK(first != FVIZ_CLIP_PLANE_ID_INVALID && second != FVIZ_CLIP_PLANE_ID_INVALID && first != second);
    CHECK(fviz_mapper_clipping_plane_count(mapper) == 2u);
    CHECK(fviz_mapper_clipping_plane_id(mapper, 0u) == first);
    CHECK(fviz_mapper_clipping_plane_id(mapper, 1u) == second);
    fviz_mapper_set_scalar_visibility(mapper, FVIZ_TRUE);
    CHECK(fviz_mapper_add_clipping_plane_with_id(mapper,
        fviz_plane_from_point_normal(fviz_vec3(0,0,1), fviz_vec3(0,0,1)), &third) == FVIZ_OK);
    CHECK(third != first && third != second);
    CHECK(fviz_mapper_remove_clipping_plane(mapper, third) == FVIZ_OK);
    CHECK(fviz_mapper_update_clipping_plane(mapper, second,
        fviz_plane_from_point_normal(fviz_vec3(0,2,0), fviz_vec3(0,1,0))) == FVIZ_OK);
    CHECK(fviz_mapper_clipping_plane(mapper, 1u, &plane) == FVIZ_OK);
    CHECK(fabsf(fviz_plane_distance_to_point(plane, fviz_vec3(0,2,0))) < 1.0e-5f);
    CHECK(fviz_mapper_remove_clipping_plane(mapper, first) == FVIZ_OK);
    CHECK(fviz_mapper_clipping_plane_count(mapper) == 1u);
    CHECK(fviz_mapper_clipping_plane_id(mapper, 0u) == second);
    fviz_release(mapper);
    return 0;
}

static int test_representation_and_projection(void)
{
    FVizRenderer* renderer = NULL;
    FVizWidgetRepresentation* representation = NULL;
    FVizActor* actor = NULL;
    FVizTextActor2D* text2d = NULL;
    FVizBillboardTextActor3D* billboard = NULL;
    FVizLabelSet3D* labels = NULL;
    FVizVec3 display;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_widget_representation_create(renderer, &representation) == FVIZ_OK);
    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    CHECK(fviz_widget_representation_add_actor(representation, actor) == FVIZ_OK);
    CHECK(fviz_widget_representation_actor_count(representation) == 1u);
    CHECK(fviz_widget_representation_actor_at(representation, 0u) == actor);
    CHECK(fviz_text_actor_2d_create(&text2d) == FVIZ_OK);
    CHECK(fviz_billboard_text_actor_3d_create(&billboard) == FVIZ_OK);
    CHECK(fviz_label_set_3d_create(&labels) == FVIZ_OK);
    CHECK(fviz_widget_representation_add_text_actor_2d(representation, text2d) == FVIZ_OK);
    CHECK(fviz_widget_representation_add_billboard_text_actor_3d(representation, billboard) == FVIZ_OK);
    CHECK(fviz_widget_representation_add_label_set_3d(representation, labels) == FVIZ_OK);
    CHECK(fviz_renderer_text_actor_2d_count(renderer) == 1u);
    CHECK(fviz_renderer_billboard_text_actor_3d_count(renderer) == 1u);
    CHECK(fviz_renderer_label_set_3d_count(renderer) == 1u);
    fviz_widget_representation_set_visible(representation, FVIZ_FALSE);
    CHECK(fviz_actor_is_visible(actor) == FVIZ_FALSE);
    CHECK(fviz_text_actor_2d_is_visible(text2d) == FVIZ_FALSE);
    CHECK(fviz_billboard_text_actor_3d_is_visible(billboard) == FVIZ_FALSE);
    CHECK(fviz_label_set_3d_visible(labels) == FVIZ_FALSE);
    fviz_widget_representation_set_visible(representation, FVIZ_TRUE);
    CHECK(fviz_actor_is_visible(actor) == FVIZ_TRUE);
    CHECK(fviz_text_actor_2d_is_visible(text2d) == FVIZ_TRUE);
    CHECK(fviz_billboard_text_actor_3d_is_visible(billboard) == FVIZ_TRUE);
    CHECK(fviz_label_set_3d_visible(labels) == FVIZ_TRUE);
    CHECK(fviz_renderer_world_to_display(renderer, fviz_vec3(0,0,0), 800, 600, &display) == FVIZ_OK);
    CHECK(fabsf(display.x - 400.0f) < 2.0f);
    CHECK(fabsf(display.y - 300.0f) < 2.0f);
    fviz_release(actor);
    fviz_release(text2d);
    fviz_release(billboard);
    fviz_release(labels);
    fviz_release(representation);
    CHECK(fviz_renderer_text_actor_2d_count(renderer) == 0u);
    CHECK(fviz_renderer_billboard_text_actor_3d_count(renderer) == 0u);
    CHECK(fviz_renderer_label_set_3d_count(renderer) == 0u);
    fviz_release(renderer);
    return 0;
}

static int test_manipulator(void)
{
    FVizRenderer* renderer = NULL;
    FVizWidgetManipulator* manipulator = NULL;
    FVizInteractionEvent down = make_event(FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_LEFT, 400, 300);
    FVizInteractionEvent move = make_event(FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE, 440, 300);
    FVizVec3 world, delta;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_widget_manipulator_create(&manipulator) == FVIZ_OK);
    fviz_widget_manipulator_set_mode(manipulator, FVIZ_WIDGET_MANIPULATOR_VIEW_PLANE);
    CHECK(fviz_widget_manipulator_begin(manipulator, renderer, &down, fviz_vec3(0,0,0)) == FVIZ_OK);
    CHECK(fviz_widget_manipulator_active(manipulator) == FVIZ_TRUE);
    CHECK(fviz_widget_manipulator_update(manipulator, renderer, &move, &world, &delta) == FVIZ_OK);
    {
        FVizCamera* camera = fviz_renderer_camera(renderer);
        const FVizVec3 view_normal = fviz_vec3_normalize(
            fviz_vec3_sub(fviz_camera_target(camera), fviz_camera_position(camera)));
        CHECK(fviz_vec3_length(delta) > 0.0f);
        CHECK(fabsf(fviz_vec3_dot(delta, view_normal)) < 1.0e-4f);
    }
    fviz_widget_manipulator_end(manipulator);
    CHECK(fviz_widget_manipulator_active(manipulator) == FVIZ_FALSE);
    fviz_release(manipulator);
    fviz_release(renderer);
    return 0;
}

static int test_handle_widget(void)
{
    FVizRenderer* renderer = NULL;
    FVizHandleWidget* handle = NULL;
    FVizInteractionEvent event;
    FVizVec3 before;
    FVizVec3 after;
    FVizVec3 view_normal;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_handle_widget_create(NULL, renderer, &handle) == FVIZ_OK);
    fviz_handle_widget_set_position(handle, fviz_vec3(0,0,0));
    fviz_handle_widget_set_size(handle, 13.0f);
    CHECK(fabsf(fviz_handle_widget_size(handle) - 13.0f) < 1.0e-6f);
    before = fviz_handle_widget_position(handle);
    event = make_event(FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_LEFT, 400, 300);
    CHECK(fviz_widget_process_event(fviz_handle_widget_widget(handle), &event) == FVIZ_TRUE);
    CHECK(fviz_widget_state(fviz_handle_widget_widget(handle)) == FVIZ_WIDGET_STATE_ACTIVE);
    event = make_event(FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE, 430, 300);
    CHECK(fviz_widget_process_event(fviz_handle_widget_widget(handle), &event) == FVIZ_TRUE);
    after = fviz_handle_widget_position(handle);
    view_normal = fviz_vec3_normalize(fviz_vec3_sub(
        fviz_camera_target(fviz_renderer_camera(renderer)),
        fviz_camera_position(fviz_renderer_camera(renderer))));
    CHECK(fviz_vec3_length(fviz_vec3_sub(after, before)) > 0.0f);
    CHECK(fabsf(fviz_vec3_dot(fviz_vec3_sub(after, before), view_normal)) < 1.0e-4f);
    event = make_event(FVIZ_INTERACTION_KEY_DOWN, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
    event.key = FVIZ_KEY_ESCAPE;
    CHECK(fviz_widget_process_event(fviz_handle_widget_widget(handle), &event) == FVIZ_TRUE);
    after = fviz_handle_widget_position(handle);
    CHECK(fviz_vec3_length(fviz_vec3_sub(after, before)) < 1.0e-5f);
    CHECK(fviz_widget_state(fviz_handle_widget_widget(handle)) == FVIZ_WIDGET_STATE_START);
    fviz_release(handle);
    fviz_release(renderer);
    return 0;
}

static int test_line_widget(void)
{
    FVizRenderer* renderer = NULL;
    FVizLineWidget* line = NULL;
    FVizInteractionEvent event;
    FVizVec3 p1;
    FVizVec3 p2;
    FVizVec3 display;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_line_widget_create(NULL, renderer, &line) == FVIZ_OK);
    fviz_line_widget_set_points(line, fviz_vec3(-0.5f,0,0), fviz_vec3(0.5f,0,0));
    CHECK(fabsf(fviz_line_widget_length(line) - 1.0f) < 1.0e-5f);
    CHECK(fviz_renderer_world_to_display(renderer, fviz_vec3(-0.5f,0,0), 800, 600, &display) == FVIZ_OK);
    event = make_event(FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_LEFT, (int)display.x, (int)display.y);
    CHECK(fviz_widget_process_event(fviz_line_widget_widget(line), &event) == FVIZ_TRUE);
    event = make_event(FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE, (int)display.x + 30, (int)display.y);
    CHECK(fviz_widget_process_event(fviz_line_widget_widget(line), &event) == FVIZ_TRUE);
    fviz_line_widget_get_points(line, &p1, &p2);
    CHECK(fviz_vec3_length(fviz_vec3_sub(p1, fviz_vec3(-0.5f,0,0))) > 1.0e-5f);
    CHECK(fviz_vec3_length(fviz_vec3_sub(p2, fviz_vec3(0.5f,0,0))) < 1.0e-5f);
    event = make_event(FVIZ_INTERACTION_KEY_DOWN, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
    event.key = FVIZ_KEY_ESCAPE;
    CHECK(fviz_widget_process_event(fviz_line_widget_widget(line), &event) == FVIZ_TRUE);
    fviz_line_widget_get_points(line, &p1, &p2);
    CHECK(fviz_vec3_length(fviz_vec3_sub(p1, fviz_vec3(-0.5f,0,0))) < 1.0e-5f);
    fviz_release(line);
    fviz_release(renderer);
    return 0;
}

static int test_plane_and_box_widgets(void)
{
    FVizRenderer* renderer = NULL;
    FVizPlaneWidget* plane = NULL;
    FVizBoxWidget* box = NULL;
    FVizInteractionEvent event;
    FVizVec3 before, after;
    FVizBounds bounds;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_plane_widget_create(NULL, renderer, &plane) == FVIZ_OK);
    CHECK(fviz_plane_widget_set_normal(plane, fviz_vec3(0,0,1)) == FVIZ_OK);
    fviz_plane_widget_set_size(plane, 2.0f);
    before = fviz_plane_widget_origin(plane);
    event = make_event(FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_LEFT, 400, 300);
    CHECK(fviz_widget_process_event(fviz_plane_widget_widget(plane), &event) == FVIZ_TRUE);
    CHECK(fviz_widget_state(fviz_plane_widget_widget(plane)) == FVIZ_WIDGET_STATE_ACTIVE);
    event = make_event(FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE, 440, 300);
    CHECK(fviz_widget_process_event(fviz_plane_widget_widget(plane), &event) == FVIZ_TRUE);
    after = fviz_plane_widget_origin(plane);
    CHECK(after.x > before.x);
    event = make_event(FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_LEFT, 440, 300);
    CHECK(fviz_widget_process_event(fviz_plane_widget_widget(plane), &event) == FVIZ_TRUE);
    CHECK(fviz_widget_state(fviz_plane_widget_widget(plane)) == FVIZ_WIDGET_STATE_START);

    CHECK(fviz_box_widget_create(NULL, renderer, &box) == FVIZ_OK);
    bounds = fviz_box_widget_bounds(box);
    {
        FVizVec3 face_display;
        const float original_max_x = bounds.max.x;
        CHECK(fviz_renderer_world_to_display(renderer,
            fviz_vec3(bounds.max.x, 0.0f, 0.0f), 800, 600, &face_display) == FVIZ_OK);
        event = make_event(FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_LEFT,
            (int)face_display.x, (int)face_display.y);
        CHECK(fviz_widget_process_event(fviz_box_widget_widget(box), &event) == FVIZ_TRUE);
        event = make_event(FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE,
            (int)face_display.x + 25, (int)face_display.y);
        CHECK(fviz_widget_process_event(fviz_box_widget_widget(box), &event) == FVIZ_TRUE);
        bounds = fviz_box_widget_bounds(box);
        CHECK(fabsf(bounds.max.x - original_max_x) > 1.0e-5f);
        CHECK(fabsf(bounds.min.x + 0.5f) < 1.0e-5f);
        event = make_event(FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_LEFT,
            (int)face_display.x + 25, (int)face_display.y);
        CHECK(fviz_widget_process_event(fviz_box_widget_widget(box), &event) == FVIZ_TRUE);
    }
    CHECK(bounds.valid == FVIZ_TRUE);
    CHECK(fabsf(bounds.min.x + 0.5f) < 1.0e-6f && fabsf(bounds.max.z - 0.5f) < 1.0e-6f);
    bounds.min = fviz_vec3(-1,-2,-3);
    bounds.max = fviz_vec3(4,5,6);
    bounds.valid = FVIZ_TRUE;
    CHECK(fviz_box_widget_set_bounds(box, &bounds) == FVIZ_OK);
    bounds = fviz_box_widget_bounds(box);
    CHECK(fabsf(bounds.max.y - 5.0f) < 1.0e-6f);
    fviz_release(box);
    fviz_release(plane);
    fviz_release(renderer);
    return 0;
}

static int test_measurements(void)
{
    FVizRenderer* renderer = NULL;
    FVizDistanceWidget* distance = NULL;
    FVizAngleWidget* angle = NULL;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_distance_widget_create(NULL, renderer, &distance) == FVIZ_OK);
    CHECK(fviz_distance_widget_set_points(distance, fviz_vec3(0,0,0), fviz_vec3(3,4,0)) == FVIZ_OK);
    CHECK(fviz_distance_widget_completed(distance) == FVIZ_TRUE);
    CHECK(fabsf(fviz_distance_widget_distance(distance) - 5.0f) < 1.0e-5f);
    CHECK(fviz_billboard_text_actor_3d_is_visible(fviz_distance_widget_label(distance)) == FVIZ_TRUE);
    CHECK(fviz_widget_set_enabled(fviz_distance_widget_widget(distance), FVIZ_FALSE) == FVIZ_OK);
    CHECK(fviz_billboard_text_actor_3d_is_visible(fviz_distance_widget_label(distance)) == FVIZ_FALSE);
    CHECK(fviz_distance_widget_set_points(distance, fviz_vec3(0,0,0), fviz_vec3(6,8,0)) == FVIZ_OK);
    CHECK(fviz_billboard_text_actor_3d_is_visible(fviz_distance_widget_label(distance)) == FVIZ_FALSE);
    CHECK(fviz_widget_set_enabled(fviz_distance_widget_widget(distance), FVIZ_TRUE) == FVIZ_OK);
    CHECK(fviz_billboard_text_actor_3d_is_visible(fviz_distance_widget_label(distance)) == FVIZ_TRUE);
    CHECK(fabsf(fviz_distance_widget_distance(distance) - 10.0f) < 1.0e-5f);
    CHECK(fviz_widget_set_enabled(fviz_distance_widget_widget(distance), FVIZ_FALSE) == FVIZ_OK);
    fviz_distance_widget_reset(distance);
    CHECK(fviz_widget_set_enabled(fviz_distance_widget_widget(distance), FVIZ_TRUE) == FVIZ_OK);
    CHECK(fviz_billboard_text_actor_3d_is_visible(fviz_distance_widget_label(distance)) == FVIZ_FALSE);
    CHECK(fviz_distance_widget_completed(distance) == FVIZ_FALSE);

    CHECK(fviz_angle_widget_create(NULL, renderer, &angle) == FVIZ_OK);
    CHECK(fviz_angle_widget_set_points(angle,
        fviz_vec3(1,0,0), fviz_vec3(0,0,0), fviz_vec3(0,1,0)) == FVIZ_OK);
    CHECK(fviz_angle_widget_completed(angle) == FVIZ_TRUE);
    CHECK(fabsf(fviz_angle_widget_angle_degrees(angle) - 90.0f) < 1.0e-4f);
    CHECK(fviz_billboard_text_actor_3d_is_visible(fviz_angle_widget_label(angle)) == FVIZ_TRUE);
    fviz_release(angle);
    fviz_release(distance);
    fviz_release(renderer);
    return 0;
}

static int test_probe_lifecycle(void)
{
    FVizRenderer* renderer = NULL;
    FVizProbeWidget* probe = NULL;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_probe_widget_create(NULL, renderer, &probe) == FVIZ_OK);
    CHECK(fviz_renderer_billboard_text_actor_3d_count(renderer) == 1u);
    CHECK(fviz_billboard_text_actor_3d_is_visible(fviz_probe_widget_label(probe)) == FVIZ_FALSE);
    CHECK(fviz_probe_widget_set_array_name(probe, "S, Mises") == FVIZ_OK);
    CHECK(strcmp(fviz_probe_widget_array_name(probe), "S, Mises") == 0);
    CHECK(fviz_widget_set_enabled(fviz_probe_widget_widget(probe), FVIZ_FALSE) == FVIZ_OK);
    fviz_probe_widget_clear(probe);
    CHECK(fviz_widget_set_enabled(fviz_probe_widget_widget(probe), FVIZ_TRUE) == FVIZ_OK);
    CHECK(fviz_billboard_text_actor_3d_is_visible(fviz_probe_widget_label(probe)) == FVIZ_FALSE);
    fviz_release(probe);
    CHECK(fviz_renderer_billboard_text_actor_3d_count(renderer) == 0u);
    fviz_release(renderer);
    return 0;
}

static int test_section_cut_preserves_external_planes(void)
{
    FVizRenderer* renderer = NULL;
    FVizSectionCutWidget* section = NULL;
    FVizActor* actor = NULL;
    FVizMapper* mapper;
    FVizClipPlaneId external = FVIZ_CLIP_PLANE_ID_INVALID;
    FVizPlane plane;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    mapper = fviz_actor_mapper(actor);
    CHECK(mapper != NULL);
    CHECK(fviz_mapper_add_clipping_plane_with_id(mapper,
        fviz_plane_from_point_normal(fviz_vec3(1,0,0), fviz_vec3(1,0,0)), &external) == FVIZ_OK);
    CHECK(fviz_section_cut_widget_create(NULL, renderer, &section) == FVIZ_OK);
    CHECK(fviz_section_cut_widget_add_actor(section, actor) == FVIZ_OK);
    CHECK(fviz_mapper_clipping_plane_count(mapper) == 2u);
    fviz_plane_widget_set_origin(fviz_section_cut_widget_plane_widget(section), fviz_vec3(0,2,0));
    CHECK(fviz_plane_widget_set_normal(fviz_section_cut_widget_plane_widget(section), fviz_vec3(0,1,0)) == FVIZ_OK);
    CHECK(fviz_section_cut_widget_update(section) == FVIZ_OK);
    CHECK(fviz_mapper_clipping_plane(mapper, 0u, &plane) == FVIZ_OK);
    CHECK(fviz_mapper_clipping_plane_id(mapper, 0u) == external);
    CHECK(fabsf(fviz_plane_distance_to_point(plane, fviz_vec3(1,0,0))) < 1.0e-5f);
    fviz_release(section);
    CHECK(fviz_mapper_clipping_plane_count(mapper) == 1u);
    CHECK(fviz_mapper_clipping_plane_id(mapper, 0u) == external);
    fviz_release(actor);
    fviz_release(renderer);
    return 0;
}

int main(void)
{
    CHECK(test_mapper_clip_plane_ids() == 0);
    CHECK(test_representation_and_projection() == 0);
    CHECK(test_manipulator() == 0);
    CHECK(test_handle_widget() == 0);
    CHECK(test_line_widget() == 0);
    CHECK(test_plane_and_box_widgets() == 0);
    CHECK(test_measurements() == 0);
    CHECK(test_probe_lifecycle() == 0);
    CHECK(test_section_cut_preserves_external_planes() == 0);
    return 0;
}
