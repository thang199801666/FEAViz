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

int main(void)
{
    int result=0;
    if((result=test_contour_surface())!=0)
    { fprintf(stderr,"test_contour_surface failed at line %d\n",result); return result; }
    if((result=test_contour_lines())!=0)
    { fprintf(stderr,"test_contour_lines failed at line %d\n",result); return result; }
    if((result=test_extrema())!=0)
    { fprintf(stderr,"test_extrema failed at line %d\n",result); return result; }
    printf("FVizTestFEAVisualizationContours passed\n");
    return 0;
}
