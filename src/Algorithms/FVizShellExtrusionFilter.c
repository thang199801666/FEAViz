#include <math.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Algorithms/FVizShellExtrusionFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

typedef struct FVizShellEdge
{
    uint32_t a,b;
    uint32_t count;
    FVizSize source_triangle;
} FVizShellEdge;

struct FVizShellExtrusionFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    double thickness;
};

static void fviz_shell_extrusion_destroy(FVizObject* object)
{ fviz_release(((FVizShellExtrusionFilter*)object)->algorithm); }

static const FVizObjectClass g_fviz_shell_extrusion_class={
    FVIZ_TYPE_SHELL_EXTRUSION_FILTER,"FVizShellExtrusionFilter",&g_fviz_object_class,
    fviz_shell_extrusion_destroy,NULL
};

static FVizMTime fviz_shell_extrusion_state_mtime(const void* state)
{ return state!=NULL?fviz_object_mtime((const FVizObject*)state):0u; }

static uint64_t fviz_shell_edge_hash(uint32_t a,uint32_t b)
{
    uint64_t x=((uint64_t)a<<32)|(uint64_t)b;
    x^=x>>30; x*=UINT64_C(0xbf58476d1ce4e5b9); x^=x>>27; x*=UINT64_C(0x94d049bb133111eb); x^=x>>31;
    return x;
}

static FVizSize fviz_shell_next_pow2(FVizSize v)
{
    FVizSize p=1u;
    while (p<v && p<=((FVizSize)-1)/2u) p*=2u;
    return p>=v?p:0u;
}

