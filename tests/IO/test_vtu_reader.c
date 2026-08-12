#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

#ifndef FVIZ_TESTDATA_DIR
#define FVIZ_TESTDATA_DIR "."
#endif

#define TESTDATA(path) FVIZ_TESTDATA_DIR "/" path

static int test_vtu_hex(void)
{
    FVizUnstructuredGrid* grid = NULL;
    const FVizDataArray* temperature;
    const FVizDataArray* stress;
    const float* temp_data;
    const float* stress_data;
    CHECK(fviz_vtu_read(TESTDATA("hex.vtu"), &grid) == FVIZ_OK);
    CHECK(grid != NULL);
    CHECK(fviz_unstructured_grid_point_count(grid) == 8u);
    CHECK(fviz_unstructured_grid_cell_count(grid) == 1u);
    {
        FVizBounds bounds = fviz_unstructured_grid_bounds(grid);
        CHECK(bounds.valid == FVIZ_TRUE);
        CHECK(fabsf(bounds.min.x) < 1.0e-5f);
        CHECK(fabsf(bounds.max.x - 1.0f) < 1.0e-5f);
        CHECK(fabsf(bounds.max.z - 1.0f) < 1.0e-5f);
    }
    {
        const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(grid));
        CHECK(fabsf(points[7].x - 0.0f) < 1.0e-5f);
        CHECK(fabsf(points[7].y - 1.0f) < 1.0e-5f);
        CHECK(fabsf(points[7].z - 1.0f) < 1.0e-5f);
    }
    temperature = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(grid), "temperature");
    CHECK(temperature != NULL);
    CHECK(fviz_data_array_tuple_count(temperature) == 8u);
    temp_data = (const float*)fviz_data_array_const_data((FVizDataArray*)temperature);
    CHECK(fabsf(temp_data[0] - 0.0f) < 1.0e-5f);
    CHECK(fabsf(temp_data[7] - 70.0f) < 1.0e-5f);
    stress = fviz_attribute_set_const_get(fviz_unstructured_grid_cell_data(grid), "stress");
    CHECK(stress != NULL);
    CHECK(fviz_data_array_tuple_count(stress) == 1u);
    stress_data = (const float*)fviz_data_array_const_data((FVizDataArray*)stress);
    CHECK(fabsf(stress_data[0] - 123.5f) < 1.0e-4f);
    fviz_release(grid);
    return 0;
}

static int test_vtu_surface_scalars(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizPolyData* surface = NULL;
    CHECK(fviz_vtu_read(TESTDATA("hex.vtu"), &grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_extract_surface_scalars(grid, &surface) == FVIZ_OK);
    CHECK(fviz_poly_data_triangle_count(surface) == 12u);
    CHECK(fviz_poly_data_const_scalars(surface) != NULL);
    fviz_release(surface);
    fviz_release(grid);
    return 0;
}

static int test_vtu_missing_file(void)
{
    FVizUnstructuredGrid* grid = NULL;
    CHECK(fviz_vtu_read(TESTDATA("does_not_exist.vtu"), &grid) == FVIZ_ERROR_IO);
    CHECK(grid == NULL);
    return 0;
}

static int test_vtu_binary(void)
{
    FVizUnstructuredGrid* grid = NULL;
    const FVizDataArray* temperature;
    const float* temp_data;
    CHECK(fviz_vtu_read(TESTDATA("hex_binary.vtu"), &grid) == FVIZ_OK);
    CHECK(grid != NULL);
    CHECK(fviz_unstructured_grid_point_count(grid) == 8u);
    CHECK(fviz_unstructured_grid_cell_count(grid) == 1u);
    {
        const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(grid));
        CHECK(fabsf(points[0].x - 0.0f) < 1.0e-4f);
        CHECK(fabsf(points[7].y - 1.0f) < 1.0e-4f);
        CHECK(fabsf(points[7].z - 1.0f) < 1.0e-4f);
    }
    temperature = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(grid), "temperature");
    CHECK(temperature != NULL);
    CHECK(fviz_data_array_tuple_count(temperature) == 8u);
    temp_data = (const float*)fviz_data_array_const_data((FVizDataArray*)temperature);
    CHECK(fabsf(temp_data[0] - 0.0f) < 1.0e-4f);
    CHECK(fabsf(temp_data[3] - 30.0f) < 1.0e-4f);
    CHECK(fabsf(temp_data[7] - 70.0f) < 1.0e-4f);
    fviz_release(grid);
    return 0;
}

