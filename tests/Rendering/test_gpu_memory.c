#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizPolyData* data = NULL;
    FVizActor* actor = NULL;
    FVizRenderWindow* window = NULL;
    FVizRenderer* renderer;
    FVizScene* scene;
    FVizRenderCapabilities capabilities;
    FVizRenderStatistics statistics;
    FVizGPUMemoryOptions options;
    const FVizVec3 points[3] = {
        {-1.0f, -1.0f, 0.0f},
        {1.0f, -1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };

    if (fviz_render_window_supported() == FVIZ_FALSE) return 0;
    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points(data, points, 3u, NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, 0u, 1u, 2u) == FVIZ_OK);
    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    CHECK(fviz_actor_set_poly_data(actor, data) == FVIZ_OK);
    CHECK(fviz_render_window_create_offscreen(64, 64, &window) == FVIZ_OK);
    fviz_render_window_get_capabilities(window, &capabilities);
    if (capabilities.modern_pipeline == FVIZ_FALSE)
    {
        fviz_release(window); fviz_release(actor); fviz_release(data);
        return 0;
    }
    renderer = fviz_render_window_renderer(window);
    scene = fviz_renderer_scene(renderer);
    fviz_gpu_memory_options_initialize(&options);
    options.unused_resource_retention_frames = 2u;
    CHECK(fviz_render_window_set_gpu_memory_options(window, &options) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(scene, actor) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 1u);
    CHECK(statistics.resident_mesh_gpu_bytes > 0u);
    CHECK(statistics.resident_geometry_gpu_bytes > 0u);
    CHECK(statistics.resident_geometry_gpu_bytes +
        statistics.resident_attribute_gpu_bytes +
        statistics.resident_instance_gpu_bytes == statistics.resident_mesh_gpu_bytes);

    CHECK(fviz_scene_remove_actor(scene, actor) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 1u);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 1u);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 0u);
    CHECK(statistics.gpu_resource_evictions == 1u);

    options.unused_resource_retention_frames = 0u;
    CHECK(fviz_render_window_set_gpu_memory_options(window, &options) == FVIZ_OK);
    fviz_mapper_set_gpu_residency_pinned(fviz_actor_mapper(actor), FVIZ_TRUE);
    CHECK(fviz_scene_add_actor(scene, actor) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    CHECK(fviz_scene_remove_actor(scene, actor) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 1u);
    CHECK(statistics.pinned_gpu_resources == 1u);
    fviz_mapper_set_gpu_residency_pinned(fviz_actor_mapper(actor), FVIZ_FALSE);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 0u);

    options.mesh_byte_budget = 1u;
    options.unused_resource_retention_frames = 100u;
    CHECK(fviz_render_window_set_gpu_memory_options(window, &options) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(scene, actor) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.gpu_mesh_byte_budget == 1u);
    CHECK(statistics.gpu_mesh_budget_exceeded == FVIZ_TRUE);
    CHECK(fviz_scene_remove_actor(scene, actor) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 0u);
    CHECK(statistics.gpu_mesh_budget_exceeded == FVIZ_FALSE);
    CHECK(statistics.gpu_resource_evictions == 1u);

    options.mesh_byte_budget = 0u;
    CHECK(fviz_render_window_set_gpu_memory_options(window, &options) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(scene, actor) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    CHECK(fviz_render_window_release_gpu_mesh_resources(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 0u);
    CHECK(statistics.resident_mesh_gpu_bytes == 0u);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.resident_actor_resources == 1u);

    fviz_release(window);
    fviz_release(actor);
    fviz_release(data);
    return 0;
}
