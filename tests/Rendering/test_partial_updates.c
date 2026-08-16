#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizPolyData* data = NULL;
    FVizDataArray* scalars = NULL;
    FVizActor* actor = NULL;
    FVizRenderWindow* window = NULL;
    FVizRenderer* renderer;
    FVizRenderCapabilities capabilities;
    FVizRenderStatistics statistics;
    const FVizVec3 points[4] = {
        {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}
    };
    const float initial[4] = {0.0f, 0.3f, 0.7f, 1.0f};
    float changed = 0.5f;

    if (fviz_render_window_supported() == FVIZ_FALSE) return 0;
    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points(data, points, 4u, NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, 0u, 1u, 2u) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, 0u, 2u, 3u) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalars) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(scalars, initial, 4u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(data), "value", scalars) == FVIZ_OK);
    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    CHECK(fviz_actor_set_poly_data(actor, data) == FVIZ_OK);
    {
        FVizArraySelection selection;
        fviz_array_selection_initialize(&selection);
        selection.name = "value";
        selection.association = FVIZ_ASSOCIATION_POINTS;
        selection.component_mode = FVIZ_COMPONENT_DIRECT;
        CHECK(fviz_mapper_set_array_selection(fviz_actor_mapper(actor), &selection) == FVIZ_OK);
    }
    fviz_mapper_set_scalar_visibility(fviz_actor_mapper(actor), FVIZ_TRUE);
    fviz_mapper_set_scalar_range(fviz_actor_mapper(actor), 0.0f, 1.0f);
    CHECK(fviz_render_window_create_offscreen(96, 96, &window) == FVIZ_OK);
    fviz_render_window_get_capabilities(window, &capabilities);
    if (capabilities.modern_pipeline == FVIZ_FALSE)
    {
        fviz_release(window); fviz_release(actor); fviz_release(scalars); fviz_release(data);
        return 0;
    }
    renderer = fviz_render_window_renderer(window);
    CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);

    CHECK(fviz_data_array_set_tuple(scalars, 2u, &changed) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.gpu_uploads == 1u);
    CHECK(statistics.gpu_upload_bytes == 4u * sizeof(float));

    CHECK(fviz_poly_data_set_point(data, 1u, fviz_vec3(1.1f, -1.0f, 0.0f)) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.gpu_uploads == 1u);
    CHECK(statistics.gpu_upload_bytes == sizeof(FVizVec3));

    {
        FVizPolyData* glyph_source = NULL;
        FVizGlyphMapper* glyph_mapper = NULL;
        FVizActor* glyph_actor = NULL;
        FVizGlyphInstance instances[4];
        FVizSize i;
        CHECK(fviz_scene_remove_actor(fviz_renderer_scene(renderer), actor) == FVIZ_OK);
        CHECK(fviz_poly_data_create(&glyph_source) == FVIZ_OK);
        CHECK(fviz_poly_data_add_point(glyph_source, fviz_vec3(0.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
        CHECK(fviz_poly_data_add_point(glyph_source, fviz_vec3(0.2f, 0.0f, 0.0f), NULL) == FVIZ_OK);
        CHECK(fviz_poly_data_add_point(glyph_source, fviz_vec3(0.0f, 0.2f, 0.0f), NULL) == FVIZ_OK);
        CHECK(fviz_poly_data_add_triangle(glyph_source, 0u, 1u, 2u) == FVIZ_OK);
        CHECK(fviz_poly_data_compute_normals(glyph_source) == FVIZ_OK);
        CHECK(fviz_glyph_mapper_create(&glyph_mapper) == FVIZ_OK);
        CHECK(fviz_glyph_mapper_set_source_poly_data(glyph_mapper, glyph_source) == FVIZ_OK);
        for (i = 0u; i < 4u; ++i)
        {
            fviz_glyph_instance_initialize(&instances[i]);
            instances[i].position = fviz_vec3((float)i * 0.4f, 0.0f, 0.0f);
        }
        CHECK(fviz_glyph_mapper_add_instances(glyph_mapper, instances, 4u) == FVIZ_OK);
        CHECK(fviz_actor_create(&glyph_actor) == FVIZ_OK);
        CHECK(fviz_actor_set_glyph_mapper(glyph_actor, glyph_mapper) == FVIZ_OK);
        CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer), glyph_actor) == FVIZ_OK);
        CHECK(fviz_render_window_render(window) == FVIZ_OK);
        instances[2].position.y = 0.25f;
        CHECK(fviz_glyph_mapper_set_instance(glyph_mapper, 2u, &instances[2]) == FVIZ_OK);
        CHECK(fviz_render_window_render(window) == FVIZ_OK);
        fviz_render_window_get_statistics(window, &statistics);
        CHECK(statistics.gpu_uploads == 1u);
        CHECK(statistics.gpu_upload_bytes == 20u * sizeof(float));
        fviz_release(glyph_actor);
        fviz_release(glyph_mapper);
        fviz_release(glyph_source);
    }

    fviz_release(window);
    fviz_release(actor);
    fviz_release(scalars);
    fviz_release(data);
    return 0;
}
