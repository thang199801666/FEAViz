#include <math.h>
#include <FViz/FViz.h>

#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)

static int test_tube(void)
{
    FVizPolyData* input=NULL; FVizTubeFilter* filter=NULL; FVizPolyData* output;
    FVizDataArray* point_values=NULL; FVizDataArray* cell_values=NULL;
    const FVizDataArray* mapped_points; const FVizDataArray* mapped_cells;
    uint32_t p0,p1; double v;
    CHECK(fviz_poly_data_create(&input)==FVIZ_OK);
    CHECK(fviz_poly_data_add_point(input,fviz_vec3(0,0,0),&p0)==FVIZ_OK);
    CHECK(fviz_poly_data_add_point(input,fviz_vec3(2,0,0),&p1)==FVIZ_OK);
    CHECK(fviz_poly_data_add_line(input,p0,p1)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&point_values)==FVIZ_OK);
    v=10.0; CHECK(fviz_data_array_append_tuple(point_values,&v)==FVIZ_OK);
    v=20.0; CHECK(fviz_data_array_append_tuple(point_values,&v)==FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(input),"NodeValue",point_values)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&cell_values)==FVIZ_OK);
    v=99.0; CHECK(fviz_data_array_append_tuple(cell_values,&v)==FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_cell_data(input),"BeamValue",cell_values)==FVIZ_OK);
    CHECK(fviz_tube_filter_create(&filter)==FVIZ_OK);
    fviz_tube_filter_set_sides(filter,4u);
    fviz_tube_filter_set_radius(filter,0.2);
    fviz_tube_filter_set_capping(filter,FVIZ_FALSE);
    CHECK(fviz_tube_filter_set_input_data(filter,input)==FVIZ_OK);
    CHECK(fviz_tube_filter_update(filter)==FVIZ_OK);
    output=fviz_tube_filter_output(filter);
    CHECK(output!=NULL && fviz_poly_data_point_count(output)==8u && fviz_poly_data_triangle_count(output)==8u);
    CHECK(fviz_poly_data_validate(output)==FVIZ_OK);
    mapped_points=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(output),"NodeValue");
    mapped_cells=fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(output),"BeamValue");
    CHECK(mapped_points!=NULL && fviz_data_array_tuple_count(mapped_points)==8u);
    CHECK(mapped_cells!=NULL && fviz_data_array_tuple_count(mapped_cells)==8u);
    CHECK(fviz_data_array_get_component(mapped_points,0u,0u,&v)==FVIZ_OK && fabs(v-10.0)<1e-12);
    CHECK(fviz_data_array_get_component(mapped_points,7u,0u,&v)==FVIZ_OK && fabs(v-20.0)<1e-12);
    CHECK(fviz_data_array_get_component(mapped_cells,4u,0u,&v)==FVIZ_OK && fabs(v-99.0)<1e-12);
    fviz_release(filter); fviz_release(cell_values); fviz_release(point_values); fviz_release(input);
    return 0;
}

static int test_shell_extrusion(void)
{
    FVizPolyData* input=NULL; FVizShellExtrusionFilter* filter=NULL; FVizPolyData* output;
    FVizDataArray* cells=NULL; const FVizDataArray* mapped=NULL;
    uint32_t ids[4]; FVizVec3 p; double v;
    CHECK(fviz_poly_data_create(&input)==FVIZ_OK);
    CHECK(fviz_poly_data_add_point(input,fviz_vec3(0,0,0),&ids[0])==FVIZ_OK);
    CHECK(fviz_poly_data_add_point(input,fviz_vec3(1,0,0),&ids[1])==FVIZ_OK);
    CHECK(fviz_poly_data_add_point(input,fviz_vec3(1,1,0),&ids[2])==FVIZ_OK);
    CHECK(fviz_poly_data_add_point(input,fviz_vec3(0,1,0),&ids[3])==FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(input,ids[0],ids[1],ids[2])==FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(input,ids[0],ids[2],ids[3])==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&cells)==FVIZ_OK);
    v=7.0; CHECK(fviz_data_array_append_tuple(cells,&v)==FVIZ_OK);
    v=8.0; CHECK(fviz_data_array_append_tuple(cells,&v)==FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_poly_data_cell_data(input),"ShellValue",cells)==FVIZ_OK);
    CHECK(fviz_shell_extrusion_filter_create(&filter)==FVIZ_OK);
    fviz_shell_extrusion_filter_set_thickness(filter,0.2);
    CHECK(fviz_shell_extrusion_filter_set_input_data(filter,input)==FVIZ_OK);
    CHECK(fviz_shell_extrusion_filter_update(filter)==FVIZ_OK);
    output=fviz_shell_extrusion_filter_output(filter);
    CHECK(output!=NULL && fviz_poly_data_point_count(output)==8u && fviz_poly_data_triangle_count(output)==12u);
    CHECK(fviz_poly_data_get_point(output,0u,&p)==FVIZ_OK && fabs((double)p.z-0.1)<1e-6);
    CHECK(fviz_poly_data_get_point(output,4u,&p)==FVIZ_OK && fabs((double)p.z+0.1)<1e-6);
    mapped=fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(output),"ShellValue");
    CHECK(mapped!=NULL && fviz_data_array_tuple_count(mapped)==12u);
    CHECK(fviz_data_array_get_component(mapped,0u,0u,&v)==FVIZ_OK && fabs(v-7.0)<1e-12);
    CHECK(fviz_data_array_get_component(mapped,2u,0u,&v)==FVIZ_OK && fabs(v-7.0)<1e-12);
    CHECK(fviz_poly_data_validate(output)==FVIZ_OK);
    fviz_release(filter); fviz_release(cells); fviz_release(input);
    return 0;
}

int main(void)
{
    int r=test_tube(); if (r!=0) return r;
    return test_shell_extrusion();
}
