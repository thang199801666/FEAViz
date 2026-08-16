#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FEA/FVizFEA.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#expr); return __LINE__; } } while(0)

/* Builds a 2x2 quad surface (two triangles) with a linear scalar ramp. */
static FVizResult build_quad_surface(FVizPolyData** out_surface)
{
    const FVizVec3 points[4]={
        {0.0f,0.0f,0.0f},{1.0f,0.0f,0.0f},{1.0f,1.0f,0.0f},{0.0f,1.0f,0.0f}};
    const double values[4]={0.0,10.0,20.0,30.0};
    const uint64_t cell_ids[2]={7u,8u};
    const uint64_t face_ids[2]={3u,4u};
    FVizPolyData* surface=NULL;
    FVizDataArray* scalars=NULL;
    FVizDataArray* cells=NULL;
    FVizDataArray* faces=NULL;
    FVizResult result=FVIZ_OK;
    if(out_surface==NULL)return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_surface=NULL;
    if(fviz_poly_data_create(&surface)!=FVIZ_OK||
        fviz_poly_data_add_points(surface,points,4u,NULL)!=FVIZ_OK||
        fviz_poly_data_add_triangle(surface,0u,1u,2u)!=FVIZ_OK||
        fviz_poly_data_add_triangle(surface,0u,2u,3u)!=FVIZ_OK||
        fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&scalars)!=FVIZ_OK||
        fviz_data_array_append_tuples(scalars,values,4u)!=FVIZ_OK||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&cells)!=FVIZ_OK||
        fviz_data_array_append_tuples(cells,cell_ids,2u)!=FVIZ_OK||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&faces)!=FVIZ_OK||
        fviz_data_array_append_tuples(faces,face_ids,2u)!=FVIZ_OK||
        fviz_attribute_set_add(fviz_poly_data_point_data(surface),"stress",scalars)!=FVIZ_OK||
        fviz_attribute_set_add(fviz_poly_data_cell_data(surface),"FVizOriginalCellIds",cells)!=FVIZ_OK||
        fviz_attribute_set_add(fviz_poly_data_cell_data(surface),"FVizOriginalFaceIds",faces)!=FVIZ_OK||
        fviz_poly_data_validate(surface)!=FVIZ_OK)
    {
        result=fviz_last_error_code();
        goto fail;
    }
    *out_surface=surface;
    surface=NULL;
    result=FVIZ_OK;
fail:
    fviz_release(faces);fviz_release(cells);fviz_release(scalars);fviz_release(surface);
    return result;
}

static int test_contour_surface(void)
{
    FVizPolyData* surface=NULL;
    FVizPolyData* contour=NULL;
    const FVizDataArray* colors=NULL;
    double r=0.0,g=0.0,b=0.0;
    CHECK(build_quad_surface(&surface)==FVIZ_OK);
    CHECK(fviz_fea_build_contour_surface(surface,"stress",1u,
        0.0f,30.0f,"contour_rgb",&contour)==FVIZ_OK);
    CHECK(fviz_poly_data_point_count(contour)==4u);
    CHECK(fviz_poly_data_triangle_count(contour)==2u);
    colors=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(contour),"contour_rgb");
    CHECK(colors!=NULL);
    CHECK(fviz_data_array_components(colors)==3u);
    CHECK(fviz_data_array_tuple_count(colors)==4u);
    /* Min vertex (value 0) should be red-ish (blue end of Abaqus rainbow is
     * at high normalized, red at low). Verify colors are finite and valid. */
    CHECK(fviz_data_array_get_component(colors,0u,0u,&r)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(colors,0u,1u,&g)==FVIZ_OK);
    CHECK(fviz_data_array_get_component(colors,0u,2u,&b)==FVIZ_OK);
    CHECK(isfinite(r)&&isfinite(g)&&isfinite(b));
    fviz_release(contour);
    fviz_release(surface);
    return 0;
}

