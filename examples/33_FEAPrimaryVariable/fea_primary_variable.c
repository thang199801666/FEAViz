#include <stdio.h>
#include <FViz/FEA/FVizFEA.h>

int main(void)
{
    const FVizVec3 points[3]={{0,0,0},{1,0,0},{0,1,0}};
    const FVizId triangle[3]={0,1,2};
    const uint64_t element_label=101u;
    const int32_t ip=1;
    const double stress[6]={120.0,40.0,10.0,15.0,0.0,0.0};
    FVizUnstructuredGrid* grid=NULL; FVizDataArray *cell_ids=NULL,*entity=NULL,*local=NULL,*values=NULL;
    FVizFEAField* field=NULL; FVizFEAFieldBlockDescriptor block;
    FVizFEAPrimaryVariableEvaluator* evaluator=NULL; FVizFEAPrimaryVariable selection; FVizFEAPrimaryVariableResult* result=NULL;
    double lo=0.0,hi=0.0;
    if (fviz_unstructured_grid_create(&grid)!=FVIZ_OK || fviz_unstructured_grid_add_points_ids(grid,points,3u,NULL)!=FVIZ_OK ||
        fviz_unstructured_grid_add_cell_ids(grid,FVIZ_CELL_TRIANGLE,3u,triangle)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&cell_ids)!=FVIZ_OK || fviz_data_array_append_tuple(cell_ids,&element_label)!=FVIZ_OK ||
        fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid),"ElementLabels",cell_ids)!=FVIZ_OK ||
        fviz_attribute_set_set_active(fviz_unstructured_grid_cell_data(grid),FVIZ_ATTRIBUTE_GLOBAL_IDS,"ElementLabels")!=FVIZ_OK ||
        fviz_fea_field_create("S","Cauchy stress",FVIZ_FEA_FIELD_TENSOR_3D_SYMMETRIC,&field)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT64,6u,&values)!=FVIZ_OK || fviz_data_array_append_tuple(values,stress)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&entity)!=FVIZ_OK || fviz_data_array_append_tuple(entity,&element_label)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_INT32,1u,&local)!=FVIZ_OK || fviz_data_array_append_tuple(local,&ip)!=FVIZ_OK)
        goto fail;
    fviz_fea_field_block_descriptor_initialize(&block); block.instance_name="PART-1-1"; block.position=FVIZ_FEA_POSITION_INTEGRATION_POINT;
    block.entity_ids=entity; block.local_ids=local; block.values=values;
    if (fviz_fea_field_add_block(field,&block,NULL)!=FVIZ_OK || fviz_fea_primary_variable_evaluator_create(&evaluator)!=FVIZ_OK) goto fail;
    fviz_fea_primary_variable_initialize(&selection); selection.instance_name="PART-1-1"; selection.operation=FVIZ_FEA_PRIMARY_INVARIANT;
    selection.invariant=FVIZ_FEA_INVARIANT_MISES; selection.target_position=FVIZ_FEA_POSITION_NODAL; selection.averaging_threshold_percent=1000.0;
    if (fviz_fea_primary_variable_evaluate(evaluator,field,grid,&selection,&result)!=FVIZ_OK) goto fail;
    (void)fviz_fea_primary_variable_result_display_range(result,&lo,&hi);
    printf("Primary S:Mises -> %s, tuples=%llu, range=[%.6g, %.6g]\n",
        fviz_fea_result_position_name(fviz_fea_primary_variable_result_target_position(result)),
        (unsigned long long)fviz_data_array_tuple_count(fviz_fea_primary_variable_result_display_values(result)),lo,hi);
    fviz_release(result);fviz_release(evaluator);fviz_release(field);fviz_release(values);fviz_release(entity);fviz_release(local);fviz_release(cell_ids);fviz_release(grid);
    return 0;
fail:
    fprintf(stderr,"FEAViz error: %s\n",fviz_last_error_message());
    fviz_release(result);fviz_release(evaluator);fviz_release(field);fviz_release(values);fviz_release(entity);fviz_release(local);fviz_release(cell_ids);fviz_release(grid);
    return 1;
}
