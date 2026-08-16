#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>
#include <FViz/Algorithms/FVizDeformation.h>

#define CHECK(x) do { if(!(x)){ fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while(0)

int main(void)
{
    FVizPoints* points=NULL;
    FVizPoints* deformed=NULL;
    FVizDataArray* vectors=NULL;
    FVizDeformationMetrics metrics;
    FVizBounds bounds;
    FVizVec3 p[2]={fviz_vec3(0.0f,0.0f,0.0f),fviz_vec3(10.0f,0.0f,0.0f)};
    double u[2][3]={{0.0,0.0,0.0},{2.0,0.0,0.0}};
    double scale=0.0;
    const FVizVec3* out;

    CHECK(fviz_points_create(&points)==FVIZ_OK);
    CHECK(fviz_points_append_many_ids(points,p,2u,NULL)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,3u,&vectors)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(vectors,u,2u)==FVIZ_OK);
    CHECK(fviz_deformation_measure_vectors(vectors,&metrics)==FVIZ_OK);
    CHECK(metrics.tuple_count==2u && metrics.finite_tuple_count==2u);
    CHECK(fabs(metrics.maximum_magnitude-2.0)<1.0e-12);
    CHECK(fabs(metrics.rms_magnitude-sqrt(2.0))<1.0e-12);

    bounds=fviz_points_bounds(points);
    CHECK(fviz_deformation_compute_auto_scale(bounds,vectors,0.10,0.0,1000.0,&scale,&metrics)==FVIZ_OK);
    CHECK(fabs(scale-0.5)<1.0e-12);
    CHECK(fviz_deformation_apply_to_points(points,vectors,2.0,&deformed)==FVIZ_OK);
    out=fviz_points_data(deformed);
    CHECK(fabs((double)out[0].x)<1.0e-6);
    CHECK(fabs((double)out[1].x-14.0)<1.0e-6);
    CHECK(fviz_deformation_update_points(deformed,points,vectors,0.5)==FVIZ_OK);
    out=fviz_points_data(deformed);
    CHECK(fabs((double)out[1].x-11.0)<1.0e-6);

    fviz_release(deformed);
    fviz_release(vectors);
    fviz_release(points);
    return 0;
}
