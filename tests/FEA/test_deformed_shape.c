#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FEA/FVizFEA.h>

#define CHECK(x) do { if(!(x)){ fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while(0)

static int build_grid(FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* grid=NULL;
    FVizDataArray* ids=NULL;
    const FVizVec3 points[4]={
        {0.0f,0.0f,0.0f},{10.0f,0.0f,0.0f},{0.0f,1.0f,0.0f},{0.0f,0.0f,1.0f}
    };
    const FVizId cell[4]={0u,1u,2u,3u};
    const uint64_t labels[4]={10u,20u,30u,40u};
    CHECK(fviz_unstructured_grid_create(&grid)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points_ids(grid,points,4u,NULL)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell_ids(grid,FVIZ_CELL_TETRA,4u,cell)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64,1u,&ids)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(ids,labels,4u)==FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid),"NodeLabels",ids)==FVIZ_OK);
    CHECK(fviz_attribute_set_set_active(fviz_unstructured_grid_point_data(grid),FVIZ_ATTRIBUTE_GLOBAL_IDS,"NodeLabels")==FVIZ_OK);
    fviz_release(ids);
    *out_grid=grid;
    return 0;
}

static int build_frame(FVizFEAFrame** out_frame,FVizDataArray** out_values)
{
    FVizFEAFrame* frame=NULL;
    FVizFEAField* field=NULL;
    FVizDataArray *values=NULL,*ids=NULL;
    FVizFEAFieldBlockDescriptor block;
    FVizFEAFrameInfo info;
    const uint64_t labels[4]={40u,20u,10u,30u};
    const double u[4][3]={{4.0,0.0,0.0},{2.0,0.0,0.0},{1.0,0.0,0.0},{3.0,0.0,0.0}};
    const char* comps[3]={"U1","U2","U3"};

    fviz_fea_frame_info_initialize(&info);
    info.frame_id=1; info.frame_value=1.0; info.description="End";
    CHECK(fviz_fea_frame_create(&info,&frame)==FVIZ_OK);
    CHECK(fviz_fea_field_create("U","Spatial displacement",FVIZ_FEA_FIELD_VECTOR,&field)==FVIZ_OK);
    CHECK(fviz_fea_field_set_component_labels(field,comps,3u)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,3u,&values)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(values,u,4u)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_UINT64,1u,&ids)==FVIZ_OK);
    CHECK(fviz_data_array_append_tuples(ids,labels,4u)==FVIZ_OK);
    fviz_fea_field_block_descriptor_initialize(&block);
    block.instance_name="PART-1-1";
    block.position=FVIZ_FEA_POSITION_NODAL;
    block.entity_ids=ids;
    block.values=values;
    CHECK(fviz_fea_field_add_block(field,&block,NULL)==FVIZ_OK);
    CHECK(fviz_fea_frame_add_field(frame,field)==FVIZ_OK);
    fviz_release(ids);
    fviz_release(field);
    *out_frame=frame;
    *out_values=values;
    return 0;
}

int main(void)
{
    FVizUnstructuredGrid* grid=NULL;
    FVizFEAFrame* frame=NULL;
    FVizDataArray* source_values=NULL;
    FVizFEADeformedShapeController* controller=NULL;
    FVizFEADeformedShapeResult *result=NULL,*cached=NULL;
    FVizFEADeformedShapeOptions options;
    const FVizDataArray* mapped;
    const FVizVec3* points;
    FVizFEADeformedShapeCacheStatistics stats;
    double v=0.0;
    double changed[3]={5.0,0.0,0.0};

    CHECK(build_grid(&grid)==0);
    CHECK(build_frame(&frame,&source_values)==0);
    CHECK(fviz_fea_deformed_shape_controller_create(&controller)==FVIZ_OK);
    fviz_fea_deformed_shape_options_initialize(&options);
    options.instance_name="PART-1-1";
    options.scale_mode=FVIZ_FEA_DEFORMATION_SCALE_UNIFORM;
    options.uniform_scale=2.0;
    CHECK(fviz_fea_deformed_shape_evaluate(controller,frame,grid,&options,&result)==FVIZ_OK);
    CHECK(result!=NULL);
    CHECK(fviz_fea_deformed_shape_result_mapped_point_count(result)==4u);
    CHECK(fviz_fea_deformed_shape_result_base_grid(result)==grid);
    CHECK(fviz_fea_deformed_shape_result_missing_point_count(result)==0u);
    CHECK(fabs(fviz_fea_deformed_shape_result_scale_factor(result)-2.0)<1.0e-12);
    mapped=fviz_fea_deformed_shape_result_displacements(result);
    CHECK(fviz_data_array_get_component(mapped,0u,0u,&v)==FVIZ_OK && fabs(v-1.0)<1.0e-12);
    CHECK(fviz_data_array_get_component(mapped,3u,0u,&v)==FVIZ_OK && fabs(v-4.0)<1.0e-12);
    points=fviz_points_data(fviz_unstructured_grid_points((FVizUnstructuredGrid*)fviz_fea_deformed_shape_result_grid(result)));
    CHECK(fabs((double)points[0].x-2.0)<1.0e-6);
    CHECK(fabs((double)points[1].x-14.0)<1.0e-6);

    CHECK(fviz_fea_deformed_shape_evaluate(controller,frame,grid,&options,&cached)==FVIZ_OK);
    stats=fviz_fea_deformed_shape_controller_cache_statistics(controller);
    CHECK(stats.hits==1u && stats.misses==1u && stats.populated!=FVIZ_FALSE);
    fviz_release(cached); cached=NULL;

    CHECK(fviz_data_array_set_tuple(source_values,0u,changed)==FVIZ_OK);
    CHECK(fviz_fea_deformed_shape_evaluate(controller,frame,grid,&options,&cached)==FVIZ_OK);
    stats=fviz_fea_deformed_shape_controller_cache_statistics(controller);
    CHECK(stats.misses==2u);
    fviz_release(cached); cached=NULL;

    options.state=FVIZ_FEA_DEFORMATION_SUPERIMPOSED;
    options.scale_mode=FVIZ_FEA_DEFORMATION_SCALE_AUTO;
    options.auto_target_fraction=0.10;
    CHECK(fviz_fea_deformed_shape_evaluate(controller,frame,grid,&options,&cached)==FVIZ_OK);
    CHECK(fviz_fea_deformed_shape_result_state(cached)==FVIZ_FEA_DEFORMATION_SUPERIMPOSED);
    CHECK(fviz_fea_deformed_shape_result_base_grid(cached)==grid);
    CHECK(fviz_fea_deformed_shape_result_scale_factor(cached)>0.0);
    CHECK(fviz_fea_deformed_shape_result_metrics(cached)->maximum_magnitude>=5.0);

    fviz_release(cached);
    fviz_release(result);
    fviz_release(controller);
    fviz_release(source_values);
    fviz_release(frame);
    fviz_release(grid);
    return 0;
}
