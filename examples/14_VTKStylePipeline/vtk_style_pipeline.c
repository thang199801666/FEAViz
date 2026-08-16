#include <stdio.h>

#include <FViz/FViz.h>

static int fail(FVizResult result, const char* operation)
{
    fprintf(stderr, "%s failed (%d): %s\n", operation, (int)result, fviz_last_error_message());
    return 1;
}

int main(void)
{
    FVizSphereSource* sphere = NULL;
    FVizElevationFilter* elevation = NULL;
    FVizTransform* transform = NULL;
    FVizTransformPolyDataFilter* moved = NULL;
    FVizCubeSource* cube = NULL;
    FVizAppendPolyDataFilter* append = NULL;
    FVizCleanPolyDataFilter* clean = NULL;
    FVizPolyData* output;
    FVizResult result;
    int status = 1;

    result = fviz_sphere_source_create(&sphere);
    if (result != FVIZ_OK) return fail(result, "create sphere");
    if ((result = fviz_sphere_source_set_resolution(sphere, 32u, 16u)) != FVIZ_OK ||
        (result = fviz_sphere_source_set_radius(sphere, 0.75)) != FVIZ_OK)
        goto done;

    if ((result = fviz_elevation_filter_create(&elevation)) != FVIZ_OK) goto done;
    fviz_elevation_filter_set_low_point(elevation, fviz_vec3(0.0f, 0.0f, -0.75f));
    fviz_elevation_filter_set_high_point(elevation, fviz_vec3(0.0f, 0.0f, 0.75f));
    fviz_elevation_filter_set_scalar_range(elevation, 0.0, 1.0);
    if ((result = fviz_elevation_filter_set_input_connection(
             elevation, fviz_sphere_source_output_port(sphere))) != FVIZ_OK)
        goto done;

    if ((result = fviz_transform_create(&transform)) != FVIZ_OK) goto done;
    fviz_transform_translate(transform, fviz_vec3(1.25f, 0.0f, 0.0f));
    if ((result = fviz_transform_poly_data_filter_create(transform, &moved)) != FVIZ_OK ||
        (result = fviz_transform_poly_data_filter_set_input_connection(
             moved, fviz_elevation_filter_output_port(elevation))) != FVIZ_OK)
        goto done;

    if ((result = fviz_cube_source_create(&cube)) != FVIZ_OK) goto done;
    fviz_cube_source_set_center(cube, fviz_vec3(-1.25f, 0.0f, 0.0f));
    if ((result = fviz_cube_source_set_lengths(cube, 1.0, 1.0, 1.0)) != FVIZ_OK) goto done;

    if ((result = fviz_append_poly_data_filter_create(&append)) != FVIZ_OK ||
        (result = fviz_append_poly_data_filter_set_input_connection(
             append, fviz_transform_poly_data_filter_output_port(moved))) != FVIZ_OK ||
        (result = fviz_append_poly_data_filter_add_input_connection(
             append, fviz_cube_source_output_port(cube))) != FVIZ_OK)
        goto done;

    if ((result = fviz_clean_poly_data_filter_create(&clean)) != FVIZ_OK ||
        (result = fviz_clean_poly_data_filter_set_input_connection(
             clean, fviz_append_poly_data_filter_output_port(append))) != FVIZ_OK ||
        (result = fviz_clean_poly_data_filter_set_tolerance(clean, 1.0e-6)) != FVIZ_OK ||
        (result = fviz_clean_poly_data_filter_update(clean)) != FVIZ_OK)
        goto done;

    output = fviz_clean_poly_data_filter_output(clean);
    if (output == NULL || fviz_poly_data_validate(output) != FVIZ_OK)
    {
        result = FVIZ_ERROR_INVALID_STATE;
        goto done;
    }

    printf("VTK-style pipeline output: %zu points, %zu triangles, %zu lines\n",
        (size_t)fviz_poly_data_point_count(output),
        (size_t)fviz_poly_data_triangle_count(output),
        (size_t)fviz_poly_data_line_count(output));
    status = 0;

done:
    if (status != 0) (void)fail(result, "VTK-style pipeline");
    fviz_release(clean);
    fviz_release(append);
    fviz_release(cube);
    fviz_release(moved);
    fviz_release(transform);
    fviz_release(elevation);
    fviz_release(sphere);
    return status;
}
