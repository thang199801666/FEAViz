#include <stdio.h>
#include <stdlib.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { \
    (void)fprintf(stderr, "transparency check failed at line %d: %s\n", __LINE__, #expr); \
    return __LINE__; \
} } while (0)

static FVizResult create_triangle_actor(
    float red, float green, float blue, float opacity, float z,
    FVizActor** out_actor)
{
    FVizPolyData* data = NULL;
    FVizActor* actor = NULL;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    FVizResult result;
    if (out_actor == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_actor = NULL;
    result = fviz_poly_data_create(&data);
    if (result != FVIZ_OK) return result;
    if (fviz_poly_data_add_point(data, fviz_vec3(-1.0f, -0.8f, 0.0f), &a) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(1.0f, -0.8f, 0.0f), &b) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(0.0f, 1.0f, 0.0f), &c) != FVIZ_OK ||
        fviz_poly_data_add_triangle(data, a, b, c) != FVIZ_OK ||
        fviz_poly_data_compute_normals(data) != FVIZ_OK ||
        fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, data) != FVIZ_OK)
    {
        fviz_release(actor);
        fviz_release(data);
        return fviz_last_error_code();
    }
    fviz_actor_set_color(actor, red, green, blue);
    fviz_actor_set_opacity(actor, opacity);
    fviz_actor_set_position(actor, fviz_vec3(0.0f, 0.0f, z));
    *out_actor = actor;
    fviz_release(data);
    return FVIZ_OK;
}

int main(void)
{
    const int width = 96;
    const int height = 72;
    const FVizSize bytes = (FVizSize)width * (FVizSize)height * 4u;
    FVizRenderWindow* window = NULL;
    FVizRenderer* renderer;
    FVizScene* scene;
    FVizActor* red = NULL;
    FVizActor* blue = NULL;
    FVizRenderCapabilities capabilities;
    FVizRenderStatistics statistics;
    uint8_t* first = NULL;
    uint8_t* second = NULL;
    FVizSize i;
    unsigned max_difference = 0u;
    FVizSize colored_pixels = 0u;

    if (fviz_render_window_supported() == FVIZ_FALSE) return 0;
    CHECK(fviz_render_window_create_offscreen(width, height, &window) == FVIZ_OK);
    fviz_render_window_set_fxaa(window, FVIZ_FALSE);
    renderer = fviz_render_window_renderer(window);
    scene = fviz_renderer_scene(renderer);
    CHECK(create_triangle_actor(1.0f, 0.05f, 0.05f, 0.55f, 0.0f, &red) == FVIZ_OK);
    CHECK(create_triangle_actor(0.05f, 0.15f, 1.0f, 0.45f, 0.15f, &blue) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(scene, red) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(scene, blue) == FVIZ_OK);
    fviz_renderer_set_background(renderer, 0.0f, 0.0f, 0.0f);
    fviz_renderer_fit_camera(renderer, 1.25f);
    fviz_render_window_get_capabilities(window, &capabilities);
    /* Depth peeling is now implemented on the GL backend; on backends that
     * expose the capability the mode is applied, otherwise it falls back. */

    first = (uint8_t*)fviz_alloc(bytes);
    second = (uint8_t*)fviz_alloc(bytes);
    CHECK(first != NULL && second != NULL);
    if (capabilities.weighted_oit_supported != FVIZ_FALSE)
    {
        fviz_renderer_set_transparency_mode(renderer, FVIZ_TRANSPARENCY_WEIGHTED_BLENDED);
        CHECK(fviz_render_window_render(window) == FVIZ_OK);
        CHECK(fviz_render_window_read_rgba8(window, first, bytes) == FVIZ_OK);
        fviz_render_window_get_statistics(window, &statistics);
        CHECK(statistics.transparency_mode_requested == FVIZ_TRANSPARENCY_WEIGHTED_BLENDED);
        CHECK(statistics.transparency_mode_applied == FVIZ_TRANSPARENCY_WEIGHTED_BLENDED);

        CHECK(fviz_scene_remove_actor(scene, red) == FVIZ_OK);
        CHECK(fviz_scene_remove_actor(scene, blue) == FVIZ_OK);
        CHECK(fviz_scene_add_actor(scene, blue) == FVIZ_OK);
        CHECK(fviz_scene_add_actor(scene, red) == FVIZ_OK);
        CHECK(fviz_render_window_render(window) == FVIZ_OK);
        CHECK(fviz_render_window_read_rgba8(window, second, bytes) == FVIZ_OK);
        for (i = 0u; i < bytes; ++i)
        {
            const unsigned difference = first[i] > second[i]
                ? (unsigned)(first[i] - second[i]) : (unsigned)(second[i] - first[i]);
            if (difference > max_difference) max_difference = difference;
        }
        for (i = 0u; i < bytes; i += 4u)
            if (first[i] > 4u || first[i + 1u] > 4u || first[i + 2u] > 4u) ++colored_pixels;
        CHECK(colored_pixels > 100u);
        CHECK(max_difference <= 2u);
    }

    fviz_renderer_set_transparency_mode(renderer, FVIZ_TRANSPARENCY_DEPTH_PEELING);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.transparency_mode_requested == FVIZ_TRANSPARENCY_DEPTH_PEELING);
    CHECK(statistics.transparency_mode_applied == FVIZ_TRANSPARENCY_DEPTH_PEELING ||
        statistics.transparency_mode_applied == FVIZ_TRANSPARENCY_SORTED);

    fviz_free(second);
    fviz_free(first);
    fviz_release(blue);
    fviz_release(red);
    fviz_release(window);
    return 0;
}
