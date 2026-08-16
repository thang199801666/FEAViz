#include <FViz/FViz.h>
#include <stdio.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value,TIME_UTC)!=TIME_UTC) return 0.0;
    return (double)value.tv_sec+(double)value.tv_nsec*1e-9;
}

int main(void)
{
    const int64_t extent[6]={0,48,0,48,0,48};
    const FVizSize sample_count=100000u;
    FVizImageData* image=NULL;
    FVizDataArray* scalar=NULL;
    FVizDataArray* vector=NULL;
    FVizPolyData* points=NULL;
    FVizResampleWithDataSet* filter=NULL;
    FVizVec3* sample_points=NULL;
    FVizSize i;
    double start,finish,naive_start,naive_finish;
    volatile double naive_sum=0.0;
    if (fviz_image_data_create(&image)!=FVIZ_OK || fviz_image_data_set_extent(image,extent)!=FVIZ_OK ||
        fviz_image_data_allocate_point_scalars(image,"Temperature",FVIZ_DATA_FLOAT32,1u,&scalar)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32,3u,&vector)!=FVIZ_OK ||
        fviz_data_array_resize(vector,fviz_image_data_point_count(image))!=FVIZ_OK) goto fail;
    {
        float* s=(float*)fviz_data_array_data(scalar);
        float* v=(float*)fviz_data_array_data(vector);
        for (i=0u;i<fviz_image_data_point_count(image);++i)
        {
            int64_t ijk[3];
            if (fviz_image_data_point_ijk(image,(FVizId)i,ijk)!=FVIZ_OK) goto fail;
            s[i]=(float)(ijk[0]+2*ijk[1]+3*ijk[2]);
            v[i*3u+0u]=(float)ijk[0]; v[i*3u+1u]=(float)ijk[1]; v[i*3u+2u]=(float)ijk[2];
        }
    }
    if (fviz_attribute_set_add(fviz_image_data_point_data(image),"PositionVector",vector)!=FVIZ_OK ||
        fviz_attribute_set_set_active(fviz_image_data_point_data(image),FVIZ_ATTRIBUTE_VECTORS,"PositionVector")!=FVIZ_OK)
        goto fail;
    sample_points=(FVizVec3*)fviz_alloc(sample_count*sizeof(*sample_points));
    if (sample_points==NULL) goto fail;
    for (i=0u;i<sample_count;++i)
    {
        const uint32_t x=(uint32_t)(i%47u), y=(uint32_t)((i/47u)%47u), z=(uint32_t)((i/(47u*47u))%47u);
        sample_points[i]=fviz_vec3((float)x+0.37f,(float)y+0.23f,(float)z+0.61f);
    }
    naive_start=wall_seconds();
    for (i=0u;i<sample_count;++i)
    {
        const double p[3]={(double)sample_points[i].x,(double)sample_points[i].y,(double)sample_points[i].z};
        double value=0.0;
        uint32_t component;
        if (fviz_image_data_sample_point_array(image,"Temperature",p,0u,&value)!=FVIZ_OK) goto fail;
        naive_sum += value;
        for (component=0u;component<3u;++component)
        {
            if (fviz_image_data_sample_point_array(image,"PositionVector",p,component,&value)!=FVIZ_OK) goto fail;
            naive_sum += value;
        }
    }
    naive_finish=wall_seconds();
    if (fviz_poly_data_create(&points)!=FVIZ_OK || fviz_poly_data_add_points_ids(points,sample_points,sample_count,NULL)!=FVIZ_OK ||
        fviz_resample_with_data_set_create(&filter)!=FVIZ_OK ||
        fviz_resample_with_data_set_set_input_data(filter,points)!=FVIZ_OK ||
        fviz_resample_with_data_set_set_source_data(filter,(FVizDataObject*)image)!=FVIZ_OK) goto fail;
    start=wall_seconds();
    if (fviz_resample_with_data_set_update(filter)!=FVIZ_OK) goto fail;
    finish=wall_seconds();
    if (fviz_resample_with_data_set_output(filter)==NULL ||
        fviz_attribute_set_const_get(fviz_poly_data_const_point_data(fviz_resample_with_data_set_output(filter)),"Temperature")==NULL)
        goto fail;
    puts("samples,naive_seconds,shared_stencil_seconds,speedup,samples_per_second,checksum");
    printf("%llu,%.9f,%.9f,%.2f,%.2f,%.3f\n",(unsigned long long)sample_count,naive_finish-naive_start,finish-start,
        (naive_finish-naive_start)/(finish-start),(double)sample_count/(finish-start),(double)naive_sum);
    fviz_release(filter); fviz_release(points); fviz_free(sample_points); fviz_release(vector); fviz_release(scalar); fviz_release(image);
    return 0;
fail:
    fviz_release(filter); fviz_release(points); fviz_free(sample_points); fviz_release(vector); fviz_release(scalar); fviz_release(image);
    return 1;
}