static int test_vtu_round_trip_mode(FVizVTUOutputMode mode, FVizVTUHeaderWidth width)
{
    const char* path = mode == FVIZ_VTU_OUTPUT_ASCII
        ? "FVizRoundTripAscii.vtu" : "FVizRoundTripAppended.vtu";
    static const FVizVec3 points[8] = {
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}};
    static const uint32_t ids[8] = {0,1,2,3,4,5,6,7};
    FVizUnstructuredGrid* source = NULL;
    FVizUnstructuredGrid* loaded = NULL;
    FVizDataArray* global_ids = NULL;
    FVizDataArray* temperature = NULL;
    FVizDataArray* stress = NULL;
    FVizDataArray* metadata = NULL;
    FVizVTUWriterOptions options;
    FVizSize i;
    CHECK(fviz_unstructured_grid_create(&source) == FVIZ_OK);
    for (i = 0u; i < 8u; ++i)
        CHECK(fviz_unstructured_grid_add_point(source, points[i], NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(source, FVIZ_CELL_HEXAHEDRON, 8u, ids) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &global_ids) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &temperature) == FVIZ_OK);
    for (i = 0u; i < 8u; ++i)
    {
        const uint64_t id = UINT64_C(9007199254740993) + i;
        const double value = i == 6u ? NAN : i == 7u ? INFINITY : (double)i * 0.125;
        CHECK(fviz_data_array_append_tuple(global_ids, &id) == FVIZ_OK);
        CHECK(fviz_data_array_append_tuple(temperature, &value) == FVIZ_OK);
    }
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(source),
        "OriginalPointIds", global_ids) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(source),
        "Temperature", temperature) == FVIZ_OK);
    CHECK(fviz_attribute_set_set_active(fviz_unstructured_grid_point_data(source),
        FVIZ_ATTRIBUTE_SCALARS, "Temperature") == FVIZ_OK);
    CHECK(fviz_attribute_set_set_active(fviz_unstructured_grid_point_data(source),
        FVIZ_ATTRIBUTE_GLOBAL_IDS, "OriginalPointIds") == FVIZ_OK);
    {
        const float tuple[6] = {1.0f, 2.0f, 3.0f, 0.5f, -0.25f, 4.0f};
        const int32_t meta[2] = {-7, 2026};
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 6u, &stress) == FVIZ_OK);
        CHECK(fviz_data_array_append_tuple(stress, tuple) == FVIZ_OK);
        CHECK(fviz_attribute_set_add(fviz_unstructured_grid_cell_data(source),
            "Stress", stress) == FVIZ_OK);
        CHECK(fviz_attribute_set_set_active(fviz_unstructured_grid_cell_data(source),
            FVIZ_ATTRIBUTE_TENSORS, "Stress") == FVIZ_OK);
        CHECK(fviz_data_array_create(FVIZ_DATA_INT32, 2u, &metadata) == FVIZ_OK);
        CHECK(fviz_data_array_append_tuple(metadata, meta) == FVIZ_OK);
        CHECK(fviz_attribute_set_add(fviz_unstructured_grid_field_data(source),
            "Metadata", metadata) == FVIZ_OK);
    }
    fviz_vtu_writer_options_initialize(&options);
    options.output_mode = mode;
    options.header_width = width;
    CHECK(fviz_vtu_write(path, source, &options) == FVIZ_OK);
    {
        FVizVTUReaderOptions limits;
        FVizUnstructuredGrid* rejected = NULL;
        fviz_vtu_reader_options_initialize(&limits);
        limits.maximum_points = 7u;
        CHECK(fviz_vtu_read_with_options(path, &limits, &rejected) == FVIZ_ERROR_OVERFLOW);
        CHECK(rejected == NULL);
    }
    CHECK(fviz_vtu_read(path, &loaded) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_point_count(loaded) == 8u);
    CHECK(fviz_unstructured_grid_cell_count(loaded) == 1u);
    CHECK(fviz_cell_array_connectivity_size(fviz_unstructured_grid_cells(loaded)) == 8u);
    {
        const FVizDataArray* loaded_ids = fviz_attribute_set_const_get(
            fviz_unstructured_grid_point_data(loaded), "OriginalPointIds");
        const FVizDataArray* loaded_temperature = fviz_attribute_set_const_get(
            fviz_unstructured_grid_point_data(loaded), "Temperature");
        const FVizDataArray* loaded_stress = fviz_attribute_set_const_get(
            fviz_unstructured_grid_cell_data(loaded), "Stress");
        const FVizDataArray* loaded_metadata = fviz_attribute_set_const_get(
            fviz_unstructured_grid_field_data(loaded), "Metadata");
        CHECK(loaded_ids != NULL && fviz_data_array_type(loaded_ids) == FVIZ_DATA_UINT64);
        CHECK(((const uint64_t*)fviz_data_array_const_data(loaded_ids))[0] ==
            UINT64_C(9007199254740993));
        CHECK(loaded_temperature != NULL && fviz_data_array_type(loaded_temperature) == FVIZ_DATA_FLOAT64);
        CHECK(isnan(((const double*)fviz_data_array_const_data(loaded_temperature))[6]));
        CHECK(isinf(((const double*)fviz_data_array_const_data(loaded_temperature))[7]));
        CHECK(loaded_stress != NULL && fviz_data_array_components(loaded_stress) == 6u);
        CHECK(((const float*)fviz_data_array_const_data(loaded_stress))[4] == -0.25f);
        CHECK(loaded_metadata != NULL && fviz_data_array_type(loaded_metadata) == FVIZ_DATA_INT32);
        CHECK(((const int32_t*)fviz_data_array_const_data(loaded_metadata))[1] == 2026);
        CHECK(strcmp(fviz_attribute_set_active_name(
            fviz_unstructured_grid_point_data(loaded), FVIZ_ATTRIBUTE_SCALARS),
            "Temperature") == 0);
        CHECK(strcmp(fviz_attribute_set_active_name(
            fviz_unstructured_grid_point_data(loaded), FVIZ_ATTRIBUTE_GLOBAL_IDS),
            "OriginalPointIds") == 0);
        CHECK(strcmp(fviz_attribute_set_active_name(
            fviz_unstructured_grid_cell_data(loaded), FVIZ_ATTRIBUTE_TENSORS),
            "Stress") == 0);
    }
    options.compress = FVIZ_TRUE;
    CHECK(fviz_vtu_write(path, source, &options) == FVIZ_ERROR_NOT_SUPPORTED);
    CHECK(remove(path) == 0);
    fviz_release(metadata);
    fviz_release(stress);
    fviz_release(temperature);
    fviz_release(global_ids);
    fviz_release(loaded);
    fviz_release(source);
    return 0;
}

int main(void)
{
    CHECK(test_vtu_hex() == 0);
    CHECK(test_vtu_surface_scalars() == 0);
    CHECK(test_vtu_missing_file() == 0);
    CHECK(test_vtu_binary() == 0);
    CHECK(test_vtu_round_trip_mode(FVIZ_VTU_OUTPUT_ASCII, FVIZ_VTU_HEADER_UINT32) == 0);
    CHECK(test_vtu_round_trip_mode(FVIZ_VTU_OUTPUT_APPENDED_RAW, FVIZ_VTU_HEADER_UINT64) == 0);
    return 0;
}
