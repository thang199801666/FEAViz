#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define BEAM_NX 32u
#define BEAM_NY 4u
#define BEAM_NZ 4u
#define BEAM_LENGTH 10.0f
#define BEAM_HEIGHT 1.0f
#define BEAM_WIDTH 1.0f
#define BEAM_TIP_DEFLECTION 2.5f
#define BEAM_MAX_STRESS_MPA 250.0f

static uint32_t beam_point_id(FVizSize x, FVizSize y, FVizSize z)
{
    return (uint32_t)(x + (BEAM_NX + 1u) * (y + (BEAM_NY + 1u) * z));
}

static FVizResult build_cantilever_beam(FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizResult result = FVIZ_OK;
    FVizSize x;
    FVizSize y;
    FVizSize z;

    if (out_grid == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_grid = NULL;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &stress) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &displacement) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto cleanup;
    }

    for (z = 0u; z <= BEAM_NZ; ++z)
    {
        const float pz = ((float)z / (float)BEAM_NZ - 0.5f) * BEAM_WIDTH;
        for (y = 0u; y <= BEAM_NY; ++y)
        {
            const float py = ((float)y / (float)BEAM_NY - 0.5f) * BEAM_HEIGHT;
            for (x = 0u; x <= BEAM_NX; ++x)
            {
                const float px = (float)x / (float)BEAM_NX * BEAM_LENGTH;
                if (fviz_unstructured_grid_add_point(grid, fviz_vec3(px, py, pz), NULL) != FVIZ_OK)
                {
                    result = fviz_last_error_code();
                    goto cleanup;
                }
            }
        }
    }

    for (z = 0u; z < BEAM_NZ; ++z)
    {
        for (y = 0u; y < BEAM_NY; ++y)
        {
            for (x = 0u; x < BEAM_NX; ++x)
            {
                const uint32_t ids[8] = {
                    beam_point_id(x, y, z),
                    beam_point_id(x + 1u, y, z),
                    beam_point_id(x + 1u, y + 1u, z),
                    beam_point_id(x, y + 1u, z),
                    beam_point_id(x, y, z + 1u),
                    beam_point_id(x + 1u, y, z + 1u),
                    beam_point_id(x + 1u, y + 1u, z + 1u),
                    beam_point_id(x, y + 1u, z + 1u)
                };
                if (fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, ids) != FVIZ_OK)
                {
                    result = fviz_last_error_code();
                    goto cleanup;
                }
            }
        }
    }

    if (fviz_data_array_resize(stress, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK ||
        fviz_data_array_resize(displacement, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto cleanup;
    }

    for (z = 0u; z <= BEAM_NZ; ++z)
    {
        for (y = 0u; y <= BEAM_NY; ++y)
        {
            const float py = ((float)y / (float)BEAM_NY - 0.5f) * BEAM_HEIGHT;
            for (x = 0u; x <= BEAM_NX; ++x)
            {
                const uint32_t id = beam_point_id(x, y, z);
                const float xi = (float)x / (float)BEAM_NX;
                const float vertical = -BEAM_TIP_DEFLECTION * xi * xi * (3.0f - xi) * 0.5f;
                const float slope = -BEAM_TIP_DEFLECTION * 1.5f * xi * (2.0f - xi) / BEAM_LENGTH;
                const float tuple[3] = {-py * slope, vertical, 0.0f};
                const float sigma = BEAM_MAX_STRESS_MPA * (1.0f - xi) *
                    fabsf(2.0f * py / BEAM_HEIGHT);
                if (fviz_data_array_set_tuple(stress, id, &sigma) != FVIZ_OK ||
                    fviz_data_array_set_tuple(displacement, id, tuple) != FVIZ_OK)
                {
                    result = fviz_last_error_code();
                    goto cleanup;
                }
            }
        }
    }

    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "von_mises_mpa", stress) != FVIZ_OK ||
        fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "displacement", displacement) != FVIZ_OK ||
        fviz_unstructured_grid_validate(grid) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto cleanup;
    }

    *out_grid = grid;
    grid = NULL;

