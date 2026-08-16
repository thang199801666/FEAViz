#include <FViz/FViz.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec v;
    return timespec_get(&v, TIME_UTC)==TIME_UTC ? (double)v.tv_sec+(double)v.tv_nsec*1.0e-9 : 0.0;
}
int main(void)
{
    const uint32_t n=256u, edge=n+1u;
    const FVizSize point_count=(FVizSize)edge*edge;
    const FVizSize triangle_count=(FVizSize)n*n*2u;
    FVizVec3* points=(FVizVec3*)fviz_alloc(point_count*sizeof(*points));
    uint32_t* triangles=(uint32_t*)fviz_alloc(triangle_count*3u*sizeof(*triangles));
    float* scalars=(float*)fviz_alloc(point_count*sizeof(*scalars));
    FVizPolyData* mesh=NULL; FVizDataArray* array=NULL; FVizContourFilter* filter=NULL;
    const float levels[7]={-0.75f,-0.5f,-0.25f,0.0f,0.25f,0.5f,0.75f};
    FVizSize p=0u,t=0u; uint32_t x,y; double start,seconds;
    if(points==NULL||triangles==NULL||scalars==NULL) return 2;
    for(y=0u;y<edge;++y) for(x=0u;x<edge;++x)
    {
        const float fx=(float)x/(float)n, fy=(float)y/(float)n;
        points[p]=fviz_vec3(fx,fy,0.0f);
        scalars[p]=sinf(fx*18.8495559f)*cosf(fy*12.5663706f);
        ++p;
    }
    for(y=0u;y<n;++y) for(x=0u;x<n;++x)
    {
        const uint32_t a=y*edge+x,b=a+1u,c=a+edge,d=c+1u;
        triangles[t++]=a; triangles[t++]=b; triangles[t++]=d;
        triangles[t++]=a; triangles[t++]=d; triangles[t++]=c;
    }
    if(fviz_poly_data_create(&mesh)!=FVIZ_OK ||
       fviz_poly_data_add_points(mesh,points,point_count,NULL)!=FVIZ_OK ||
       fviz_poly_data_add_triangles(mesh,triangles,triangle_count)!=FVIZ_OK ||
       fviz_data_array_create(FVIZ_DATA_FLOAT32,1u,&array)!=FVIZ_OK ||
       fviz_data_array_append_tuples(array,scalars,point_count)!=FVIZ_OK ||
       fviz_attribute_set_add(fviz_poly_data_point_data(mesh),"S",array)!=FVIZ_OK ||
       fviz_contour_filter_create("S",levels,7u,&filter)!=FVIZ_OK ||
       fviz_contour_filter_set_input(filter,mesh)!=FVIZ_OK) return 3;
    start=wall_seconds();
    if(fviz_contour_filter_update(filter)!=FVIZ_OK) return 4;
    seconds=wall_seconds()-start;
    printf("points=%llu triangles=%llu levels=7 lines=%llu seconds=%.6f\n",
        (unsigned long long)point_count,(unsigned long long)triangle_count,
        (unsigned long long)fviz_poly_data_line_cell_count(fviz_contour_filter_output(filter)),seconds);
    fviz_release(filter); fviz_release(array); fviz_release(mesh);
    fviz_free(scalars); fviz_free(triangles); fviz_free(points);
    return 0;
}
