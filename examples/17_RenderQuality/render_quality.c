#include <stdio.h>

#include <FViz/FViz.h>

static void release_all(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    FVizLight* light,
    FVizActor* actor,
    FVizSphereSource* sphere)
{
    fviz_release(window);
    fviz_release(renderer);
    fviz_release(light);
    fviz_release(actor);
    fviz_release(sphere);
}

int main(void)
{
    FVizSphereSource* sphere = NULL;
    FVizActor* actor = NULL;
    FVizRenderer* renderer = NULL;
    FVizLight* fill_light = NULL;
    FVizRenderWindow* window = NULL;
    FVizRenderWindowOptions options;
    FVizFXAAOptions fxaa;
    FVizRenderCapabilities capabilities;
    FVizRenderStatistics statistics;
    FVizResult result;

    fviz_render_window_options_initialize(&options);
    options.multisamples = 8u;
    options.fxaa = FVIZ_TRUE;
    options.adaptive_antialiasing = FVIZ_FALSE;
    options.swap_interval = 0;

    fviz_fxaa_options_initialize(&fxaa);
    fxaa.relative_threshold = 0.10f;
    fxaa.absolute_threshold = 0.025f;
    fxaa.span_max = 8.0f;

    if (fviz_sphere_source_create(&sphere) != FVIZ_OK ||
        fviz_sphere_source_set_resolution(sphere, 96u, 48u) != FVIZ_OK ||
        fviz_sphere_source_update(sphere) != FVIZ_OK ||
        fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, fviz_sphere_source_output(sphere)) != FVIZ_OK ||
        fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) != FVIZ_OK)
    {
        fprintf(stderr, "FEAViz render-quality scene setup failed: %s\n", fviz_last_error_message());
        release_all(window, renderer, fill_light, actor, sphere);
        return 1;
    }

    fviz_actor_set_color(actor, 0.18f, 0.58f, 0.96f);
    fviz_actor_set_material(actor, 0.16f, 0.78f, 0.45f, 48.0f);
    fviz_actor_set_shading_mode(actor, FVIZ_SHADING_SMOOTH);
    fviz_actor_set_cull_mode(actor, FVIZ_CULL_BACK);
    fviz_renderer_set_background(renderer, 0.025f, 0.035f, 0.055f);
    fviz_renderer_set_background2(renderer, 0.16f, 0.22f, 0.34f);
    fviz_renderer_set_gradient_background(renderer, FVIZ_TRUE);

    if (fviz_light_create(&fill_light) == FVIZ_OK)
    {
        fviz_light_set_type(fill_light, FVIZ_LIGHT_SCENE);
        fviz_light_set_position(fill_light, fviz_vec3(-3.0f, 2.0f, 4.0f));
        fviz_light_set_color(fill_light, 0.72f, 0.82f, 1.0f);
        fviz_light_set_intensity(fill_light, 0.35f);
        (void)fviz_renderer_add_light(renderer, fill_light);
    }
    fviz_renderer_fit_camera(renderer, 1.18f);

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        printf("Render-quality scene validated headlessly; native renderer is unavailable on this platform.\n");
        printf("Requested AA: 8x MSAA + tuned FXAA; lights=%zu\n",
            (size_t)fviz_renderer_light_count(renderer));
        release_all(window, renderer, fill_light, actor, sphere);
        return 0;
    }

    result = fviz_render_window_create_offscreen_with_options(1280, 720, &options, &window);
    if (result != FVIZ_OK ||
        fviz_render_window_set_renderer(window, renderer) != FVIZ_OK ||
        fviz_render_window_set_fxaa_options(window, &fxaa) != FVIZ_OK ||
        fviz_render_window_render(window) != FVIZ_OK)
    {
        fprintf(stderr, "FEAViz quality render failed: %s\n", fviz_last_error_message());
        release_all(window, renderer, fill_light, actor, sphere);
        return 1;
    }

    fviz_render_window_get_capabilities(window, &capabilities);
    fviz_render_window_get_statistics(window, &statistics);
    printf("Renderer: OpenGL %u.%u | modern=%d | requested MSAA=8 | actual MSAA=%u | FXAA=%d\n",
        capabilities.gl_major, capabilities.gl_minor,
        (int)capabilities.modern_pipeline, capabilities.sample_count,
        (int)statistics.fxaa_applied);
    printf("Frame: %.3f ms render + %.3f ms present | draws=%llu | triangles=%llu | uploads=%llu\n",
        statistics.render_seconds * 1000.0,
        statistics.present_seconds * 1000.0,
        (unsigned long long)statistics.draw_calls,
        (unsigned long long)statistics.triangles,
        (unsigned long long)statistics.gpu_uploads);

    release_all(window, renderer, fill_light, actor, sphere);
    return 0;
}