cleanup:
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return result;
}

static FVizResult add_surface_hex_edges(FVizPolyData* surface)
{
    FVizSize x;
    FVizSize y;
    FVizSize z;
    for (z = 0u; z <= BEAM_NZ; ++z)
        for (y = 0u; y <= BEAM_NY; ++y)
            if (z == 0u || z == BEAM_NZ || y == 0u || y == BEAM_NY)
                for (x = 0u; x < BEAM_NX; ++x)
                    if (fviz_poly_data_add_line(surface, beam_point_id(x, y, z), beam_point_id(x + 1u, y, z)) != FVIZ_OK)
                        return fviz_last_error_code();

    for (z = 0u; z <= BEAM_NZ; ++z)
        for (x = 0u; x <= BEAM_NX; ++x)
            if (z == 0u || z == BEAM_NZ || x == 0u || x == BEAM_NX)
                for (y = 0u; y < BEAM_NY; ++y)
                    if (fviz_poly_data_add_line(surface, beam_point_id(x, y, z), beam_point_id(x, y + 1u, z)) != FVIZ_OK)
                        return fviz_last_error_code();

    for (y = 0u; y <= BEAM_NY; ++y)
        for (x = 0u; x <= BEAM_NX; ++x)
            if (y == 0u || y == BEAM_NY || x == 0u || x == BEAM_NX)
                for (z = 0u; z < BEAM_NZ; ++z)
                    if (fviz_poly_data_add_line(surface, beam_point_id(x, y, z), beam_point_id(x, y, z + 1u)) != FVIZ_OK)
                        return fviz_last_error_code();
    return FVIZ_OK;
}

static int validate_beam_result(
    const FVizUnstructuredGrid* original,
    FVizUnstructuredGrid* warped,
    const FVizPolyData* surface)
{
    const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(warped));
    const FVizDataArray* scalars = fviz_poly_data_const_scalars(surface);
    const float* stress;
    const uint32_t tip_id = beam_point_id(BEAM_NX, BEAM_NY / 2u, BEAM_NZ / 2u);
    const uint32_t clamp_top_id = beam_point_id(0u, BEAM_NY, BEAM_NZ / 2u);
    if (fviz_unstructured_grid_point_count(original) != 825u ||
        fviz_unstructured_grid_cell_count(original) != 512u ||
        fviz_poly_data_triangle_count(surface) != 1088u ||
        fviz_poly_data_line_count(surface) != 1088u || scalars == NULL)
        return 1;
    stress = (const float*)fviz_data_array_const_data(scalars);
    if (fabsf(points[tip_id].y + BEAM_TIP_DEFLECTION) > 1.0e-4f ||
        fabsf(stress[clamp_top_id] - BEAM_MAX_STRESS_MPA) > 1.0e-4f ||
        fabsf(stress[tip_id]) > 1.0e-4f)
        return 1;
    return 0;
}

