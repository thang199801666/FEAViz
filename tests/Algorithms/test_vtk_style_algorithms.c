#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

static int near_value(double a, double b)
{
    return fabs(a - b) < 1.0e-5;
}

int main(void)
{
    FVizPlaneSource* plane = NULL;
    FVizCubeSource* cube = NULL;
    FVizSphereSource* sphere = NULL;
    FVizElevationFilter* elevation = NULL;
    FVizTransform* transform = NULL;
    FVizTransformPolyDataFilter* transform_filter = NULL;
    FVizAppendPolyDataFilter* append = NULL;
    FVizCleanPolyDataFilter* clean = NULL;
    FVizMapper* mapper = NULL;
    FVizPolyData* dirty = NULL;
    FVizDataArray* dirty_values = NULL;
    FVizDataArray* dirty_cells = NULL;
    FVizPolyData* data;
    const FVizDataArray* scalars;
    FVizBounds bounds;
    double minimum = 0.0;
    double maximum = 0.0;
    uint64_t executions;

    CHECK(fviz_plane_source_create(&plane) == FVIZ_OK);
    CHECK(fviz_plane_source_set_resolution(plane, 2u, 1u) == FVIZ_OK);
    CHECK(fviz_plane_source_update(plane) == FVIZ_OK);
    data = fviz_plane_source_output(plane);
    CHECK(data != NULL);
    CHECK(fviz_poly_data_point_count(data) == 6u);
    CHECK(fviz_poly_data_triangle_count(data) == 4u);
    bounds = fviz_poly_data_bounds(data);
    CHECK(bounds.valid == FVIZ_TRUE);
    CHECK(near_value(bounds.min.x, -0.5) && near_value(bounds.max.x, 0.5));

    executions = fviz_executive_execution_count(fviz_algorithm_executive(fviz_plane_source_algorithm(plane)));
    CHECK(fviz_plane_source_update(plane) == FVIZ_OK);
    CHECK(fviz_executive_execution_count(fviz_algorithm_executive(fviz_plane_source_algorithm(plane))) == executions);
    fviz_plane_source_set_point1(plane, fviz_vec3(1.5f, -0.5f, 0.0f));
    CHECK(fviz_plane_source_update(plane) == FVIZ_OK);
    CHECK(fviz_executive_execution_count(fviz_algorithm_executive(fviz_plane_source_algorithm(plane))) > executions);

    CHECK(fviz_elevation_filter_create(&elevation) == FVIZ_OK);
    fviz_elevation_filter_set_low_point(elevation, fviz_vec3(-0.5f, 0.0f, 0.0f));
    fviz_elevation_filter_set_high_point(elevation, fviz_vec3(1.5f, 0.0f, 0.0f));
    fviz_elevation_filter_set_scalar_range(elevation, 10.0, 20.0);
    CHECK(fviz_elevation_filter_set_input_connection(elevation, fviz_plane_source_output_port(plane)) == FVIZ_OK);
    CHECK(fviz_elevation_filter_update(elevation) == FVIZ_OK);
    data = fviz_elevation_filter_output(elevation);
    CHECK(data != NULL);
    scalars = fviz_attribute_set_const_get(fviz_poly_data_const_point_data(data), "Elevation");
    CHECK(scalars != NULL);
    CHECK(fviz_data_array_get_range(scalars, 0, FVIZ_TRUE, &minimum, &maximum) == FVIZ_OK);
    CHECK(near_value(minimum, 10.0) && near_value(maximum, 20.0));

    CHECK(fviz_transform_create(&transform) == FVIZ_OK);
    fviz_transform_translate(transform, fviz_vec3(0.0f, 0.0f, 2.0f));
    CHECK(fviz_transform_poly_data_filter_create(transform, &transform_filter) == FVIZ_OK);
    CHECK(fviz_transform_poly_data_filter_set_input_connection(
        transform_filter, fviz_elevation_filter_output_port(elevation)) == FVIZ_OK);
    CHECK(fviz_transform_poly_data_filter_update(transform_filter) == FVIZ_OK);
    data = fviz_transform_poly_data_filter_output(transform_filter);
    CHECK(data != NULL);
    bounds = fviz_poly_data_bounds(data);
    CHECK(near_value(bounds.min.z, 2.0) && near_value(bounds.max.z, 2.0));
    scalars = fviz_attribute_set_const_get(fviz_poly_data_const_point_data(data), "Elevation");
    CHECK(scalars != NULL);

    CHECK(fviz_cube_source_create(&cube) == FVIZ_OK);
    CHECK(fviz_cube_source_set_lengths(cube, 2.0, 4.0, 6.0) == FVIZ_OK);
    fviz_cube_source_set_center(cube, fviz_vec3(3.0f, 0.0f, 0.0f));
    CHECK(fviz_cube_source_update(cube) == FVIZ_OK);
    data = fviz_cube_source_output(cube);
    CHECK(fviz_poly_data_point_count(data) == 24u);
    CHECK(fviz_poly_data_triangle_count(data) == 12u);
    CHECK(fviz_poly_data_has_normals(data) == FVIZ_TRUE);
    bounds = fviz_poly_data_bounds(data);
    CHECK(near_value(bounds.min.x, 2.0) && near_value(bounds.max.x, 4.0));
    CHECK(near_value(bounds.min.y, -2.0) && near_value(bounds.max.y, 2.0));
    CHECK(near_value(bounds.min.z, -3.0) && near_value(bounds.max.z, 3.0));

    CHECK(fviz_sphere_source_create(&sphere) == FVIZ_OK);
    CHECK(fviz_sphere_source_set_resolution(sphere, 8u, 4u) == FVIZ_OK);
    CHECK(fviz_sphere_source_set_radius(sphere, 2.0) == FVIZ_OK);
    CHECK(fviz_sphere_source_update(sphere) == FVIZ_OK);
    data = fviz_sphere_source_output(sphere);
    CHECK(fviz_poly_data_point_count(data) == 26u);
    CHECK(fviz_poly_data_triangle_count(data) == 48u);
    CHECK(fviz_poly_data_has_normals(data) == FVIZ_TRUE);
    bounds = fviz_poly_data_bounds(data);
    CHECK(near_value(bounds.min.z, -2.0) && near_value(bounds.max.z, 2.0));

    CHECK(fviz_append_poly_data_filter_create(&append) == FVIZ_OK);
    CHECK(fviz_append_poly_data_filter_set_input_connection(
        append, fviz_transform_poly_data_filter_output_port(transform_filter)) == FVIZ_OK);
    CHECK(fviz_append_poly_data_filter_add_input_connection(
        append, fviz_cube_source_output_port(cube)) == FVIZ_OK);
    CHECK(fviz_append_poly_data_filter_add_input_connection(
        append, fviz_sphere_source_output_port(sphere)) == FVIZ_OK);
    CHECK(fviz_append_poly_data_filter_input_count(append) == 3u);
    CHECK(fviz_append_poly_data_filter_update(append) == FVIZ_OK);
    data = fviz_append_poly_data_filter_output(append);
    CHECK(data != NULL);
    CHECK(fviz_poly_data_point_count(data) == 6u + 24u + 26u);
    CHECK(fviz_poly_data_triangle_count(data) == 4u + 12u + 48u);
    CHECK(fviz_poly_data_validate(data) == FVIZ_OK);
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_point_data(data), "Elevation") == NULL);

    CHECK(fviz_mapper_create(&mapper) == FVIZ_OK);
    CHECK(fviz_mapper_set_algorithm_connection(mapper, fviz_append_poly_data_filter_output_port(append)) == FVIZ_OK);
    CHECK(fviz_mapper_update(mapper) == FVIZ_OK);
    CHECK(fviz_mapper_poly_data(mapper) == data);

    CHECK(fviz_poly_data_create(&dirty) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(dirty, fviz_vec3(0.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(dirty, fviz_vec3(1.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(dirty, fviz_vec3(0.0f, 1.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(dirty, fviz_vec3(0.0001f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(dirty, 0u, 1u, 2u) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(dirty, 3u, 1u, 2u) == FVIZ_OK);
    CHECK(fviz_poly_data_add_line(dirty, 0u, 3u) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &dirty_values) == FVIZ_OK);
    {
        float values[4] = {10.0f, 20.0f, 30.0f, 99.0f};
        uint32_t i;
        for (i = 0u; i < 4u; ++i) CHECK(fviz_data_array_append_tuple(dirty_values, &values[i]) == FVIZ_OK);
    }
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(dirty), "Value", dirty_values) == FVIZ_OK);
    CHECK(fviz_poly_data_set_scalars(dirty, dirty_values) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &dirty_cells) == FVIZ_OK);
    {
        uint64_t ids[2] = {100u, 101u};
        CHECK(fviz_data_array_append_tuple(dirty_cells, &ids[0]) == FVIZ_OK);
        CHECK(fviz_data_array_append_tuple(dirty_cells, &ids[1]) == FVIZ_OK);
    }
    CHECK(fviz_attribute_set_add(fviz_poly_data_cell_data(dirty), "FVizOriginalCellIds", dirty_cells) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_field_data(dirty), "MetadataIds", dirty_cells) == FVIZ_OK);
    CHECK(fviz_clean_poly_data_filter_create(&clean) == FVIZ_OK);
    CHECK(fviz_clean_poly_data_filter_set_tolerance(clean, 0.001) == FVIZ_OK);
    CHECK(fviz_clean_poly_data_filter_set_input_data(clean, dirty) == FVIZ_OK);
    CHECK(fviz_clean_poly_data_filter_update(clean) == FVIZ_OK);
    data = fviz_clean_poly_data_filter_output(clean);
    CHECK(data != NULL);
    CHECK(fviz_poly_data_point_count(data) == 3u);
    CHECK(fviz_poly_data_triangle_count(data) == 2u);
    CHECK(fviz_poly_data_line_count(data) == 0u);
    scalars = fviz_attribute_set_const_get(fviz_poly_data_const_point_data(data), "Value");
    CHECK(scalars != NULL && fviz_data_array_tuple_count(scalars) == 3u);
    CHECK(fviz_data_array_get_component(scalars, 0u, 0u, &minimum) == FVIZ_OK && near_value(minimum, 10.0));
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(data), "FVizOriginalCellIds") != NULL);
    CHECK(fviz_data_array_tuple_count(fviz_attribute_set_const_get(
        fviz_poly_data_const_cell_data(data), "FVizOriginalCellIds")) == 2u);
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_field_data(data), "MetadataIds") != NULL);
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_field_data(data), "MetadataIds") != dirty_cells);

    fviz_append_poly_data_filter_remove_all_inputs(append);
    CHECK(fviz_append_poly_data_filter_input_count(append) == 0u);
    CHECK(fviz_append_poly_data_filter_update(append) == FVIZ_ERROR_INVALID_STATE);

    fviz_release(clean);
    fviz_release(dirty_cells);
    fviz_release(dirty_values);
    fviz_release(dirty);
    fviz_release(mapper);
    fviz_release(append);
    fviz_release(sphere);
    fviz_release(cube);
    fviz_release(transform_filter);
    fviz_release(transform);
    fviz_release(elevation);
    fviz_release(plane);
    puts("VTK-style source/filter tests passed");
    return 0;
}