static FVizResult fviz_shell_map_attributes(
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
            if (map[i]>=tuples) { fviz_release(dst); fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"shell extrusion attribute mapping is out of range"); return FVIZ_ERROR_INVALID_STATE; }
            tuple=fviz_data_array_const_tuple(src,map[i]);
            if (tuple==NULL || fviz_data_array_append_tuple(dst,tuple)!=FVIZ_OK) { fviz_release(dst); return fviz_last_error_code(); }
        }
        if (fviz_attribute_set_add(destination,name,dst)!=FVIZ_OK) { fviz_release(dst); return fviz_last_error_code(); }
        fviz_release(dst);
    }
    for (a=0u;a<(FVizSize)FVIZ_ATTRIBUTE_ROLE_COUNT;++a)
    {
        const char* active=fviz_attribute_set_active_name(source,(FVizAttributeRole)a);
        if (active!=NULL && fviz_attribute_set_const_get(destination,active)!=NULL &&
            fviz_attribute_set_set_active(destination,(FVizAttributeRole)a,active)!=FVIZ_OK) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_shell_copy_fields(const FVizAttributeSet* source,FVizAttributeSet* destination)
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

static FVizResult fviz_shell_build_edges(
    const uint32_t* triangles,FVizSize triangle_count,
    FVizShellEdge** out_edges,FVizSize* out_edge_count,FVizSize* out_boundary_count)
{
    FVizShellEdge* edges=NULL; FVizSize* slots=NULL;
    FVizSize edge_capacity=0u,slot_count=0u,edge_count=0u,boundary=0u,t;
    if (out_edges==NULL || out_edge_count==NULL || out_boundary_count==NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_edges=NULL; *out_edge_count=0u; *out_boundary_count=0u;
    if (triangle_count==0u) return FVIZ_OK;
    if (fviz_size_multiply(triangle_count,3u,&edge_capacity)!=FVIZ_OK) return fviz_last_error_code();
    slot_count=fviz_shell_next_pow2(edge_capacity+edge_capacity/2u+1u);
    if (slot_count==0u) { fviz_internal_set_error(FVIZ_ERROR_OVERFLOW,"shell edge table size overflow"); return FVIZ_ERROR_OVERFLOW; }
    edges=(FVizShellEdge*)fviz_alloc(edge_capacity*sizeof(*edges));
    slots=(FVizSize*)fviz_alloc(slot_count*sizeof(*slots));
    if (edges==NULL || slots==NULL) { fviz_free(edges); fviz_free(slots); return fviz_last_error_code(); }
    memset(slots,0,slot_count*sizeof(*slots));
    for (t=0u;t<triangle_count;++t)
    {
        const uint32_t tri[3]={triangles[3u*t],triangles[3u*t+1u],triangles[3u*t+2u]};
        uint32_t e;
        for (e=0u;e<3u;++e)
        {
            uint32_t a=tri[e],b=tri[(e+1u)%3u]; FVizSize slot;
            if (a>b) { const uint32_t tmp=a; a=b; b=tmp; }
            slot=(FVizSize)(fviz_shell_edge_hash(a,b)&(uint64_t)(slot_count-1u));
            while (slots[slot]!=0u)
            {
                FVizShellEdge* existing=&edges[slots[slot]-1u];
                if (existing->a==a && existing->b==b) { existing->count+=1u; break; }
                slot=(slot+1u)&(slot_count-1u);
            }
            if (slots[slot]==0u)
            {
                FVizShellEdge* created=&edges[edge_count];
                created->a=a; created->b=b; created->count=1u; created->source_triangle=t;
                slots[slot]=edge_count+1u; edge_count+=1u;
            }
        }
    }
    for (t=0u;t<edge_count;++t) if (edges[t].count==1u) boundary+=1u;
    fviz_free(slots); *out_edges=edges; *out_edge_count=edge_count; *out_boundary_count=boundary; return FVIZ_OK;
}

static FVizResult fviz_shell_extrusion_process(FVizAlgorithm* algorithm,const FVizPipelineRequestInfo* request,void* state)
{
    FVizShellExtrusionFilter* filter=(FVizShellExtrusionFilter*)state;
    FVizPolyData* input; FVizPolyData* output=NULL;
    FVizVec3* normals=NULL; FVizSize* point_map=NULL; FVizSize* cell_map=NULL;
    FVizShellEdge* edges=NULL; FVizSize edge_count=0u,boundary_count=0u;
    const uint32_t* triangles; const FVizVec3* points;
    FVizSize point_count,triangle_count,output_triangles,t,i,cc=0u;
    FVizSize normal_bytes=0u,point_map_count=0u,point_map_bytes=0u,cell_map_bytes=0u;
    const float half=(float)(0.5*filter->thickness);
    if (request->type!=FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input=(FVizPolyData*)fviz_algorithm_resolved_input(algorithm,0u,0u);
    if (input==NULL) { fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"shell extrusion filter has no input"); return FVIZ_ERROR_INVALID_STATE; }
    if (fviz_poly_data_validate(input)!=FVIZ_OK) return fviz_last_error_code();
    point_count=fviz_poly_data_point_count(input); triangle_count=fviz_poly_data_triangle_count(input);
    if (fviz_poly_data_vert_cell_count(input)!=0u || fviz_poly_data_line_cell_count(input)!=0u ||
        fviz_poly_data_strip_cell_count(input)!=0u || fviz_poly_data_poly_cell_count(input)!=triangle_count)
    { fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"shell extrusion requires triangle-only PolyData"); return FVIZ_ERROR_NOT_SUPPORTED; }
    if (point_count>UINT32_MAX/2u) { fviz_internal_set_error(FVIZ_ERROR_OVERFLOW,"shell extrusion exceeds uint32 render-index capacity"); return FVIZ_ERROR_OVERFLOW; }
    triangles=fviz_poly_data_triangle_indices(input); points=fviz_poly_data_points(input);
    if (triangle_count!=0u && triangles==NULL) { fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"shell triangle cache is unavailable"); return FVIZ_ERROR_INVALID_STATE; }
    if (fviz_shell_build_edges(triangles,triangle_count,&edges,&edge_count,&boundary_count)!=FVIZ_OK) return fviz_last_error_code();
    if (fviz_size_multiply(boundary_count,2u,&output_triangles)!=FVIZ_OK || output_triangles>((FVizSize)-1)-2u*triangle_count)
    { fviz_free(edges); fviz_internal_set_error(FVIZ_ERROR_OVERFLOW,"shell extrusion triangle count overflow"); return FVIZ_ERROR_OVERFLOW; }
    output_triangles+=2u*triangle_count;
    if (fviz_size_multiply(point_count,sizeof(*normals),&normal_bytes)!=FVIZ_OK ||
        fviz_size_multiply(point_count,2u,&point_map_count)!=FVIZ_OK ||
        fviz_size_multiply(point_map_count,sizeof(*point_map),&point_map_bytes)!=FVIZ_OK ||
        fviz_size_multiply(output_triangles,sizeof(*cell_map),&cell_map_bytes)!=FVIZ_OK) goto fail;
    normals=point_count!=0u?(FVizVec3*)fviz_alloc(normal_bytes):NULL;
    point_map=point_count!=0u?(FVizSize*)fviz_alloc(point_map_bytes):NULL;
    cell_map=output_triangles!=0u?(FVizSize*)fviz_alloc(cell_map_bytes):NULL;
    if ((point_count!=0u && (normals==NULL || point_map==NULL)) || (output_triangles!=0u && cell_map==NULL)) goto fail;
    if (point_count!=0u) memset(normals,0,point_count*sizeof(*normals));
    for (t=0u;t<triangle_count;++t)
    {
        const uint32_t a=triangles[3u*t],b=triangles[3u*t+1u],c=triangles[3u*t+2u];
        const FVizVec3 n=fviz_vec3_cross(fviz_vec3_sub(points[b],points[a]),fviz_vec3_sub(points[c],points[a]));
        normals[a]=fviz_vec3_add(normals[a],n); normals[b]=fviz_vec3_add(normals[b],n); normals[c]=fviz_vec3_add(normals[c],n);
    }
    if (fviz_poly_data_create(&output)!=FVIZ_OK || fviz_poly_data_reserve(output,2u*point_count,output_triangles)!=FVIZ_OK) goto fail;
    for (i=0u;i<point_count;++i)
    {
        FVizVec3 n=fviz_vec3_normalize(normals[i]); uint32_t id;
        if (fviz_vec3_length(n)<=1.0e-20f) n=fviz_vec3(0,0,1);
        if (fviz_poly_data_add_point(output,fviz_vec3_add(points[i],fviz_vec3_scale(n,half)),&id)!=FVIZ_OK) goto fail;
        point_map[i]=i;
    }
    for (i=0u;i<point_count;++i)
    {
        FVizVec3 n=fviz_vec3_normalize(normals[i]); uint32_t id;
        if (fviz_vec3_length(n)<=1.0e-20f) n=fviz_vec3(0,0,1);
        if (fviz_poly_data_add_point(output,fviz_vec3_sub(points[i],fviz_vec3_scale(n,half)),&id)!=FVIZ_OK) goto fail;
        point_map[point_count+i]=i;
    }
    for (t=0u;t<triangle_count;++t)
    {
        const uint32_t a=triangles[3u*t],b=triangles[3u*t+1u],c=triangles[3u*t+2u];
        if (fviz_poly_data_add_triangle(output,a,b,c)!=FVIZ_OK) goto fail;
        cell_map[cc++]=t;
    }
    for (t=0u;t<triangle_count;++t)
    {
        const uint32_t a=triangles[3u*t],b=triangles[3u*t+1u],c=triangles[3u*t+2u];
        const uint32_t n=(uint32_t)point_count;
        if (fviz_poly_data_add_triangle(output,n+a,n+c,n+b)!=FVIZ_OK) goto fail;
        cell_map[cc++]=t;
    }
    for (i=0u;i<edge_count;++i) if (edges[i].count==1u)
    {
        const uint32_t a=edges[i].a,b=edges[i].b,n=(uint32_t)point_count;
        if (fviz_poly_data_add_triangle(output,a,n+a,n+b)!=FVIZ_OK ||
            fviz_poly_data_add_triangle(output,a,n+b,b)!=FVIZ_OK) goto fail;
        cell_map[cc++]=edges[i].source_triangle; cell_map[cc++]=edges[i].source_triangle;
    }
    if (cc!=output_triangles) { fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"shell extrusion topology accounting failed"); goto fail; }
    if (fviz_shell_map_attributes(fviz_poly_data_const_point_data(input),point_map,2u*point_count,fviz_poly_data_point_data(output))!=FVIZ_OK ||
        fviz_shell_map_attributes(fviz_poly_data_const_cell_data(input),cell_map,output_triangles,fviz_poly_data_cell_data(output))!=FVIZ_OK ||
        fviz_shell_copy_fields(fviz_poly_data_const_field_data(input),fviz_poly_data_field_data(output))!=FVIZ_OK ||
        fviz_poly_data_compute_normals(output)!=FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm,0u,(FVizDataObject*)output)!=FVIZ_OK) goto fail;
    fviz_free(normals); fviz_free(point_map); fviz_free(cell_map); fviz_free(edges); fviz_release(output); return FVIZ_OK;
