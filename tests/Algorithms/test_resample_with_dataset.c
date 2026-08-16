#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void)
{
    FVizImageData* image = NULL;
    FVizDataArray* values = NULL;
    FVizPolyData* sample = NULL;
    FVizResampleWithDataSet* filter = NULL;
    FVizPolyData* output;
    const FVizDataArray* sampled;
    const FVizDataArray* mask;
    const int64_t extent[6] = {0,1,0,1,0,1};
    FVizVec3 points[3] = {
        {0.5f,0.5f,0.5f},
        {1.0f,1.0f,1.0f},
        {2.0f,2.0f,2.0f}
    };
    float image_values[8];
    FVizSize i;
    double v = 0.0;

    CHECK(fviz_image_data_create(&image) == FVIZ_OK);
    CHECK(fviz_image_data_set_extent(image, extent) == FVIZ_OK);
    CHECK(fviz_image_data_allocate_point_scalars(image, "Linear", FVIZ_DATA_FLOAT32, 1u, &values) == FVIZ_OK);
    for (i=0u;i<8u;++i)
    {
        int64_t ijk[3];
        CHECK(fviz_image_data_point_ijk(image,(FVizId)i,ijk) == FVIZ_OK);
        image_values[i]=(float)(ijk[0]+2*ijk[1]+3*ijk[2]);
    }
    CHECK(fviz_data_array_resize(values,8u) == FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(values,NULL,0u) == FVIZ_OK);
    for (i=0u;i<8u;++i) CHECK(fviz_data_array_set_tuple(values,i,&image_values[i]) == FVIZ_OK);

    CHECK(fviz_poly_data_create(&sample) == FVIZ_OK);
    CHECK(fviz_poly_data_add_points_ids(sample,points,3u,NULL) == FVIZ_OK);
    CHECK(fviz_resample_with_data_set_create(&filter) == FVIZ_OK);
    CHECK(fviz_resample_with_data_set_set_input_data(filter,sample) == FVIZ_OK);
    CHECK(fviz_resample_with_data_set_set_source_data(filter,(FVizDataObject*)image) == FVIZ_OK);
    CHECK(fviz_resample_with_data_set_update(filter) == FVIZ_OK);
    output=fviz_resample_with_data_set_output(filter);
    CHECK(output != NULL && fviz_poly_data_point_count(output) == 3u);
    sampled=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(output),"Linear");
    mask=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(output),"FVizValidPointMask");
    CHECK(sampled != NULL && mask != NULL);
    CHECK(fviz_data_array_get_component(sampled,0u,0u,&v) == FVIZ_OK && fabs(v-3.0)<1e-6);
    CHECK(fviz_data_array_get_component(sampled,1u,0u,&v) == FVIZ_OK && fabs(v-6.0)<1e-6);
    CHECK(((const uint8_t*)fviz_data_array_const_data(mask))[0] == 1u);
    CHECK(((const uint8_t*)fviz_data_array_const_data(mask))[1] == 1u);
    CHECK(((const uint8_t*)fviz_data_array_const_data(mask))[2] == 0u);

    fviz_release(filter);
    fviz_release(sample);
    fviz_release(values);
    fviz_release(image);
    return 0;
}
