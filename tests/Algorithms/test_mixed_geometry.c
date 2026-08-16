#include <math.h>
#include <FViz/FViz.h>
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)

int main(void)
{
    FVizUnstructuredGrid* g=NULL;
    FVizPolyData* out=NULL;
    FVizUnstructuredGridGeometryFilter* filter=NULL;
    FVizDataArray* cell_values=NULL;
    const FVizDataArray* mapped=NULL;
    FVizId id;
    FVizId cell_ids[10];
    FVizSize i;
    CHECK(fviz_unstructured_grid_create(&g)==FVIZ_OK);
    /* Vertex cell. */
    CHECK(fviz_unstructured_grid_add_points_ids(g,(FVizVec3[]){{-2,0,0}},1u,&id)==FVIZ_OK);
    cell_ids[0]=id; CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_VERTEX,1u,cell_ids)==FVIZ_OK);
    /* Quadratic beam/truss. */
    {
        const FVizVec3 p[3]={{-1,0,0},{1,0,0},{0,0.2f,0}};
        FVizId first;
        CHECK(fviz_unstructured_grid_add_points_ids(g,p,3u,&first)==FVIZ_OK);
        cell_ids[0]=first; cell_ids[1]=first+1u; cell_ids[2]=first+2u;
        CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_QUADRATIC_EDGE,3u,cell_ids)==FVIZ_OK);
    }
    /* Quadratic shell triangle. */
    {
        const FVizVec3 p[6]={{0,0,0},{1,0,0},{0,1,0},{0.5f,0,0},{0.5f,0.5f,0},{0,0.5f,0}};
        FVizId first;
        CHECK(fviz_unstructured_grid_add_points_ids(g,p,6u,&first)==FVIZ_OK);
        for(i=0u;i<6u;++i) cell_ids[i]=first+i;
        CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_QUADRATIC_TRIANGLE,6u,cell_ids)==FVIZ_OK);
    }
    /* Quadratic tetra solid. */
    {
        const FVizVec3 p[10]={{0,0,1},{1,0,1},{0,1,1},{0,0,2},{0.5f,0,1},{0.5f,0.5f,1},{0,0.5f,1},{0,0,1.5f},{0.5f,0,1.5f},{0,0.5f,1.5f}};
        FVizId first;
        CHECK(fviz_unstructured_grid_add_points_ids(g,p,10u,&first)==FVIZ_OK);
        for(i=0u;i<10u;++i) cell_ids[i]=first+i;
        CHECK(fviz_unstructured_grid_add_cell_ids(g,FVIZ_CELL_QUADRATIC_TETRA,10u,cell_ids)==FVIZ_OK);
    }
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&cell_values)==FVIZ_OK);
    for(i=0u;i<4u;++i){const double v=100.0+(double)i; CHECK(fviz_data_array_append_tuple(cell_values,&v)==FVIZ_OK);}
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_cell_data(g),"ElementValue",cell_values)==FVIZ_OK);
    CHECK(fviz_attribute_set_set_active(fviz_unstructured_grid_cell_data(g),FVIZ_ATTRIBUTE_SCALARS,"ElementValue")==FVIZ_OK);

    CHECK(fviz_unstructured_grid_extract_geometry(g,&out)==FVIZ_OK);
    CHECK(fviz_poly_data_vert_cell_count(out)==1u);
    CHECK(fviz_poly_data_line_count(out)==2u);
    CHECK(fviz_poly_data_triangle_count(out)==20u);
    CHECK(fviz_poly_data_cell_count(out)==23u);
    mapped=fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(out),"ElementValue");
    CHECK(mapped!=NULL && fviz_data_array_tuple_count(mapped)==23u);
    for(i=0u;i<23u;++i)
    {
        double v=0.0; CHECK(fviz_data_array_get_component(mapped,i,0u,&v)==FVIZ_OK);
        if(i==0u) CHECK(fabs(v-100.0)<1e-12);
        else if(i<3u) CHECK(fabs(v-101.0)<1e-12);
        else if(i<7u) CHECK(fabs(v-102.0)<1e-12);
        else CHECK(fabs(v-103.0)<1e-12);
    }
    fviz_release(out); out=NULL;
    CHECK(fviz_unstructured_grid_geometry_filter_create(&filter)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_geometry_filter_set_input_data(filter,g)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_geometry_filter_update(filter)==FVIZ_OK);
    out=fviz_unstructured_grid_geometry_filter_output(filter);
    CHECK(out!=NULL && fviz_poly_data_triangle_count(out)==20u);
    fviz_release(filter); fviz_release(cell_values); fviz_release(g);
    return 0;
}
