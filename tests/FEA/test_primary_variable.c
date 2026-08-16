#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <FViz/FEA/FVizFEA.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed at %s:%d: %s\n",__FILE__,__LINE__,#expr); return 1; } } while (0)

static FVizDataArray* make_u64(const uint64_t* values,FVizSize count)
{
    FVizDataArray* array=NULL;
    if (fviz_data_array_create(FVIZ_DATA_UINT64,1u,&array)!=FVIZ_OK ||
        fviz_data_array_append_tuples(array,values,count)!=FVIZ_OK)
    { fviz_release(array); return NULL; }
    return array;
}

static FVizDataArray* make_i32(const int32_t* values,FVizSize count)
{
    FVizDataArray* array=NULL;
    if (fviz_data_array_create(FVIZ_DATA_INT32,1u,&array)!=FVIZ_OK ||
        fviz_data_array_append_tuples(array,values,count)!=FVIZ_OK)
    { fviz_release(array); return NULL; }
    return array;
}

static FVizDataArray* make_f64(const double* values,FVizSize count,uint32_t components)
{
    FVizDataArray* array=NULL;
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64,components,&array)!=FVIZ_OK ||
        fviz_data_array_append_tuples(array,values,count)!=FVIZ_OK)
    { fviz_release(array); return NULL; }
    return array;
}

static FVizUnstructuredGrid* make_grid(void)
{
    static const FVizVec3 points[4]={{0,0,0},{1,0,0},{0,1,0},{1,1,0}};
    static const FVizId tri0[3]={0,1,2};
    static const FVizId tri1[3]={1,3,2};
    static const uint64_t point_labels[4]={1,2,3,4};
    static const uint64_t cell_labels[2]={101,102};
    FVizUnstructuredGrid* grid=NULL;
    FVizDataArray *pids=NULL,*cids=NULL;
    if (fviz_unstructured_grid_create(&grid)!=FVIZ_OK ||
        fviz_unstructured_grid_add_points_ids(grid,points,4u,NULL)!=FVIZ_OK ||
        fviz_unstructured_grid_add_cell_ids(grid,FVIZ_CELL_TRIANGLE,3u,tri0)!=FVIZ_OK ||
        fviz_unstructured_grid_add_cell_ids(grid,FVIZ_CELL_TRIANGLE,3u,tri1)!=FVIZ_OK)
        goto fail;
    pids=make_u64(point_labels,4u); cids=make_u64(cell_labels,2u);
    if (pids==NULL || cids==NULL ||
        fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid),"NodeLabels",pids)!=FVIZ_OK ||
        fviz_attribute_set_set_active(fviz_unstructured_grid_point_data(grid),FVIZ_ATTRIBUTE_GLOBAL_IDS,"NodeLabels")!=FVIZ_OK ||
        fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid),"ElementLabels",cids)!=FVIZ_OK ||
        fviz_attribute_set_set_active(fviz_unstructured_grid_cell_data(grid),FVIZ_ATTRIBUTE_GLOBAL_IDS,"ElementLabels")!=FVIZ_OK)
        goto fail;
    fviz_release(pids); fviz_release(cids); return grid;
fail:
    fviz_release(pids); fviz_release(cids); fviz_release(grid); return NULL;
}

