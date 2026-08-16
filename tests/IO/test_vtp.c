#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void)
{
    const char* path = "FVizRoundTrip.vtp";
    FVizPolyData* input = NULL;
    FVizPolyData* output = NULL;
    FVizDataArray* pscalars = NULL;
    FVizDataArray* cdata = NULL;
    FVizDataArray* field = NULL;
    FVizVTPWriterOptions options;
    FVizId ids[4];
    float point_values[4] = {1,2,3,4};
    int32_t cell_value = 17;
    double field_values[4] = {10,20,30,40};
    FVizCellView view;

    CHECK(fviz_poly_data_create(&input) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points_ids(input, &(FVizVec3){0,0,0}, 1u, &ids[0]) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points_ids(input, &(FVizVec3){1,0,0}, 1u, &ids[1]) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points_ids(input, &(FVizVec3){1,1,0}, 1u, &ids[2]) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points_ids(input, &(FVizVec3){0,1,0}, 1u, &ids[3]) == FVIZ_OK);
    CHECK(fviz_poly_data_add_cell_ids(input, FVIZ_CELL_POLYGON, 4u, ids) == FVIZ_OK);

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &pscalars) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(pscalars, point_values, 4u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(input), "Temperature", pscalars) == FVIZ_OK);
    CHECK(fviz_attribute_set_set_active(fviz_poly_data_point_data(input), FVIZ_ATTRIBUTE_SCALARS, "Temperature") == FVIZ_OK);

    CHECK(fviz_data_array_create(FVIZ_DATA_INT32, 1u, &cdata) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(cdata, &cell_value) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_cell_data(input), "MaterialId", cdata) == FVIZ_OK);

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 2u, &field) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(field, field_values, 2u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_field_data(input), "Meta & Info", field) == FVIZ_OK);

    fviz_vtp_writer_options_initialize(&options);
    CHECK(fviz_vtp_write(path, input, &options) == FVIZ_OK);
    CHECK(fviz_vtp_read(path, &output) == FVIZ_OK);
    CHECK(fviz_poly_data_point_count(output) == 4u);
    CHECK(fviz_poly_data_cell_count(output) == 1u);
    CHECK(fviz_cell_array_cell_view(fviz_poly_data_polys(output), 0u, &view) == FVIZ_OK);
    CHECK(view.point_count == 4u);
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_point_data(output), "Temperature") != NULL);
    CHECK(strcmp(fviz_attribute_set_active_name(fviz_poly_data_const_point_data(output), FVIZ_ATTRIBUTE_SCALARS), "Temperature") == 0);
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(output), "MaterialId") != NULL);
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_field_data(output), "Meta & Info") != NULL);
    CHECK(fviz_data_array_tuple_count(fviz_attribute_set_const_get(fviz_poly_data_const_field_data(output), "Meta & Info")) == 2u);


    {
        FILE* unsupported = fopen("FVizUnsupported.vtp", "wb");
        FVizPolyData* rejected = NULL;
        CHECK(unsupported != NULL);
        CHECK(fputs("<VTKFile type=\"PolyData\"><PolyData><Piece NumberOfPoints=\"1\" NumberOfVerts=\"0\" NumberOfLines=\"0\" NumberOfStrips=\"0\" NumberOfPolys=\"0\"><Points><DataArray type=\"Float32\" NumberOfComponents=\"3\" format=\"binary\">AAAA</DataArray></Points></Piece></PolyData></VTKFile>", unsupported) != EOF);
        CHECK(fclose(unsupported) == 0);
        CHECK(fviz_vtp_read("FVizUnsupported.vtp", &rejected) == FVIZ_ERROR_NOT_SUPPORTED);
        CHECK(rejected == NULL);
        (void)remove("FVizUnsupported.vtp");
    }

    {
        FILE* unsupported = fopen("FVizMultiPiece.vtp", "wb");
        FVizPolyData* rejected = NULL;
        CHECK(unsupported != NULL);
        CHECK(fputs("<VTKFile type=\"PolyData\"><PolyData><Piece NumberOfPoints=\"1\" NumberOfVerts=\"0\" NumberOfLines=\"0\" NumberOfStrips=\"0\" NumberOfPolys=\"0\"><Points><DataArray type=\"Float32\" NumberOfComponents=\"3\" format=\"ascii\">0 0 0</DataArray></Points></Piece><Piece NumberOfPoints=\"1\" NumberOfVerts=\"0\" NumberOfLines=\"0\" NumberOfStrips=\"0\" NumberOfPolys=\"0\"><Points><DataArray type=\"Float32\" NumberOfComponents=\"3\" format=\"ascii\">1 0 0</DataArray></Points></Piece></PolyData></VTKFile>", unsupported) != EOF);
        CHECK(fclose(unsupported) == 0);
        CHECK(fviz_vtp_read("FVizMultiPiece.vtp", &rejected) == FVIZ_ERROR_NOT_SUPPORTED);
        CHECK(rejected == NULL);
        (void)remove("FVizMultiPiece.vtp");
    }

    (void)remove(path);
    fviz_release(field);
    fviz_release(cdata);
    fviz_release(pscalars);
    fviz_release(output);
    fviz_release(input);
    return 0;
}
