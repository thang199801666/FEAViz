#include <math.h>
#include <FViz/FEA/FVizFEA.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int test_hex8_full_integration(void)
{
    static const FVizVec3 points[8]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    static const float signs[8][3]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    FVizId ids[8]={0,1,2,3,4,5,6,7};
    FVizUnstructuredGrid* g=NULL;
    FVizDataArray* ip=NULL;
    FVizDataArray* nodal=NULL;
    FVizVec3 gauss[8];
    FVizSize offsets[2]={0u,8u};
    FVizSize n=0u,i;
    CHECK(fviz_unstructured_grid_create(&g)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(g,points,8u,NULL)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_HEXAHEDRON,8u,ids)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&ip)==FVIZ_OK);
    CHECK(fviz_integration_point_standard_coordinates(FVIZ_CELL_HEXAHEDRON,8u,gauss,8u,&n)==FVIZ_OK && n==8u);
    for (i=0u;i<8u;++i)
    {
        const double value=2.0+3.0*(double)gauss[i].x-4.0*(double)gauss[i].y+5.0*(double)gauss[i].z;
        CHECK(fviz_data_array_append_tuple(ip,&value)==FVIZ_OK);
    }
    CHECK(fviz_unstructured_grid_extrapolate_integration_point_data(g,ip,offsets,NULL,NULL,&nodal)==FVIZ_OK);
    CHECK(fviz_data_array_tuple_count(nodal)==8u && fviz_data_array_components(nodal)==1u);
    for (i=0u;i<8u;++i)
    {
        double value=0.0;
        const double expected=2.0+3.0*(double)signs[i][0]-4.0*(double)signs[i][1]+5.0*(double)signs[i][2];
        CHECK(fviz_data_array_get_component(nodal,i,0u,&value)==FVIZ_OK);
        CHECK(fabs(value-expected)<2.0e-6);
    }
    fviz_release(nodal); fviz_release(ip); fviz_release(g);
    return 0;
}

static int test_tet4_mean_fallback(void)
{
    static const FVizVec3 points[4]={{0,0,0},{1,0,0},{0,1,0},{0,0,1}};
    FVizId ids[4]={0,1,2,3};
    FVizSize offsets[2]={0u,1u};
    FVizUnstructuredGrid* g=NULL;
    FVizDataArray* ip=NULL;
    FVizDataArray* nodal=NULL;
    double value=42.0;
    FVizSize i;
    CHECK(fviz_unstructured_grid_create(&g)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(g,points,4u,NULL)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_TETRA,4u,ids)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&ip)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(ip,&value)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_extrapolate_integration_point_data(g,ip,offsets,NULL,NULL,&nodal)==FVIZ_OK);
    for (i=0u;i<4u;++i)
    {
        double v=0.0; CHECK(fviz_data_array_get_component(nodal,i,0u,&v)==FVIZ_OK); CHECK(fabs(v-42.0)<1.0e-12);
    }
    fviz_release(nodal); fviz_release(ip); fviz_release(g);
    return 0;
}

static int test_hex20_27_point_integration(void)
{
    static const FVizVec3 node_parametric[20]={
        {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
        {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1},
        {0,-1,-1},{1,0,-1},{0,1,-1},{-1,0,-1},
        {0,-1,1},{1,0,1},{0,1,1},{-1,0,1},
        {-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}
    };
    FVizId ids[20];
    FVizVec3 gauss[27];
    FVizSize offsets[2]={0u,27u};
    FVizUnstructuredGrid* g=NULL;
    FVizDataArray* ip=NULL;
    FVizDataArray* nodal=NULL;
    FVizSize i,n=0u;
    for (i=0u;i<20u;++i) ids[i]=(FVizId)i;
    CHECK(fviz_unstructured_grid_create(&g)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(g,node_parametric,20u,NULL)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_QUADRATIC_HEXAHEDRON,20u,ids)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&ip)==FVIZ_OK);
    CHECK(fviz_integration_point_standard_coordinates(FVIZ_CELL_QUADRATIC_HEXAHEDRON,27u,gauss,27u,&n)==FVIZ_OK && n==27u);
    for (i=0u;i<27u;++i)
    {
        const double value=7.0+2.5*(double)gauss[i].x-1.75*(double)gauss[i].y+4.25*(double)gauss[i].z;
        CHECK(fviz_data_array_append_tuple(ip,&value)==FVIZ_OK);
    }
    CHECK(fviz_unstructured_grid_extrapolate_integration_point_data(g,ip,offsets,NULL,NULL,&nodal)==FVIZ_OK);
    CHECK(fviz_data_array_tuple_count(nodal)==20u);
    for (i=0u;i<20u;++i)
    {
        double actual=0.0;
        const double expected=7.0+2.5*(double)node_parametric[i].x-1.75*(double)node_parametric[i].y+4.25*(double)node_parametric[i].z;
        CHECK(fviz_data_array_get_component(nodal,i,0u,&actual)==FVIZ_OK);
        CHECK(fabs(actual-expected)<2.0e-5);
    }
    fviz_release(nodal); fviz_release(ip); fviz_release(g);
    return 0;
}

static int test_options_contract(void)
{
    FVizIntegrationPointExtrapolationOptions options;
    FVizSize count=123u;
    fviz_integration_point_extrapolation_options_initialize(&options);
    CHECK(options.struct_size==sizeof(options));
    CHECK(options.fallback_policy==FVIZ_INTEGRATION_POINT_CELL_MEAN);
    CHECK(fviz_integration_point_standard_coordinates(FVIZ_CELL_HEXAHEDRON,8u,NULL,1u,&count)==FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(count==8u);
    return 0;
}

int main(void)
{
    int r=test_hex8_full_integration();
    if (r!=0) return r;
    r=test_tet4_mean_fallback(); if (r!=0) return r;
    r=test_hex20_27_point_integration(); if (r!=0) return r;
    return test_options_contract();
}
