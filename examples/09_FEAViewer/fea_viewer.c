#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

static FVizResult build_bracket(
    FVizUnstructuredGrid** out_grid,
    FVizDataArray** out_stress,
    FVizDataArray** out_displacement)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizSize i;
    FVizSize x;
    FVizSize y;
    FVizSize z;
    const FVizSize n = 10u;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &stress) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &displacement) != FVIZ_OK) return fviz_last_error_code();

    for (z = 0u; z < n; ++z)
    {
        for (y = 0u; y < n; ++y)
        {
            for (x = 0u; x < n; ++x)
            {
                const FVizVec3 point = fviz_vec3(
                    (float)x / (float)(n - 1u) * 2.0f - 1.0f,
                    (float)y / (float)(n - 1u) * 2.0f - 1.0f,
                    (float)z / (float)(n - 1u) * 2.0f - 1.0f);
                uint32_t id;
                if (fviz_unstructured_grid_add_point(grid, point, &id) != FVIZ_OK) return fviz_last_error_code();
            }
        }
    }
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
                {
                    fviz_release(grid);
                    return fviz_last_error_code();
                }
            }

    if (fviz_data_array_resize(stress, fviz_unstructured_grid_cell_count(grid)) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < fviz_unstructured_grid_cell_count(grid); ++i)
    {
        const float value = sinf((float)i * 0.31f) * 0.5f + 0.5f;
        if (fviz_data_array_set_tuple(stress, i, &value) != FVIZ_OK) return fviz_last_error_code();
    }
    if (fviz_data_array_resize(displacement, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK) return fviz_last_error_code();
    {
        const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(grid));
        for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        {
            const float magnitude = (points[i].x + 1.0f) * 0.5f;
            const float tuple[3] = {
                magnitude * 0.15f,
                points[i].y * magnitude * 0.05f,
                points[i].z * magnitude * 0.08f
            };
            if (fviz_data_array_set_tuple(displacement, i, tuple) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid), "stress", stress) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "displacement", displacement) != FVIZ_OK) return fviz_last_error_code();
    *out_grid = grid;
    *out_stress = stress;
    *out_displacement = displacement;
    return FVIZ_OK;
}

static FVizResult add_scalar_actor(FVizRenderer* renderer, FVizPolyData* data, FVizVec3 position)
{
    FVizActor* actor = NULL;
    FVizMapper* mapper = NULL;
    if (fviz_actor_create(&actor) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_actor_set_poly_data(actor, data) != FVIZ_OK) return fviz_last_error_code();
    fviz_actor_set_position(actor, position);
    mapper = fviz_actor_mapper(actor);
    fviz_mapper_set_scalar_visibility(mapper, FVIZ_TRUE);
    fviz_mapper_set_scalar_range(mapper, 0.0f, 1.0f);
    if (fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) != FVIZ_OK) return fviz_last_error_code();
    fviz_release(actor);
    return FVIZ_OK;
}

int main(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizFilter* smooth = NULL;
    FVizFilter* deform = NULL;
    FVizUnstructuredGrid* warped = NULL;
    FVizPolyData* surface = NULL;
    FVizPolyData* slice = NULL;
    FVizRenderer* renderer = NULL;
    FVizRenderWindow* window = NULL;
    FVizResult result;

    result = build_bracket(&grid, &stress, &displacement);
    if (result != FVIZ_OK) { fprintf(stderr, "build failed: %s\n", fviz_last_error_message()); return 1; }

    if (fviz_cell_data_to_point_filter_create(&smooth) != FVIZ_OK ||
        fviz_filter_set_input(smooth, grid) != FVIZ_OK ||
        fviz_filter_update(smooth) != FVIZ_OK ||
        fviz_warp_filter_create("displacement", 1.5, &deform) != FVIZ_OK ||
        fviz_filter_set_input(deform, fviz_filter_output(smooth)) != FVIZ_OK ||
        fviz_filter_update(deform) != FVIZ_OK)
    {
        fprintf(stderr, "pipeline failed: %s\n", fviz_last_error_message());
        fviz_release(deform);
        fviz_release(smooth);
        fviz_release(displacement);
        fviz_release(stress);
        fviz_release(grid);
        return 1;
    }
    warped = fviz_filter_output(deform);
    if (fviz_unstructured_grid_extract_surface_scalars(warped, &surface) != FVIZ_OK ||
        fviz_unstructured_grid_slice(warped,
            fviz_plane_from_point_normal(fviz_vec3(0.0f, 0.0f, 0.0f), fviz_vec3(0.0f, 1.0f, 0.0f)),
            &slice) != FVIZ_OK)
    {
        fprintf(stderr, "surface/slice failed: %s\n", fviz_last_error_message());
        fviz_release(deform);
        fviz_release(smooth);
        fviz_release(displacement);
        fviz_release(stress);
        fviz_release(grid);
        return 1;
    }
    printf("FEAViz %s | FEA viewer: cells=%zu surface_tris=%zu slice_tris=%zu\n",
        fviz_version_string(),
        (size_t)fviz_unstructured_grid_cell_count(grid),
        (size_t)fviz_poly_data_triangle_count(surface),
        (size_t)fviz_poly_data_triangle_count(slice));

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        fprintf(stderr, "Native viewer backend is not available on this platform yet.\n");
        fviz_release(slice);
        fviz_release(surface);
        fviz_release(deform);
        fviz_release(smooth);
        fviz_release(displacement);
        fviz_release(stress);
        fviz_release(grid);
        return 2;
    }

    if (fviz_renderer_create(&renderer) != FVIZ_OK ||
        add_scalar_actor(renderer, surface, fviz_vec3(-1.6f, 0.0f, 0.0f)) != FVIZ_OK ||
        add_scalar_actor(renderer, slice, fviz_vec3(1.6f, 0.0f, 0.0f)) != FVIZ_OK ||
        fviz_render_window_create(1280, 800, "FEAViz 0.3.3 - FEA Viewer", &window) != FVIZ_OK ||
        fviz_render_window_set_renderer(window, renderer) != FVIZ_OK)
    {
        fprintf(stderr, "viewer setup failed: %s\n", fviz_last_error_message());
        fviz_release(window);
        fviz_release(renderer);
        fviz_release(slice);
        fviz_release(surface);
        fviz_release(deform);
        fviz_release(smooth);
        fviz_release(displacement);
        fviz_release(stress);
        fviz_release(grid);
        return 1;
    }

    fviz_renderer_set_background(renderer, 0.08f, 0.09f, 0.12f);
    fviz_renderer_fit_camera(renderer, 1.5f);
    printf("Deformed surface (left) and interior slice (right), colored by cell stress (smoothed to points)\n");
    printf("Controls: LMB orbit | MMB pan | wheel zoom | F fit | W wireframe | Esc close\n");
    result = fviz_render_window_run(window);
    if (result != FVIZ_OK) fprintf(stderr, "render loop failed: %s\n", fviz_last_error_message());

    fviz_release(window);
    fviz_release(renderer);
    fviz_release(slice);
    fviz_release(surface);
    fviz_release(deform);
    fviz_release(smooth);
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return result == FVIZ_OK ? 0 : 1;
}
