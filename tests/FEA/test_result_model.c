#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FEA/FVizFEA.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#expr); return 1; } } while(0)

static int test_field_blocks_and_invariants(void)
{
    static const char* const stress_labels[6]={"S11","S22","S33","S12","S13","S23"};
    static const double stress_values[3][6]={
        {100.0,0.0,0.0,0.0,0.0,0.0},
        {50.0,50.0,50.0,0.0,0.0,0.0},
        {0.0,0.0,0.0,30.0,0.0,0.0}
    };
    static const uint64_t element_labels[3]={101u,102u,103u};
    static const uint32_t ip_ids[3]={1u,1u,1u};
    FVizFEAField* field=NULL;
    FVizDataArray *values=NULL,*ids=NULL,*locals=NULL,*derived=NULL;
    FVizFEAFieldBlockDescriptor block;
    FVizSize component=0u;
    double v=0.0;

    CHECK(fviz_fea_field_create("S","Stress",FVIZ_FEA_FIELD_TENSOR_3D_SYMMETRIC,&field)==FVIZ_OK);
    CHECK(fviz_fea_field_set_component_labels(field,stress_labels,6u)==FVIZ_OK);
    CHECK(fviz_fea_field_find_component(field,"S12",&component)==FVIZ_OK && component==3u);
    CHECK((fviz_fea_field_valid_invariants(field)&FVIZ_FEA_INVARIANT_BIT(FVIZ_FEA_INVARIANT_MISES))!=0u);

    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,6u,&values)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(values,stress_values,3u)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64,1u,&ids)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(ids,element_labels,3u)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT32,1u,&locals)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(locals,ip_ids,3u)==FVIZ_OK);

    fviz_fea_field_block_descriptor_initialize(&block);
    block.instance_name="PART-1-1";
    block.position=FVIZ_FEA_POSITION_INTEGRATION_POINT;
    block.section_point_number=1;
    block.section_point_label="SPOS";
    block.entity_ids=ids;
    block.local_ids=locals;
    block.values=values;
    CHECK(fviz_fea_field_add_block(field,&block,NULL)==FVIZ_OK);
    CHECK(fviz_fea_field_block_count(field)==1u);
    CHECK(strcmp(fviz_fea_field_block_instance_name(field,0u),"PART-1-1")==0);
    CHECK(fviz_fea_field_block_position(field,0u)==FVIZ_FEA_POSITION_INTEGRATION_POINT);
    CHECK(fviz_fea_field_block_section_point_number(field,0u)==1);
    CHECK(strcmp(fviz_fea_field_block_section_point_label(field,0u),"SPOS")==0);

    CHECK(fviz_fea_field_evaluate_invariant(field,0u,FVIZ_FEA_INVARIANT_MISES,&derived)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(derived,0u,0u,&v)==FVIZ_OK && fabs(v-100.0)<1e-10);
    CHECK(fviz_data_array_get_component(derived,1u,0u,&v)==FVIZ_OK && fabs(v)<1e-10);
    CHECK(fviz_data_array_get_component(derived,2u,0u,&v)==FVIZ_OK && fabs(v-sqrt(3.0)*30.0)<1e-10);
    fviz_release(derived); derived=NULL;

    CHECK(fviz_fea_field_evaluate_invariant(field,0u,FVIZ_FEA_INVARIANT_PRESSURE,&derived)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(derived,0u,0u,&v)==FVIZ_OK && fabs(v+100.0/3.0)<1e-10);
    CHECK(fviz_data_array_get_component(derived,1u,0u,&v)==FVIZ_OK && fabs(v+50.0)<1e-10);
    fviz_release(derived); derived=NULL;

    CHECK(fviz_fea_field_evaluate_invariant(field,0u,FVIZ_FEA_INVARIANT_MAX_PRINCIPAL,&derived)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(derived,2u,0u,&v)==FVIZ_OK && fabs(v-30.0)<1e-10);
    fviz_release(derived); derived=NULL;
    CHECK(fviz_fea_field_evaluate_invariant(field,0u,FVIZ_FEA_INVARIANT_MIN_PRINCIPAL,&derived)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(derived,2u,0u,&v)==FVIZ_OK && fabs(v+30.0)<1e-10);
    fviz_release(derived); derived=NULL;
    CHECK(fviz_fea_field_evaluate_invariant(field,0u,FVIZ_FEA_INVARIANT_TRESCA,&derived)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(derived,2u,0u,&v)==FVIZ_OK && fabs(v-60.0)<1e-10);
    fviz_release(derived); derived=NULL;

    CHECK(fviz_fea_field_evaluate_component(field,0u,3u,&derived)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(derived,2u,0u,&v)==FVIZ_OK && fabs(v-30.0)<1e-10);

    fviz_release(derived);
    fviz_release(locals);
    fviz_release(ids);
    fviz_release(values);
    fviz_release(field);
    return 0;
}