static int test_native_and_cache(FVizUnstructuredGrid* grid,FVizFEAPrimaryVariableEvaluator* evaluator)
{
    static const uint64_t ids[4]={1,2,3,4};
    static const double data[4]={1,2,3,4};
    FVizFEAField* field=NULL; FVizDataArray *values=NULL,*entity=NULL;
    FVizFEAFieldBlockDescriptor block; FVizFEAPrimaryVariable variable; FVizFEAPrimaryVariableResult* result=NULL;
    FVizFEAPrimaryVariableCacheStatistics stats;
    const FVizDataArray* display; double v=0.0,lo=0.0,hi=0.0;
    CHECK(fviz_fea_field_create("TEMP","Temperature",FVIZ_FEA_FIELD_SCALAR,&field)==FVIZ_OK);
    values=make_f64(data,4u,1u); entity=make_u64(ids,4u); CHECK(values!=NULL && entity!=NULL);
    fviz_fea_field_block_descriptor_initialize(&block); block.instance_name="PART-1-1"; block.position=FVIZ_FEA_POSITION_NODAL; block.entity_ids=entity; block.values=values;
    CHECK(fviz_fea_field_add_block(field,&block,NULL)==FVIZ_OK);
    fviz_fea_primary_variable_initialize(&variable); variable.instance_name="PART-1-1"; variable.target_position=FVIZ_FEA_POSITION_NODAL;
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_OK);
    display=fviz_fea_primary_variable_result_display_values(result); CHECK(display!=NULL && fviz_data_array_tuple_count(display)==4u);
    CHECK(fviz_data_array_get_component(display,3u,0u,&v)==FVIZ_OK && fabs(v-4.0)<1e-12);
    CHECK(fviz_fea_primary_variable_result_display_range(result,&lo,&hi)==FVIZ_TRUE && lo==1.0 && hi==4.0);
    fviz_release(result); result=NULL;
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_OK);
    stats=fviz_fea_primary_variable_evaluator_cache_statistics(evaluator); CHECK(stats.hits>=1u && stats.entries==1u);
    fviz_release(result); result=NULL;
    CHECK(fviz_data_array_set_component(values,0u,0u,9.0)==FVIZ_OK);
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_OK);
    stats=fviz_fea_primary_variable_evaluator_cache_statistics(evaluator); CHECK(stats.misses>=2u);
    CHECK(fviz_data_array_get_component(fviz_fea_primary_variable_result_display_values(result),0u,0u,&v)==FVIZ_OK && fabs(v-9.0)<1e-12);
    fviz_release(result); fviz_release(values); fviz_release(entity); fviz_release(field); return 0;
}

static int test_centroid(FVizUnstructuredGrid* grid,FVizFEAPrimaryVariableEvaluator* evaluator)
{
    static const uint64_t ids[2]={101,102}; static const double data[2]={10,20};
    FVizFEAField* field=NULL; FVizDataArray *values=NULL,*entity=NULL; FVizFEAFieldBlockDescriptor block;
    FVizFEAPrimaryVariable variable; FVizFEAPrimaryVariableResult* result=NULL; double a=0,b=0;
    CHECK(fviz_fea_field_create("EVOL","Element value",FVIZ_FEA_FIELD_SCALAR,&field)==FVIZ_OK);
    values=make_f64(data,2u,1u); entity=make_u64(ids,2u); CHECK(values && entity);
    fviz_fea_field_block_descriptor_initialize(&block); block.position=FVIZ_FEA_POSITION_CENTROID; block.entity_ids=entity; block.values=values;
    CHECK(fviz_fea_field_add_block(field,&block,NULL)==FVIZ_OK);
    fviz_fea_primary_variable_initialize(&variable); variable.target_position=FVIZ_FEA_POSITION_CENTROID;
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_OK);
    CHECK(fviz_fea_primary_variable_result_association(result)==FVIZ_FEA_DISPLAY_ASSOCIATION_CELL);
    CHECK(fviz_data_array_get_component(fviz_fea_primary_variable_result_display_values(result),0u,0u,&a)==FVIZ_OK &&
          fviz_data_array_get_component(fviz_fea_primary_variable_result_display_values(result),1u,0u,&b)==FVIZ_OK && a==10.0 && b==20.0);
    fviz_release(result); fviz_release(values); fviz_release(entity); fviz_release(field); return 0;
}

