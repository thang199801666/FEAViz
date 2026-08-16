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
    FVizClipPolyDataFilter* clip = NULL;
    FVizSmoothPolyDataFilter* smooth = NULL;
    FVizDecimatePolyDataFilter* decimate = NULL;
    FVizPolyData* output;
    FVizResult result = FVIZ_OK;
    int status = 1;

    if ((result = fviz_sphere_source_create(&sphere)) != FVIZ_OK ||
        (result = fviz_sphere_source_set_resolution(sphere, 96u, 48u)) != FVIZ_OK)
        goto done;

    if ((result = fviz_clip_poly_data_filter_create(&clip)) != FVIZ_OK) goto done;
    fviz_clip_poly_data_filter_set_plane(
        clip, fviz_plane_from_point_normal(fviz_vec3(0.0f, 0.0f, 0.0f), fviz_vec3(1.0f, 0.0f, 0.0f)));
    if ((result = fviz_clip_poly_data_filter_set_input_connection(
             clip, fviz_sphere_source_output_port(sphere))) != FVIZ_OK)
        goto done;

    if ((result = fviz_smooth_poly_data_filter_create(&smooth)) != FVIZ_OK ||
        (result = fviz_smooth_poly_data_filter_set_iterations(smooth, 8u)) != FVIZ_OK ||
        (result = fviz_smooth_poly_data_filter_set_relaxation_factor(smooth, 0.05)) != FVIZ_OK ||
        (result = fviz_smooth_poly_data_filter_set_input_connection(
             smooth, fviz_clip_poly_data_filter_output_port(clip))) != FVIZ_OK)
        goto done;

    if ((result = fviz_decimate_poly_data_filter_create(&decimate)) != FVIZ_OK ||
        (result = fviz_decimate_poly_data_filter_set_target_reduction(decimate, 0.55)) != FVIZ_OK ||
        (result = fviz_decimate_poly_data_filter_set_input_connection(
             decimate, fviz_smooth_poly_data_filter_output_port(smooth))) != FVIZ_OK ||
        (result = fviz_decimate_poly_data_filter_update(decimate)) != FVIZ_OK)
        goto done;

    output = fviz_decimate_poly_data_filter_output(decimate);
    if (output == NULL || fviz_poly_data_validate(output) != FVIZ_OK)
    {
        result = FVIZ_ERROR_INVALID_STATE;
        goto done;
    }

    printf("Mesh-processing pipeline: %zu points / %zu triangles -> decimated %zu points / %zu triangles\n",
        (size_t)fviz_poly_data_point_count(fviz_smooth_poly_data_filter_output(smooth)),
        (size_t)fviz_poly_data_triangle_count(fviz_smooth_poly_data_filter_output(smooth)),
        (size_t)fviz_poly_data_point_count(output),
        (size_t)fviz_poly_data_triangle_count(output));
    status = 0;

done:
    if (status != 0) (void)fail(result, "mesh-processing pipeline");
    fviz_release(decimate);
    fviz_release(smooth);
    fviz_release(clip);
    fviz_release(sphere);
    return status;
}