static int test_contour_lines(void)
{
    FVizPolyData* surface=NULL;
    FVizPolyData* lines=NULL;
    const FVizDataArray* levels=NULL;
    const FVizDataArray* cells=NULL;
    const FVizDataArray* faces=NULL;
    CHECK(build_quad_surface(&surface)==FVIZ_OK);
    CHECK(fviz_fea_build_contour_lines(surface,"stress",1u,
        0.0f,30.0f,4u,"contour_level",&lines)==FVIZ_OK);
    CHECK(fviz_poly_data_line_count(lines)>0u);
    levels=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(lines),"contour_level");
    CHECK(levels!=NULL);
    CHECK(fviz_data_array_tuple_count(levels)==fviz_poly_data_point_count(lines));
    /* Mid-levels are 3.75, 11.25, 18.75, 26.25; every vertex scalar must equal
     * one of these. */
    {
        FVizSize point;
        for(point=0u;point<fviz_poly_data_point_count(lines);++point)
        {
            double value=0.0;
            CHECK(fviz_data_array_get_component(levels,point,0u,&value)==FVIZ_OK);
            CHECK(fabs(value-3.75)<1e-4||fabs(value-11.25)<1e-4||
                fabs(value-18.75)<1e-4||fabs(value-26.25)<1e-4);
        }
    }
    cells=fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(lines),"FVizOriginalCellIds");
    faces=fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(lines),"FVizOriginalFaceIds");
    CHECK(cells!=NULL);
    CHECK(faces!=NULL);
    fviz_release(lines);
    fviz_release(surface);
    return 0;
}

static int test_extrema(void)
{
    FVizPolyData* surface=NULL;
    FVizFEAExtrema extrema;
    CHECK(build_quad_surface(&surface)==FVIZ_OK);
    fviz_fea_extrema_initialize(&extrema);
    CHECK(fviz_fea_find_extrema(surface,"stress",1u,&extrema)==FVIZ_OK);
    CHECK(extrema.min_value==0.0);
    CHECK(extrema.max_value==30.0);
    CHECK(extrema.min_point_id==0u);
    CHECK(extrema.max_point_id==3u);
    /* Point 0 is in triangle 0 (cell 7, face 3) and triangle 1 (cell 8, face 4);
     * the last triangle scanned wins, so it must be one of the valid pairs. */
    CHECK((extrema.min_cell_id==7u&&extrema.min_face_id==3u)||
          (extrema.min_cell_id==8u&&extrema.min_face_id==4u));
    CHECK((extrema.max_cell_id==7u&&extrema.max_face_id==3u)||
          (extrema.max_cell_id==8u&&extrema.max_face_id==4u));
    fviz_release(surface);
    return 0;
}

/* Builds a 2x2x1 hex grid with a nodal stress field and a nodal displacement
 * vector, plus a result field so primary-variable evaluation can run. */