static int test_averaging_boundaries(FVizUnstructuredGrid* grid,FVizFEAPrimaryVariableEvaluator* evaluator)
{
    static const uint64_t e1[3]={101,101,101},e2[3]={102,102,102};
    static const int32_t local[3]={1,2,3}; static const double v1[3]={10,10,10},v2[3]={100,100,100};
    FVizFEAField* field=NULL; FVizDataArray *a1=NULL,*a2=NULL,*id1=NULL,*id2=NULL,*l1=NULL,*l2=NULL;
    FVizFEAFieldBlockDescriptor block; FVizFEAPrimaryVariable variable; FVizFEAPrimaryVariableResult* result=NULL;
    const FVizDataArray *display,*mask; double v=0.0;
    CHECK(fviz_fea_field_create("S","Scalar stress",FVIZ_FEA_FIELD_SCALAR,&field)==FVIZ_OK);
    a1=make_f64(v1,3u,1u);a2=make_f64(v2,3u,1u);id1=make_u64(e1,3u);id2=make_u64(e2,3u);l1=make_i32(local,3u);l2=make_i32(local,3u);
    CHECK(a1&&a2&&id1&&id2&&l1&&l2);
    fviz_fea_field_block_descriptor_initialize(&block); block.position=FVIZ_FEA_POSITION_ELEMENT_NODAL; block.entity_ids=id1; block.local_ids=l1; block.values=a1;
    CHECK(fviz_fea_field_add_block(field,&block,NULL)==FVIZ_OK);
    block.entity_ids=id2; block.local_ids=l2; block.values=a2; CHECK(fviz_fea_field_add_block(field,&block,NULL)==FVIZ_OK);
    fviz_fea_primary_variable_initialize(&variable); variable.source_position=FVIZ_FEA_POSITION_ELEMENT_NODAL; variable.target_position=FVIZ_FEA_POSITION_NODAL;
    variable.local_id_base=FVIZ_FEA_LOCAL_ID_ONE_BASED; variable.averaging_threshold_percent=1000.0; variable.average_across_blocks=FVIZ_FALSE;
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_OK);
    display=fviz_fea_primary_variable_result_display_values(result); mask=fviz_fea_primary_variable_result_discontinuity_mask(result);
    CHECK(((const uint8_t*)fviz_data_array_const_data(mask))[1]==1u && ((const uint8_t*)fviz_data_array_const_data(mask))[2]==1u);
    CHECK(isnan(((const double*)fviz_data_array_const_data(display))[1]));
    fviz_release(result); result=NULL;
    variable.average_across_blocks=FVIZ_TRUE;
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(fviz_fea_primary_variable_result_display_values(result),1u,0u,&v)==FVIZ_OK && fabs(v-55.0)<1e-12);
    fviz_release(result); result=NULL;
    variable.averaging_threshold_percent=20.0;
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_OK);
    CHECK(((const uint8_t*)fviz_data_array_const_data(fviz_fea_primary_variable_result_discontinuity_mask(result)))[1]==1u);
    fviz_release(result);
    fviz_release(a1);fviz_release(a2);fviz_release(id1);fviz_release(id2);fviz_release(l1);fviz_release(l2);fviz_release(field);return 0;
}

static int test_integration_point_and_filter(FVizUnstructuredGrid* grid,FVizFEAPrimaryVariableEvaluator* evaluator)
{
    static const char* const labels[6]={"S11","S22","S33","S12","S13","S23"};
    static const uint64_t elem_ids[2]={101,102}; static const int32_t ip_ids[2]={1,1};
    static const double stress[2][6]={{10,0,0,0,0,0},{30,0,0,0,0,0}}; static const uint64_t only_first[1]={101};
    FVizFEAField* field=NULL; FVizDataArray *values=NULL,*entity=NULL,*local=NULL,*filter=NULL;
    FVizFEAFieldBlockDescriptor block; FVizFEAPrimaryVariable variable; FVizFEAPrimaryVariableResult* result=NULL; double v=0.0;
    CHECK(fviz_fea_field_create("S","Stress",FVIZ_FEA_FIELD_TENSOR_3D_SYMMETRIC,&field)==FVIZ_OK);
    CHECK(fviz_fea_field_set_component_labels(field,labels,6u)==FVIZ_OK);
    values=make_f64(&stress[0][0],2u,6u);entity=make_u64(elem_ids,2u);local=make_i32(ip_ids,2u);filter=make_u64(only_first,1u);CHECK(values&&entity&&local&&filter);
    fviz_fea_field_block_descriptor_initialize(&block);block.position=FVIZ_FEA_POSITION_INTEGRATION_POINT;block.entity_ids=entity;block.local_ids=local;block.values=values;
    CHECK(fviz_fea_field_add_block(field,&block,NULL)==FVIZ_OK);
    fviz_fea_primary_variable_initialize(&variable);variable.operation=FVIZ_FEA_PRIMARY_INVARIANT;variable.invariant=FVIZ_FEA_INVARIANT_MISES;
    variable.source_position=FVIZ_FEA_POSITION_INTEGRATION_POINT;variable.target_position=FVIZ_FEA_POSITION_NODAL;variable.averaging_threshold_percent=1000.0;
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(fviz_fea_primary_variable_result_display_values(result),1u,0u,&v)==FVIZ_OK && fabs(v-20.0)<1e-10);
    fviz_release(result);result=NULL;
    variable.entity_filter_ids=filter;
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(fviz_fea_primary_variable_result_display_values(result),0u,0u,&v)==FVIZ_OK && fabs(v-10.0)<1e-10);
    CHECK(isnan(((const double*)fviz_data_array_const_data(fviz_fea_primary_variable_result_display_values(result)))[3]));
    fviz_release(result);fviz_release(values);fviz_release(entity);fviz_release(local);fviz_release(filter);fviz_release(field);return 0;
}

