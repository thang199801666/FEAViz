#include <stdio.h>

#include <FViz/FViz.h>

static FVizResult create_demo_cube(FVizPolyData** out_data)
{
    static const FVizVec3 points[8] = {
        {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},
        { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
        {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f},
        { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}
    };
    static const uint32_t triangles[12][3] = {
        {0,2,1},{0,3,2}, {4,5,6},{4,6,7},
        {0,1,5},{0,5,4}, {3,7,6},{3,6,2},
        {0,4,7},{0,7,3}, {1,2,6},{1,6,5}
    };
    FVizPolyData* data;
    FVizSize i;
    if (fviz_poly_data_create(&data) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < FVIZ_ARRAY_COUNT(points); ++i)
    {
        if (fviz_poly_data_add_point(data, points[i], NULL) != FVIZ_OK)
        {
            fviz_release(data);
            return fviz_last_error_code();
        }
    }
    for (i = 0u; i < FVIZ_ARRAY_COUNT(triangles); ++i)
    {
        if (fviz_poly_data_add_triangle(data, triangles[i][0], triangles[i][1], triangles[i][2]) != FVIZ_OK)
        {
            fviz_release(data);
            return fviz_last_error_code();
        }
    }
    if (fviz_poly_data_compute_normals(data) != FVIZ_OK)
    {
        fviz_release(data);
        return fviz_last_error_code();
    }
    *out_data = data;
    return FVIZ_OK;
}

int main(int argc, char** argv)
{
    FVizPolyData* data = NULL;
    FVizActor* actor = NULL;
    FVizRenderer* renderer = NULL;
    FVizRenderWindow* window = NULL;
    FVizResult result;

    if (argc > 1)
    {
        result = fviz_mesh_read(argv[1], &data);
        if (result != FVIZ_OK)
        {
            fprintf(stderr, "FEAViz: failed to load '%s': %s (%s)\n",
                argv[1], fviz_result_string(result), fviz_last_error_message());
            return 1;
        }
    }
    else
    {
        result = create_demo_cube(&data);
        if (result != FVIZ_OK) return 1;
    }

    printf("FEAViz %s | points=%zu triangles=%zu\n",
        fviz_version_string(),
        (size_t)fviz_poly_data_point_count(data),
        (size_t)fviz_poly_data_triangle_count(data));

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        fprintf(stderr, "Native viewer backend is not available on this platform yet.\n");
        fviz_release(data);
        return 2;
    }

    if (fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, data) != FVIZ_OK ||
        fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) != FVIZ_OK ||
        fviz_render_window_create(1280, 800, "FEAViz 0.1 - 3D Viewer", &window) != FVIZ_OK ||
        fviz_render_window_set_renderer(window, renderer) != FVIZ_OK)
    {
        fprintf(stderr, "FEAViz viewer setup failed: %s\n", fviz_last_error_message());
        fviz_release(window);
        fviz_release(renderer);
        fviz_release(actor);
        fviz_release(data);
        return 1;
    }

    fviz_actor_set_color(actor, 0.25f, 0.62f, 0.95f);
    fviz_renderer_set_background(renderer, 0.08f, 0.09f, 0.12f);
    fviz_renderer_fit_camera(renderer, 1.25f);

    printf("Controls: LMB orbit | MMB pan | wheel zoom | F fit | W wireframe | Esc close\n");
    result = fviz_render_window_run(window);
    if (result != FVIZ_OK)
    {
        fprintf(stderr, "FEAViz render loop failed: %s\n", fviz_last_error_message());
    }

    fviz_release(window);
    fviz_release(renderer);
    fviz_release(actor);
    fviz_release(data);
    return result == FVIZ_OK ? 0 : 1;
}
