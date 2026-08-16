#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

static int nearf_value(float a, float b)
{
    return fabsf(a - b) < 1.0e-5f;
}

int main(void)
{
    FVizPolyData* mesh = NULL;
    FVizSmoothPolyDataFilter* smooth = NULL;
    FVizDecimatePolyDataFilter* decimate = NULL;
    FVizClipPolyDataFilter* clip = NULL;
    FVizPlaneSource* plane = NULL;
    FVizPolyData* output;
    const FVizVec3 points[5] = {
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f},
        {-1.0f,  1.0f, 0.0f},
        { 0.0f,  0.0f, 1.0f}
    };
    const uint32_t triangles[12] = {
        0u, 1u, 4u,
        1u, 2u, 4u,
        2u, 3u, 4u,
        3u, 0u, 4u
    };
    const FVizVec3* out_points;

    CHECK(fviz_poly_data_create(&mesh) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points(mesh, points, 5u, NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangles(mesh, triangles, 4u) == FVIZ_OK);
    CHECK(fviz_poly_data_validate(mesh) == FVIZ_OK);

    CHECK(fviz_smooth_poly_data_filter_create(&smooth) == FVIZ_OK);
    CHECK(fviz_smooth_poly_data_filter_set_iterations(smooth, 1u) == FVIZ_OK);
    CHECK(fviz_smooth_poly_data_filter_set_relaxation_factor(smooth, 1.0) == FVIZ_OK);
    fviz_smooth_poly_data_filter_set_boundary_smoothing(smooth, FVIZ_FALSE);
    CHECK(fviz_smooth_poly_data_filter_set_input_data(smooth, mesh) == FVIZ_OK);
    CHECK(fviz_smooth_poly_data_filter_update(smooth) == FVIZ_OK);
    output = fviz_smooth_poly_data_filter_output(smooth);
    CHECK(output != NULL);
    CHECK(fviz_poly_data_point_count(output) == 5u);
    CHECK(fviz_poly_data_triangle_count(output) == 4u);
    out_points = fviz_poly_data_points(output);
    CHECK(nearf_value(out_points[0].z, 0.0f));
    CHECK(nearf_value(out_points[1].z, 0.0f));
    CHECK(nearf_value(out_points[2].z, 0.0f));
    CHECK(nearf_value(out_points[3].z, 0.0f));
    CHECK(nearf_value(out_points[4].z, 0.0f));
    CHECK(fviz_poly_data_has_normals(output) == FVIZ_TRUE);

    CHECK(fviz_plane_source_create(&plane) == FVIZ_OK);
    CHECK(fviz_plane_source_set_resolution(plane, 20u, 20u) == FVIZ_OK);
    CHECK(fviz_plane_source_update(plane) == FVIZ_OK);
    CHECK(fviz_poly_data_triangle_count(fviz_plane_source_output(plane)) == 800u);
    CHECK(fviz_decimate_poly_data_filter_create(&decimate) == FVIZ_OK);
    CHECK(fviz_decimate_poly_data_filter_set_target_reduction(decimate, 0.8) == FVIZ_OK);
    CHECK(fviz_decimate_poly_data_filter_set_input_connection(decimate, fviz_plane_source_output_port(plane)) == FVIZ_OK);
    CHECK(fviz_decimate_poly_data_filter_update(decimate) == FVIZ_OK);
    output = fviz_decimate_poly_data_filter_output(decimate);
    CHECK(output != NULL);
    CHECK(fviz_poly_data_triangle_count(output) > 0u);
    CHECK(fviz_poly_data_triangle_count(output) < 800u);
    CHECK(fviz_poly_data_point_count(output) < fviz_poly_data_point_count(fviz_plane_source_output(plane)));
    CHECK(fviz_poly_data_validate(output) == FVIZ_OK);
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(output), "FVizOriginalCellIds") != NULL);

    {
        FVizDataArray* x_values = NULL;
        const float values[5] = {-1.0f, 1.0f, 1.0f, -1.0f, 0.0f};
        const FVizDataArray* clipped_values;
        FVizSize i;
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &x_values) == FVIZ_OK);
        CHECK(fviz_data_array_append_tuples(x_values, values, 5u) == FVIZ_OK);
        CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(mesh), "X", x_values) == FVIZ_OK);
        CHECK(fviz_attribute_set_set_active(fviz_poly_data_point_data(mesh), FVIZ_ATTRIBUTE_SCALARS, "X") == FVIZ_OK);
        fviz_release(x_values);

        CHECK(fviz_clip_poly_data_filter_create(&clip) == FVIZ_OK);
        fviz_clip_poly_data_filter_set_plane(clip, fviz_plane_from_point_normal(
            fviz_vec3(0.0f, 0.0f, 0.0f), fviz_vec3(1.0f, 0.0f, 0.0f)));
        CHECK(fviz_clip_poly_data_filter_set_input_data(clip, mesh) == FVIZ_OK);
        CHECK(fviz_clip_poly_data_filter_update(clip) == FVIZ_OK);
        output = fviz_clip_poly_data_filter_output(clip);
        CHECK(output != NULL);
        CHECK(fviz_poly_data_triangle_count(output) > 0u);
        CHECK(fviz_poly_data_validate(output) == FVIZ_OK);
        clipped_values = fviz_attribute_set_const_get(fviz_poly_data_const_point_data(output), "X");
        CHECK(clipped_values != NULL);
        CHECK(fviz_data_array_tuple_count(clipped_values) == fviz_poly_data_point_count(output));
        for (i = 0u; i < fviz_poly_data_point_count(output); ++i)
        {
            double value = 0.0;
            CHECK(fviz_poly_data_points(output)[i].x >= -1.0e-5f);
            CHECK(fviz_data_array_get_component(clipped_values, i, 0u, &value) == FVIZ_OK);
            CHECK(fabs(value - fviz_poly_data_points(output)[i].x) < 1.0e-5);
        }
        CHECK(fviz_attribute_set_const_get(
            fviz_poly_data_const_cell_data(output), "FVizOriginalCellIds") != NULL);

        fviz_clip_poly_data_filter_set_inside_out(clip, FVIZ_TRUE);
        CHECK(fviz_clip_poly_data_filter_update(clip) == FVIZ_OK);
        output = fviz_clip_poly_data_filter_output(clip);
        CHECK(output != NULL);
        for (i = 0u; i < fviz_poly_data_point_count(output); ++i)
            CHECK(fviz_poly_data_points(output)[i].x <= 1.0e-5f);
    }

    fviz_release(clip);
    fviz_release(decimate);
    fviz_release(plane);
    fviz_release(smooth);
    fviz_release(mesh);
    return 0;
}
