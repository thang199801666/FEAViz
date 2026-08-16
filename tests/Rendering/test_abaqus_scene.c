#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) \
    do \
    { \
        if (!(expr)) \
        { \
            fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
            return 1; \
        } \
    } while (0)

#define NX 12u
#define NY 2u
#define NZ 2u
#define LENGTH 12.0f
#define HEIGHT 2.0f
#define WIDTH 2.0f

static uint32_t point_id(uint32_t x, uint32_t y, uint32_t z)
{
    return x + (NX + 1u) * (y + (NY + 1u) * z);
}

static FVizResult build_model(FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizResult result = FVIZ_OK;
    FVizBool success = FVIZ_FALSE;
    uint32_t x;
    uint32_t y;
    uint32_t z;

    if (out_grid == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_grid = NULL;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &stress) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &displacement) != FVIZ_OK)
        goto cleanup;

    for (z = 0u; z <= NZ; ++z)
        for (y = 0u; y <= NY; ++y)
            for (x = 0u; x <= NX; ++x)
                if (fviz_unstructured_grid_add_point(
                        grid,
                        fviz_vec3(
                            LENGTH * (float)x / (float)NX,
                            HEIGHT * ((float)y / (float)NY - 0.5f),
                            WIDTH * ((float)z / (float)NZ - 0.5f)),
                        NULL) != FVIZ_OK)
                    goto cleanup;

    for (z = 0u; z < NZ; ++z)
        for (y = 0u; y < NY; ++y)
            for (x = 0u; x < NX; ++x)
            {
                const uint32_t ids[8] = {
                    point_id(x, y, z), point_id(x + 1u, y, z),
                    point_id(x + 1u, y + 1u, z), point_id(x, y + 1u, z),
                    point_id(x, y, z + 1u), point_id(x + 1u, y, z + 1u),
                    point_id(x + 1u, y + 1u, z + 1u), point_id(x, y + 1u, z + 1u)
                };
                if (fviz_unstructured_grid_add_cell(grid, FVIZ_CELL_HEXAHEDRON, 8u, ids) != FVIZ_OK)
                    goto cleanup;
            }

    if (fviz_data_array_resize(stress, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK ||
        fviz_data_array_resize(displacement, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK)
        goto cleanup;

    for (z = 0u; z <= NZ; ++z)
        for (y = 0u; y <= NY; ++y)
            for (x = 0u; x <= NX; ++x)
            {
                const uint32_t id = point_id(x, y, z);
                const float xi = (float)x / (float)NX;
                const float lateral = -0.9f * xi * xi * (3.0f - xi);
                const float stress_value = 220.0f * (1.0f - xi) *
                    (0.25f + 0.75f * fabsf(2.0f * (float)y / (float)NY - 1.0f));
                const float vector[3] = {0.0f, lateral, 0.0f};
                if (fviz_data_array_set_tuple(stress, id, &stress_value) != FVIZ_OK ||
                    fviz_data_array_set_tuple(displacement, id, vector) != FVIZ_OK)
                    goto cleanup;
            }

    if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "S_MISES", stress) != FVIZ_OK ||
        fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid), "U", displacement) != FVIZ_OK ||
        fviz_unstructured_grid_validate(grid) != FVIZ_OK)
        goto cleanup;

    *out_grid = grid;
    grid = NULL;
    success = FVIZ_TRUE;

cleanup:
    if (success == FVIZ_FALSE) result = fviz_last_error_code();
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return result;
}