fail:
    fviz_free(normals); fviz_free(point_map); fviz_free(cell_map); fviz_free(edges); fviz_release(output); return fviz_last_error_code();
}

FVizResult fviz_shell_extrusion_filter_create(FVizShellExtrusionFilter** out_filter)
{
    FVizShellExtrusionFilter* filter; FVizAlgorithmCallbacks callbacks;
    if (out_filter==NULL) { fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"out_filter must not be NULL"); return FVIZ_ERROR_INVALID_ARGUMENT; }
    *out_filter=NULL;
    filter=(FVizShellExtrusionFilter*)fviz_internal_object_allocate(sizeof(*filter),&g_fviz_shell_extrusion_class,NULL);
    if (filter==NULL) return fviz_last_error_code();
    filter->thickness=0.1;
    fviz_algorithm_callbacks_initialize(&callbacks); callbacks.process_request=fviz_shell_extrusion_process; callbacks.get_state_mtime=fviz_shell_extrusion_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u,1u,&callbacks,filter,&filter->algorithm)!=FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm,0u,FVIZ_TYPE_POLY_DATA,FVIZ_FALSE,FVIZ_FALSE)!=FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm,0u,FVIZ_TYPE_POLY_DATA)!=FVIZ_OK)
    { fviz_release(filter); return fviz_last_error_code(); }
    *out_filter=filter; return FVIZ_OK;
}
FVizResult fviz_shell_extrusion_filter_set_input_data(FVizShellExtrusionFilter* f,FVizPolyData* input){return f!=NULL?fviz_algorithm_set_input_data(f->algorithm,0u,(FVizDataObject*)input):FVIZ_ERROR_INVALID_ARGUMENT;}
FVizResult fviz_shell_extrusion_filter_set_input_connection(FVizShellExtrusionFilter* f,FVizAlgorithmOutput* input){return f!=NULL?fviz_algorithm_set_input_connection(f->algorithm,0u,input):FVIZ_ERROR_INVALID_ARGUMENT;}
void fviz_shell_extrusion_filter_set_thickness(FVizShellExtrusionFilter* f,double v){if(f!=NULL&&isfinite(v)&&v>0.0&&f->thickness!=v){f->thickness=v;fviz_object_modified((FVizObject*)f);}}
double fviz_shell_extrusion_filter_thickness(const FVizShellExtrusionFilter* f){return f!=NULL?f->thickness:0.0;}
FVizAlgorithm* fviz_shell_extrusion_filter_algorithm(FVizShellExtrusionFilter* f){return f!=NULL?f->algorithm:NULL;}
FVizAlgorithmOutput* fviz_shell_extrusion_filter_output_port(FVizShellExtrusionFilter* f){return f!=NULL?fviz_algorithm_output_port(f->algorithm,0u):NULL;}
FVizPolyData* fviz_shell_extrusion_filter_output(FVizShellExtrusionFilter* f){return f!=NULL?(FVizPolyData*)fviz_algorithm_output_data(f->algorithm,0u):NULL;}
FVizResult fviz_shell_extrusion_filter_update(FVizShellExtrusionFilter* f){return f!=NULL?fviz_algorithm_update(f->algorithm):FVIZ_ERROR_INVALID_ARGUMENT;}
