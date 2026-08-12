#include <math.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizResult build_grid(FVizUnstructuredGrid** out_grid, FVizDataArray** out_stress, FVizDataArray** out_displacement)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizSize z;
    FVizSize y;
    FVizSize x;
    const FVizSize n = 3u;
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK) return fviz_last_error_code();
    for (z = 0u; z < n; ++z)
        for (y = 0u; y < n; ++y)
            for (x = 0u; x < n; ++x)
                if (fviz_unstructured_grid_add_point(grid, fviz_vec3((float)x, (float)y, (float)z), NULL) != FVIZ_OK)
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
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &stress) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &displacement) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_resize(stress, fviz_unstructured_grid_cell_count(grid)) != FVIZ_OK) return fviz_last_error_code();
    {
        FVizSize i;
        for (i = 0u; i < fviz_unstructured_grid_cell_count(grid); ++i)
        {
            const float value = (float)i;
            if (fviz_data_array_set_tuple(stress, i, &value) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    if (fviz_data_array_resize(displacement, fviz_unstructured_grid_point_count(grid)) != FVIZ_OK) return fviz_last_error_code();
    {
        const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(grid));
        FVizSize i;
        for (i = 0u; i < fviz_unstructured_grid_point_count(grid); ++i)
        {
            const float tuple[3] = {points[i].x * 0.1f, 0.0f, 0.0f};
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

static int test_threshold_filter(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizFilter* filter = NULL;
    FVizUnstructuredGrid* output = NULL;
    CHECK(build_grid(&grid, &stress, &displacement) == FVIZ_OK);
    CHECK(fviz_threshold_filter_create("stress", 2.0, 5.0, &filter) == FVIZ_OK);
    CHECK(filter != NULL);
    CHECK(fviz_object_type_id((const FVizObject*)filter) == FVIZ_TYPE_THRESHOLD_FILTER);
    CHECK(fviz_filter_set_input(filter, grid) == FVIZ_OK);
    CHECK(fviz_filter_update(filter) == FVIZ_OK);
    output = fviz_filter_output(filter);
    CHECK(output != NULL);
    CHECK(fviz_unstructured_grid_cell_count(output) <= fviz_unstructured_grid_cell_count(grid));
    CHECK(fviz_unstructured_grid_cell_count(output) > 0u);
    CHECK(fviz_filter_update(filter) == FVIZ_OK);
    CHECK(fviz_filter_output(filter) == output);
    fviz_release(filter);
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

static int test_warp_filter(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizFilter* filter = NULL;
    FVizUnstructuredGrid* output = NULL;
    const FVizVec3* points;
    const FVizVec3* original;
    FVizSize i;
    CHECK(build_grid(&grid, &stress, &displacement) == FVIZ_OK);
    CHECK(fviz_warp_filter_create("displacement", 2.0, &filter) == FVIZ_OK);
    CHECK(fviz_object_type_id((const FVizObject*)filter) == FVIZ_TYPE_WARP_FILTER);
    CHECK(fviz_filter_set_input(filter, grid) == FVIZ_OK);
    CHECK(fviz_filter_update(filter) == FVIZ_OK);
    output = fviz_filter_output(filter);
    CHECK(output != NULL);
    points = fviz_points_data(fviz_unstructured_grid_points(output));
    original = fviz_points_data(fviz_unstructured_grid_points(grid));
    for (i = 0u; i < fviz_unstructured_grid_point_count(output); ++i)
    {
        CHECK(fabsf(points[i].x - (original[i].x + original[i].x * 0.1f * 2.0f)) < 1.0e-4f);
    }
    fviz_release(filter);
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

static int test_cell_to_point_filter(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizFilter* filter = NULL;
    FVizUnstructuredGrid* output = NULL;
    const FVizDataArray* values;
    const float* data;
    CHECK(build_grid(&grid, &stress, &displacement) == FVIZ_OK);
    CHECK(fviz_cell_data_to_point_filter_create(&filter) == FVIZ_OK);
    CHECK(fviz_object_type_id((const FVizObject*)filter) == FVIZ_TYPE_CELL_DATA_TO_POINT_FILTER);
    CHECK(fviz_filter_set_input(filter, grid) == FVIZ_OK);
    CHECK(fviz_filter_update(filter) == FVIZ_OK);
    output = fviz_filter_output(filter);
    CHECK(output != NULL);
    values = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(output), "stress");
    CHECK(values != NULL);
    data = (const float*)fviz_data_array_const_data((FVizDataArray*)values);
    CHECK(fabsf(data[13] - 3.5f) < 1.0e-4f);
    fviz_release(filter);
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

static int test_transform_filter(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizTransform* transform = NULL;
    FVizFilter* filter = NULL;
    FVizUnstructuredGrid* first_output;
    FVizUnstructuredGrid* second_output;
    const FVizVec3* points;
    CHECK(build_grid(&grid, &stress, &displacement) == FVIZ_OK);
    CHECK(fviz_transform_create(&transform) == FVIZ_OK);
    fviz_transform_translate(transform, fviz_vec3(2.0f, 0.0f, 0.0f));
    CHECK(fviz_transform_filter_create(transform, &filter) == FVIZ_OK);
    CHECK(fviz_transform_filter_transform(filter) == transform);
    CHECK(fviz_filter_set_input(filter, grid) == FVIZ_OK);
    CHECK(fviz_filter_update(filter) == FVIZ_OK);
    first_output = fviz_filter_output(filter);
    CHECK(first_output != NULL);
    CHECK(fviz_retain(first_output) == first_output);
    points = fviz_points_data(fviz_unstructured_grid_points(first_output));
    CHECK(fabsf(points[0].x - 2.0f) < 1.0e-5f);
    CHECK(fviz_unstructured_grid_cell_count(first_output) ==
        fviz_unstructured_grid_cell_count(grid));
    fviz_transform_translate(transform, fviz_vec3(3.0f, 0.0f, 0.0f));
    CHECK(fviz_filter_update(filter) == FVIZ_OK);
    second_output = fviz_filter_output(filter);
    CHECK(second_output != NULL && second_output != first_output);
    points = fviz_points_data(fviz_unstructured_grid_points(second_output));
    CHECK(fabsf(points[0].x - 5.0f) < 1.0e-5f);
    fviz_release(first_output);
    fviz_release(filter);
    fviz_release(transform);
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

static int test_connected_render_pipeline(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* displacement = NULL;
    FVizFilter* smooth = NULL;
    FVizFilter* warp = NULL;
    FVizFilter* surface = NULL;
    FVizFilter* slice = NULL;
    FVizActor* actor = NULL;
    FVizRenderer* renderer = NULL;
    FVizMapper* mapper;
    FVizPolyData* first_surface;
    FVizPolyData* second_surface;
    FVizPolyData* mutated_surface;
    const FVizVec3* points;
    FVizMTime grid_mtime;
    CHECK(fviz_filter_output_type(NULL) == FVIZ_FILTER_OUTPUT_NONE);
    CHECK(build_grid(&grid, &stress, &displacement) == FVIZ_OK);
    CHECK(fviz_cell_data_to_point_filter_create(&smooth) == FVIZ_OK);
    CHECK(fviz_warp_filter_create("displacement", 2.0, &warp) == FVIZ_OK);
    CHECK(fviz_surface_filter_create(FVIZ_TRUE, &surface) == FVIZ_OK);
    CHECK(fviz_slice_filter_create(
        fviz_plane_from_point_normal(fviz_vec3(0.0f, 1.0f, 0.0f), fviz_vec3(0.0f, 1.0f, 0.0f)),
        &slice) == FVIZ_OK);
    CHECK(fviz_filter_output_type(smooth) == FVIZ_FILTER_OUTPUT_UNSTRUCTURED_GRID);
    CHECK(fviz_filter_output_type(surface) == FVIZ_FILTER_OUTPUT_POLY_DATA);
    CHECK(fviz_algorithm_set_input_data(
        fviz_filter_algorithm(smooth), 0u, (FVizDataObject*)grid) == FVIZ_OK);
    CHECK(fviz_algorithm_set_input_connection(
        fviz_filter_algorithm(warp), 0u, fviz_filter_output_port(smooth)) == FVIZ_OK);
    CHECK(fviz_algorithm_set_input_connection(
        fviz_filter_algorithm(surface), 0u, fviz_filter_output_port(warp)) == FVIZ_OK);
    CHECK(fviz_filter_set_input_connection(slice, warp) == FVIZ_OK);
    CHECK(fviz_filter_input_connection(warp) == smooth);
    CHECK(fviz_algorithm_set_input_connection(
        fviz_filter_algorithm(smooth), 0u, fviz_filter_output_port(warp)) ==
        FVIZ_ERROR_INVALID_ARGUMENT);

    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    mapper = fviz_actor_mapper(actor);
    CHECK(fviz_mapper_set_algorithm_connection(mapper, fviz_filter_output_port(warp)) ==
        FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_mapper_set_algorithm_connection(mapper, fviz_filter_output_port(surface)) == FVIZ_OK);
    CHECK(fviz_mapper_input_connection(mapper) == surface);
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) == FVIZ_OK);
    CHECK(fviz_renderer_update(renderer) == FVIZ_OK);
    first_surface = fviz_mapper_poly_data(mapper);
    CHECK(first_surface != NULL);
    CHECK(fviz_retain(first_surface) == first_surface);
    CHECK(first_surface == fviz_filter_poly_data_output(surface));
    CHECK(fviz_poly_data_triangle_count(first_surface) == 48u);
    CHECK(fviz_poly_data_has_normals(first_surface) == FVIZ_TRUE);
    CHECK(fviz_poly_data_const_scalars(first_surface) != NULL);
    points = fviz_poly_data_points(first_surface);
    CHECK(fabsf(points[2].x - 2.4f) < 1.0e-4f);
    CHECK(fviz_renderer_update(renderer) == FVIZ_OK);
    CHECK(fviz_mapper_poly_data(mapper) == first_surface);

    CHECK(fviz_warp_filter_set_scale(warp, 3.0) == FVIZ_OK);
    CHECK(fviz_renderer_update(renderer) == FVIZ_OK);
    second_surface = fviz_mapper_poly_data(mapper);
    CHECK(second_surface != NULL && second_surface != first_surface);
    CHECK(fviz_retain(second_surface) == second_surface);
    points = fviz_poly_data_points(second_surface);
    CHECK(fabsf(points[2].x - 2.6f) < 1.0e-4f);
    fviz_release(first_surface);
    first_surface = NULL;

    grid_mtime = fviz_object_mtime((const FVizObject*)grid);
    {
        const float changed_displacement[3] = {1.0f, 0.0f, 0.0f};
        CHECK(fviz_data_array_set_tuple(displacement, 2u, changed_displacement) == FVIZ_OK);
    }
    CHECK(fviz_object_mtime((const FVizObject*)grid) > grid_mtime);
    CHECK(fviz_renderer_update(renderer) == FVIZ_OK);
    mutated_surface = fviz_mapper_poly_data(mapper);
    CHECK(mutated_surface != NULL && mutated_surface != second_surface);
    points = fviz_poly_data_points(mutated_surface);
    CHECK(fabsf(points[2].x - 5.0f) < 1.0e-4f);
    fviz_release(second_surface);
    second_surface = NULL;

    CHECK(fviz_filter_update(slice) == FVIZ_OK);
    CHECK(fviz_filter_poly_data_output(slice) != NULL);
    CHECK(fviz_poly_data_triangle_count(fviz_filter_poly_data_output(slice)) > 0u);
    CHECK(fviz_slice_filter_set_plane(slice,
        fviz_plane_from_point_normal(fviz_vec3(0.0f, 0.5f, 0.0f), fviz_vec3(0.0f, 1.0f, 0.0f))) == FVIZ_OK);
    CHECK(fviz_filter_update(slice) == FVIZ_OK);

    fviz_release(renderer);
    fviz_release(actor);
    fviz_release(slice);
    fviz_release(surface);
    fviz_release(warp);
    fviz_release(smooth);
    fviz_release(displacement);
    fviz_release(stress);
    fviz_release(grid);
    return 0;
}

int main(void)
{
    CHECK(test_threshold_filter() == 0);
    CHECK(test_warp_filter() == 0);
    CHECK(test_cell_to_point_filter() == 0);
    CHECK(test_transform_filter() == 0);
    CHECK(test_connected_render_pipeline() == 0);
    return 0;
}
