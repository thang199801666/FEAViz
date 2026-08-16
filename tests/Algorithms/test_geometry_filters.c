#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

static int near_value(double a, double b)
{
    return fabs(a - b) < 1.0e-5;
}

int main(void)
{
    FVizCellArray* cells = NULL;
    FVizCellArray* cells_copy = NULL;
    FVizPolyData* generic = NULL;
    FVizPolyData* tri_output;
    FVizPolyData* normal_output;
    FVizPolyData* crease = NULL;
    FVizPolyData* edge_output;
    FVizTriangleFilter* triangle = NULL;
    FVizPolyDataNormalsFilter* normals_filter = NULL;
    FVizFeatureEdgesFilter* feature_edges = NULL;
    FVizAppendPolyDataFilter* append_generic = NULL;
    FVizCleanPolyDataFilter* clean_generic = NULL;
    FVizPolyDataConnectivityFilter* connectivity = NULL;
    FVizPolyData* disconnected = NULL;
    FVizPolyData* dirty_generic = NULL;
    FVizDataArray* generic_cell_ids = NULL;
    FVizDataArray* generic_original_ids = NULL;
    FVizDataArray* dirty_generic_cell_ids = NULL;
    const FVizDataArray* array;
    const FVizVec3* normals;
    FVizId wide_ids[20];
    uint32_t poly_vertex_ids[2] = {0u, 1u};
    uint32_t poly_line_ids[3] = {0u, 1u, 2u};
    uint32_t strip_ids[4] = {0u, 1u, 4u, 5u};
    uint32_t i;

    CHECK(fviz_cell_array_create(&cells) == FVIZ_OK);
    for (i = 0u; i < 20u; ++i) wide_ids[i] = i;
    CHECK(fviz_cell_array_append_ids(cells, FVIZ_CELL_POLY_LINE, 20u, wide_ids) == FVIZ_OK);
    CHECK(fviz_cell_array_point_count(cells, 0u) == 20u);
    CHECK(fviz_cell_array_validate(cells, 20u) == FVIZ_OK);
    CHECK(fviz_cell_array_deep_copy(cells, &cells_copy) == FVIZ_OK);
    CHECK(cells_copy != cells && fviz_cell_array_count(cells_copy) == 1u);
    CHECK(fviz_cell_array_point_ids(cells_copy, 0u)[19] == 19u);

    CHECK(fviz_poly_data_create(&generic) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(generic, fviz_vec3(0.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(generic, fviz_vec3(1.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(generic, fviz_vec3(1.0f, 1.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(generic, fviz_vec3(0.0f, 1.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(generic, fviz_vec3(1.0f, 0.0f, 1.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(generic, fviz_vec3(1.0f, 1.0f, 1.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_poly_vertex(generic, 2u, poly_vertex_ids) == FVIZ_OK);
    CHECK(fviz_poly_data_add_poly_line(generic, 3u, poly_line_ids) == FVIZ_OK);
    CHECK(fviz_poly_data_add_quad(generic, 0u, 1u, 2u, 3u) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle_strip(generic, 4u, strip_ids) == FVIZ_OK);
    CHECK(fviz_poly_data_vert_cell_count(generic) == 1u);
    CHECK(fviz_poly_data_line_cell_count(generic) == 1u);
    CHECK(fviz_poly_data_poly_cell_count(generic) == 1u);
    CHECK(fviz_poly_data_strip_cell_count(generic) == 1u);
    CHECK(fviz_poly_data_cell_count(generic) == 4u);
    CHECK(fviz_poly_data_triangle_count(generic) == 0u);
    CHECK(fviz_poly_data_line_count(generic) == 0u);
    CHECK(fviz_poly_data_validate(generic) == FVIZ_OK);

    CHECK(fviz_data_array_create(FVIZ_DATA_UINT32, 1u, &generic_cell_ids) == FVIZ_OK);
    for (i = 0u; i < 4u; ++i)
    {
        const uint32_t value = 10u + i;
        CHECK(fviz_data_array_append_tuple(generic_cell_ids, &value) == FVIZ_OK);
    }
    CHECK(fviz_attribute_set_add(fviz_poly_data_cell_data(generic), "CellTag", generic_cell_ids) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &generic_original_ids) == FVIZ_OK);
    for (i = 0u; i < 4u; ++i)
    {
        const uint64_t value = 1000u + i;
        CHECK(fviz_data_array_append_tuple(generic_original_ids, &value) == FVIZ_OK);
    }
    CHECK(fviz_attribute_set_add(
        fviz_poly_data_cell_data(generic), "FVizOriginalCellIds", generic_original_ids) == FVIZ_OK);

    CHECK(fviz_triangle_filter_create(&triangle) == FVIZ_OK);
    CHECK(fviz_triangle_filter_set_input_data(triangle, generic) == FVIZ_OK);
    CHECK(fviz_triangle_filter_update(triangle) == FVIZ_OK);
    tri_output = fviz_triangle_filter_output(triangle);
    CHECK(tri_output != NULL);
    CHECK(fviz_poly_data_point_count(tri_output) == 6u);
    CHECK(fviz_poly_data_vert_cell_count(tri_output) == 2u);
    CHECK(fviz_poly_data_line_count(tri_output) == 2u);
    CHECK(fviz_poly_data_triangle_count(tri_output) == 4u);
    CHECK(fviz_poly_data_strip_cell_count(tri_output) == 0u);
    CHECK(fviz_poly_data_cell_count(tri_output) == 8u);
    CHECK(fviz_poly_data_validate(tri_output) == FVIZ_OK);

    CHECK(fviz_append_poly_data_filter_create(&append_generic) == FVIZ_OK);
    CHECK(fviz_append_poly_data_filter_set_input_data(append_generic, generic) == FVIZ_OK);
    CHECK(fviz_append_poly_data_filter_update(append_generic) == FVIZ_OK);
    CHECK(fviz_poly_data_vert_cell_count(fviz_append_poly_data_filter_output(append_generic)) == 1u);
    CHECK(fviz_poly_data_line_cell_count(fviz_append_poly_data_filter_output(append_generic)) == 1u);
    CHECK(fviz_poly_data_poly_cell_count(fviz_append_poly_data_filter_output(append_generic)) == 1u);
    CHECK(fviz_poly_data_strip_cell_count(fviz_append_poly_data_filter_output(append_generic)) == 1u);
    array = fviz_attribute_set_const_get(
        fviz_poly_data_const_cell_data(fviz_append_poly_data_filter_output(append_generic)), "CellTag");
    CHECK(array != NULL && fviz_data_array_tuple_count(array) == 4u);

    fviz_append_poly_data_filter_remove_all_inputs(append_generic);
    CHECK(fviz_append_poly_data_filter_set_input_connection(append_generic, fviz_triangle_filter_output_port(triangle)) == FVIZ_OK);
    CHECK(fviz_append_poly_data_filter_add_input_connection(append_generic, fviz_triangle_filter_output_port(triangle)) == FVIZ_OK);
    CHECK(fviz_append_poly_data_filter_update(append_generic) == FVIZ_OK);
    CHECK(fviz_poly_data_cell_count(fviz_append_poly_data_filter_output(append_generic)) == 16u);
    array = fviz_attribute_set_const_get(
        fviz_poly_data_const_cell_data(fviz_append_poly_data_filter_output(append_generic)), "CellTag");
    CHECK(array != NULL && fviz_data_array_tuple_count(array) == 16u);
    {
        double first_tag = 0.0, second_input_first_tag = 0.0;
        CHECK(fviz_data_array_get_component(array, 0u, 0u, &first_tag) == FVIZ_OK);
        CHECK(fviz_data_array_get_component(array, 8u, 0u, &second_input_first_tag) == FVIZ_OK);
        CHECK(near_value(first_tag, 10.0) && near_value(second_input_first_tag, 10.0));
    }

    array = fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(tri_output), "FVizOriginalCellIds");
    CHECK(array != NULL && fviz_data_array_tuple_count(array) == 8u);
    {
        const double expected_ids[8] = {1000.0, 1000.0, 1001.0, 1001.0, 1002.0, 1002.0, 1003.0, 1003.0};
        for (i = 0u; i < 8u; ++i)
        {
            double value = 0.0;
            CHECK(fviz_data_array_get_component(array, i, 0u, &value) == FVIZ_OK);
            CHECK(near_value(value, expected_ids[i]));
        }
    }
    array = fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(tri_output), "CellTag");
    CHECK(array != NULL && fviz_data_array_tuple_count(array) == 8u);

    CHECK(fviz_poly_data_normals_filter_create(&normals_filter) == FVIZ_OK);
    CHECK(fviz_poly_data_normals_filter_set_input_data(normals_filter, generic) == FVIZ_OK);
    CHECK(fviz_poly_data_normals_filter_update(normals_filter) == FVIZ_OK);
    normal_output = fviz_poly_data_normals_filter_output(normals_filter);
    CHECK(normal_output != NULL && fviz_poly_data_has_normals(normal_output) == FVIZ_TRUE);
    normals = fviz_poly_data_normals(normal_output);
    CHECK(normals != NULL);
    CHECK(fviz_vec3_length(normals[2]) > 0.9f);
    array = fviz_attribute_set_const_get(fviz_poly_data_const_point_data(normal_output), "Normals");
    CHECK(array != NULL && fviz_data_array_components(array) == 3u);
    CHECK(fviz_attribute_set_const_active(fviz_poly_data_const_point_data(normal_output), FVIZ_ATTRIBUTE_NORMALS) == array);

    CHECK(fviz_poly_data_create(&dirty_generic) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(dirty_generic, fviz_vec3(0.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(dirty_generic, fviz_vec3(1.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(dirty_generic, fviz_vec3(1.0f, 1.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(dirty_generic, fviz_vec3(0.0f, 1.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(dirty_generic, fviz_vec3(0.0001f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    {
        const uint32_t pv[2] = {0u, 4u};
        const uint32_t pl[3] = {0u, 4u, 1u};
        const uint32_t st[4] = {0u, 4u, 1u, 2u};
        CHECK(fviz_poly_data_add_poly_vertex(dirty_generic, 2u, pv) == FVIZ_OK);
        CHECK(fviz_poly_data_add_poly_line(dirty_generic, 3u, pl) == FVIZ_OK);
        CHECK(fviz_poly_data_add_quad(dirty_generic, 0u, 1u, 2u, 3u) == FVIZ_OK);
        CHECK(fviz_poly_data_add_triangle_strip(dirty_generic, 4u, st) == FVIZ_OK);
    }
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT32, 1u, &dirty_generic_cell_ids) == FVIZ_OK);
    for (i = 0u; i < 4u; ++i)
    {
        const uint32_t value = 100u + i;
        CHECK(fviz_data_array_append_tuple(dirty_generic_cell_ids, &value) == FVIZ_OK);
    }
    CHECK(fviz_attribute_set_add(
        fviz_poly_data_cell_data(dirty_generic), "CellTag", dirty_generic_cell_ids) == FVIZ_OK);
    CHECK(fviz_clean_poly_data_filter_create(&clean_generic) == FVIZ_OK);
    CHECK(fviz_clean_poly_data_filter_set_tolerance(clean_generic, 0.001) == FVIZ_OK);
    CHECK(fviz_clean_poly_data_filter_set_input_data(clean_generic, dirty_generic) == FVIZ_OK);
    CHECK(fviz_clean_poly_data_filter_update(clean_generic) == FVIZ_OK);
    CHECK(fviz_poly_data_point_count(fviz_clean_poly_data_filter_output(clean_generic)) == 4u);
    CHECK(fviz_poly_data_vert_cell_count(fviz_clean_poly_data_filter_output(clean_generic)) == 1u);
    CHECK(fviz_poly_data_line_cell_count(fviz_clean_poly_data_filter_output(clean_generic)) == 1u);
    CHECK(fviz_poly_data_poly_cell_count(fviz_clean_poly_data_filter_output(clean_generic)) == 1u);
    CHECK(fviz_poly_data_strip_cell_count(fviz_clean_poly_data_filter_output(clean_generic)) == 1u);
    CHECK(fviz_poly_data_validate(fviz_clean_poly_data_filter_output(clean_generic)) == FVIZ_OK);
    array = fviz_attribute_set_const_get(
        fviz_poly_data_const_cell_data(fviz_clean_poly_data_filter_output(clean_generic)), "CellTag");
    CHECK(array != NULL && fviz_data_array_tuple_count(array) == 4u);

    CHECK(fviz_poly_data_create(&crease) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(crease, fviz_vec3(0.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(crease, fviz_vec3(1.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(crease, fviz_vec3(0.0f, 1.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(crease, fviz_vec3(0.0f, 0.0f, 1.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(crease, 0u, 1u, 2u) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(crease, 0u, 3u, 1u) == FVIZ_OK);
    CHECK(fviz_feature_edges_filter_create(&feature_edges) == FVIZ_OK);
    fviz_feature_edges_filter_set_boundary_edges(feature_edges, FVIZ_FALSE);
    CHECK(fviz_feature_edges_filter_set_feature_angle(feature_edges, 30.0) == FVIZ_OK);
    CHECK(fviz_feature_edges_filter_set_input_data(feature_edges, crease) == FVIZ_OK);
    CHECK(fviz_feature_edges_filter_update(feature_edges) == FVIZ_OK);
    edge_output = fviz_feature_edges_filter_output(feature_edges);
    CHECK(edge_output != NULL);
    CHECK(fviz_poly_data_line_count(edge_output) == 1u);
    array = fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(edge_output), "EdgeType");
    CHECK(array != NULL && fviz_data_array_tuple_count(array) == 1u);
    {
        double edge_type = 0.0;
        CHECK(fviz_data_array_get_component(array, 0u, 0u, &edge_type) == FVIZ_OK);
        CHECK(near_value(edge_type, (double)FVIZ_FEATURE_EDGE_FEATURE));
    }

    CHECK(fviz_poly_data_create(&disconnected) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(disconnected, fviz_vec3(0.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(disconnected, fviz_vec3(1.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(disconnected, fviz_vec3(0.0f, 1.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(disconnected, fviz_vec3(1.0f, 1.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(disconnected, fviz_vec3(10.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(disconnected, fviz_vec3(11.0f, 0.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(disconnected, fviz_vec3(10.0f, 1.0f, 0.0f), NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(disconnected, 0u, 1u, 2u) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(disconnected, 1u, 3u, 2u) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(disconnected, 4u, 5u, 6u) == FVIZ_OK);
    CHECK(fviz_poly_data_connectivity_filter_create(&connectivity) == FVIZ_OK);
    CHECK(fviz_poly_data_connectivity_filter_set_input_data(connectivity, disconnected) == FVIZ_OK);
    CHECK(fviz_poly_data_connectivity_filter_update(connectivity) == FVIZ_OK);
    CHECK(fviz_poly_data_connectivity_filter_region_count(connectivity) == 2u);
    CHECK(fviz_poly_data_triangle_count(fviz_poly_data_connectivity_filter_output(connectivity)) == 3u);
    array = fviz_attribute_set_const_get(
        fviz_poly_data_const_cell_data(fviz_poly_data_connectivity_filter_output(connectivity)), "RegionId");
    CHECK(array != NULL && fviz_data_array_tuple_count(array) == 3u);
    {
        double r0 = -1.0, r1 = -1.0, r2 = -1.0;
        CHECK(fviz_data_array_get_component(array, 0u, 0u, &r0) == FVIZ_OK);
        CHECK(fviz_data_array_get_component(array, 1u, 0u, &r1) == FVIZ_OK);
        CHECK(fviz_data_array_get_component(array, 2u, 0u, &r2) == FVIZ_OK);
        CHECK(near_value(r0, 0.0) && near_value(r1, 0.0) && near_value(r2, 1.0));
    }
    fviz_poly_data_connectivity_filter_set_extraction_mode(connectivity, FVIZ_CONNECTIVITY_LARGEST_REGION);
    CHECK(fviz_poly_data_connectivity_filter_update(connectivity) == FVIZ_OK);
    CHECK(fviz_poly_data_connectivity_filter_region_count(connectivity) == 2u);
    CHECK(fviz_poly_data_triangle_count(fviz_poly_data_connectivity_filter_output(connectivity)) == 2u);
    array = fviz_attribute_set_const_get(
        fviz_poly_data_const_cell_data(fviz_poly_data_connectivity_filter_output(connectivity)), "FVizOriginalCellIds");
    CHECK(array != NULL && fviz_data_array_tuple_count(array) == 2u);

    fviz_release(connectivity);
    fviz_release(disconnected);
    fviz_release(clean_generic);
    fviz_release(dirty_generic_cell_ids);
    fviz_release(dirty_generic);
    fviz_release(append_generic);
    fviz_release(feature_edges);
    fviz_release(crease);
    fviz_release(normals_filter);
    fviz_release(triangle);
    fviz_release(generic_original_ids);
    fviz_release(generic_cell_ids);
    fviz_release(generic);
    fviz_release(cells_copy);
    fviz_release(cells);
    puts("general PolyData/geometry filter tests passed");
    return 0;
}
