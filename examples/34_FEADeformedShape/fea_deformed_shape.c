#include <stdio.h>

#include <FViz/FEA/FVizFEA.h>

#define TRY(call) do { FVizResult _r=(call); if(_r!=FVIZ_OK){ fprintf(stderr,"%s failed: %s\n",#call,fviz_last_error_message()); return 1; } } while(0)

int main(void)
{
    FVizUnstructuredGrid* grid=NULL;
    FVizDataArray *node_ids=NULL,*u_values=NULL,*u_ids=NULL;
    FVizFEAField* u_field=NULL;
    FVizFEAFrame* frame=NULL;
    FVizFEADeformedShapeController* controller=NULL;
    FVizFEADeformedShapeResult* result=NULL;
    FVizFEAFieldBlockDescriptor block;
    FVizFEAFrameInfo frame_info;
    FVizFEADeformedShapeOptions options;
    const FVizVec3 points[4]={{0,0,0},{10,0,0},{0,4,0},{0,0,3}};
    const FVizId tet[4]={0,1,2,3};
    const uint64_t mesh_labels[4]={101,102,103,104};
    const uint64_t result_labels[4]={104,102,101,103};
    const double displacements[4][3]={{0.0,0.0,0.6},{0.3,0.0,0.0},{0.0,0.0,0.0},{0.0,0.4,0.0}};

    TRY(fviz_unstructured_grid_create(&grid));
    TRY(fviz_unstructured_grid_add_points_ids(grid,points,4,NULL));
    TRY(fviz_unstructured_grid_add_cell_ids(grid,FVIZ_CELL_TETRA,4,tet));
    TRY(fviz_data_array_create(FVIZ_DATA_UINT64,1,&node_ids));
    TRY(fviz_data_array_append_tuples(node_ids,mesh_labels,4));
    TRY(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid),"NodeLabels",node_ids));
    TRY(fviz_attribute_set_set_active(fviz_unstructured_grid_point_data(grid),FVIZ_ATTRIBUTE_GLOBAL_IDS,"NodeLabels"));

    TRY(fviz_fea_field_create("U","Spatial displacement",FVIZ_FEA_FIELD_VECTOR,&u_field));
    TRY(fviz_data_array_create(FVIZ_DATA_FLOAT64,3,&u_values));
    TRY(fviz_data_array_append_tuples(u_values,displacements,4));
    TRY(fviz_data_array_create(FVIZ_DATA_UINT64,1,&u_ids));
    TRY(fviz_data_array_append_tuples(u_ids,result_labels,4));
    fviz_fea_field_block_descriptor_initialize(&block);
    block.instance_name="PART-1-1";
    block.position=FVIZ_FEA_POSITION_NODAL;
    block.entity_ids=u_ids;
    block.values=u_values;
    TRY(fviz_fea_field_add_block(u_field,&block,NULL));

    fviz_fea_frame_info_initialize(&frame_info);
    frame_info.frame_id=1;
    frame_info.frame_value=1.0;
    frame_info.description="Load end";
    TRY(fviz_fea_frame_create(&frame_info,&frame));
    TRY(fviz_fea_frame_add_field(frame,u_field));

    TRY(fviz_fea_deformed_shape_controller_create(&controller));
    fviz_fea_deformed_shape_options_initialize(&options);
    options.instance_name="PART-1-1";
    options.scale_mode=FVIZ_FEA_DEFORMATION_SCALE_AUTO;
    options.auto_target_fraction=0.15;
    TRY(fviz_fea_deformed_shape_evaluate(controller,frame,grid,&options,&result));

    printf("FEAViz %s FEA deformed shape\n",fviz_version_string());
    printf("mapped points: %zu / %zu\n",
           (size_t)fviz_fea_deformed_shape_result_mapped_point_count(result),
           (size_t)fviz_unstructured_grid_point_count(grid));
    printf("max |U|: %.6g\n",fviz_fea_deformed_shape_result_metrics(result)->maximum_magnitude);
    printf("auto scale: %.6g\n",fviz_fea_deformed_shape_result_scale_factor(result));

    fviz_release(result);
    fviz_release(controller);
    fviz_release(frame);
    fviz_release(u_ids);
    fviz_release(u_values);
    fviz_release(u_field);
    fviz_release(node_ids);
    fviz_release(grid);
    return 0;
}