static FVizResult build_result_grid(FVizUnstructuredGrid** out_grid, FVizFEAField** out_field)
{
    const FVizVec3 points[8]={
        {0.0f,0.0f,0.0f},{1.0f,0.0f,0.0f},{1.0f,1.0f,0.0f},{0.0f,1.0f,0.0f},
        {0.0f,0.0f,1.0f},{1.0f,0.0f,1.0f},{1.0f,1.0f,1.0f},{0.0f,1.0f,1.0f}};
    const uint32_t hex[8]={0u,1u,2u,3u,4u,5u,6u,7u};
    FVizUnstructuredGrid* grid=NULL;
    FVizFEAField* field=NULL;
    FVizDataArray* values=NULL;
    FVizDataArray* ids=NULL;
    FVizFEAFieldBlockDescriptor block;
    FVizSize i;
    if(out_grid==NULL||out_field==NULL)return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_grid=NULL;*out_field=NULL;
    if(fviz_unstructured_grid_create(&grid)!=FVIZ_OK||
        fviz_unstructured_grid_add_points(grid,points,8u,NULL)!=FVIZ_OK||
        fviz_unstructured_grid_add_cell(grid,FVIZ_CELL_HEXAHEDRON,8u,hex)!=FVIZ_OK||
        fviz_fea_field_create("S","Stress",FVIZ_FEA_FIELD_SCALAR,&field)!=FVIZ_OK||
        fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&values)!=FVIZ_OK||
        fviz_data_array_resize(values,8u)!=FVIZ_OK||
        fviz_data_array_create(FVIZ_DATA_UINT64,1u,&ids)!=FVIZ_OK||
        fviz_data_array_resize(ids,8u)!=FVIZ_OK)goto fail;
    for(i=0u;i<8u;++i)
    {
        const double value=10.0*(double)(i%4u);
        if(fviz_data_array_set_component(values,i,0u,value)!=FVIZ_OK||
            fviz_data_array_set_component(ids,i,0u,(double)i)!=FVIZ_OK)goto fail;
    }
    fviz_fea_field_block_descriptor_initialize(&block);
    block.instance_name="PART-1";
    block.position=FVIZ_FEA_POSITION_NODAL;
    block.entity_ids=ids;
    block.values=values;
    if(fviz_fea_field_add_block(field,&block,NULL)!=FVIZ_OK)goto fail;
    fviz_release(values);fviz_release(ids);
    *out_grid=grid;*out_field=field;
    return FVIZ_OK;
fail:
    fviz_release(values);fviz_release(ids);fviz_release(field);fviz_release(grid);
    return fviz_last_error_code();
}

static int test_result_contour(void)
{
    FVizUnstructuredGrid* grid=NULL;
    FVizFEAField* field=NULL;
    FVizFEAPrimaryVariableEvaluator* evaluator=NULL;
    FVizFEAPrimaryVariable variable;
    FVizFEAPrimaryVariableResult* result=NULL;
    FVizPolyData* surface=NULL;
    const FVizDataArray* colors=NULL;
    CHECK(build_result_grid(&grid,&field)==FVIZ_OK);
    CHECK(fviz_fea_primary_variable_evaluator_create(&evaluator)==FVIZ_OK);
    fviz_fea_primary_variable_initialize(&variable);
    variable.operation=FVIZ_FEA_PRIMARY_COMPONENT;
    variable.component=0u;
    variable.target_position=FVIZ_FEA_POSITION_NODAL;
    variable.source_position=FVIZ_FEA_POSITION_NODAL;
    CHECK(fviz_fea_primary_variable_evaluate(evaluator,field,grid,&variable,&result)==FVIZ_OK);
    CHECK(fviz_fea_build_contour_surface_from_result(result,grid,FVIZ_FEA_CONTOUR_SMOOTH,
        0.0f,30.0f,1u,"result_rgb",&surface)==FVIZ_OK);
    CHECK(fviz_poly_data_point_count(surface)>0u);
    colors=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(surface),"result_rgb");
    CHECK(colors!=NULL&&fviz_data_array_components(colors)==3u);
    fviz_release(surface);
    surface=NULL;
    CHECK(fviz_fea_build_contour_surface_from_result(result,grid,FVIZ_FEA_CONTOUR_BANDED,
        0.0f,30.0f,6u,"result_rgb",&surface)==FVIZ_OK);
    CHECK(fviz_poly_data_point_count(surface)>0u);
    colors=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(surface),"result_rgb");
    CHECK(colors!=NULL&&fviz_data_array_components(colors)==3u);
    fviz_release(surface);surface=NULL;
    fviz_release(result);fviz_release(evaluator);fviz_release(field);fviz_release(grid);
    return 0;
}