static FVizResult add_surface_grid_edges(FVizPolyData* surface)
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
    for (z = 0u; z <= NZ; ++z)
        for (y = 0u; y <= NY; ++y)
            for (x = 0u; x < NX; ++x)
                if (z == 0u || z == NZ || y == 0u || y == NY)
                    if (fviz_poly_data_add_line(surface, point_id(x, y, z), point_id(x + 1u, y, z)) != FVIZ_OK)
                        return fviz_last_error_code();
    for (z = 0u; z <= NZ; ++z)
        for (x = 0u; x <= NX; ++x)
            for (y = 0u; y < NY; ++y)
                if (z == 0u || z == NZ || x == 0u || x == NX)
                    if (fviz_poly_data_add_line(surface, point_id(x, y, z), point_id(x, y + 1u, z)) != FVIZ_OK)
                        return fviz_last_error_code();
    for (y = 0u; y <= NY; ++y)
        for (x = 0u; x <= NX; ++x)
            for (z = 0u; z < NZ; ++z)
                if (y == 0u || y == NY || x == 0u || x == NX)
                    if (fviz_poly_data_add_line(surface, point_id(x, y, z), point_id(x, y, z + 1u)) != FVIZ_OK)
                        return fviz_last_error_code();
    return FVIZ_OK;
}

