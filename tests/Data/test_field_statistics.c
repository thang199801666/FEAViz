#include <math.h>
#include <stdio.h>
#include <FViz/FViz.h>
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#expr); return 1; } } while(0)

static FVizResult make_leaf(float x0,float x1,double a,double b,FVizPolyData** out)
{
    FVizPolyData* p=NULL; FVizDataArray* s=NULL; const FVizVec3 points[2]={{x0,0,0},{x1,0,0}}; const double v[2]={a,b};
    if (fviz_poly_data_create(&p)!=FVIZ_OK || fviz_poly_data_add_points(p,points,2u,NULL)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&s)!=FVIZ_OK || fviz_data_array_append_tuples(s,v,2u)!=FVIZ_OK ||
        fviz_attribute_set_add(fviz_poly_data_point_data(p),"S",s)!=FVIZ_OK)
    { fviz_release(s); fviz_release(p); return fviz_last_error_code(); }
    fviz_release(s); *out=p; return FVIZ_OK;
}

int main(void)
{
    FVizPolyData *a=NULL,*b=NULL,*c=NULL;
    FVizPartitionedDataSet *parts0=NULL,*parts1=NULL;
    FVizTemporalDataSet* temporal=NULL;
    FVizFieldStatisticsOptions options;
    FVizFieldStatistics stats;
    FVizFieldMoments moments;
    CHECK(make_leaf(0,1,2.0,-3.0,&a)==FVIZ_OK);
    CHECK(make_leaf(10,11,7.0,4.0,&b)==FVIZ_OK);
    CHECK(make_leaf(20,21,1.0,12.0,&c)==FVIZ_OK);
    CHECK(fviz_partitioned_data_set_create(&parts0)==FVIZ_OK);
    CHECK(fviz_partitioned_data_set_add_partition(parts0,(FVizDataObject*)a,"A",NULL)==FVIZ_OK);
    CHECK(fviz_partitioned_data_set_add_partition(parts0,(FVizDataObject*)b,"B",NULL)==FVIZ_OK);
    CHECK(fviz_partitioned_data_set_create(&parts1)==FVIZ_OK);
    CHECK(fviz_partitioned_data_set_add_partition(parts1,(FVizDataObject*)c,"C",NULL)==FVIZ_OK);
    CHECK(fviz_temporal_data_set_create(&temporal)==FVIZ_OK);
    CHECK(fviz_temporal_data_set_add_step(temporal,0.0,(FVizDataObject*)parts0,NULL)==FVIZ_OK);
    CHECK(fviz_temporal_data_set_add_step(temporal,2.0,(FVizDataObject*)parts1,NULL)==FVIZ_OK);
    fviz_field_statistics_options_initialize(&options);
    CHECK(fviz_field_statistics_compute((FVizDataObject*)temporal,"S",&options,&stats)==FVIZ_OK);
    CHECK(stats.valid==FVIZ_TRUE && stats.finite_tuple_count==6u);
    CHECK(fabs(stats.minimum.value+3.0)<1e-12 && stats.minimum.has_time && stats.minimum.temporal_index==0u);
    CHECK(stats.minimum.has_partition && stats.minimum.partition_index==0u && stats.minimum.tuple_id==1u);
    CHECK(stats.minimum.has_world_position && fabs((double)stats.minimum.world_position.x-1.0)<1e-6);
    CHECK(fabs(stats.maximum.value-12.0)<1e-12 && stats.maximum.has_time && stats.maximum.temporal_index==1u);
    CHECK(stats.maximum.has_partition && stats.maximum.partition_index==0u && stats.maximum.tuple_id==1u);
    CHECK(stats.maximum.has_world_position && fabs((double)stats.maximum.world_position.x-21.0)<1e-6);
    CHECK(fabs(stats.maximum.time-2.0)<1e-12);
    CHECK(fviz_field_statistics_compute_moments((FVizDataObject*)temporal,"S",&options,&moments)==FVIZ_OK);
    CHECK(moments.valid==FVIZ_TRUE && moments.finite_tuple_count==6u);
    CHECK(fabs(moments.mean-(23.0/6.0))<1e-12);
    CHECK(fabs(moments.root_mean_square*moments.root_mean_square-(223.0/6.0))<1e-10);
    CHECK(fabs(moments.variance-((223.0/6.0)-(23.0/6.0)*(23.0/6.0)))<1e-12);
    CHECK(fabs(moments.standard_deviation*moments.standard_deviation-moments.variance)<1e-12);
    {
        FVizPolyData* ghost_leaf=NULL;
        FVizMultiBlockDataSet* blocks=NULL;
        FVizDataArray *values=NULL,*ghosts=NULL;
        const FVizVec3 pts[3]={{0,0,0},{1,0,0},{2,0,0}};
        const double field[3]={1.0,2.0,100.0};
        const uint8_t flags[3]={FVIZ_GHOST_NONE,FVIZ_GHOST_NONE,FVIZ_GHOST_DUPLICATE};
        CHECK(fviz_poly_data_create(&ghost_leaf)==FVIZ_OK);
        CHECK(fviz_poly_data_add_points(ghost_leaf,pts,3u,NULL)==FVIZ_OK);
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&values)==FVIZ_OK);
        CHECK(fviz_data_array_append_tuples(values,field,3u)==FVIZ_OK);
        CHECK(fviz_data_array_create(FVIZ_DATA_UINT8,1u,&ghosts)==FVIZ_OK);
        CHECK(fviz_data_array_append_tuples(ghosts,flags,3u)==FVIZ_OK);
        CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(ghost_leaf),"S",values)==FVIZ_OK);
        CHECK(fviz_attribute_set_add(fviz_poly_data_point_data(ghost_leaf),FVIZ_GHOST_ARRAY_NAME,ghosts)==FVIZ_OK);
        CHECK(fviz_multi_block_data_set_create(&blocks)==FVIZ_OK);
        CHECK(fviz_multi_block_data_set_add_block(blocks,(FVizDataObject*)ghost_leaf,"ghosted",NULL)==FVIZ_OK);
        CHECK(fviz_field_statistics_compute((FVizDataObject*)blocks,"S",&options,&stats)==FVIZ_OK);
        CHECK(stats.finite_tuple_count==2u && fabs(stats.maximum.value-2.0)<1e-12);
        CHECK(fviz_field_statistics_compute_moments((FVizDataObject*)blocks,"S",&options,&moments)==FVIZ_OK);
        CHECK(moments.finite_tuple_count==2u && fabs(moments.mean-1.5)<1e-12);
        CHECK(fabs(moments.variance-0.25)<1e-12);
        options.ignore_ghosts=FVIZ_FALSE;
        CHECK(fviz_field_statistics_compute((FVizDataObject*)blocks,"S",&options,&stats)==FVIZ_OK);
        CHECK(stats.finite_tuple_count==3u && fabs(stats.maximum.value-100.0)<1e-12);
        options.ignore_ghosts=FVIZ_TRUE;
        fviz_release(blocks); fviz_release(ghosts); fviz_release(values); fviz_release(ghost_leaf);
    }
    fviz_release(temporal); fviz_release(parts1); fviz_release(parts0); fviz_release(c); fviz_release(b); fviz_release(a);
    return 0;
}
