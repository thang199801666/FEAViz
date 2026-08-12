#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

static FVizUnstructuredGrid* g_grid = NULL;
static FVizPointLocator* g_locator = NULL;

static FVizResult build_bracket(FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* temperature = NULL;
    FVizSize i;
    FVizSize x;
    FVizSize y;
    FVizSize z;
    const FVizSize n = 8u;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &temperature) != FVIZ_OK) return fviz_last_error_code();
    for (z = 0u; z < n; ++z)
        for (y = 0u; y < n; ++y)
            for (x = 0u; x < n; ++x)
                if (fviz_unstructured_grid_add_point(grid, fviz_vec3(
                        (float)x / (float)(n - 1u) * 2.0f - 1.0f,
                        (float)y / (float)(n - 1u) * 2.0f - 1.0f,
                        (float)z / (float)(n - 1u) * 2.0f - 1.0f), NULL) != FVIZ_OK)
                    return fviz_last_error_code();
    for (z = 0u; z + 1u < n; ++z)
        for (y = 0u; y + 1u < n; ++y)
            for (x = 0u; x + 1u < n; ++x)
            {
                const uint32_t base = (uint32_t)(z * n * n + y * n + x);
                const uint32_t n32 = (uint32_t)n;
                const uint32_t ids[8] = {
                    base, base + 1u, base + n32 + 1u, base + n32,
                    base + n32 * n32, base + n32 * n32 + 1u,
                    base + n32 * n32 + n32 + 1u, base + n32 * n32 + n32
                };
                if (fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, ids) != FVIZ_OK)
                    return fviz_last_error_code();
            }
    if (fviz_data_array_resize(temperature, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK) return fviz_last_error_code();
    {
        const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(grid));
        for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        {
            const float scalar = (points[i].x + points[i].y + points[i].z) * 0.5f + 0.5f;
            if (fviz_data_array_set_tuple(temperature, i, &scalar) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "temperature", temperature) != FVIZ_OK) return fviz_last_error_code();
    fviz_release(temperature);
    *out_grid = grid;
    return FVIZ_OK;
}

static void on_pick(FVizRenderWindow* window, int x, int y, const FVizRayHit* hit, void* user_data)
{
    float value = 0.0f;
    (void)window;
    (void)user_data;
    if (fviz_point_locator_interpolate_scalar(g_locator, "temperature", hit->point, &value) == FVIZ_OK)
    {
        printf("Pick @ (%d, %d): world (%.3f, %.3f, %.3f)  temperature = %.4f\n",
            x, y,
            (double)hit->point.x, (double)hit->point.y, (double)hit->point.z,
            (double)value);
    }
    else
    {
        printf("Pick @ (%d, %d): hit surface but point outside interpolation grid\n", x, y);
    }
}

int main(void)
{
    FVizPolyData* surface = NULL;
    FVizRenderer* renderer = NULL;
    FVizRenderWindow* window = NULL;
    FVizActor* actor = NULL;
    FVizMapper* mapper = NULL;
    FVizResult result;

    result = build_bracket(&g_grid);
    if (result != FVIZ_OK) { fprintf(stderr, "build failed: %s\n", fviz_last_error_message()); return 1; }
    if (fviz_unstructured_grid_extract_surface_scalars(g_grid, &surface) != FVIZ_OK)
    {
        fprintf(stderr, "surface failed: %s\n", fviz_last_error_message());
        fviz_release(g_grid);
        return 1;
    }
    if (fviz_point_locator_create(&g_locator) != FVIZ_OK ||
        fviz_point_locator_set_grid(g_locator, g_grid) != FVIZ_OK)
    {
        fprintf(stderr, "locator failed: %s\n", fviz_last_error_message());
        fviz_release(surface);
        fviz_release(g_grid);
        return 1;
    }
    printf("FEAViz %s | Picking: click the model to probe temperature\n", fviz_version_string());

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        fprintf(stderr, "Native viewer backend is not available on this platform yet.\n");
        fviz_release(g_locator);
        fviz_release(surface);
        fviz_release(g_grid);
        return 2;
    }

    if (fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, surface) != FVIZ_OK ||
        fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) != FVIZ_OK ||
        fviz_render_window_create(1280, 800, "FEAViz 0.4.0 - FEA Picking", &window) != FVIZ_OK ||
        fviz_render_window_set_renderer(window, renderer) != FVIZ_OK)
    {
        fprintf(stderr, "viewer setup failed: %s\n", fviz_last_error_message());
        fviz_release(window);
        fviz_release(renderer);
        fviz_release(actor);
        fviz_release(g_locator);
        fviz_release(surface);
        fviz_release(g_grid);
        return 1;
    }

    mapper = fviz_actor_mapper(actor);
    fviz_mapper_set_scalar_visibility(mapper, FVIZ_TRUE);
    fviz_mapper_set_scalar_range(mapper, 0.0f, 1.0f);
    fviz_renderer_set_background(renderer, 0.08f, 0.09f, 0.12f);
    fviz_renderer_fit_camera(renderer, 1.3f);
    fviz_render_window_set_pick_callback(window, on_pick, NULL);

    printf("Controls: LMB orbit (click to probe) | MMB pan | wheel zoom | F fit | W wireframe | Esc close\n");
    result = fviz_render_window_run(window);
    if (result != FVIZ_OK) fprintf(stderr, "render loop failed: %s\n", fviz_last_error_message());

    fviz_release(window);
    fviz_release(renderer);
    fviz_release(actor);
    fviz_release(g_locator);
    fviz_release(surface);
    fviz_release(g_grid);
    return result == FVIZ_OK ? 0 : 1;
}