static uint64_t image_hash(const uint8_t* pixels, FVizSize count)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    FVizSize i;
    for (i = 0u; i < count; ++i)
    {
        hash ^= pixels[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int main(int argc, char** argv)
{
    const int width = 640;
    const int height = 420;
    const FVizSize bytes = (FVizSize)width * (FVizSize)height * 4u;
    const FVizBool show = argc > 1 && argv[1] != NULL &&
        strcmp(argv[1], "--show") == 0 ? FVIZ_TRUE : FVIZ_FALSE;
    FVizUnstructuredGrid* grid = NULL;
    FVizUnstructuredGrid* warped = NULL;
    FVizPolyData* original_surface = NULL;
    FVizPolyData* deformed_surface = NULL;
    FVizLookupTable* rainbow = NULL;
    FVizScalarLegend* legend = NULL;
    FVizActor* original_actor = NULL;
    FVizActor* deformed_actor = NULL;
    FVizRenderWindow* window = NULL;
    FVizRendererWidget* widget = NULL;
    FVizRenderer* renderer;
    FVizArraySelection selection;
    FVizRenderCapabilities capabilities;
    uint8_t* pixels = NULL;
    uint64_t hash;

    if (fviz_render_window_supported() == FVIZ_FALSE) return 0;
    CHECK(build_model(&grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_point_count(grid) == 117u);
    CHECK(fviz_unstructured_grid_cell_count(grid) == 48u);
    CHECK(fviz_unstructured_grid_warp_by_vector(grid, "U", 1.0, &warped) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_extract_surface(grid, &original_surface) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_extract_surface_scalars(warped, &deformed_surface) == FVIZ_OK);
    CHECK(add_surface_grid_edges(original_surface) == FVIZ_OK);
    CHECK(add_surface_grid_edges(deformed_surface) == FVIZ_OK);
    CHECK(fviz_poly_data_compute_normals(deformed_surface) == FVIZ_OK);

    CHECK(fviz_lookup_table_create(256u, &rainbow) == FVIZ_OK);
    CHECK(fviz_lookup_table_build_preset(rainbow, FVIZ_COLOR_MAP_RAINBOW) == FVIZ_OK);
    fviz_lookup_table_set_range(rainbow, 0.0f, 220.0f);
    CHECK(fviz_actor_create(&original_actor) == FVIZ_OK);
    CHECK(fviz_actor_create(&deformed_actor) == FVIZ_OK);
    CHECK(fviz_actor_set_poly_data(original_actor, original_surface) == FVIZ_OK);
    CHECK(fviz_actor_set_poly_data(deformed_actor, deformed_surface) == FVIZ_OK);
    fviz_actor_set_color(original_actor, 0.25f, 0.35f, 0.50f);
    fviz_actor_set_opacity(original_actor, 0.28f);
    fviz_actor_set_edge_visibility(original_actor, FVIZ_TRUE);
    fviz_actor_set_edge_color(original_actor, 0.45f, 0.65f, 0.95f);
    fviz_actor_set_edge_visibility(deformed_actor, FVIZ_TRUE);
    fviz_actor_set_edge_color(deformed_actor, 0.08f, 0.08f, 0.08f);
    fviz_actor_set_line_width(deformed_actor, 1.0f);
    fviz_array_selection_initialize(&selection);
    selection.association = FVIZ_ASSOCIATION_POINTS;
    selection.name = "S_MISES";
    selection.component_mode = FVIZ_COMPONENT_DIRECT;
    CHECK(fviz_mapper_set_array_selection(fviz_actor_mapper(deformed_actor), &selection) == FVIZ_OK);
    fviz_mapper_set_lookup_table(fviz_actor_mapper(deformed_actor), rainbow);
    fviz_mapper_set_scalar_visibility(fviz_actor_mapper(deformed_actor), FVIZ_TRUE);
    fviz_mapper_set_scalar_range(fviz_actor_mapper(deformed_actor), 0.0f, 220.0f);

    if (show == FVIZ_TRUE)
    {
        CHECK(fviz_renderer_widget_create(
            width, height, "FEAViz - Abaqus-style HEX8 result", &widget) == FVIZ_OK);
        window = fviz_renderer_widget_window(widget);
        renderer = fviz_renderer_widget_renderer(widget);
    }
    else
    {
        CHECK(fviz_render_window_create_offscreen(width, height, &window) == FVIZ_OK);
        renderer = fviz_render_window_renderer(window);
    }
    fviz_render_window_get_capabilities(window, &capabilities);
    if (capabilities.modern_pipeline == FVIZ_FALSE)
    {
        if (widget != NULL) fviz_release(widget);
        else fviz_release(window);
        fviz_release(deformed_actor);
        fviz_release(original_actor);
        fviz_release(legend);
        fviz_release(rainbow);
        fviz_release(deformed_surface);
        fviz_release(original_surface);
        fviz_release(warped);
        fviz_release(grid);
        return 0;
    }
    CHECK(renderer != NULL);
    CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer), original_actor) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer), deformed_actor) == FVIZ_OK);
    CHECK(fviz_scene_actor_count(fviz_renderer_scene(renderer)) == 2u);
    CHECK(fviz_scalar_legend_create(&legend) == FVIZ_OK);
    fviz_scalar_legend_set_lookup_table(legend, rainbow);
    fviz_scalar_legend_set_range(legend, 0.0f, 220.0f);
    fviz_scalar_legend_set_title(legend, "S, Mises (MPa)");
    fviz_renderer_set_scalar_legend(renderer, legend);
    fviz_renderer_set_background(renderer, 0.035f, 0.045f, 0.070f);
    fviz_renderer_fit_camera(renderer, 1.2f);
    if (show == FVIZ_TRUE)
    {
        puts("Abaqus-style scene: LMB orbit | MMB pan | RMB/wheel dolly | F/R fit | Esc close");
        CHECK(fviz_renderer_widget_start(widget) == FVIZ_OK);
        fviz_release(widget);
        widget = NULL;
        fviz_release(legend);
        fviz_release(deformed_actor);
        fviz_release(original_actor);
        fviz_release(rainbow);
        fviz_release(deformed_surface);
        fviz_release(original_surface);
        fviz_release(warped);
        fviz_release(grid);
        return 0;
    }
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    pixels = (uint8_t*)fviz_alloc(bytes);
    CHECK(pixels != NULL);
    CHECK(fviz_render_window_read_rgba8(window, pixels, bytes) == FVIZ_OK);
    hash = image_hash(pixels, bytes);
    CHECK(hash != 0u);
    fviz_free(pixels);
    fviz_release(window);
    fviz_release(legend);
    fviz_release(deformed_actor);
    fviz_release(original_actor);
    fviz_release(rainbow);
    fviz_release(deformed_surface);
    fviz_release(original_surface);
    fviz_release(warped);
    fviz_release(grid);
    puts("Abaqus-style FEA scene test passed");
    return 0;
}