static int test_duplicate_global_ids_rejected(FVizFEAPrimaryVariableEvaluator* evaluator)
{
    static const FVizVec3 points[4]={{0,0,0},{1,0,0},{0,1,0},{1,1,0}};
    static const FVizId tri0[3]={0,1,2};
    static const FVizId tri1[3]={1,3,2};
    static const uint64_t duplicate_point_labels[4]={1,1,3,4};
    static const uint64_t result_ids[4]={1,2,3,4};
    static const double data[4]={1,2,3,4};
    FVizUnstructuredGrid* grid=NULL;
    FVizDataArray *labels=NULL,*values=NULL,*entity=NULL;
    FVizFEAField* field=NULL;
    FVizFEAFieldBlockDescriptor block;
    FVizFEAPrimaryVariable variable;
    FVizFEAPrimaryVariableResult* result=NULL;
    CHECK(fviz_unstructured_grid_create(&grid)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(grid,points,4u,NULL)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell_ids(grid,FVIZ_CELL_TRIANGLE,3u,tri0)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell_ids(grid,FVIZ_CELL_TRIANGLE,3u,tri1)==FVIZ_OK);
    labels=make_u64(duplicate_point_labels,4u);
    CHECK(labels!=NULL);
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid),"DuplicateNodeLabels",labels)==FVIZ_OK);
    CHECK(fviz_attribute_set_set_active(fviz_unstructured_grid_point_data(grid),FVIZ_ATTRIBUTE_GLOBAL_IDS,"DuplicateNodeLabels")==FVIZ_OK);
    CHECK(fviz_fea_field_create("TEMP","Temperature",FVIZ_FEA_FIELD_SCALAR,&field)==FVIZ_OK);
    values=make_f64(data,4u,1u); entity=make_u64(result_ids,4u); CHECK(values&&entity);
    fviz_fea_field_block_descriptor_initialize(&block);
    block.position=FVIZ_FEA_POSITION_NODAL; block.entity_ids=entity; block.values=values;
    CHECK(fviz_fea_field_add_block(field,&block,NULL)==FVIZ_OK);
    fviz_fea_primary_variable_initialize(&variable); variable.target_position=FVIZ_FEA_POSITION_NODAL;
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(result==NULL);
    fviz_release(labels); fviz_release(values); fviz_release(entity); fviz_release(field); fviz_release(grid);
    return 0;
}

int main(void)
{
    FVizUnstructuredGrid* grid=make_grid(); FVizFEAPrimaryVariableEvaluator* evaluator=NULL;
    CHECK(grid!=NULL); CHECK(fviz_fea_primary_variable_evaluator_create(&evaluator)==FVIZ_OK);
    CHECK(test_native_and_cache(grid,evaluator)==0);
    fviz_fea_primary_variable_evaluator_clear_cache(evaluator);
    CHECK(test_centroid(grid,evaluator)==0);
    fviz_fea_primary_variable_evaluator_clear_cache(evaluator);
    CHECK(test_averaging_boundaries(grid,evaluator)==0);
    fviz_fea_primary_variable_evaluator_clear_cache(evaluator);
    CHECK(test_integration_point_and_filter(grid,evaluator)==0);
    fviz_fea_primary_variable_evaluator_clear_cache(evaluator);
    CHECK(test_duplicate_global_ids_rejected(evaluator)==0);
    fviz_release(evaluator); fviz_release(grid);
    return 0;
}