int main(int argc, char** argv)
{
    const FVizBool validate_only = argc > 1 && strcmp(argv[1], "--validate") == 0 ? FVIZ_TRUE : FVIZ_FALSE;
    FVizUnstructuredGrid* grid = NULL;
    FVizFilter* warp = NULL;
    FVizUnstructuredGrid* warped = NULL;
    FVizPolyData* surface = NULL;
    FVizLookupTable* rainbow = NULL;
    FVizActor* actor = NULL;
    FVizRendererWidget* widget = NULL;
    FVizRenderer* renderer = NULL;
    FVizScalarLegend* legend = NULL;
    FVizResult result = FVIZ_OK;
    int exit_code = 1;

    if (build_cantilever_beam(&grid) != FVIZ_OK ||
        fviz_warp_filter_create("displacement", 1.0, &warp) != FVIZ_OK ||
        fviz_filter_set_input(warp, grid) != FVIZ_OK ||
        fviz_filter_update(warp) != FVIZ_OK)
    {
        fprintf(stderr, "Bent-beam FEA setup failed: %s\n", fviz_last_error_message());
        goto cleanup;
    }
    warped = fviz_filter_output(warp);
    if (fviz_unstructured_grid_extract_surface_scalars(warped, &surface) != FVIZ_OK ||
        add_surface_hex_edges(surface) != FVIZ_OK ||
        fviz_poly_data_compute_normals(surface) != FVIZ_OK)
    {
        fprintf(stderr, "Bent-beam surface extraction failed: %s\n", fviz_last_error_message());
        goto cleanup;
    }
    if (validate_beam_result(grid, warped, surface) != 0)
    {
        fprintf(stderr, "Bent-beam validation failed\n");
        goto cleanup;
    }

    printf("FEAViz %s | cantilever HEX8: points=%zu cells=%zu triangles=%zu grid_lines=%zu\n",
        fviz_version_string(),
        (size_t)fviz_unstructured_grid_point_count(grid),
        (size_t)fviz_unstructured_grid_cell_count(grid),
        (size_t)fviz_poly_data_triangle_count(surface),
        (size_t)fviz_poly_data_line_count(surface));
    printf("Tip deflection: %.2f | max Von Mises stress: %.1f MPa | colormap: Rainbow\n",
        (double)BEAM_TIP_DEFLECTION, (double)BEAM_MAX_STRESS_MPA);
    if (validate_only == FVIZ_TRUE)
    {
        exit_code = 0;
        goto cleanup;
    }
    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        fprintf(stderr, "Native viewer backend is not available on this platform.\n");
        exit_code = 2;
        goto cleanup;
    }

    if (fviz_lookup_table_create(256u, &rainbow) != FVIZ_OK ||
        fviz_lookup_table_build_preset(rainbow, FVIZ_COLOR_MAP_RAINBOW) != FVIZ_OK ||
        fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, surface) != FVIZ_OK ||
        fviz_renderer_widget_create(1400, 850, "FEAViz 0.9.0 - Bent HEX8 Beam / Rainbow", &widget) != FVIZ_OK ||
        fviz_renderer_widget_add_actor(widget, actor) != FVIZ_OK ||
        fviz_scalar_legend_create(&legend) != FVIZ_OK ||
        (renderer = fviz_renderer_widget_renderer(widget)) == NULL)
    {
        fprintf(stderr, "Bent-beam viewer setup failed: %s\n", fviz_last_error_message());
        goto cleanup;
    }

    fviz_lookup_table_set_range(rainbow, 0.0f, BEAM_MAX_STRESS_MPA);
    fviz_mapper_set_lookup_table(fviz_actor_mapper(actor), rainbow);
    fviz_mapper_set_scalar_visibility(fviz_actor_mapper(actor), FVIZ_TRUE);
    fviz_mapper_set_scalar_range(fviz_actor_mapper(actor), 0.0f, BEAM_MAX_STRESS_MPA);
    fviz_scalar_legend_set_lookup_table(legend, rainbow);
    fviz_scalar_legend_set_range(legend, 0.0f, BEAM_MAX_STRESS_MPA);
    fviz_scalar_legend_set_title(legend, "Von Mises Stress (MPa) - Rainbow");
    fviz_renderer_set_scalar_legend(renderer, legend);
    fviz_renderer_set_background(renderer, 0.035f, 0.045f, 0.070f);
    fviz_renderer_fit_camera(renderer, 1.25f);
    fviz_camera_orbit(fviz_renderer_camera(renderer), -0.30f, 0.18f);

    printf("Controls: LMB orbit | MMB pan | RMB/wheel dolly | F/R fit | W wireframe | S surface | Esc close\n");
    result = fviz_renderer_widget_start(widget);
    if (result != FVIZ_OK)
    {
        fprintf(stderr, "Render loop failed: %s\n", fviz_last_error_message());
        goto cleanup;
    }
    exit_code = 0;

cleanup:
    fviz_release(widget);
    fviz_release(legend);
    fviz_release(actor);
    fviz_release(rainbow);
    fviz_release(surface);
    fviz_release(warp);
    fviz_release(grid);
    return exit_code;
}
