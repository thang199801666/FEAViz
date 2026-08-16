#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

int main(void)
{
    FVizUnstructuredGrid* source = NULL;
    FVizPolyData* samples = NULL;
    FVizDataArray* temperature = NULL;
    FVizProbeFilter* probe = NULL;
    FVizPolyData* output;
    const FVizDataArray* sampled;
    const FVizDataArray* mask;
    const FVizVec3 source_points[8] = {
        {0.0f,0.0f,0.0f}, {1.0f,0.0f,0.0f}, {1.0f,1.0f,0.0f}, {0.0f,1.0f,0.0f},
        {0.0f,0.0f,1.0f}, {1.0f,0.0f,1.0f}, {1.0f,1.0f,1.0f}, {0.0f,1.0f,1.0f}
    };
    const uint32_t hex[8] = {0u,1u,2u,3u,4u,5u,6u,7u};
    const float temperatures[8] = {0.0f,1.0f,2.0f,1.0f,1.0f,2.0f,3.0f,2.0f};
    const FVizVec3 sample_points[3] = {
        {0.5f,0.5f,0.5f},
        {0.25f,0.25f,0.25f},
        {2.0f,2.0f,2.0f}
    };
    double value = 0.0;

    CHECK(fviz_unstructured_grid_create(&source) == FVIZ_OK);
    for (uint32_t i = 0u; i < 8u; ++i)
        CHECK(fviz_unstructured_grid_add_point(source, source_points[i], NULL) == FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(source, FVIZ_CELL_HEXAHEDRON, 8u, hex) == FVIZ_OK);
    CHECK(fviz_cell_array_convert_id_storage(fviz_unstructured_grid_cells(source), FVIZ_ID_STORAGE_UINT64) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &temperature) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(temperature, temperatures, 8u) == FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(source), "Temperature", temperature) == FVIZ_OK);
    CHECK(fviz_attribute_set_set_active(
        fviz_unstructured_grid_point_data(source), FVIZ_ATTRIBUTE_SCALARS, "Temperature") == FVIZ_OK);

    CHECK(fviz_poly_data_create(&samples) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points(samples, sample_points, 3u, NULL) == FVIZ_OK);

    CHECK(fviz_probe_filter_create(&probe) == FVIZ_OK);
    CHECK(fviz_probe_filter_set_input_data(probe, samples) == FVIZ_OK);
    CHECK(fviz_probe_filter_set_source_data(probe, source) == FVIZ_OK);
    CHECK(fviz_probe_filter_update(probe) == FVIZ_OK);
    output = fviz_probe_filter_output(probe);
    CHECK(output != NULL);
    CHECK(fviz_poly_data_point_count(output) == 3u);
    sampled = fviz_attribute_set_const_get(fviz_poly_data_const_point_data(output), "Temperature");
    mask = fviz_attribute_set_const_get(fviz_poly_data_const_point_data(output), "FVizValidPointMask");
    CHECK(sampled != NULL && mask != NULL);
    CHECK(fviz_attribute_set_const_active(
        fviz_poly_data_const_point_data(output), FVIZ_ATTRIBUTE_SCALARS) == sampled);
    CHECK(fviz_poly_data_const_scalars(output) == sampled);
    CHECK(fviz_data_array_get_component(sampled, 0u, 0u, &value) == FVIZ_OK);
    CHECK(fabs(value - 1.5) < 1.0e-5);
    CHECK(fviz_data_array_get_component(sampled, 1u, 0u, &value) == FVIZ_OK);
    CHECK(fabs(value - 0.75) < 1.0e-5);
    CHECK(fviz_data_array_get_component(sampled, 2u, 0u, &value) == FVIZ_OK);
    CHECK(fabs(value) < 1.0e-8);
    CHECK(fviz_data_array_get_component(mask, 0u, 0u, &value) == FVIZ_OK && value == 1.0);
    CHECK(fviz_data_array_get_component(mask, 1u, 0u, &value) == FVIZ_OK && value == 1.0);
    CHECK(fviz_data_array_get_component(mask, 2u, 0u, &value) == FVIZ_OK && value == 0.0);

    fviz_release(probe);
    fviz_release(samples);
    fviz_release(temperature);
    fviz_release(source);
    return 0;
}