static int test_banded_ex(void)
{
    FVizPolyData* surface=NULL;
    FVizPolyData* banded=NULL;
    FVizFEABandedSurfaceOptions options;
    const FVizDataArray* colors=NULL;
    CHECK(build_quad_surface(&surface)==FVIZ_OK);
    fviz_fea_banded_surface_options_initialize(&options);
    options.enabled=FVIZ_TRUE;
    options.reversed=FVIZ_TRUE;
    options.below_range_color[0]=0.0f;options.below_range_color[1]=0.0f;options.below_range_color[2]=1.0f;
    options.above_range_color[0]=1.0f;options.above_range_color[1]=0.0f;options.above_range_color[2]=0.0f;
    CHECK(fviz_fea_build_abaqus_banded_surface_ex(surface,"stress",1u,
        -10.0f,40.0f,5u,&options,"banded_rgb",&banded)==FVIZ_OK);
    colors=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(banded),"banded_rgb");
    CHECK(colors!=NULL&&fviz_data_array_components(colors)==3u);
    CHECK(fviz_poly_data_triangle_count(banded)>0u);
    fviz_release(banded);
    fviz_release(surface);
    return 0;
}

static int test_slice_contour(void)
{
    FVizUnstructuredGrid* grid=NULL;
    FVizFEAField* field=NULL;
    FVizPolyData* slice=NULL;
    FVizPlane plane;
    CHECK(build_result_grid(&grid,&field)==FVIZ_OK);
    /* Add a point scalar to the grid so extract_geometry/slice can carry it. */
    {
        FVizDataArray* scalars=NULL;
        FVizSize i;
        CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&scalars)==FVIZ_OK);
        CHECK(fviz_data_array_resize(scalars,fviz_unstructured_grid_point_count(grid))==FVIZ_OK);
        for(i=0u;i<fviz_unstructured_grid_point_count(grid);++i)
            CHECK(fviz_data_array_set_component(scalars,i,0u,5.0*(double)(i%4u))==FVIZ_OK);
        CHECK(fviz_attribute_set_add(fviz_unstructured_grid_point_data(grid),"stress",scalars)==FVIZ_OK);
        fviz_release(scalars);
    }
    plane = fviz_plane_from_point_normal(
        fviz_vec3(0.0f, 0.0f, 0.5f), fviz_vec3(0.0f, 0.0f, 1.0f));
    CHECK(fviz_fea_slice_contour(grid,plane,"stress",1u,FVIZ_FEA_CONTOUR_SMOOTH,
        0.0f,15.0f,1u,"slice_rgb",&slice)==FVIZ_OK);
    CHECK(fviz_poly_data_point_count(slice)>0u);
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_point_data(slice),"slice_rgb")!=NULL);
    fviz_release(slice);slice=NULL;
    CHECK(fviz_fea_slice_contour(grid,plane,"stress",1u,FVIZ_FEA_CONTOUR_BANDED,
        0.0f,15.0f,5u,"slice_rgb",&slice)==FVIZ_OK);
    CHECK(fviz_poly_data_point_count(slice)>0u);
    fviz_release(slice);
    fviz_release(field);fviz_release(grid);
    return 0;
}

