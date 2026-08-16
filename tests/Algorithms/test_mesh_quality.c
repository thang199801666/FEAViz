#include <math.h>
#include <stdio.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#expr); return 1; } } while(0)

static FVizResult make_hex(FVizUnstructuredGrid** out_grid)
{
    static const FVizVec3 p[8]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    static const uint32_t ids[8]={0,1,2,3,4,5,6,7};
    FVizUnstructuredGrid* grid=NULL;
    if (fviz_unstructured_grid_create(&grid)!=FVIZ_OK ||
        fviz_unstructured_grid_add_points(grid,p,8u,NULL)!=FVIZ_OK ||
        fviz_unstructured_grid_add_cell(grid,FVIZ_CELL_HEXAHEDRON,8u,ids)!=FVIZ_OK)
    { fviz_release(grid); return fviz_last_error_code(); }
    *out_grid=grid; return FVIZ_OK;
}


static FVizResult make_hex20(FVizUnstructuredGrid** out_grid)
{
    static const FVizVec3 p[20]={
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1},
        {0.5f,0,0},{1,0.5f,0},{0.5f,1,0},{0,0.5f,0},
        {0.5f,0,1},{1,0.5f,1},{0.5f,1,1},{0,0.5f,1},
        {0,0,0.5f},{1,0,0.5f},{1,1,0.5f},{0,1,0.5f}
    };
    FVizId ids[20];
    FVizUnstructuredGrid* grid=NULL;
    FVizSize i;
    for (i=0u;i<20u;++i) ids[i]=(FVizId)i;
    if (fviz_unstructured_grid_create(&grid)!=FVIZ_OK ||
        fviz_unstructured_grid_add_points_ids(grid,p,20u,NULL)!=FVIZ_OK ||
        fviz_unstructured_grid_add_cell_ids(grid,FVIZ_CELL_QUADRATIC_HEXAHEDRON,20u,ids)!=FVIZ_OK)
    { fviz_release(grid); return fviz_last_error_code(); }
    *out_grid=grid; return FVIZ_OK;
}

static double one_value(const FVizDataArray* array)
{
    double v=NAN;
    (void)fviz_data_array_get_component(array,0u,0u,&v);
    return v;
}

int main(void)
{
    FVizUnstructuredGrid* grid=NULL;
    FVizDataArray* quality=NULL;
    FVizMeshQualityFilter* filter=NULL;
    FVizUnstructuredGrid* output;
    const FVizDataArray* result;
    CHECK(make_hex(&grid)==FVIZ_OK);
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_MEASURE,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality)-1.0)<1e-12); fviz_release(quality); quality=NULL;
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_EDGE_RATIO,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality)-1.0)<1e-12); fviz_release(quality); quality=NULL;
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_SCALED_JACOBIAN,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality)-1.0)<1e-12); fviz_release(quality); quality=NULL;
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_MIN_CORNER_ANGLE,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality)-90.0)<1e-10); fviz_release(quality); quality=NULL;
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_MAX_CORNER_ANGLE,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality)-90.0)<1e-10); fviz_release(quality); quality=NULL;
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_WARPAGE,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality))<1e-10); fviz_release(quality); quality=NULL;

    CHECK(fviz_mesh_quality_filter_create(&filter)==FVIZ_OK);
    CHECK(fviz_mesh_quality_filter_set_input_data(filter,grid)==FVIZ_OK);
    fviz_mesh_quality_filter_set_metric(filter,FVIZ_MESH_QUALITY_EDGE_RATIO);
    CHECK(fviz_mesh_quality_filter_set_result_name(filter,"Quality")==FVIZ_OK);
    CHECK(fviz_mesh_quality_filter_update(filter)==FVIZ_OK);
    output=fviz_mesh_quality_filter_output(filter);
    CHECK(output!=NULL && output!=grid);
    CHECK(fviz_unstructured_grid_points(output)==fviz_unstructured_grid_points(grid));
    CHECK(fviz_unstructured_grid_cells(output)==fviz_unstructured_grid_cells(grid));
    result=fviz_attribute_set_const_get(fviz_unstructured_grid_cell_data(output),"Quality");
    CHECK(result!=NULL && fabs(one_value(result)-1.0)<1e-12);
    CHECK(fviz_attribute_set_active_name(fviz_unstructured_grid_cell_data(output),FVIZ_ATTRIBUTE_SCALARS)!=NULL);

    fviz_release(filter);
    fviz_release(grid);

    grid=NULL; quality=NULL;
    CHECK(make_hex20(&grid)==FVIZ_OK);
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_MEASURE,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality)-1.0)<1e-10); fviz_release(quality); quality=NULL;
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_EDGE_RATIO,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality)-1.0)<1e-12); fviz_release(quality); quality=NULL;
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_SCALED_JACOBIAN,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality)-1.0)<1e-12); fviz_release(quality); quality=NULL;
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_MIN_CORNER_ANGLE,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality)-90.0)<1e-10); fviz_release(quality); quality=NULL;
    CHECK(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_WARPAGE,&quality)==FVIZ_OK);
    CHECK(fabs(one_value(quality))<1e-10); fviz_release(quality);
    fviz_release(grid);
    return 0;
}