static int test_vector_magnitude(void)
{
    static const char* const labels[3]={"U1","U2","U3"};
    const double data[2][3]={{3.0,4.0,0.0},{1.0,2.0,2.0}};
    FVizFEAField* field=NULL;
    FVizDataArray *values=NULL,*magnitude=NULL;
    FVizFEAFieldBlockDescriptor block;
    double v=0.0;
    CHECK(fviz_fea_field_create("U","Spatial displacement",FVIZ_FEA_FIELD_VECTOR,&field)==FVIZ_OK);
    CHECK(fviz_fea_field_set_component_labels(field,labels,3u)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,3u,&values)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(values,data,2u)==FVIZ_OK);
    fviz_fea_field_block_descriptor_initialize(&block);
    block.instance_name="PART-1-1";
    block.position=FVIZ_FEA_POSITION_NODAL;
    block.values=values;
    CHECK(fviz_fea_field_add_block(field,&block,NULL)==FVIZ_OK);
    CHECK(fviz_fea_field_evaluate_invariant(field,0u,FVIZ_FEA_INVARIANT_MAGNITUDE,&magnitude)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(magnitude,0u,0u,&v)==FVIZ_OK && fabs(v-5.0)<1e-12);
    CHECK(fviz_data_array_get_component(magnitude,1u,0u,&v)==FVIZ_OK && fabs(v-3.0)<1e-12);
    fviz_release(magnitude); fviz_release(values); fviz_release(field);
    return 0;
}

static int test_database_hierarchy_and_mtime(void)
{
    FVizFEAResultDatabase* database=NULL;
    FVizFEAStep* step=NULL;
    FVizFEAFrame *frame0=NULL,*frame1=NULL;
    FVizFEAField* u=NULL;
    FVizDataArray* values=NULL;
    FVizFEAFieldBlockDescriptor block;
    FVizFEAFrameInfo info;
    FVizFEAHistoryRegion* history_region=NULL;
    FVizFEAHistorySeries* history_series=NULL;
    FVizMTime before;
    FVizSize lower=0u,upper=0u;
    double alpha=0.0;
    const double zero[3]={0.0,0.0,0.0};
    const double moved[3]={1.0,2.0,3.0};

    CHECK(fviz_fea_result_database_create(&database)==FVIZ_OK);
    CHECK(fviz_fea_step_create("Step-1","Static load",FVIZ_FEA_STEP_TIME,1.0,&step)==FVIZ_OK);

    fviz_fea_frame_info_initialize(&info);
    info.frame_id=0; info.increment_number=0; info.frame_value=0.0; info.description="Initial";
    CHECK(fviz_fea_frame_create(&info,&frame0)==FVIZ_OK);
    info.frame_id=1; info.increment_number=1; info.frame_value=1.0; info.description="End";
    CHECK(fviz_fea_frame_create(&info,&frame1)==FVIZ_OK);

    CHECK(fviz_fea_field_create("U","Displacement",FVIZ_FEA_FIELD_VECTOR,&u)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,3u,&values)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuple(values,zero)==FVIZ_OK);
    fviz_fea_field_block_descriptor_initialize(&block);
    block.position=FVIZ_FEA_POSITION_NODAL;
    block.instance_name="PART-1-1";
    block.values=values;
    CHECK(fviz_fea_field_add_block(u,&block,NULL)==FVIZ_OK);
    CHECK(fviz_fea_frame_add_field(frame1,u)==FVIZ_OK);
    CHECK(fviz_fea_step_add_frame(step,frame0,NULL)==FVIZ_OK);
    CHECK(fviz_fea_step_add_frame(step,frame1,NULL)==FVIZ_OK);
    CHECK(fviz_fea_result_database_add_step(database,step,NULL)==FVIZ_OK);

    CHECK(fviz_fea_result_database_step_count(database)==1u);
    CHECK(fviz_fea_result_database_step(database,"Step-1")==step);
    CHECK(fviz_fea_step_frame_count(step)==2u);
    CHECK(fviz_fea_frame_field(frame1,"U")==u);
    CHECK(fviz_fea_step_find_frame_value(step,0.25,&lower,&upper,&alpha)==FVIZ_OK);
    CHECK(lower==0u && upper==1u && fabs(alpha-0.25)<1e-12);

    CHECK(fviz_fea_history_region_create("Node PART-1-1.1","Nodal history",&history_region)==FVIZ_OK);
    CHECK(fviz_fea_history_series_create("U2","Displacement U2",&history_series)==FVIZ_OK);
    CHECK(fviz_fea_history_series_append(history_series,0.0,0.0)==FVIZ_OK);
    CHECK(fviz_fea_history_series_append(history_series,1.0,4.0)==FVIZ_OK);
    CHECK(fviz_fea_history_region_add_series(history_region,history_series)==FVIZ_OK);
    CHECK(fviz_fea_step_add_history_region(step,history_region)==FVIZ_OK);
    CHECK(fviz_fea_step_history_region_count(step)==1u);
    CHECK(fviz_fea_step_history_region(step,"Node PART-1-1.1")==history_region);
    CHECK(fviz_fea_history_region_series(history_region,"U2")==history_series);
    CHECK(fviz_fea_history_series_interpolate(history_series,0.25,&alpha)==FVIZ_OK && fabs(alpha-1.0)<1e-12);

    before=fviz_object_mtime((FVizObject*)database);
    CHECK(fviz_data_array_set_tuple(values,0u,moved)==FVIZ_OK);
    CHECK(fviz_object_mtime((FVizObject*)database)>before);
    before=fviz_object_mtime((FVizObject*)database);
    CHECK(fviz_fea_history_series_append(history_series,2.0,8.0)==FVIZ_OK);
    CHECK(fviz_object_mtime((FVizObject*)database)>before);

    fviz_release(history_series);
    fviz_release(history_region);
    fviz_release(values);
    fviz_release(u);
    fviz_release(frame1);
    fviz_release(frame0);
    fviz_release(step);
    fviz_release(database);
    return 0;
}

int main(void)
{
    int r=test_field_blocks_and_invariants();
    if (r!=0) return r;
    r=test_vector_magnitude();
    if (r!=0) return r;
    return test_database_hierarchy_and_mtime();
}
