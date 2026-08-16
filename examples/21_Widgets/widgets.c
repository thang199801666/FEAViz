#include <stdio.h>

#include <FViz/FViz.h>

static void cleanup(
    FVizRenderWindow* window,
    FVizProbeWidget* probe,
    FVizSectionCutWidget* section,
    FVizAngleWidget* angle,
    FVizDistanceWidget* distance,
    FVizLineWidget* line,
    FVizHandleWidget* handle,
    FVizBoxWidget* box,
    FVizPlaneWidget* plane,
    FVizActor* actor,
    FVizSphereSource* sphere,
    FVizRenderer* renderer)
{
    fviz_release(window);
    fviz_release(probe);
    fviz_release(section);
    fviz_release(angle);
    fviz_release(distance);
    fviz_release(line);
    fviz_release(handle);
    fviz_release(box);
    fviz_release(plane);
    fviz_release(actor);
    fviz_release(sphere);
    fviz_release(renderer);
}

int main(void)
{
    FVizRenderer* renderer = NULL;
    FVizSphereSource* sphere = NULL;
    FVizActor* actor = NULL;
    FVizPlaneWidget* plane = NULL;
    FVizBoxWidget* box = NULL;
    FVizHandleWidget* handle = NULL;
    FVizLineWidget* line = NULL;
    FVizDistanceWidget* distance = NULL;
    FVizAngleWidget* angle = NULL;
    FVizSectionCutWidget* section = NULL;
    FVizProbeWidget* probe = NULL;
    FVizRenderWindow* window = NULL;
    FVizRenderWindowOptions options;
    FVizBounds bounds;

    if (fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_sphere_source_create(&sphere) != FVIZ_OK ||
        fviz_sphere_source_set_resolution(sphere, 32u, 16u) != FVIZ_OK ||
        fviz_sphere_source_update(sphere) != FVIZ_OK ||
        fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, fviz_sphere_source_output(sphere)) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) != FVIZ_OK ||
        fviz_plane_widget_create(NULL, renderer, &plane) != FVIZ_OK ||
        fviz_box_widget_create(NULL, renderer, &box) != FVIZ_OK ||
        fviz_handle_widget_create(NULL, renderer, &handle) != FVIZ_OK ||
        fviz_line_widget_create(NULL, renderer, &line) != FVIZ_OK ||
        fviz_distance_widget_create(NULL, renderer, &distance) != FVIZ_OK ||
        fviz_angle_widget_create(NULL, renderer, &angle) != FVIZ_OK ||
        fviz_section_cut_widget_create(NULL, renderer, &section) != FVIZ_OK ||
        fviz_probe_widget_create(NULL, renderer, &probe) != FVIZ_OK)
    {
        fprintf(stderr, "Widget setup failed: %s\n", fviz_last_error_message());
        cleanup(window, probe, section, angle, distance, line, handle, box, plane, actor, sphere, renderer);
        return 1;
    }

    fviz_actor_set_color(actor, 0.20f, 0.52f, 0.88f);
    fviz_plane_widget_set_origin(plane, fviz_vec3(-1.4f, 0.0f, 0.0f));
    (void)fviz_plane_widget_set_normal(plane, fviz_vec3(1.0f, 0.2f, 0.0f));
    fviz_plane_widget_set_size(plane, 1.0f);

    bounds.min = fviz_vec3(0.75f, -0.5f, -0.5f);
    bounds.max = fviz_vec3(1.75f, 0.5f, 0.5f);
    bounds.valid = FVIZ_TRUE;
    (void)fviz_box_widget_set_bounds(box, &bounds);
    fviz_handle_widget_set_position(handle, fviz_vec3(0.0f, 1.25f, 0.0f));
    fviz_line_widget_set_points(line, fviz_vec3(-0.9f, -1.15f, 0.0f), fviz_vec3(0.9f, -1.15f, 0.0f));
    (void)fviz_distance_widget_set_points(distance, fviz_vec3(-0.7f, 0.8f, 0.0f), fviz_vec3(0.7f, 0.8f, 0.0f));
    (void)fviz_angle_widget_set_points(angle,
        fviz_vec3(-0.8f, -0.4f, 0.0f), fviz_vec3(0.0f, -0.4f, 0.0f), fviz_vec3(0.0f, 0.4f, 0.0f));

    fviz_plane_widget_set_origin(fviz_section_cut_widget_plane_widget(section), fviz_vec3(0.0f, 0.0f, 0.0f));
    (void)fviz_plane_widget_set_normal(fviz_section_cut_widget_plane_widget(section), fviz_vec3(1.0f, 0.0f, 0.0f));
    fviz_plane_widget_set_size(fviz_section_cut_widget_plane_widget(section), 1.5f);
    if (fviz_section_cut_widget_add_actor(section, actor) != FVIZ_OK)
    {
        fprintf(stderr, "Section-cut binding failed: %s\n", fviz_last_error_message());
        cleanup(window, probe, section, angle, distance, line, handle, box, plane, actor, sphere, renderer);
        return 2;
    }
    (void)fviz_probe_widget_set_array_name(probe, "S");

    fviz_renderer_set_background(renderer, 0.025f, 0.035f, 0.055f);
    fviz_renderer_set_background2(renderer, 0.10f, 0.15f, 0.24f);
    fviz_renderer_set_gradient_background(renderer, FVIZ_TRUE);
    fviz_renderer_fit_camera(renderer, 1.25f);

    printf("FEAViz widgets: actors=%zu, billboards=%zu, section-targets=%zu, distance=%.3f, angle=%.1f, line=%.3f\n",
        (size_t)fviz_scene_actor_count(fviz_renderer_scene(renderer)),
        (size_t)fviz_renderer_billboard_text_actor_3d_count(renderer),
        (size_t)fviz_section_cut_widget_actor_count(section),
        (double)fviz_distance_widget_distance(distance),
        (double)fviz_angle_widget_angle_degrees(angle),
        (double)fviz_line_widget_length(line));

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        printf("Headless widget-framework validation complete; native renderer is unavailable on this platform.\n");
        cleanup(window, probe, section, angle, distance, line, handle, box, plane, actor, sphere, renderer);
        return 0;
    }

    fviz_render_window_options_initialize(&options);
    options.multisamples = 4u;
    options.fxaa = FVIZ_TRUE;
    if (fviz_render_window_create_offscreen_with_options(1280, 720, &options, &window) != FVIZ_OK ||
        fviz_render_window_set_renderer(window, renderer) != FVIZ_OK ||
        fviz_render_window_render(window) != FVIZ_OK)
    {
        fprintf(stderr, "Widget render failed: %s\n", fviz_last_error_message());
        cleanup(window, probe, section, angle, distance, line, handle, box, plane, actor, sphere, renderer);
        return 3;
    }

    cleanup(window, probe, section, angle, distance, line, handle, box, plane, actor, sphere, renderer);
    return 0;
}
