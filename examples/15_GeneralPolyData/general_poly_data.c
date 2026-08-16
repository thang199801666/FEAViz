#include <stdint.h>
#include <stdio.h>

#include <FViz/FViz.h>

static int fail(FVizResult result, const char* operation)
{
    fprintf(stderr, "%s failed (%d): %s\n", operation, (int)result, fviz_last_error_message());
    return 1;
}

int main(void)
{
    FVizPolyData* input = NULL;
    FVizCleanPolyDataFilter* clean = NULL;
    FVizTriangleFilter* triangles = NULL;
    FVizPolyDataNormalsFilter* normals = NULL;
    FVizPolyDataConnectivityFilter* connectivity = NULL;
    FVizFeatureEdgesFilter* edges = NULL;
    FVizPolyData* surface;
    FVizPolyData* regions;
    FVizPolyData* feature_edges;
    FVizResult result = FVIZ_OK;
    int status = 1;
    const uint32_t polyline[4] = {0u, 6u, 1u, 2u};
    const uint32_t strip[4] = {1u, 4u, 2u, 5u};

    if ((result = fviz_poly_data_create(&input)) != FVIZ_OK) goto done;
    if ((result = fviz_poly_data_add_point(input, fviz_vec3(0.0f, 0.0f, 0.0f), NULL)) != FVIZ_OK ||
        (result = fviz_poly_data_add_point(input, fviz_vec3(1.0f, 0.0f, 0.0f), NULL)) != FVIZ_OK ||
        (result = fviz_poly_data_add_point(input, fviz_vec3(1.0f, 1.0f, 0.0f), NULL)) != FVIZ_OK ||
        (result = fviz_poly_data_add_point(input, fviz_vec3(0.0f, 1.0f, 0.0f), NULL)) != FVIZ_OK ||
        (result = fviz_poly_data_add_point(input, fviz_vec3(1.0f, 0.0f, 1.0f), NULL)) != FVIZ_OK ||
        (result = fviz_poly_data_add_point(input, fviz_vec3(1.0f, 1.0f, 1.0f), NULL)) != FVIZ_OK ||
        (result = fviz_poly_data_add_point(input, fviz_vec3(0.00001f, 0.0f, 0.0f), NULL)) != FVIZ_OK ||
        (result = fviz_poly_data_add_quad(input, 0u, 1u, 2u, 3u)) != FVIZ_OK ||
        (result = fviz_poly_data_add_poly_line(input, 4u, polyline)) != FVIZ_OK ||
        (result = fviz_poly_data_add_triangle_strip(input, 4u, strip)) != FVIZ_OK)
        goto done;

    if ((result = fviz_clean_poly_data_filter_create(&clean)) != FVIZ_OK ||
        (result = fviz_clean_poly_data_filter_set_tolerance(clean, 1.0e-4)) != FVIZ_OK ||
        (result = fviz_clean_poly_data_filter_set_input_data(clean, input)) != FVIZ_OK)
        goto done;

    if ((result = fviz_triangle_filter_create(&triangles)) != FVIZ_OK ||
        (result = fviz_triangle_filter_set_input_connection(
             triangles, fviz_clean_poly_data_filter_output_port(clean))) != FVIZ_OK)
        goto done;

    if ((result = fviz_poly_data_normals_filter_create(&normals)) != FVIZ_OK ||
        (result = fviz_poly_data_normals_filter_set_input_connection(
             normals, fviz_triangle_filter_output_port(triangles))) != FVIZ_OK ||
        (result = fviz_poly_data_normals_filter_update(normals)) != FVIZ_OK)
        goto done;

    surface = fviz_poly_data_normals_filter_output(normals);
    if (surface == NULL || fviz_poly_data_validate(surface) != FVIZ_OK)
    {
        result = FVIZ_ERROR_INVALID_STATE;
        goto done;
    }

    if ((result = fviz_poly_data_connectivity_filter_create(&connectivity)) != FVIZ_OK ||
        (result = fviz_poly_data_connectivity_filter_set_input_connection(
             connectivity, fviz_poly_data_normals_filter_output_port(normals))) != FVIZ_OK ||
        (result = fviz_poly_data_connectivity_filter_update(connectivity)) != FVIZ_OK)
        goto done;
    regions = fviz_poly_data_connectivity_filter_output(connectivity);

    if ((result = fviz_feature_edges_filter_create(&edges)) != FVIZ_OK ||
        (result = fviz_feature_edges_filter_set_input_connection(
             edges, fviz_poly_data_normals_filter_output_port(normals))) != FVIZ_OK ||
        (result = fviz_feature_edges_filter_set_feature_angle(edges, 45.0)) != FVIZ_OK ||
        (result = fviz_feature_edges_filter_update(edges)) != FVIZ_OK)
        goto done;
    feature_edges = fviz_feature_edges_filter_output(edges);

    printf("General PolyData pipeline: %zu points, %zu triangles, %zu lines, %u regions, %zu selected edges\n",
        (size_t)fviz_poly_data_point_count(surface),
        (size_t)fviz_poly_data_triangle_count(surface),
        (size_t)fviz_poly_data_line_count(surface),
        (unsigned)fviz_poly_data_connectivity_filter_region_count(connectivity),
        (size_t)fviz_poly_data_line_count(feature_edges));

    if (regions == NULL || feature_edges == NULL ||
        fviz_poly_data_point_count(surface) != 6u ||
        fviz_poly_data_triangle_count(surface) != 4u ||
        fviz_poly_data_line_count(surface) != 2u)
    {
        result = FVIZ_ERROR_INVALID_STATE;
        goto done;
    }
    status = 0;

done:
    if (status != 0) (void)fail(result, "general PolyData pipeline");
    fviz_release(edges);
    fviz_release(connectivity);
    fviz_release(normals);
    fviz_release(triangles);
    fviz_release(clean);
    fviz_release(input);
    return status;
}
