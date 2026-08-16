#include <math.h>
#include <string.h>

#include <FViz/Algorithms/FVizTubeFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizTubeFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    double radius;
    uint32_t sides;
    FVizBool capping;
};

static void fviz_tube_destroy(FVizObject* object)
{
    FVizTubeFilter* filter=(FVizTubeFilter*)object;
    fviz_release(filter->algorithm);
}

static const FVizObjectClass g_fviz_tube_class={
    FVIZ_TYPE_TUBE_FILTER,"FVizTubeFilter",&g_fviz_object_class,fviz_tube_destroy,NULL
};

static FVizMTime fviz_tube_state_mtime(const void* state)
{ return state!=NULL?fviz_object_mtime((const FVizObject*)state):0u; }

static FVizResult fviz_tube_map_attributes(
    const FVizAttributeSet* source,const FVizSize* map,FVizSize map_count,FVizAttributeSet* destination)
{
    FVizSize a,i;
    for (a=0u;a<fviz_attribute_set_count(source);++a)
    {
        const char* name=fviz_attribute_set_name_at(source,a);
        const FVizDataArray* src=fviz_attribute_set_const_array_at(source,a);
        FVizDataArray* dst=NULL;
        const FVizSize tuples=src!=NULL?fviz_data_array_tuple_count(src):0u;
        if (name==NULL || src==NULL) continue;
        if (fviz_data_array_create(fviz_data_array_type(src),fviz_data_array_components(src),&dst)!=FVIZ_OK ||
            fviz_data_array_reserve(dst,map_count)!=FVIZ_OK) { fviz_release(dst); return fviz_last_error_code(); }
        for (i=0u;i<map_count;++i)
        {
            const void* tuple;
            if (map[i]>=tuples) { fviz_release(dst); fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"tube attribute tuple mapping is out of range"); return FVIZ_ERROR_INVALID_STATE; }
            tuple=fviz_data_array_const_tuple(src,map[i]);
            if (tuple==NULL || fviz_data_array_append_tuple(dst,tuple)!=FVIZ_OK) { fviz_release(dst); return fviz_last_error_code(); }
        }
        if (fviz_attribute_set_add(destination,name,dst)!=FVIZ_OK) { fviz_release(dst); return fviz_last_error_code(); }
        fviz_release(dst);
    }
    for (a=0u;a<(FVizSize)FVIZ_ATTRIBUTE_ROLE_COUNT;++a)
    {
        const char* active=fviz_attribute_set_active_name(source,(FVizAttributeRole)a);
        if (active!=NULL && fviz_attribute_set_const_get(destination,active)!=NULL)
        {
            if (fviz_attribute_set_set_active(destination,(FVizAttributeRole)a,active)!=FVIZ_OK) return fviz_last_error_code();
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_tube_copy_fields(const FVizAttributeSet* source,FVizAttributeSet* destination)
{
    FVizSize i;
    for (i=0u;i<fviz_attribute_set_count(source);++i)
    {
        const char* name=fviz_attribute_set_name_at(source,i);
        FVizDataArray* array=(FVizDataArray*)fviz_attribute_set_const_array_at(source,i);
        if (name!=NULL && array!=NULL && fviz_attribute_set_add(destination,name,array)!=FVIZ_OK) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_tube_emit_segment(
    FVizPolyData* output,FVizVec3 p0,FVizVec3 p1,double radius,uint32_t sides,FVizBool capping,
    FVizSize source_p0,FVizSize source_p1,FVizSize source_cell,
    FVizSize* point_map,FVizSize* point_cursor,FVizSize* cell_map,FVizSize* cell_cursor)
{
    const float pi=3.14159265358979323846f;
    FVizVec3 axis=fviz_vec3_sub(p1,p0);
    FVizVec3 reference,u,v;
    uint32_t base=0u,j;
    float length=fviz_vec3_length(axis);
    if (!(length>1.0e-20f)) return FVIZ_OK;
    axis=fviz_vec3_scale(axis,1.0f/length);
    reference=fabsf(axis.z)<0.9f?fviz_vec3(0,0,1):fviz_vec3(0,1,0);
    u=fviz_vec3_normalize(fviz_vec3_cross(axis,reference));
    v=fviz_vec3_normalize(fviz_vec3_cross(axis,u));
    if (fviz_poly_data_point_count(output)>UINT32_MAX)
    { fviz_internal_set_error(FVIZ_ERROR_OVERFLOW,"tube geometry exceeds the uint32 render-index range"); return FVIZ_ERROR_OVERFLOW; }
    base=(uint32_t)fviz_poly_data_point_count(output);
    for (j=0u;j<sides;++j)
    {
        const float angle=2.0f*pi*(float)j/(float)sides;
        const FVizVec3 offset=fviz_vec3_add(fviz_vec3_scale(u,(float)(radius*cosf(angle))),fviz_vec3_scale(v,(float)(radius*sinf(angle))));
        uint32_t index;
        if (fviz_poly_data_add_point(output,fviz_vec3_add(p0,offset),&index)!=FVIZ_OK) return fviz_last_error_code();
        point_map[(*point_cursor)++]=source_p0;
    }
    for (j=0u;j<sides;++j)
    {
        const float angle=2.0f*pi*(float)j/(float)sides;
        const FVizVec3 offset=fviz_vec3_add(fviz_vec3_scale(u,(float)(radius*cosf(angle))),fviz_vec3_scale(v,(float)(radius*sinf(angle))));
        uint32_t index;
        if (fviz_poly_data_add_point(output,fviz_vec3_add(p1,offset),&index)!=FVIZ_OK) return fviz_last_error_code();
        point_map[(*point_cursor)++]=source_p1;
    }
    for (j=0u;j<sides;++j)
    {
        const uint32_t k=(j+1u)%sides;
        if (fviz_poly_data_add_triangle(output,base+j,base+k,base+sides+k)!=FVIZ_OK ||
            fviz_poly_data_add_triangle(output,base+j,base+sides+k,base+sides+j)!=FVIZ_OK) return fviz_last_error_code();
        cell_map[(*cell_cursor)++]=source_cell;
        cell_map[(*cell_cursor)++]=source_cell;
    }
    if (capping!=FVIZ_FALSE && sides>=3u)
    {
        for (j=1u;j+1u<sides;++j)
        {
            if (fviz_poly_data_add_triangle(output,base,base+j+1u,base+j)!=FVIZ_OK ||
                fviz_poly_data_add_triangle(output,base+sides,base+sides+j,base+sides+j+1u)!=FVIZ_OK) return fviz_last_error_code();
            cell_map[(*cell_cursor)++]=source_cell;
            cell_map[(*cell_cursor)++]=source_cell;
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_tube_process(FVizAlgorithm* algorithm,const FVizPipelineRequestInfo* request,void* state)
{
    FVizTubeFilter* filter=(FVizTubeFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output=NULL;
    const FVizCellArray* lines;
    FVizSize line_cells,segments=0u,points_needed=0u,triangles_needed=0u,line_id;
    FVizSize point_map_bytes=0u,cell_map_bytes=0u;
    FVizSize* point_map=NULL; FVizSize* cell_map=NULL; FVizSize pc=0u,cc=0u;
    if (request->type!=FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input=(FVizPolyData*)fviz_algorithm_resolved_input(algorithm,0u,0u);
    if (input==NULL) { fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"tube filter has no input"); return FVIZ_ERROR_INVALID_STATE; }
    if (fviz_poly_data_validate(input)!=FVIZ_OK) return fviz_last_error_code();
    lines=fviz_poly_data_lines(input); line_cells=fviz_cell_array_count(lines);
    for (line_id=0u;line_id<line_cells;++line_id)
    {
        FVizCellView view;
        if (fviz_cell_array_cell_view(lines,line_id,&view)!=FVIZ_OK) return fviz_last_error_code();
        if (view.point_count>=2u) segments+=view.point_count-1u;
    }
    if (fviz_size_multiply(segments,(FVizSize)(2u*filter->sides),&points_needed)!=FVIZ_OK) return fviz_last_error_code();
    {
        const FVizSize per_segment=(FVizSize)(2u*filter->sides)+(filter->capping!=FVIZ_FALSE?(FVizSize)(2u*(filter->sides-2u)):0u);
        if (fviz_size_multiply(segments,per_segment,&triangles_needed)!=FVIZ_OK) return fviz_last_error_code();
    }
    if (points_needed>UINT32_MAX ||
        fviz_size_multiply(points_needed,sizeof(*point_map),&point_map_bytes)!=FVIZ_OK ||
        fviz_size_multiply(triangles_needed,sizeof(*cell_map),&cell_map_bytes)!=FVIZ_OK)
    {
        if (fviz_last_error_code()==FVIZ_OK)
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW,"tube geometry exceeds the uint32 render-index range");
        return fviz_last_error_code();
    }
    if (fviz_poly_data_create(&output)!=FVIZ_OK || fviz_poly_data_reserve(output,points_needed,triangles_needed)!=FVIZ_OK) goto fail;
    if (points_needed!=0u) { point_map=(FVizSize*)fviz_alloc(point_map_bytes); if (point_map==NULL) goto fail; }
    if (triangles_needed!=0u) { cell_map=(FVizSize*)fviz_alloc(cell_map_bytes); if (cell_map==NULL) goto fail; }
    for (line_id=0u;line_id<line_cells;++line_id)
    {
        FVizCellView view; FVizSize j;
        if (fviz_cell_array_cell_view(lines,line_id,&view)!=FVIZ_OK) goto fail;
        for (j=0u;j+1u<view.point_count;++j)
        {
            const FVizId id0=fviz_cell_view_point_id(&view,j),id1=fviz_cell_view_point_id(&view,j+1u);
            FVizVec3 p0,p1;
            if (id0>UINT32_MAX || id1>UINT32_MAX ||
                fviz_poly_data_get_point(input,(FVizSize)id0,&p0)!=FVIZ_OK ||
                fviz_poly_data_get_point(input,(FVizSize)id1,&p1)!=FVIZ_OK) goto fail;
            if (fviz_tube_emit_segment(output,p0,p1,filter->radius,filter->sides,filter->capping,
                (FVizSize)id0,(FVizSize)id1,fviz_poly_data_vert_cell_count(input)+line_id,
                point_map,&pc,cell_map,&cc)!=FVIZ_OK) goto fail;
        }
    }
    if (pc!=fviz_poly_data_point_count(output) || cc!=fviz_poly_data_triangle_count(output))
    { fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"tube filter internal topology accounting failed"); goto fail; }
    if (fviz_tube_map_attributes(fviz_poly_data_const_point_data(input),point_map,pc,fviz_poly_data_point_data(output))!=FVIZ_OK ||
        fviz_tube_map_attributes(fviz_poly_data_const_cell_data(input),cell_map,cc,fviz_poly_data_cell_data(output))!=FVIZ_OK)
        goto fail;
    if (fviz_tube_copy_fields(fviz_poly_data_const_field_data(input),fviz_poly_data_field_data(output))!=FVIZ_OK) goto fail;
    if (fviz_poly_data_triangle_count(output)!=0u && fviz_poly_data_compute_normals(output)!=FVIZ_OK) goto fail;
    if (fviz_algorithm_set_output_data(algorithm,0u,(FVizDataObject*)output)!=FVIZ_OK) goto fail;
    fviz_free(point_map); fviz_free(cell_map); fviz_release(output); return FVIZ_OK;
fail:
    fviz_free(point_map); fviz_free(cell_map); fviz_release(output); return fviz_last_error_code();
}

FVizResult fviz_tube_filter_create(FVizTubeFilter** out_filter)
{
    FVizTubeFilter* filter; FVizAlgorithmCallbacks callbacks;
    if (out_filter==NULL) { fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"out_filter must not be NULL"); return FVIZ_ERROR_INVALID_ARGUMENT; }
    *out_filter=NULL;
    filter=(FVizTubeFilter*)fviz_internal_object_allocate(sizeof(*filter),&g_fviz_tube_class,NULL);
    if (filter==NULL) return fviz_last_error_code();
    filter->radius=0.05; filter->sides=12u; filter->capping=FVIZ_TRUE;
    fviz_algorithm_callbacks_initialize(&callbacks); callbacks.process_request=fviz_tube_process; callbacks.get_state_mtime=fviz_tube_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u,1u,&callbacks,filter,&filter->algorithm)!=FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm,0u,FVIZ_TYPE_POLY_DATA,FVIZ_FALSE,FVIZ_FALSE)!=FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm,0u,FVIZ_TYPE_POLY_DATA)!=FVIZ_OK)
    { fviz_release(filter); return fviz_last_error_code(); }
    *out_filter=filter; return FVIZ_OK;
}
FVizResult fviz_tube_filter_set_input_data(FVizTubeFilter* f,FVizPolyData* input){return f!=NULL?fviz_algorithm_set_input_data(f->algorithm,0u,(FVizDataObject*)input):FVIZ_ERROR_INVALID_ARGUMENT;}
FVizResult fviz_tube_filter_set_input_connection(FVizTubeFilter* f,FVizAlgorithmOutput* input){return f!=NULL?fviz_algorithm_set_input_connection(f->algorithm,0u,input):FVIZ_ERROR_INVALID_ARGUMENT;}
void fviz_tube_filter_set_radius(FVizTubeFilter* f,double v){if(f!=NULL&&isfinite(v)&&v>0.0&&f->radius!=v){f->radius=v;fviz_object_modified((FVizObject*)f);}}
double fviz_tube_filter_radius(const FVizTubeFilter* f){return f!=NULL?f->radius:0.0;}
void fviz_tube_filter_set_sides(FVizTubeFilter* f,uint32_t v){if(f!=NULL){if(v<3u)v=3u;if(v>128u)v=128u;if(f->sides!=v){f->sides=v;fviz_object_modified((FVizObject*)f);}}}
uint32_t fviz_tube_filter_sides(const FVizTubeFilter* f){return f!=NULL?f->sides:0u;}
void fviz_tube_filter_set_capping(FVizTubeFilter* f,FVizBool v){if(f!=NULL){v=v!=FVIZ_FALSE?FVIZ_TRUE:FVIZ_FALSE;if(f->capping!=v){f->capping=v;fviz_object_modified((FVizObject*)f);}}}
FVizBool fviz_tube_filter_capping(const FVizTubeFilter* f){return f!=NULL?f->capping:FVIZ_FALSE;}
FVizAlgorithm* fviz_tube_filter_algorithm(FVizTubeFilter* f){return f!=NULL?f->algorithm:NULL;}
FVizAlgorithmOutput* fviz_tube_filter_output_port(FVizTubeFilter* f){return f!=NULL?fviz_algorithm_output_port(f->algorithm,0u):NULL;}
FVizPolyData* fviz_tube_filter_output(FVizTubeFilter* f){return f!=NULL?(FVizPolyData*)fviz_algorithm_output_data(f->algorithm,0u):NULL;}
FVizResult fviz_tube_filter_update(FVizTubeFilter* f){return f!=NULL?fviz_algorithm_update(f->algorithm):FVIZ_ERROR_INVALID_ARGUMENT;}
