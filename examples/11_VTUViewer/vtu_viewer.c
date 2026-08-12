#include <stdio.h>

#include <FViz/FViz.h>

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "assets/testdata/hex.vtu";
    FVizUnstructuredGrid* grid = NULL;
    FVizPolyData* surface = NULL;
    FVizActor* actor = NULL;
    FVizMapper* mapper = NULL;
    FVizScalarLegend* legend = NULL;
    FVizRenderer* renderer = NULL;
    FVizRenderWindow* window = NULL;
    FVizResult result;

    result = fviz_vtu_read(path, &grid);
    if (result != FVIZ_OK)
    {
        fprintf(stderr, "FEAViz: failed to read '%s': %s (%s)\n",
            path, fviz_result_string(result), fviz_last_error_message());
        return 1;
    }
    if (fviz_unstructured_grid_extract_surface_scalars(grid, &surface) != FVIZ_OK)
    {
        fprintf(stderr, "surface extraction failed: %s\n", fviz_last_error_message());
        fviz_release(grid);
        return 1;
    }
    printf("FEAViz %s | VTU '%s': points=%zu cells=%zu surface_tris=%zu\n",
        fviz_version_string(),
        path,
        (size_t)fviz_unstructured_grid_point_count(grid),
        (size_t)fviz_unstructured_grid_cell_count(grid),
        (size_t)fviz_poly_data_triangle_count(surface));

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        fprintf(stderr, "Native viewer backend is not available on this platform yet.\n");
        fviz_release(surface);
        fviz_release(grid);
        return 2;
    }

    if (fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, surface) != FVIZ_OK ||
        fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) != FVIZ_OK ||
        fviz_render_window_create(1280, 800, "FEAViz 0.4.2 - VTU Viewer", &window) != FVIZ_OK ||
        fviz_render_window_set_renderer(window, renderer) != FVIZ_OK)
    {
        fprintf(stderr, "viewer setup failed: %s\n", fviz_last_error_message());
        fviz_release(window);
        fviz_release(renderer);
        fviz_release(actor);
        fviz_release(surface);
        fviz_release(grid);
        return 1;
    }

    if (fviz_poly_data_const_scalars(surface) != NULL)
    {
        mapper = fviz_actor_mapper(actor);
        fviz_mapper_set_scalar_visibility(mapper, FVIZ_TRUE);
        fviz_mapper_set_scalar_range(mapper, 0.0f, 70.0f);
        if (fviz_scalar_legend_create(&legend) == FVIZ_OK)
        {
            fviz_scalar_legend_set_range(legend, 0.0f, 70.0f);
            fviz_scalar_legend_set_title(legend, "temperature");
            fviz_renderer_set_scalar_legend(renderer, legend);
            fviz_release(legend);
        }
    }
    fviz_renderer_set_background(renderer, 0.08f, 0.09f, 0.12f);
    fviz_renderer_fit_camera(renderer, 1.3f);

    printf("Controls: LMB orbit | MMB pan | wheel zoom | F fit | W wireframe | Esc close\n");
    result = fviz_render_window_run(window);
    if (result != FVIZ_OK) fprintf(stderr, "render loop failed: %s\n", fviz_last_error_message());

    fviz_release(window);
    fviz_release(renderer);
    fviz_release(actor);
    fviz_release(surface);
    fviz_release(grid);
    return result == FVIZ_OK ? 0 : 1;
}
