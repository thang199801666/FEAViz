#include <stdio.h>

#include <FViz/Algorithms/FVizImageDataGeometryFilter.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Data/FVizImageData.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void)
{
    FVizImageData* image = NULL;
    FVizImageDataGeometryFilter* filter = NULL;
    FVizPolyData* output;
    FVizDataArray* cell_values = NULL;
    FVizDataArray* original_ids = NULL;
    const int64_t extent[6] = {0,2,0,2,0,2};
    FVizSize i;

    CHECK(fviz_image_data_create(&image) == FVIZ_OK);
    CHECK(fviz_image_data_set_extent(image, extent) == FVIZ_OK);
    CHECK(fviz_image_data_allocate_cell_scalars(image, "Material", FVIZ_DATA_UINT32, 1u, &cell_values) == FVIZ_OK);
    for (i=0u;i<fviz_data_array_tuple_count(cell_values);++i)
        CHECK(fviz_data_array_set_component(cell_values,i,0u,(double)(100u+i)) == FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &original_ids) == FVIZ_OK);
    CHECK(fviz_data_array_resize(original_ids, fviz_image_data_cell_count(image)) == FVIZ_OK);
    {
        uint64_t* values = (uint64_t*)fviz_data_array_data(original_ids);
        for (i = 0u; i < fviz_image_data_cell_count(image); ++i) values[i] = UINT64_C(1000000000000) + i;
    }
    CHECK(fviz_attribute_set_add(fviz_image_data_cell_data(image), "FVizOriginalCellIds", original_ids) == FVIZ_OK);
    fviz_release(original_ids);
    fviz_release(cell_values);

    CHECK(fviz_image_data_geometry_filter_create(&filter) == FVIZ_OK);
    CHECK(fviz_image_data_geometry_filter_set_input_data(filter,image) == FVIZ_OK);
    CHECK(fviz_image_data_geometry_filter_update(filter) == FVIZ_OK);
    output = fviz_image_data_geometry_filter_output(filter);
    CHECK(output != NULL);
    CHECK(fviz_poly_data_point_count(output) == 27u);
    CHECK(fviz_poly_data_poly_cell_count(output) == 48u);
    CHECK(fviz_poly_data_triangle_count(output) == 48u);
    {
        const FVizDataArray* provenance = fviz_attribute_set_const_get(
            fviz_poly_data_const_cell_data(output), "FVizOriginalCellIds");
        const uint64_t* values;
        CHECK(provenance != NULL);
        CHECK(fviz_data_array_type(provenance) == FVIZ_DATA_UINT64);
        values = (const uint64_t*)fviz_data_array_const_data(provenance);
        CHECK(values != NULL && values[0] == UINT64_C(1000000000000));
    }
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(output), "Material") != NULL);
    CHECK(fviz_data_array_tuple_count(fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(output), "Material")) == 48u);
    CHECK(fviz_poly_data_validate(output) == FVIZ_OK);

    fviz_release(filter);
    fviz_release(image);
    return 0;
}
