#include <math.h>
#include <stdio.h>
#include <FViz/FEA/FVizFEA.h>

#define TRY(x) do { FVizResult _r=(x); if(_r!=FVIZ_OK){fprintf(stderr,"FEAViz error %d: %s\n",(int)_r,fviz_last_error_message()); goto fail;} } while(0)

int main(void)
{
    static const FVizVec3 points[8]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    FVizId ids[8]={0,1,2,3,4,5,6,7};
    FVizVec3 gauss[8]; FVizSize gauss_count=0u,offsets[2]={0u,8u},i;
    FVizUnstructuredGrid* grid=NULL;
    FVizDataArray *ip_stress=NULL,*nodal_stress=NULL,*mises=NULL,*quality=NULL,*u=NULL;
    FVizPolyData *surface=NULL,*beam=NULL,*shell=NULL;
    FVizWarpVectorFilter* warp=NULL; FVizTubeFilter* tube=NULL; FVizShellExtrusionFilter* extrude=NULL;
    FVizArrayCalculatorOptions calc;
    double min_mises=0.0,max_mises=0.0,q=0.0;
    uint32_t a,b,c,d;

    TRY(fviz_unstructured_grid_create(&grid));
    TRY(fviz_unstructured_grid_add_points_ids(grid,points,8u,NULL));
    TRY(fviz_unstructured_grid_add_cell_ids(grid,FVIZ_CELL_HEXAHEDRON,8u,ids));
    TRY(fviz_data_array_create(FVIZ_DATA_FLOAT64,6u,&ip_stress));
    TRY(fviz_integration_point_standard_coordinates(FVIZ_CELL_HEXAHEDRON,8u,gauss,8u,&gauss_count));
    for(i=0u;i<gauss_count;++i)
    {
        const double s[6]={100.0+10.0*gauss[i].x,50.0+5.0*gauss[i].y,25.0+2.0*gauss[i].z,5.0,2.0,1.0};
        TRY(fviz_data_array_append_tuple(ip_stress,s));
    }
    TRY(fviz_unstructured_grid_extrapolate_integration_point_data(grid,ip_stress,offsets,NULL,NULL,&nodal_stress));
    TRY(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid),"Stress",nodal_stress));
    TRY(fviz_attribute_set_set_active(fviz_unstructured_grid_point_data(grid),FVIZ_ATTRIBUTE_TENSORS,"Stress"));

    fviz_array_calculator_options_initialize(&calc); calc.operation=FVIZ_ARRAY_CALC_EQUIVALENT_DEVIATORIC;
    TRY(fviz_array_calculator_compute(nodal_stress,&calc,&mises));
    TRY(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid),"S_Mises",mises));
    TRY(fviz_attribute_set_set_active(fviz_unstructured_grid_point_data(grid),FVIZ_ATTRIBUTE_SCALARS,"S_Mises"));
    TRY(fviz_data_array_get_range(mises,0,FVIZ_TRUE,&min_mises,&max_mises));

    TRY(fviz_mesh_quality_compute(grid,FVIZ_MESH_QUALITY_SCALED_JACOBIAN,&quality));
    TRY(fviz_data_array_get_component(quality,0u,0u,&q));

    TRY(fviz_data_array_create(FVIZ_DATA_FLOAT64,3u,&u));
    for(i=0u;i<8u;++i)
    {
        const double tuple[3]={0.02*points[i].x,0.01*points[i].y,0.05*points[i].z};
        TRY(fviz_data_array_append_tuple(u,tuple));
    }
    TRY(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid),"U",u));
    TRY(fviz_attribute_set_set_active(fviz_unstructured_grid_point_data(grid),FVIZ_ATTRIBUTE_VECTORS,"U"));
    TRY(fviz_unstructured_grid_extract_geometry(grid,&surface));
    TRY(fviz_warp_vector_filter_create(&warp));
    TRY(fviz_warp_vector_filter_set_vector_name(warp,"U"));
    fviz_warp_vector_filter_set_scale(warp,5.0);
    TRY(fviz_warp_vector_filter_set_input_data(warp,surface));
    TRY(fviz_warp_vector_filter_update(warp));

    TRY(fviz_poly_data_create(&beam));
    TRY(fviz_poly_data_add_point(beam,fviz_vec3(0,0,0),&a));
    TRY(fviz_poly_data_add_point(beam,fviz_vec3(2,0.5f,0),&b));
    TRY(fviz_poly_data_add_line(beam,a,b));
    TRY(fviz_tube_filter_create(&tube)); fviz_tube_filter_set_sides(tube,12u); fviz_tube_filter_set_radius(tube,0.05); TRY(fviz_tube_filter_set_input_data(tube,beam)); TRY(fviz_tube_filter_update(tube));

    TRY(fviz_poly_data_create(&shell));
    TRY(fviz_poly_data_add_point(shell,fviz_vec3(0,0,0),&a)); TRY(fviz_poly_data_add_point(shell,fviz_vec3(1,0,0),&b));
    TRY(fviz_poly_data_add_point(shell,fviz_vec3(1,1,0),&c)); TRY(fviz_poly_data_add_point(shell,fviz_vec3(0,1,0),&d));
    TRY(fviz_poly_data_add_triangle(shell,a,b,c)); TRY(fviz_poly_data_add_triangle(shell,a,c,d));
    TRY(fviz_shell_extrusion_filter_create(&extrude)); fviz_shell_extrusion_filter_set_thickness(extrude,0.08); TRY(fviz_shell_extrusion_filter_set_input_data(extrude,shell)); TRY(fviz_shell_extrusion_filter_update(extrude));

    printf("FEAviz 0.28 production pipeline: mises=[%.3f, %.3f] scaledJacobian=%.3f surfaceTris=%zu beamTris=%zu shellTris=%zu\n",
        min_mises,max_mises,q,
        (size_t)fviz_poly_data_triangle_count(fviz_warp_vector_filter_output(warp)),
        (size_t)fviz_poly_data_triangle_count(fviz_tube_filter_output(tube)),
        (size_t)fviz_poly_data_triangle_count(fviz_shell_extrusion_filter_output(extrude)));

    fviz_release(extrude); fviz_release(shell); fviz_release(tube); fviz_release(beam); fviz_release(warp); fviz_release(surface);
    fviz_release(u); fviz_release(quality); fviz_release(mises); fviz_release(nodal_stress); fviz_release(ip_stress); fviz_release(grid);
    return 0;
fail:
    fviz_release(extrude); fviz_release(shell); fviz_release(tube); fviz_release(beam); fviz_release(warp); fviz_release(surface);
    fviz_release(u); fviz_release(quality); fviz_release(mises); fviz_release(nodal_stress); fviz_release(ip_stress); fviz_release(grid);
    return 1;
}
