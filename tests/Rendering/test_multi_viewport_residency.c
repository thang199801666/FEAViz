#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

int main(void)
{
    static const float viewports[4][4] = {
        {0.0f, 0.5f, 0.5f, 1.0f}, {0.5f, 0.5f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 1.0f, 0.5f}
    };
    FVizRenderWindow* window = NULL;
    FVizRenderer* renderers[4] = {NULL, NULL, NULL, NULL};
    FVizActor* actors[4] = {NULL, NULL, NULL, NULL};
    FVizPolyData* data = NULL;
    FVizMapper* mapper = NULL;
    FVizRenderCapabilities capabilities;
    FVizRenderStatistics statistics;
    FVizSize i;
    if (fviz_render_window_supported() == FVIZ_FALSE) return 0;
    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(-0.7f, -0.6f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3( 0.7f, -0.6f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3( 0.0f,  0.7f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, 0u, 1u, 2u) == FVIZ_OK);
    CHECK(fviz_mapper_create(&mapper) == FVIZ_OK);
    CHECK(fviz_mapper_set_poly_data(mapper, data) == FVIZ_OK);
    CHECK(fviz_render_window_create_offscreen(160, 120, &window) == FVIZ_OK);
    fviz_render_window_get_capabilities(window, &capabilities);
    if (capabilities.modern_pipeline == FVIZ_FALSE)
    {
        fviz_release(window); fviz_release(mapper); fviz_release(data);
        return 0;
    }
    renderers[0] = fviz_render_window_renderer(window);
    CHECK(renderers[0] != NULL);
    for (i = 1u; i < 4u; ++i)
    {
        CHECK(fviz_renderer_create(&renderers[i]) == FVIZ_OK);
        CHECK(fviz_render_window_add_renderer(window, renderers[i]) == FVIZ_OK);
    }
    for (i = 0u; i < 4u; ++i)
    {
        CHECK(fviz_renderer_set_viewport(renderers[i],
            viewports[i][0], viewports[i][1], viewports[i][2], viewports[i][3]) == FVIZ_OK);
        CHECK(fviz_actor_create(&actors[i]) == FVIZ_OK);
        CHECK(fviz_actor_set_mapper(actors[i], mapper) == FVIZ_OK);
        CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderers[i]), actors[i]) == FVIZ_OK);
    }
    CHECK(fviz_render_window_renderer_count(window) == 4u);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 1u);
    CHECK(statistics.resident_mesh_gpu_bytes > 0u);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 1u);
    CHECK(statistics.gpu_uploads == 0u);
    fviz_camera_orbit(fviz_renderer_camera(renderers[0]), 0.25f, 0.0f);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 1u);
    CHECK(statistics.gpu_uploads == 0u);
    CHECK(fviz_render_window_remove_renderer(window, renderers[2]) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 1u);
    CHECK(fviz_render_window_renderer_count(window) == 3u);
    for (i = 0u; i < 4u; ++i) fviz_release(actors[i]);
    for (i = 1u; i < 4u; ++i) fviz_release(renderers[i]);
    fviz_release(window);
    fviz_release(mapper);
    fviz_release(data);
    return 0;
}
