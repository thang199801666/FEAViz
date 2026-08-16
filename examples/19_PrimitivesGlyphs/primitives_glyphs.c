#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

static void cleanup(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    FVizActor* glyph_actor,
    FVizActor* primitive_actor,
    FVizGlyphMapper* glyph_mapper,
    FVizArrowSource* arrow,
    FVizPolyData* primitives)
{
    fviz_release(window);
    fviz_release(renderer);
    fviz_release(glyph_actor);
    fviz_release(primitive_actor);
    fviz_release(glyph_mapper);
    fviz_release(arrow);
    fviz_release(primitives);
}

int main(void)
{
    FVizPolyData* primitives = NULL;
    FVizArrowSource* arrow = NULL;
    FVizGlyphMapper* glyph_mapper = NULL;
    FVizActor* primitive_actor = NULL;
    FVizActor* glyph_actor = NULL;
    FVizRenderer* renderer = NULL;
    FVizRenderWindow* window = NULL;
    FVizRenderWindowOptions options;
    FVizVec3 points[8];
    FVizGlyphInstance instances[256];
    uint32_t line_ids[8] = {0u,1u, 2u,3u, 4u,5u, 6u,7u};
    uint32_t vertex_ids[8] = {0u,1u,2u,3u,4u,5u,6u,7u};
    unsigned int i;

    for (i = 0u; i < 8u; ++i)
    {
        const float a = (float)(6.2831853071795864769 * (double)i / 8.0);
        points[i] = fviz_vec3(cosf(a) * 2.0f, sinf(a) * 2.0f, 0.0f);
    }
    if (fviz_poly_data_create(&primitives) != FVIZ_OK ||
        fviz_poly_data_add_points(primitives, points, 8u, NULL) != FVIZ_OK ||
        fviz_poly_data_add_lines(primitives, line_ids, 4u) != FVIZ_OK ||
        fviz_poly_data_add_poly_vertex(primitives, 8u, vertex_ids) != FVIZ_OK ||
        fviz_arrow_source_create(&arrow) != FVIZ_OK ||
        fviz_arrow_source_set_radial_resolution(arrow, 16u) != FVIZ_OK ||
        fviz_arrow_source_update(arrow) != FVIZ_OK ||
        fviz_glyph_mapper_create(&glyph_mapper) != FVIZ_OK ||
        fviz_glyph_mapper_set_source_poly_data(glyph_mapper, fviz_arrow_source_output(arrow)) != FVIZ_OK)
    {
        fprintf(stderr, "FEAViz primitive/glyph setup failed: %s\n", fviz_last_error_message());
        cleanup(window, renderer, glyph_actor, primitive_actor, glyph_mapper, arrow, primitives);
        return 1;
    }

    for (i = 0u; i < 256u; ++i)
    {
        const float u = (float)i / 255.0f;
        const float angle = u * 6.28318530718f * 5.0f;
        const float radius = 0.8f + 2.8f * u;
        fviz_glyph_instance_initialize(&instances[i]);
        instances[i].position = fviz_vec3(radius * cosf(angle), radius * sinf(angle), 2.0f * u - 1.0f);
        instances[i].orientation = fviz_quat_from_axis_angle(fviz_vec3(0.0f, 0.0f, 1.0f), angle + 1.57079632679f);
        instances[i].scale = fviz_vec3(0.22f + 0.18f * u, 0.22f + 0.18f * u, 0.22f + 0.18f * u);
        instances[i].color[0] = u;
        instances[i].color[1] = 0.25f + 0.65f * (1.0f - u);
        instances[i].color[2] = 1.0f - u;
        instances[i].color[3] = 1.0f;
    }
    if (fviz_glyph_mapper_add_instances(glyph_mapper, instances, 256u) != FVIZ_OK ||
        fviz_actor_create(&primitive_actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(primitive_actor, primitives) != FVIZ_OK ||
        fviz_actor_create(&glyph_actor) != FVIZ_OK ||
        fviz_actor_set_glyph_mapper(glyph_actor, glyph_mapper) != FVIZ_OK ||
        fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), primitive_actor) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), glyph_actor) != FVIZ_OK)
    {
        fprintf(stderr, "FEAViz primitive/glyph scene failed: %s\n", fviz_last_error_message());
        cleanup(window, renderer, glyph_actor, primitive_actor, glyph_mapper, arrow, primitives);
        return 2;
    }

    fviz_actor_set_edge_visibility(primitive_actor, FVIZ_TRUE);
    fviz_actor_set_line_width(primitive_actor, 5.0f);
    fviz_actor_set_line_cap(primitive_actor, FVIZ_LINE_CAP_ROUND);
    fviz_actor_set_line_dash(primitive_actor, 14.0f, 7.0f, 0.0f);
    fviz_actor_set_edge_color(primitive_actor, 0.15f, 0.8f, 0.95f);
    fviz_actor_set_point_visibility(primitive_actor, FVIZ_TRUE);
    fviz_actor_set_point_size(primitive_actor, 16.0f);
    fviz_actor_set_point_shape(primitive_actor, FVIZ_POINT_SPHERE_IMPOSTOR);
    fviz_actor_set_point_color(primitive_actor, 1.0f, 0.6f, 0.1f);
    fviz_actor_set_material(glyph_actor, 0.16f, 0.78f, 0.30f, 32.0f);
    fviz_actor_set_cull_mode(glyph_actor, FVIZ_CULL_BACK);
    fviz_renderer_set_background(renderer, 0.025f, 0.035f, 0.055f);
    fviz_renderer_set_background2(renderer, 0.12f, 0.18f, 0.28f);
    fviz_renderer_set_gradient_background(renderer, FVIZ_TRUE);
    fviz_renderer_fit_camera(renderer, 1.15f);

    printf("FEAViz primitives: lines=%zu, vertex cells=%zu; glyphs=%zu, source triangles=%zu\n",
        (size_t)fviz_poly_data_line_count(primitives),
        (size_t)fviz_poly_data_vert_cell_count(primitives),
        (size_t)fviz_glyph_mapper_instance_count(glyph_mapper),
        (size_t)fviz_poly_data_triangle_count(fviz_arrow_source_output(arrow)));

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        printf("Headless validation complete; native GPU instancing is unavailable on this platform.\n");
        cleanup(window, renderer, glyph_actor, primitive_actor, glyph_mapper, arrow, primitives);
        return 0;
    }

    fviz_render_window_options_initialize(&options);
    options.multisamples = 4u;
    options.fxaa = FVIZ_TRUE;
    if (fviz_render_window_create_offscreen_with_options(1280, 720, &options, &window) != FVIZ_OK ||
        fviz_render_window_set_renderer(window, renderer) != FVIZ_OK ||
        fviz_render_window_render(window) != FVIZ_OK)
    {
        fprintf(stderr, "FEAViz primitive/glyph render failed: %s\n", fviz_last_error_message());
        cleanup(window, renderer, glyph_actor, primitive_actor, glyph_mapper, arrow, primitives);
        return 3;
    }

    cleanup(window, renderer, glyph_actor, primitive_actor, glyph_mapper, arrow, primitives);
    return 0;
}
