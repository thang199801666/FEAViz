#include <math.h>
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

int main(void)
{
    CHECK(test_vtu_hex() == 0);
    CHECK(test_vtu_surface_scalars() == 0);
    CHECK(test_vtu_missing_file() == 0);
    return 0;
}
