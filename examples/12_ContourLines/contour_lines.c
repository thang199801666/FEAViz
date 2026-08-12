#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

static FVizResult build_wave(FVizPolyData** out_data, FVizDataArray** out_scalars)
{
    FVizPolyData* data = NULL;
    FVizDataArray* scalars = NULL;
    FVizSize y;
    FVizSize x;
    const FVizSize n = 64u;
    if (fviz_poly_data_create(&data) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalars) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_resize(scalars, n * n) != FVIZ_OK) return fviz_last_error_code();
    for (y = 0u; y < n; ++y)
    {
        for (x = 0u; x < n; ++x)
        {
            const float px = (float)x / (float)(n - 1u) * 6.0f - 3.0f;
            const float py = (float)y / (float)(n - 1u) * 6.0f - 3.0f;
            const float scalar = sinf(px) * cosf(py);
            const FVizSize index = y * n + x;
            if (fviz_poly_data_add_point(data, fviz_vec3(px, py, 0.0f), NULL) != FVIZ_OK) return fviz_last_error_code();
            if (fviz_data_array_set_tuple(scalars, index, &scalar) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    for (y = 0u; y + 1u < n; ++y)
    {
        for (x = 0u; x + 1u < n; ++x)
        {
            const uint32_t a = (uint32_t)(y * n + x);
            const uint32_t b = (uint32_t)(a + 1u);
            const uint32_t c = (uint32_t)((y + 1u) * n + x);
            const uint32_t d = (uint32_t)(c + 1u);
            if (fviz_poly_data_add_triangle(data, a, c, b) != FVIZ_OK ||
                fviz_poly_data_add_triangle(data, b, c, d) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_poly_data_compute_normals(data) != FVIZ_OK ||
        fviz_poly_data_set_scalars(data, scalars) != FVIZ_OK) return fviz_last_error_code();
    *out_data = data;
    *out_scalars = scalars;
    return FVIZ_OK;
}

int main(void)
{
    FVizPolyData* field = NULL;
    FVizDataArray* scalars = NULL;
    FVizContourFilter* contour = NULL;
    FVizPolyData* lines = NULL;
    FVizActor* surface_actor = NULL;
    FVizActor* line_actor = NULL;
    FVizMapper* mapper = NULL;
    FVizScalarLegend* legend = NULL;
    FVizRenderer* renderer = NULL;
    FVizRenderWindow* window = NULL;
    FVizResult result;
    float levels[9];
    FVizSize i;

    result = build_wave(&field, &scalars);
    if (result != FVIZ_OK) { fprintf(stderr, "wave setup failed: %s\n", fviz_last_error_message()); return 1; }

    for (i = 0u; i < 9u; ++i)
    {
        levels[i] = -0.8f + (float)i * 0.2f;
    }
    if (fviz_contour_filter_create("", levels, 9u, &contour) != FVIZ_OK ||
        fviz_contour_filter_set_input(contour, field) != FVIZ_OK ||
        fviz_contour_filter_update(contour) != FVIZ_OK)
    {
        fprintf(stderr, "contour failed: %s\n", fviz_last_error_message());
        fviz_release(scalars);
        fviz_release(field);
        return 1;
    }
    lines = fviz_contour_filter_output(contour);
    printf("FEAViz %s | Contour lines: %zu isoline segments\n",
        fviz_version_string(),
        (size_t)fviz_poly_data_line_count(lines));

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        fprintf(stderr, "Native viewer backend is not available on this platform yet.\n");
        fviz_release(contour);
        fviz_release(scalars);
        fviz_release(field);
        return 2;
    }

    if (fviz_actor_create(&surface_actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(surface_actor, field) != FVIZ_OK ||
        fviz_actor_create(&line_actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(line_actor, lines) != FVIZ_OK ||
        fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), surface_actor) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), line_actor) != FVIZ_OK ||
        fviz_render_window_create(1280, 800, "FEAViz 0.4.3 - Contour Lines", &window) != FVIZ_OK ||
        fviz_render_window_set_renderer(window, renderer) != FVIZ_OK)
    {
        fprintf(stderr, "viewer setup failed: %s\n", fviz_last_error_message());
        fviz_release(window);
        fviz_release(renderer);
        fviz_release(line_actor);
        fviz_release(surface_actor);
        fviz_release(contour);
        fviz_release(scalars);
        fviz_release(field);
        return 1;
    }

    mapper = fviz_actor_mapper(surface_actor);
    fviz_mapper_set_scalar_visibility(mapper, FVIZ_TRUE);
    fviz_mapper_set_scalar_range(mapper, -1.0f, 1.0f);
    fviz_actor_set_position(line_actor, fviz_vec3(0.0f, 0.0f, 0.02f));
    if (fviz_scalar_legend_create(&legend) == FVIZ_OK)
    {
        fviz_scalar_legend_set_range(legend, -1.0f, 1.0f);
        fviz_scalar_legend_set_title(legend, "sin(x)cos(y)");
        fviz_renderer_set_scalar_legend(renderer, legend);
        fviz_release(legend);
    }
    fviz_renderer_set_background(renderer, 0.08f, 0.09f, 0.12f);
    fviz_renderer_fit_camera(renderer, 1.4f);

    printf("Controls: LMB orbit | MMB pan | wheel zoom | F fit | W wireframe | Esc close\n");
    result = fviz_render_window_run(window);
    if (result != FVIZ_OK) fprintf(stderr, "render loop failed: %s\n", fviz_last_error_message());

    fviz_release(window);
    fviz_release(renderer);
    fviz_release(line_actor);
    fviz_release(surface_actor);
    fviz_release(contour);
    fviz_release(scalars);
    fviz_release(field);
    return result == FVIZ_OK ? 0 : 1;
}
