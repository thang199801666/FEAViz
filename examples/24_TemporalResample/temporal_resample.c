#include <math.h>
#include <stdio.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"FEAViz temporal/resample example failed at line %d: %s\n",__LINE__,fviz_last_error_message()); goto fail; } } while(0)

int main(void)
{
    FVizImageData* image=NULL;
    FVizDataArray* vectors=NULL;
    FVizPolyData* samples=NULL;
    FVizResampleWithDataSet* resample=NULL;
    FVizArrayCalculatorFilter* magnitude_filter=NULL;
    FVizPartitionedDataSet* partitions=NULL;
    FVizTemporalDataSet* timeline=NULL;
    FVizArrayCalculatorOptions calc;
    const int64_t extent[6]={0,2,0,2,0,2};
    const FVizVec3 sample_points[3]={{0.5f,0.5f,0.0f},{1.0f,1.0f,1.0f},{1.5f,1.5f,2.0f}};
    FVizPolyData* result;
    const FVizDataArray* magnitude;
    FVizSize i, nearest=0u;

    CHECK(fviz_image_data_create(&image)==FVIZ_OK);
    CHECK(fviz_image_data_set_extent(image,extent)==FVIZ_OK);
    CHECK(fviz_image_data_allocate_point_scalars(image,"Velocity",FVIZ_DATA_FLOAT32,3u,&vectors)==FVIZ_OK);
    for (i=0u;i<fviz_image_data_point_count(image);++i)
    {
        int64_t ijk[3];
        float v[3];
        CHECK(fviz_image_data_point_ijk(image,(FVizId)i,ijk)==FVIZ_OK);
        v[0]=(float)ijk[0]; v[1]=(float)ijk[1]; v[2]=(float)ijk[2];
        CHECK(fviz_data_array_set_tuple(vectors,i,v)==FVIZ_OK);
    }
    CHECK(fviz_attribute_set_set_active(fviz_image_data_point_data(image),FVIZ_ATTRIBUTE_VECTORS,"Velocity")==FVIZ_OK);

    CHECK(fviz_poly_data_create(&samples)==FVIZ_OK);
    CHECK(fviz_poly_data_add_points_ids(samples,sample_points,3u,NULL)==FVIZ_OK);
    CHECK(fviz_resample_with_data_set_create(&resample)==FVIZ_OK);
    CHECK(fviz_resample_with_data_set_set_input_data(resample,samples)==FVIZ_OK);
    CHECK(fviz_resample_with_data_set_set_source_data(resample,(FVizDataObject*)image)==FVIZ_OK);

    CHECK(fviz_array_calculator_filter_create(&magnitude_filter)==FVIZ_OK);
    CHECK(fviz_array_calculator_filter_set_input_connection(magnitude_filter,fviz_resample_with_data_set_output_port(resample))==FVIZ_OK);
    CHECK(fviz_array_calculator_filter_set_array(magnitude_filter,FVIZ_ARRAY_CALC_POINT_DATA,"Velocity")==FVIZ_OK);
    CHECK(fviz_array_calculator_filter_set_result_name(magnitude_filter,"VelocityMagnitude")==FVIZ_OK);
    fviz_array_calculator_options_initialize(&calc);
    calc.operation=FVIZ_ARRAY_CALC_MAGNITUDE;
    CHECK(fviz_array_calculator_filter_set_options(magnitude_filter,&calc)==FVIZ_OK);
    CHECK(fviz_array_calculator_filter_update(magnitude_filter)==FVIZ_OK);
    result=fviz_array_calculator_filter_output(magnitude_filter);
    magnitude=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(result),"VelocityMagnitude");
    CHECK(magnitude!=NULL && fviz_data_array_tuple_count(magnitude)==3u);

    CHECK(fviz_partitioned_data_set_create(&partitions)==FVIZ_OK);
    CHECK(fviz_partitioned_data_set_add_partition(partitions,(FVizDataObject*)result,"Sampled result",NULL)==FVIZ_OK);
    CHECK(fviz_partitioned_data_set_add_partition(partitions,(FVizDataObject*)samples,"Sampling geometry",NULL)==FVIZ_OK);
    CHECK(fviz_temporal_data_set_create(&timeline)==FVIZ_OK);
    CHECK(fviz_temporal_data_set_add_step(timeline,0.0,(FVizDataObject*)result,NULL)==FVIZ_OK);
    CHECK(fviz_temporal_data_set_add_step(timeline,1.0,(FVizDataObject*)partitions,NULL)==FVIZ_OK);
    CHECK(fviz_temporal_data_set_find_nearest(timeline,0.8,&nearest)==FVIZ_OK);

    printf("FEAViz temporal/resample: points=%zu magnitude0=%.6g magnitude2=%.6g partitions=%zu nearest-time=%.1f\n",
        (size_t)fviz_poly_data_point_count(result),
        ((const double*)fviz_data_array_const_data(magnitude))[0],
        ((const double*)fviz_data_array_const_data(magnitude))[2],
        (size_t)fviz_partitioned_data_set_count(partitions),
        fviz_temporal_data_set_time(timeline,nearest));

    fviz_release(timeline); fviz_release(partitions); fviz_release(magnitude_filter); fviz_release(resample);
    fviz_release(samples); fviz_release(vectors); fviz_release(image);
    return 0;
fail:
    fviz_release(timeline); fviz_release(partitions); fviz_release(magnitude_filter); fviz_release(resample);
    fviz_release(samples); fviz_release(vectors); fviz_release(image);
    return 1;
}