static int test_element_facet_surface(void)
{
    FVizUnstructuredGrid* grid=NULL;
    FVizPolyData* surface=NULL;
    FVizPolyData* facet=NULL;
    FVizDataArray* cell_scalars=NULL;
    const FVizDataArray* colors=NULL;
    FVizSize i;
    /* Two disconnected hex cells (two volume cells) so cell-data scalar has
     * two tuples and surface has distinct original cell ids. */
    const FVizVec3 points[16]={
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1},
        {2,0,0},{3,0,0},{3,1,0},{2,1,0},{2,0,1},{3,0,1},{3,1,1},{2,1,1}};
    const uint32_t hex_a[8]={0,1,2,3,4,5,6,7};
    const uint32_t hex_b[8]={8,9,10,11,12,13,14,15};
    CHECK(fviz_unstructured_grid_create(&grid)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_points(grid,points,16u,NULL)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(grid,FVIZ_CELL_HEXAHEDRON,8u,hex_a)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_add_cell(grid,FVIZ_CELL_HEXAHEDRON,8u,hex_b)==FVIZ_OK);
    CHECK(fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&cell_scalars)==FVIZ_OK);
    CHECK(fviz_data_array_resize(cell_scalars,2u)==FVIZ_OK);
    CHECK(fviz_data_array_set_component(cell_scalars,0u,0u,10.0)==FVIZ_OK);
    CHECK(fviz_data_array_set_component(cell_scalars,1u,0u,90.0)==FVIZ_OK);
    CHECK(fviz_attribute_set_add(fviz_unstructured_grid_cell_data(grid),"estress",cell_scalars)==FVIZ_OK);
    CHECK(fviz_unstructured_grid_extract_geometry(grid,&surface)==FVIZ_OK);
    CHECK(fviz_poly_data_triangle_count(surface)>0u);
    CHECK(fviz_fea_build_element_facet_surface(grid,surface,"estress",1u,
        0.0f,100.0f,"facet_rgb",&facet)==FVIZ_OK);
    CHECK(fviz_poly_data_triangle_count(facet)==fviz_poly_data_triangle_count(surface));
    colors=fviz_attribute_set_const_get(fviz_poly_data_const_point_data(facet),"facet_rgb");
    CHECK(colors!=NULL&&fviz_data_array_components(colors)==3u);
    CHECK(fviz_data_array_tuple_count(colors)==fviz_poly_data_point_count(facet));
    /* Every triangle is flat: its 3 vertices share the same RGB tuple. */
    {
        const uint32_t* tris=fviz_poly_data_triangle_indices(facet);
        for(i=0u;i<fviz_poly_data_triangle_count(facet);++i)
        {
            double r0,g0,b0,r1,g1,b1,r2,g2,b2;
            CHECK(fviz_data_array_get_component(colors,tris[i*3u+0u],0u,&r0)==FVIZ_OK);
            CHECK(fviz_data_array_get_component(colors,tris[i*3u+0u],1u,&g0)==FVIZ_OK);
            CHECK(fviz_data_array_get_component(colors,tris[i*3u+0u],2u,&b0)==FVIZ_OK);
            CHECK(fviz_data_array_get_component(colors,tris[i*3u+1u],0u,&r1)==FVIZ_OK);
            CHECK(fviz_data_array_get_component(colors,tris[i*3u+1u],1u,&g1)==FVIZ_OK);
            CHECK(fviz_data_array_get_component(colors,tris[i*3u+1u],2u,&b1)==FVIZ_OK);
            CHECK(fviz_data_array_get_component(colors,tris[i*3u+2u],0u,&r2)==FVIZ_OK);
            CHECK(fviz_data_array_get_component(colors,tris[i*3u+2u],1u,&g2)==FVIZ_OK);
            CHECK(fviz_data_array_get_component(colors,tris[i*3u+2u],2u,&b2)==FVIZ_OK);
            CHECK(r0==r1&&r1==r2&&g0==g1&&g1==g2&&b0==b1&&b1==b2);
        }
    }
    /* Provenance survived. */
    CHECK(fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(facet),"FVizOriginalCellIds")!=NULL);
    fviz_release(facet);
    fviz_release(surface);
    fviz_release(cell_scalars);
    fviz_release(grid);
    return 0;
}

int main(void)
{
    int result=0;
    if((result=test_contour_surface())!=0)
    { fprintf(stderr,"test_contour_surface failed at line %d\n",result); return result; }
    if((result=test_contour_lines())!=0)
    { fprintf(stderr,"test_contour_lines failed at line %d\n",result); return result; }
    if((result=test_extrema())!=0)
    { fprintf(stderr,"test_extrema failed at line %d\n",result); return result; }
    if((result=test_result_contour())!=0)
    { fprintf(stderr,"test_result_contour failed at line %d\n",result); return result; }
    if((result=test_banded_ex())!=0)
    { fprintf(stderr,"test_banded_ex failed at line %d\n",result); return result; }
    if((result=test_slice_contour())!=0)
    { fprintf(stderr,"test_slice_contour failed at line %d\n",result); return result; }
    if((result=test_element_facet_surface())!=0)
    { fprintf(stderr,"test_element_facet_surface failed at line %d\n",result); return result; }
    printf("FVizTestFEAVisualizationContours passed\n");
    return 0;
}
