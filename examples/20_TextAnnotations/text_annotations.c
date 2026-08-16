#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

static void cleanup(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    FVizScalarLegend* legend,
    FVizLabelSet3D* labels,
    FVizBillboardTextActor3D* probe_label,
    FVizTextActor2D* title,
    FVizActor* actor,
    FVizSphereSource* sphere)
{
    fviz_release(window);
    fviz_release(renderer);
    fviz_release(legend);
    fviz_release(labels);
    fviz_release(probe_label);
    fviz_release(title);
    fviz_release(actor);
    fviz_release(sphere);
}

int main(void)
{
    FVizSphereSource* sphere = NULL;
    FVizActor* actor = NULL;
    FVizRenderer* renderer = NULL;
    FVizTextActor2D* title = NULL;
    FVizBillboardTextActor3D* probe_label = NULL;
    FVizLabelSet3D* labels = NULL;
    FVizScalarLegend* legend = NULL;
    FVizRenderWindow* window = NULL;
    FVizRenderWindowOptions options;
    unsigned int i;

    if (fviz_sphere_source_create(&sphere) != FVIZ_OK ||
        fviz_sphere_source_set_resolution(sphere, 48u, 24u) != FVIZ_OK ||
        fviz_sphere_source_update(sphere) != FVIZ_OK ||
        fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, fviz_sphere_source_output(sphere)) != FVIZ_OK ||
        fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) != FVIZ_OK ||
        fviz_text_actor_2d_create(&title) != FVIZ_OK ||
        fviz_text_actor_2d_set_text(title, "FEAViz 0.23 | Stress result") != FVIZ_OK ||
        fviz_billboard_text_actor_3d_create(&probe_label) != FVIZ_OK ||
        fviz_billboard_text_actor_3d_set_text(probe_label, "Peak: 248.6 MPa") != FVIZ_OK ||
        fviz_label_set_3d_create(&labels) != FVIZ_OK ||
        fviz_scalar_legend_create(&legend) != FVIZ_OK)
    {
        fprintf(stderr, "Text/annotation setup failed: %s\n", fviz_last_error_message());
        cleanup(window, renderer, legend, labels, probe_label, title, actor, sphere);
        return 1;
    }

    fviz_actor_set_color(actor, 0.20f, 0.55f, 0.92f);
    fviz_actor_set_material(actor, 0.16f, 0.78f, 0.32f, 40.0f);
    fviz_renderer_set_background(renderer, 0.025f, 0.035f, 0.055f);
    fviz_renderer_set_background2(renderer, 0.10f, 0.15f, 0.24f);
    fviz_renderer_set_gradient_background(renderer, FVIZ_TRUE);

    fviz_text_actor_2d_set_position(title, 24.0f, 680.0f);
    fviz_text_property_set_font_size(fviz_text_actor_2d_text_property(title), 18.0f);
    fviz_text_property_set_shadow(fviz_text_actor_2d_text_property(title), FVIZ_TRUE, 1.0f, -1.0f, 0.55f);
    fviz_billboard_text_actor_3d_set_world_position(probe_label, fviz_vec3(0.0f, 0.0f, 1.0f));
    fviz_billboard_text_actor_3d_set_pixel_offset(probe_label, 10.0f, 10.0f);
    fviz_text_property_set_color(fviz_billboard_text_actor_3d_text_property(probe_label), 1.0f, 0.85f, 0.25f, 1.0f);
    fviz_text_property_set_background(fviz_billboard_text_actor_3d_text_property(probe_label), 0.02f, 0.02f, 0.03f, 0.70f);

    if (fviz_label_set_3d_reserve(labels, 24u) != FVIZ_OK)
    {
        cleanup(window, renderer, legend, labels, probe_label, title, actor, sphere);
        return 2;
    }
    fviz_text_property_set_font_size(fviz_label_set_3d_text_property(labels), 11.0f);
    fviz_text_property_set_color(fviz_label_set_3d_text_property(labels), 0.86f, 0.92f, 1.0f, 0.90f);
    fviz_label_set_3d_set_pixel_offset(labels, 5.0f, 4.0f);
    for (i = 0u; i < 24u; ++i)
    {
        char label[32];
        const float a = (float)(6.2831853071795864769 * (double)i / 24.0);
        FVizVec3 p = fviz_vec3(cosf(a), sinf(a), 0.15f * sinf(3.0f * a));
        (void)snprintf(label, sizeof(label), "N%u", i + 1u);
        if (fviz_label_set_3d_add(labels, p, label, NULL) != FVIZ_OK)
        {
            cleanup(window, renderer, legend, labels, probe_label, title, actor, sphere);
            return 3;
        }
    }

    fviz_scalar_legend_set_title(legend, "S, Mises");
    fviz_scalar_legend_set_units(legend, "MPa");
    fviz_scalar_legend_set_range(legend, 0.0f, 250.0f);
    fviz_scalar_legend_set_tick_count(legend, 6u);
    if (fviz_scalar_legend_set_label_format(legend, "%.1f") != FVIZ_OK ||
        fviz_renderer_add_text_actor_2d(renderer, title) != FVIZ_OK ||
        fviz_renderer_add_billboard_text_actor_3d(renderer, probe_label) != FVIZ_OK ||
        fviz_renderer_add_label_set_3d(renderer, labels) != FVIZ_OK)
    {
        cleanup(window, renderer, legend, labels, probe_label, title, actor, sphere);
        return 4;
    }
    fviz_renderer_set_scalar_legend(renderer, legend);
    fviz_renderer_fit_camera(renderer, 1.18f);

    printf("FEAViz text annotations: 2D=%zu, billboard=%zu, label-sets=%zu, labels=%zu, legend ticks=%u\n",
        (size_t)fviz_renderer_text_actor_2d_count(renderer),
        (size_t)fviz_renderer_billboard_text_actor_3d_count(renderer),
        (size_t)fviz_renderer_label_set_3d_count(renderer),
        (size_t)fviz_label_set_3d_count(labels),
        fviz_scalar_legend_tick_count(legend));

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        printf("Headless text/annotation validation complete; native text renderer is unavailable on this platform.\n");
        cleanup(window, renderer, legend, labels, probe_label, title, actor, sphere);
        return 0;
    }

    fviz_render_window_options_initialize(&options);
    options.multisamples = 4u;
    options.fxaa = FVIZ_TRUE;
    if (fviz_render_window_create_offscreen_with_options(1280, 720, &options, &window) != FVIZ_OK ||
        fviz_render_window_set_renderer(window, renderer) != FVIZ_OK ||
        fviz_render_window_render(window) != FVIZ_OK)
    {
        fprintf(stderr, "Text/annotation render failed: %s\n", fviz_last_error_message());
        cleanup(window, renderer, legend, labels, probe_label, title, actor, sphere);
        return 5;
    }

    cleanup(window, renderer, legend, labels, probe_label, title, actor, sphere);
    return 0;
}
