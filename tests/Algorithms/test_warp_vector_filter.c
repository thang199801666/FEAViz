#include <math.h>
#include <stdio.h>
#include <FViz/FViz.h>
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#expr); return 1; } } while(0)
int main(void)
{
    FVizPolyData* input=NULL;
    FVizWarpVectorFilter* filter=NULL;
    FVizPolyData* output;
    FVizDataArray* vectors=NULL;
    const FVizVec3 points[3]={{0,0,0},{1,0,0},{0,1,0}};
    const uint32_t tri[3]={0,1,2};
    const float u[9]={0,0,1, 0,0,2, 0,0,3};
    CHECK(fviz_poly_data_create(&input)==FVIZ_OK);
    CHECK(fviz_poly_data_add_points(input,points,3u,NULL)==FVIZ_OK);
    CHECK(fviz_poly_data_add_triangles(input,tri,1u)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT32,3u,&vectors)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(vectors,u,3u)==FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(input),"U",vectors)==FVIZ_OK);
    CHECK(fviz_warp_vector_filter_create(&filter)==FVIZ_OK);
    CHECK(fviz_warp_vector_filter_set_vector_name(filter,"U")==FVIZ_OK);
    fviz_warp_vector_filter_set_scale(filter,0.5);
    CHECK(fviz_warp_vector_filter_set_input_data(filter,input)==FVIZ_OK);
    CHECK(fviz_warp_vector_filter_update(filter)==FVIZ_OK);
    output=fviz_warp_vector_filter_output(filter);
    CHECK(output!=NULL && output!=input);
    CHECK(fviz_poly_data_polys(output)==fviz_poly_data_polys(input));
    CHECK(fviz_poly_data_const_point_data(output)==fviz_poly_data_const_point_data(input));
    CHECK(fabs((double)fviz_poly_data_points(output)[0].z-0.5)<1e-6);
    CHECK(fabs((double)fviz_poly_data_points(output)[1].z-1.0)<1e-6);
    CHECK(fabs((double)fviz_poly_data_points(output)[2].z-1.5)<1e-6);
    CHECK(fabs((double)fviz_poly_data_points(input)[2].z)<1e-12);
    CHECK(fviz_poly_data_has_normals(output)==FVIZ_TRUE);
    fviz_release(filter); fviz_release(vectors); fviz_release(input);
    return 0;
}
