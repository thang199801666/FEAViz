#include <FViz/FViz.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value,TIME_UTC)!=TIME_UTC) return 0.0;
    return (double)value.tv_sec+(double)value.tv_nsec*1.0e-9;
}

int main(void)
{
    const FVizSize count=500000u;
    FVizPoints *base=NULL,*output=NULL;
    FVizDataArray* vectors=NULL;
    FVizVec3* points=NULL;
    float* u=NULL;
    FVizDeformationMetrics metrics;
    FVizSize i;
    double start,measure_seconds,create_seconds,update_seconds,scale=0.0;

    points=(FVizVec3*)malloc((size_t)count*sizeof(*points));
    u=(float*)malloc((size_t)count*3u*sizeof(*u));
    if(points==NULL||u==NULL) return 1;
    for(i=0u;i<count;++i)
    {
        points[i]=fviz_vec3((float)(i%1000u)*0.01f,(float)((i/1000u)%500u)*0.01f,0.0f);
        u[i*3u+0u]=(float)(i%97u)*1.0e-4f;
        u[i*3u+1u]=(float)(i%53u)*2.0e-4f;
        u[i*3u+2u]=(float)(i%31u)*1.0e-4f;
    }
    if(fviz_points_create(&base)!=FVIZ_OK || fviz_points_append_many_ids(base,points,count,NULL)!=FVIZ_OK ||
       fviz_data_array_create(FVIZ_DATA_FLOAT32,3u,&vectors)!=FVIZ_OK ||
       fviz_data_array_append_tuples(vectors,u,count)!=FVIZ_OK) return 2;
    free(points); free(u);

    start=wall_seconds();
    if(fviz_deformation_measure_vectors(vectors,&metrics)!=FVIZ_OK) return 3;
    measure_seconds=wall_seconds()-start;
    if(fviz_deformation_compute_auto_scale(fviz_points_bounds(base),vectors,0.1,0.0,1.0e12,&scale,NULL)!=FVIZ_OK) return 4;

    start=wall_seconds();
    if(fviz_deformation_apply_to_points(base,vectors,scale,&output)!=FVIZ_OK) return 5;
    create_seconds=wall_seconds()-start;
    start=wall_seconds();
    if(fviz_deformation_update_points(output,base,vectors,scale*0.5)!=FVIZ_OK) return 6;
    update_seconds=wall_seconds()-start;

    puts("points,measure_ms,create_ms,update_ms,max_magnitude,rms_magnitude,auto_scale");
    printf("%llu,%.6f,%.6f,%.6f,%.9g,%.9g,%.9g\n",
        (unsigned long long)count,measure_seconds*1000.0,create_seconds*1000.0,update_seconds*1000.0,
        metrics.maximum_magnitude,metrics.rms_magnitude,scale);
    fviz_release(output); fviz_release(vectors); fviz_release(base);
    return 0;
}
