#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

#ifndef FVIZ_TESTDATA_DIR
#define FVIZ_TESTDATA_DIR "."
#endif

#define TESTDATA(path) FVIZ_TESTDATA_DIR "/" path

static int test_vtk_legacy_hex(void)
{
    FVizUnstructuredGrid* grid = NULL;
    const FVizDataArray* temperature;
    const FVizDataArray* stress;
    const double* temp_data;
    const double* stress_data;
    CHECK(fviz_vtk_legacy_read(TESTDATA("hex_legacy.vtk"), &grid) == FVIZ_OK);
    CHECK(grid != NULL);
    CHECK(fviz_unstructured_grid_point_count(grid) == 8u);
    CHECK(fviz_unstructured_grid_cell_count(grid) == 1u);
    {
        const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(grid));
        CHECK(fabsf(points[0].x - 0.0f) < 1.0e-5f);
        CHECK(fabsf(points[7].y - 1.0f) < 1.0e-5f);
        CHECK(fabsf(points[7].z - 1.0f) < 1.0e-5f);
    }
    temperature = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(grid), "temperature");
    CHECK(temperature != NULL);
    CHECK(fviz_data_array_type(temperature) == FVIZ_DATA_FLOAT64);
    CHECK(fviz_data_array_tuple_count(temperature) == 8u);
    temp_data = (const double*)fviz_data_array_const_data((FVizDataArray*)temperature);
    CHECK(fabs(temp_data[0] - 0.0) < 1.0e-10);
    CHECK(fabs(temp_data[7] - 70.0) < 1.0e-10);
    stress = fviz_attribute_set_const_get(fviz_unstructured_grid_cell_data(grid), "stress");
    CHECK(stress != NULL);
    CHECK(fviz_data_array_type(stress) == FVIZ_DATA_FLOAT64);
    stress_data = (const double*)fviz_data_array_const_data((FVizDataArray*)stress);
    CHECK(fabs(stress_data[0] - 123.5) < 1.0e-10);
    fviz_release(grid);
    return 0;
}

static void write_be32(FILE* file, uint32_t value)
{
    const unsigned char bytes[4] = {
        (unsigned char)(value >> 24u),
        (unsigned char)(value >> 16u),
        (unsigned char)(value >> 8u),
        (unsigned char)value
    };
    (void)fwrite(bytes, 1u, sizeof(bytes), file);
}

static void write_be_float(FILE* file, float value)
{
    uint32_t bits;
    (void)memcpy(&bits, &value, sizeof(bits));
    write_be32(file, bits);
}

static void write_be_double(FILE* file, double value)
{
    uint64_t bits;
    unsigned char bytes[8];
    unsigned int i;
    (void)memcpy(&bits, &value, sizeof(bits));
    for (i = 0u; i < 8u; ++i) bytes[i] = (unsigned char)(bits >> (56u - i * 8u));
    (void)fwrite(bytes, 1u, sizeof(bytes), file);
}

static int write_binary_fixture(const char* path)
{
    static const double points[24] = {
        0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0
    };
    FILE* file = fopen(path, "wb");
    unsigned int i;
    if (file == NULL) return 1;
    (void)fprintf(file, "# vtk DataFile Version 3.0\nFEAViz binary test\nBINARY\nDATASET UNSTRUCTURED_GRID\n");
    (void)fprintf(file, "POINTS 8 double\n");
    for (i = 0u; i < 24u; ++i) write_be_double(file, points[i]);
    (void)fprintf(file, "\nCELLS 1 9\n");
    write_be32(file, 8u);
    for (i = 0u; i < 8u; ++i) write_be32(file, i);
    (void)fprintf(file, "\nCELL_TYPES 1\n");
    write_be32(file, 12u);
    (void)fprintf(file, "\nPOINT_DATA 8\nSCALARS temperature double 1\nLOOKUP_TABLE default\n");
    for (i = 0u; i < 8u; ++i) write_be_double(file, (double)i * 10.0);
    (void)fprintf(file, "\nVECTORS displacement float\n");
    for (i = 0u; i < 8u; ++i)
    {
        write_be_float(file, (float)i * 0.1f);
        write_be_float(file, 0.0f);
        write_be_float(file, 0.0f);
    }
    (void)fprintf(file, "\nFIELD PointFields 1\nnode_id 1 8 int\n");
    for (i = 0u; i < 8u; ++i) write_be32(file, 100u + i);
    (void)fprintf(file, "\nCELL_DATA 1\nSCALARS stress float 1\nLOOKUP_TABLE default\n");
    write_be_float(file, 123.5f);
    (void)fprintf(file, "\n");
    return fclose(file) == 0 ? 0 : 1;
}

