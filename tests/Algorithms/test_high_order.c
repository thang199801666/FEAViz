#include <math.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int test_shape_weights(void)
{
    double w[20];
    FVizSize n=0u,i;
    double sum=0.0;
    CHECK(fviz_cell_type_shape_weights(FVIZ_CELL_QUADRATIC_TETRA,fviz_vec3(0.2f,0.1f,0.3f),w,20u,&n)==FVIZ_OK);
    CHECK(n==10u);
    for (i=0u;i<n;++i) sum+=w[i];
    CHECK(fabs(sum-1.0)<1.0e-12);
    sum=0.0;
    CHECK(fviz_cell_type_shape_weights(FVIZ_CELL_QUADRATIC_HEXAHEDRON,fviz_vec3(0.2f,-0.3f,0.4f),w,20u,&n)==FVIZ_OK);
    CHECK(n==20u);
    for (i=0u;i<n;++i) sum+=w[i];
    CHECK(fabs(sum-1.0)<1.0e-12);
    CHECK(fviz_cell_type_linear_weights(FVIZ_CELL_QUADRATIC_TETRA,fviz_vec3(0,0,0),w,20u,&n)==FVIZ_ERROR_NOT_SUPPORTED);
    return 0;
}

static int test_tet10_surface(void)
{
    static const FVizVec3 p[10]={
        {0,0,0},{1,0,0},{0,1,0},{0,0,1},
        {0.5f,0,0},{0.5f,0.5f,0},{0,0.5f,0},
        {0,0,0.5f},{0.5f,0,0.5f},{0,0.5f,0.5f}
    };
    FVizId ids[10]={0,1,2,3,4,5,6,7,8,9};
    FVizUnstructuredGrid* g=NULL;
    FVizPolyData* s=NULL;
    const FVizDataArray* original=NULL;
    CHECK(fviz_unstructured_grid_create(&g)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(g,p,10u,NULL)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_QUADRATIC_TETRA,10u,ids)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_validate(g)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_extract_surface(g,&s)==FVIZ_OK);
    CHECK(fviz_poly_data_point_count(s)==10u);
    CHECK(fviz_poly_data_triangle_count(s)==16u);
    original=fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(s),"FVizOriginalCellIds");
    CHECK(original!=NULL && fviz_data_array_tuple_count(original)==16u);
    CHECK(fviz_poly_data_validate(s)==FVIZ_OK);
    fviz_release(s); fviz_release(g);
    return 0;
}

static int test_hex20_surface(void)
{
    static const FVizVec3 p[20]={
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1},
        {0.5f,0,0},{1,0.5f,0},{0.5f,1,0},{0,0.5f,0},
        {0.5f,0,1},{1,0.5f,1},{0.5f,1,1},{0,0.5f,1},
        {0,0,0.5f},{1,0,0.5f},{1,1,0.5f},{0,1,0.5f}
    };
    FVizId ids[20];
    FVizUnstructuredGrid* g=NULL;
    FVizPolyData* s=NULL;
    FVizSize i;
    for (i=0u;i<20u;++i) ids[i]=(FVizId)i;
    CHECK(fviz_unstructured_grid_create(&g)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(g,p,20u,NULL)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_QUADRATIC_HEXAHEDRON,20u,ids)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_extract_surface(g,&s)==FVIZ_OK);
    CHECK(fviz_poly_data_point_count(s)==20u);
    CHECK(fviz_poly_data_triangle_count(s)==36u);
    CHECK(fviz_poly_data_validate(s)==FVIZ_OK);
    fviz_release(s); fviz_release(g);
    return 0;
}


static int attach_linear_field(FVizUnstructuredGrid* g,const FVizVec3* points,FVizSize count)
{
    FVizDataArray* a=NULL;
    FVizSize i;
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&a)==FVIZ_OK);
    CHECK(fviz_data_array_reserve(a,count)==FVIZ_OK);
    for (i=0u;i<count;++i)
    {
        const double v=(double)points[i].x+2.0*(double)points[i].y+3.0*(double)points[i].z;
        CHECK(fviz_data_array_append_tuple(a,&v)==FVIZ_OK);
    }
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(g),"LinearField",a)==FVIZ_OK);
    fviz_release(a);
    return 0;
}

static int test_high_order_probe(void)
{
    static const FVizVec3 tet[10]={
        {0,0,0},{1,0,0},{0,1,0},{0,0,1},
        {0.5f,0,0},{0.5f,0.5f,0},{0,0.5f,0},
        {0,0,0.5f},{0.5f,0,0.5f},{0,0.5f,0.5f}
    };
    static const FVizVec3 hex[20]={
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1},
        {0.5f,0,0},{1,0.5f,0},{0.5f,1,0},{0,0.5f,0},
        {0.5f,0,1},{1,0.5f,1},{0.5f,1,1},{0,0.5f,1},
        {0,0,0.5f},{1,0,0.5f},{1,1,0.5f},{0,1,0.5f}
    };
    FVizId ids[20];
    FVizUnstructuredGrid* g=NULL;
    FVizPointLocator* locator=NULL;
    FVizSize i;
    float value=0.0f;
    for (i=0u;i<20u;++i) ids[i]=(FVizId)i;
    CHECK(fviz_unstructured_grid_create(&g)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(g,tet,10u,NULL)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_QUADRATIC_TETRA,10u,ids)==FVIZ_OK);
    CHECK(attach_linear_field(g,tet,10u)==0);
    CHECK(fviz_point_locator_create(&locator)==FVIZ_OK);
    CHECK(fviz_point_locator_set_grid(locator,g)==FVIZ_OK);
    CHECK(fviz_point_locator_interpolate_scalar(locator,"LinearField",fviz_vec3(0.2f,0.1f,0.3f),&value)==FVIZ_OK);
    CHECK(fabs((double)value-1.3)<2.0e-4);
    fviz_release(locator); fviz_release(g); locator=NULL; g=NULL;

    CHECK(fviz_unstructured_grid_create(&g)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(g,hex,20u,NULL)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_QUADRATIC_HEXAHEDRON,20u,ids)==FVIZ_OK);
    CHECK(attach_linear_field(g,hex,20u)==0);
    CHECK(fviz_point_locator_create(&locator)==FVIZ_OK);
    CHECK(fviz_point_locator_set_grid(locator,g)==FVIZ_OK);
    CHECK(fviz_point_locator_interpolate_scalar(locator,"LinearField",fviz_vec3(0.2f,0.3f,0.4f),&value)==FVIZ_OK);
    CHECK(fabs((double)value-2.0)<3.0e-4);
    fviz_release(locator); fviz_release(g);
    return 0;
}

int main(void)
{
    int r=test_shape_weights(); if (r!=0) return r;
    r=test_tet10_surface(); if (r!=0) return r;
    r=test_hex20_surface(); if (r!=0) return r;
    return test_high_order_probe();
}
