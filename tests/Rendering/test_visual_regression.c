#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static uint64_t image_hash(const uint8_t* pixels, FVizSize size)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    FVizSize i;
    for (i = 0u; i < size; ++i)
    {
        hash ^= pixels[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int main(void)
{
    const int width = 128;
    const int height = 96;
    const FVizSize byte_count = (FVizSize)width * (FVizSize)height * 4u;
    FVizRenderWindow* window = NULL;
    FVizRenderer* renderer;
    FVizRenderCapabilities capabilities;
    FVizPolyData* data = NULL;
    FVizActor* actor = NULL;
    FVizDataArray* colors = NULL;
    FVizArraySelection selection;
    uint8_t tuples[3][4] = {
        {255u, 0u, 0u, 255u},
        {0u, 255u, 0u, 180u},
        {0u, 0u, 255u, 255u}
    };
    uint8_t* baseline;
    uint8_t* repeated;
    uint8_t* clipped;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint64_t baseline_hash;
    FVizSize i;
    FVizSize changed = 0u;

    if (fviz_render_window_supported() == FVIZ_FALSE) return 0;
    CHECK(fviz_render_window_create_offscreen(width, height, &window) == FVIZ_OK);
    fviz_render_window_get_capabilities(window, &capabilities);
    if (capabilities.modern_pipeline == FVIZ_FALSE)
    {
        fviz_release(window);
        return 0;
    }
    renderer = fviz_render_window_renderer(window);
    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(-1.0f, -0.8f, 0.0f), &a) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(1.0f, -0.8f, 0.0f), &b) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0.0f, 1.0f, 0.0f), &c) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, a, b, c) == FVIZ_OK);
    CHECK(fviz_poly_data_compute_normals(data) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT8, 4u, &colors) == FVIZ_OK);
    for (i = 0u; i < 3u; ++i) CHECK(fviz_data_array_append_tuple(colors, tuples[i]) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(data), "RGBA", colors) == FVIZ_OK);
    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    CHECK(fviz_actor_set_poly_data(actor, data) == FVIZ_OK);
    fviz_actor_set_opacity(actor, 0.8f);
    fviz_actor_set_edge_visibility(actor, FVIZ_TRUE);
    fviz_actor_set_edge_color(actor, 1.0f, 1.0f, 1.0f);
    fviz_actor_set_line_width(actor, 2.0f);
    fviz_array_selection_initialize(&selection);
    selection.name = "RGBA";
    selection.component_mode = FVIZ_COMPONENT_COLOR;
    CHECK(fviz_mapper_set_array_selection(fviz_actor_mapper(actor), &selection) == FVIZ_OK);
    fviz_mapper_set_scalar_visibility(fviz_actor_mapper(actor), FVIZ_TRUE);
    CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) == FVIZ_OK);
    fviz_renderer_fit_camera(renderer, 1.2f);
    baseline = (uint8_t*)fviz_alloc(byte_count);
    repeated = (uint8_t*)fviz_alloc(byte_count);
    clipped = (uint8_t*)fviz_alloc(byte_count);
    CHECK(baseline != NULL && repeated != NULL && clipped != NULL);

    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    CHECK(fviz_render_window_read_rgba8(window, baseline, byte_count) == FVIZ_OK);
    baseline_hash = image_hash(baseline, byte_count);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    CHECK(fviz_render_window_read_rgba8(window, repeated, byte_count) == FVIZ_OK);
    CHECK(image_hash(repeated, byte_count) == baseline_hash);

    CHECK(fviz_mapper_add_clipping_plane(
        fviz_actor_mapper(actor),
        fviz_plane_from_point_normal(
            fviz_vec3(0.25f, 0.0f, 0.0f), fviz_vec3(1.0f, 0.0f, 0.0f))) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    CHECK(fviz_render_window_read_rgba8(window, clipped, byte_count) == FVIZ_OK);
    for (i = 0u; i < byte_count; ++i)
        if (baseline[i] != clipped[i]) ++changed;
    CHECK(changed > 100u);
    CHECK(fviz_render_window_write_ppm(window, "FVizVisualRegression.ppm") == FVIZ_OK);
    CHECK(remove("FVizVisualRegression.ppm") == 0);

    fviz_free(clipped);
    fviz_free(repeated);
    fviz_free(baseline);
    fviz_release(actor);
    fviz_release(colors);
    fviz_release(data);
    fviz_release(window);
    return 0;
}