static int test_vtk_legacy_binary(void)
{
    const char* path = "fviz_test_hex_binary.vtk";
    FVizUnstructuredGrid* grid = NULL;
    const FVizAttributeSet* point_data;
    const FVizDataArray* temperature;
    const FVizDataArray* displacement;
    const FVizDataArray* node_id;
    const FVizDataArray* stress;
    const double* temperatures;
    const float* displacements;
    const int32_t* node_ids;
    const float* stresses;
    CHECK(write_binary_fixture(path) == 0);
    CHECK(fviz_vtk_legacy_read(path, &grid) == FVIZ_OK);
    (void)remove(path);
    CHECK(grid != NULL);
    CHECK(fviz_unstructured_grid_point_count(grid) == 8u);
    CHECK(fviz_unstructured_grid_cell_count(grid) == 1u);
    point_data = fviz_unstructured_grid_point_data(grid);
    CHECK(fviz_attribute_set_count(point_data) == 3u);
    temperature = fviz_attribute_set_const_get(point_data, "temperature");
    displacement = fviz_attribute_set_const_get(point_data, "displacement");
    node_id = fviz_attribute_set_const_get(point_data, "node_id");
    stress = fviz_attribute_set_const_get(fviz_unstructured_grid_cell_data(grid), "stress");
    CHECK(temperature != NULL && fviz_data_array_type(temperature) == FVIZ_DATA_FLOAT64);
    CHECK(displacement != NULL && fviz_data_array_components(displacement) == 3u);
    CHECK(node_id != NULL && fviz_data_array_type(node_id) == FVIZ_DATA_INT32);
    CHECK(stress != NULL && fviz_data_array_type(stress) == FVIZ_DATA_FLOAT32);
    temperatures = (const double*)fviz_data_array_const_data(temperature);
    displacements = (const float*)fviz_data_array_const_data(displacement);
    node_ids = (const int32_t*)fviz_data_array_const_data(node_id);
    stresses = (const float*)fviz_data_array_const_data(stress);
    CHECK(fabs(temperatures[7] - 70.0) < 1.0e-10);
    CHECK(fabsf(displacements[21] - 0.7f) < 1.0e-5f);
    CHECK(node_ids[7] == 107);
    CHECK(fabsf(stresses[0] - 123.5f) < 1.0e-5f);
    fviz_release(grid);
    return 0;
}

static int test_vtk_legacy_surface(void)
{
    FVizUnstructuredGrid* grid = NULL;
    FVizPolyData* surface = NULL;
    CHECK(fviz_vtk_legacy_read(TESTDATA("hex_legacy.vtk"), &grid) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_extract_surface(grid, &surface) == FVIZ_OK);
    CHECK(fviz_poly_data_triangle_count(surface) == 12u);
    fviz_release(surface);
    fviz_release(grid);
    return 0;
}

static int test_vtk_legacy_missing(void)
{
    FVizUnstructuredGrid* grid = NULL;
    CHECK(fviz_vtk_legacy_read(TESTDATA("does_not_exist.vtk"), &grid) == FVIZ_ERROR_IO);
    CHECK(grid == NULL);
    return 0;
}

static int test_vtk_legacy_truncated_binary(void)
{
    const char* path = "fviz_test_truncated_binary.vtk";
    FVizUnstructuredGrid* grid = NULL;
    FILE* file = fopen(path, "wb");
    CHECK(file != NULL);
    (void)fprintf(file,
        "# vtk DataFile Version 3.0\ntruncated\nBINARY\nDATASET UNSTRUCTURED_GRID\nPOINTS 1 float\n");
    (void)fputc(0, file);
    CHECK(fclose(file) == 0);
    CHECK(fviz_vtk_legacy_read(path, &grid) == FVIZ_ERROR_IO);
    (void)remove(path);
    CHECK(grid == NULL);
    return 0;
}

int main(void)
{
    int result = test_vtk_legacy_hex();
    if (result != 0) return result;
    result = test_vtk_legacy_binary();
    if (result != 0) return result;
    result = test_vtk_legacy_surface();
    if (result != 0) return result;
    result = test_vtk_legacy_missing();
    if (result != 0) return result;
    result = test_vtk_legacy_truncated_binary();
    if (result != 0) return result;
    return 0;
}
