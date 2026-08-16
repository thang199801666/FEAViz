#include <string.h>
#include <FViz/FViz.h>
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int main(void)
{
    FVizMultiBlockDataSet* root=NULL; FVizPartitionedDataSet* parts=NULL;
    FVizImageData* image=NULL; FVizRectilinearGrid* rect=NULL; FVizPolyData* tri=NULL;
    FVizCompositeGeometryFilter* filter=NULL; FVizMultiBlockDataSet* out;
    const int64_t extent[6]={0,1,0,1,0,1}; const double c[2]={0.0,1.0};
    uint32_t p0,p1,p2;
    CHECK(fviz_multi_block_data_set_create(&root)==FVIZ_OK);
    CHECK(fviz_partitioned_data_set_create(&parts)==FVIZ_OK);
    CHECK(fviz_image_data_create(&image)==FVIZ_OK && fviz_image_data_set_extent(image,extent)==FVIZ_OK);
    CHECK(fviz_rectilinear_grid_create(&rect)==FVIZ_OK && fviz_rectilinear_grid_set_extent(rect,extent)==FVIZ_OK);
    CHECK(fviz_rectilinear_grid_set_coordinate_values(rect,0,c,2)==FVIZ_OK);
    CHECK(fviz_rectilinear_grid_set_coordinate_values(rect,1,c,2)==FVIZ_OK);
    CHECK(fviz_rectilinear_grid_set_coordinate_values(rect,2,c,2)==FVIZ_OK);
    CHECK(fviz_poly_data_create(&tri)==FVIZ_OK);
    CHECK(fviz_poly_data_add_point(tri,fviz_vec3(0,0,0),&p0)==FVIZ_OK);
    CHECK(fviz_poly_data_add_point(tri,fviz_vec3(1,0,0),&p1)==FVIZ_OK);
    CHECK(fviz_poly_data_add_point(tri,fviz_vec3(0,1,0),&p2)==FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(tri,p0,p1,p2)==FVIZ_OK);
    CHECK(fviz_partitioned_data_set_add_partition(parts,(FVizDataObject*)tri,"Triangle",NULL)==FVIZ_OK);
    CHECK(fviz_multi_block_data_set_add_block(root,(FVizDataObject*)image,"Image",NULL)==FVIZ_OK);
    CHECK(fviz_multi_block_data_set_add_block(root,(FVizDataObject*)rect,"Rect",NULL)==FVIZ_OK);
    CHECK(fviz_multi_block_data_set_add_block(root,(FVizDataObject*)parts,"Parts",NULL)==FVIZ_OK);
    CHECK(fviz_composite_geometry_filter_create(&filter)==FVIZ_OK);
    CHECK(fviz_composite_geometry_filter_set_input_data(filter,root)==FVIZ_OK);
    CHECK(fviz_composite_geometry_filter_update(filter)==FVIZ_OK);
    {
        FVizCompositeGeometryCacheStatistics stats=fviz_composite_geometry_filter_cache_statistics(filter);
        CHECK(stats.entries==3u && stats.hits==0u && stats.misses==3u && stats.pruned==0u);
    }
    out=fviz_composite_geometry_filter_output(filter);
    CHECK(out!=NULL && fviz_multi_block_data_set_count(out)==3u);
    CHECK(strcmp(fviz_multi_block_data_set_block_name(out,0),"Image")==0);
    CHECK(fviz_object_is_type((const FVizObject*)fviz_multi_block_data_set_const_block(out,0),FVIZ_TYPE_POLY_DATA)!=FVIZ_FALSE);
    CHECK(fviz_object_is_type((const FVizObject*)fviz_multi_block_data_set_const_block(out,1),FVIZ_TYPE_POLY_DATA)!=FVIZ_FALSE);
    CHECK(fviz_object_is_type((const FVizObject*)fviz_multi_block_data_set_const_block(out,2),FVIZ_TYPE_PARTITIONED_DATA_SET)!=FVIZ_FALSE);
    {
        const FVizPartitionedDataSet* op=(const FVizPartitionedDataSet*)fviz_multi_block_data_set_const_block(out,2);
        CHECK(fviz_partitioned_data_set_count(op)==1u);
        CHECK(strcmp(fviz_partitioned_data_set_partition_name(op,0),"Triangle")==0);
        CHECK(fviz_object_is_type((const FVizObject*)fviz_partitioned_data_set_const_partition(op,0),FVIZ_TYPE_POLY_DATA)!=FVIZ_FALSE);
    }
    {
        const double spacing[3]={2.0,1.0,1.0};
        FVizCompositeGeometryCacheStatistics before=fviz_composite_geometry_filter_cache_statistics(filter);
        FVizCompositeGeometryCacheStatistics after;
        CHECK(fviz_image_data_set_spacing(image,spacing)==FVIZ_OK);
        CHECK(fviz_composite_geometry_filter_update(filter)==FVIZ_OK);
        after=fviz_composite_geometry_filter_cache_statistics(filter);
        CHECK(after.entries==3u);
        CHECK(after.misses==before.misses+1u);
        CHECK(after.hits==before.hits+2u);
    }
    {
        FVizCompositeGeometryCacheStatistics before=fviz_composite_geometry_filter_cache_statistics(filter);
        FVizCompositeGeometryCacheStatistics after;
        CHECK(fviz_multi_block_data_set_set_block_name(root,0u,"RenamedImage")==FVIZ_OK);
        CHECK(fviz_composite_geometry_filter_update(filter)==FVIZ_OK);
        after=fviz_composite_geometry_filter_cache_statistics(filter);
        CHECK(after.entries==3u && after.misses==before.misses);
        CHECK(after.hits==before.hits+3u);
    }
    {
        FVizCompositeGeometryCacheStatistics before=fviz_composite_geometry_filter_cache_statistics(filter);
        FVizCompositeGeometryCacheStatistics after;
        CHECK(fviz_multi_block_data_set_remove_block(root,1u)==FVIZ_OK);
        CHECK(fviz_composite_geometry_filter_update(filter)==FVIZ_OK);
        after=fviz_composite_geometry_filter_cache_statistics(filter);
        CHECK(after.entries==2u);
        CHECK(after.pruned==before.pruned+1u);
        CHECK(after.misses==before.misses);
        CHECK(after.hits==before.hits+2u);
    }
    fviz_composite_geometry_filter_clear_cache(filter);
    CHECK(fviz_composite_geometry_filter_cache_statistics(filter).entries==0u);
    fviz_release(filter); fviz_release(tri); fviz_release(rect); fviz_release(image); fviz_release(parts); fviz_release(root);
    return 0;
}
