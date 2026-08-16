#include <stdio.h>

#include <FViz/FEA/FVizFEA.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed %s:%d: %s\n", \
    __FILE__, __LINE__, #x); return 1; } } while (0)

int main(void)
{
    FVizPolyData* surface = NULL;
    FVizPolyData* edges = NULL;
    FVizPolyData* banded = NULL;
    FVizDataArray* scalars = NULL;
    FVizDataArray* cell_ids = NULL;
    FVizDataArray* face_ids = NULL;
    FVizLookupTable* table = NULL;
    const FVizVec3 points[4] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}
    };
    const float values[4] = {0.0f, 0.25f, 0.75f, 1.0f};
    const uint64_t provenance[2] = {7u, 7u};
    const FVizDataArray* colors;
    float low[3];
    float high[3];

    CHECK(fviz_poly_data_create(&surface) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points(surface, points, 4u, NULL) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(surface, 0u, 1u, 2u) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(surface, 0u, 2u, 3u) == FVIZ_OK);

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalars) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(scalars, values, 4u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(surface), "stress", scalars) == FVIZ_OK);

    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &cell_ids) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(cell_ids, provenance, 2u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_cell_data(surface),
        "FVizOriginalCellIds", cell_ids) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &face_ids) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(face_ids, provenance, 2u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_cell_data(surface),
        "FVizOriginalFaceIds", face_ids) == FVIZ_OK);

    CHECK(fviz_fea_extract_element_edges(surface, &edges) == FVIZ_OK);
    CHECK(fviz_poly_data_line_count(edges) == 4u);
    CHECK(fviz_poly_data_triangle_count(edges) == 0u);

    CHECK(fviz_lookup_table_create(256u, &table) == FVIZ_OK);
    CHECK(fviz_fea_configure_abaqus_contour_lut(table, 12u) == FVIZ_OK);
    fviz_lookup_table_get_color(table, 0u, &low[0], &low[1], &low[2]);
    fviz_lookup_table_get_color(table, 255u, &high[0], &high[1], &high[2]);
    CHECK(low[2] > low[0]);
    CHECK(high[0] > high[2]);

    CHECK(fviz_fea_build_abaqus_banded_surface(surface, "stress", 1u,
        0.0f, 1.0f, 12u, "contour_rgb", &banded) == FVIZ_OK);
    CHECK(fviz_poly_data_triangle_count(banded) > 0u);
    colors = fviz_attribute_set_const_get(
        fviz_poly_data_const_point_data(banded), "contour_rgb");
    CHECK(colors != NULL);
    CHECK(fviz_data_array_components(colors) == 3u);
    CHECK(fviz_data_array_tuple_count(colors) == fviz_poly_data_point_count(banded));

    fviz_release(table);
    fviz_release(banded);
    fviz_release(edges);
    fviz_release(face_ids);
    fviz_release(cell_ids);
    fviz_release(scalars);
    fviz_release(surface);
    return 0;
}
