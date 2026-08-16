#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/Algorithms/FVizMeshQualityFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>
#include <FViz/Parallel/FVizParallel.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

#ifndef FVIZ_PI
#define FVIZ_PI 3.14159265358979323846
#endif

typedef struct FVizMeshQualityContext
{
    const FVizUnstructuredGrid* grid;
    FVizMeshQualityMetric metric;
    double* output;
} FVizMeshQualityContext;

static double fviz_quality_length(FVizVec3 a)
{
    return sqrt((double)a.x*a.x+(double)a.y*a.y+(double)a.z*a.z);
}

static double fviz_quality_triangle_area(FVizVec3 a,FVizVec3 b,FVizVec3 c)
{
    const FVizVec3 ab=fviz_vec3_sub(b,a);
    const FVizVec3 ac=fviz_vec3_sub(c,a);
    return 0.5*fviz_quality_length(fviz_vec3_cross(ab,ac));
}

static FVizResult fviz_quality_cell_points(
    const FVizUnstructuredGrid* grid,FVizSize cell_id,FVizCellView* out_view,FVizVec3* points,FVizSize capacity)
{
    FVizSize i;
    const FVizVec3* all_points=fviz_points_data(fviz_unstructured_grid_points((FVizUnstructuredGrid*)grid));
    const FVizSize point_count=fviz_unstructured_grid_point_count(grid);
    if (fviz_cell_array_cell_view(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid),cell_id,out_view)!=FVIZ_OK)
        return fviz_last_error_code();
    if (out_view->point_count>capacity)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"mesh quality cell has more points than the current quality kernel supports");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    for (i=0u;i<out_view->point_count;++i)
    {
        const FVizId id=fviz_cell_view_point_id(out_view,i);
        if (id>=(FVizId)point_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"mesh quality encountered an invalid point ID");
            return FVIZ_ERROR_INVALID_STATE;
        }
        points[i]=all_points[(FVizSize)id];
    }
    return FVIZ_OK;
}

static uint32_t fviz_quality_face_corner_count(FVizCellType type,uint32_t face_point_count)
{
    (void)type;
    if (face_point_count==6u) return 3u;
    if (face_point_count==8u || face_point_count==9u) return 4u;
    return face_point_count;
}

static FVizCellType fviz_quality_linear_corner_type(FVizCellType type)
{
    switch (type)
    {
        case FVIZ_CELL_QUADRATIC_EDGE: return FVIZ_CELL_LINE;
        case FVIZ_CELL_QUADRATIC_TRIANGLE: return FVIZ_CELL_TRIANGLE;
        case FVIZ_CELL_QUADRATIC_QUAD:
        case FVIZ_CELL_BIQUADRATIC_QUAD: return FVIZ_CELL_QUAD;
        case FVIZ_CELL_QUADRATIC_TETRA: return FVIZ_CELL_TETRA;
        case FVIZ_CELL_QUADRATIC_HEXAHEDRON: return FVIZ_CELL_HEXAHEDRON;
        case FVIZ_CELL_QUADRATIC_WEDGE: return FVIZ_CELL_WEDGE;
        case FVIZ_CELL_QUADRATIC_PYRAMID: return FVIZ_CELL_PYRAMID;
        default: return type;
    }
}

static double fviz_quality_measure(FVizCellType type,const FVizVec3* p,FVizSize count)
{
    const FVizCellTypeTraits traits=fviz_cell_type_traits(type);
    if (traits.dimension==1u && count>=2u)
    {
        if (type==FVIZ_CELL_QUADRATIC_EDGE && count>=3u)
            return fviz_quality_length(fviz_vec3_sub(p[2],p[0]))+fviz_quality_length(fviz_vec3_sub(p[1],p[2]));
        {
            FVizSize i;
            double length=0.0;
            for (i=1u;i<count;++i) length+=fviz_quality_length(fviz_vec3_sub(p[i],p[i-1u]));
            return length;
        }
    }
    if (traits.dimension==2u && count>=3u)
    {
        const FVizSize corners=(type==FVIZ_CELL_QUADRATIC_TRIANGLE)?3u:
            ((type==FVIZ_CELL_QUADRATIC_QUAD || type==FVIZ_CELL_BIQUADRATIC_QUAD)?4u:count);
        FVizSize i;
        double area=0.0;
        for (i=1u;i+1u<corners;++i) area+=fviz_quality_triangle_area(p[0],p[i],p[i+1u]);
        return area;
    }
    if (traits.dimension==3u)
    {
        uint32_t face;
        double volume6=0.0;
        for (face=0u;face<traits.face_count;++face)
        {
            uint32_t local[9];
            uint32_t face_count=0u;
            uint32_t i;
            if (fviz_cell_type_face(type,face,local,9u,&face_count)!=FVIZ_OK || face_count<3u) return NAN;
            face_count=fviz_quality_face_corner_count(type,face_count);
            for (i=1u;i+1u<face_count;++i)
            {
                const FVizVec3 a=p[local[0]], b=p[local[i]], c=p[local[i+1u]];
                volume6+=(double)a.x*((double)b.y*c.z-(double)b.z*c.y)
                       +(double)a.y*((double)b.z*c.x-(double)b.x*c.z)
                       +(double)a.z*((double)b.x*c.y-(double)b.y*c.x);
            }
        }
        return fabs(volume6)/6.0;
    }
    return NAN;
}

static double fviz_quality_edge_ratio(FVizCellType type,const FVizVec3* p)
{
    const FVizCellTypeTraits traits=fviz_cell_type_traits(type);
    uint32_t edge;
    double minimum=INFINITY,maximum=0.0;
    if (traits.edge_count==0u) return NAN;
    for (edge=0u;edge<traits.edge_count;++edge)
    {
        uint32_t local[2];
        double length;
        if (fviz_cell_type_edge(type,edge,local)!=FVIZ_OK) return NAN;
        length=fviz_quality_length(fviz_vec3_sub(p[local[1]],p[local[0]]));
        if (length<minimum) minimum=length;
        if (length>maximum) maximum=length;
    }
    return minimum>1.0e-30?maximum/minimum:INFINITY;
}

static double fviz_quality_scaled_det(FVizVec3 a,FVizVec3 b,FVizVec3 c)
{
    const double la=fviz_quality_length(a),lb=fviz_quality_length(b),lc=fviz_quality_length(c);
    if (la<=1.0e-30 || lb<=1.0e-30 || lc<=1.0e-30) return -1.0;
    return (double)fviz_vec3_dot(a,fviz_vec3_cross(b,c))/(la*lb*lc);
}

static double fviz_quality_scaled_jacobian(FVizCellType type,const FVizVec3* p)
{
    type=fviz_quality_linear_corner_type(type);
    if (type==FVIZ_CELL_TETRA)
    {
        const FVizVec3 e0=fviz_vec3_sub(p[1],p[0]);
        const FVizVec3 e1=fviz_vec3_sub(p[2],p[0]);
        const FVizVec3 e2=fviz_vec3_sub(p[3],p[0]);
        return fviz_quality_scaled_det(e0,e1,e2);
    }
    if (type==FVIZ_CELL_HEXAHEDRON)
    {
        static const uint8_t neighbors[8][3]={{1,3,4},{2,0,5},{3,1,6},{0,2,7},{7,5,0},{4,6,1},{5,7,2},{6,4,3}};
        double minimum=INFINITY;
        uint32_t corner;
        for (corner=0u;corner<8u;++corner)
        {
            const double value=fviz_quality_scaled_det(
                fviz_vec3_sub(p[neighbors[corner][0]],p[corner]),
                fviz_vec3_sub(p[neighbors[corner][1]],p[corner]),
                fviz_vec3_sub(p[neighbors[corner][2]],p[corner]));
            if (value<minimum) minimum=value;
        }
        return minimum;
    }
    {
        const FVizCellTypeTraits traits=fviz_cell_type_traits(type);
        if (traits.dimension!=3u || traits.edge_count<3u) return NAN;
        /* Generic fallback: use the first three independent edges from local point 0.
         * Absolute value intentionally avoids inventing an orientation convention for
         * wedge/pyramid cells; tetra/hex above retain signed inversion detection. */
        {
            FVizVec3 vectors[3];
            uint32_t found=0u,edge;
            for (edge=0u;edge<traits.edge_count && found<3u;++edge)
            {
                uint32_t local[2];
                if (fviz_cell_type_edge(type,edge,local)!=FVIZ_OK) continue;
                if (local[0]==0u) vectors[found++]=fviz_vec3_sub(p[local[1]],p[0]);
                else if (local[1]==0u) vectors[found++]=fviz_vec3_sub(p[local[0]],p[0]);
            }
            if (found==3u) return fabs(fviz_quality_scaled_det(vectors[0],vectors[1],vectors[2]));
        }
    }
    return NAN;
}

static double fviz_quality_angle_degrees(FVizVec3 a,FVizVec3 b)
{
    const double la=fviz_quality_length(a),lb=fviz_quality_length(b);
    double cosine;
    if (la<=1.0e-30 || lb<=1.0e-30) return NAN;
    cosine=(double)fviz_vec3_dot(a,b)/(la*lb);
    if (cosine<-1.0) cosine=-1.0; else if (cosine>1.0) cosine=1.0;
    return acos(cosine)*(180.0/FVIZ_PI);
}

static double fviz_quality_corner_angle(FVizCellType type,const FVizVec3* p,FVizBool maximum)
{
    const FVizCellTypeTraits traits=fviz_cell_type_traits(type);
    double result=maximum!=FVIZ_FALSE?-INFINITY:INFINITY;
    uint32_t face;
    FVizBool found=FVIZ_FALSE;
    if (traits.dimension==2u)
    {
        uint32_t face_count=(uint32_t)traits.fixed_point_count;
        uint32_t local[9];
        uint32_t i;
        if (fviz_cell_type_face(type,0u,local,9u,&face_count)!=FVIZ_OK)
        {
            if (type==FVIZ_CELL_TRIANGLE) { local[0]=0u; local[1]=1u; local[2]=2u; face_count=3u; }
            else if (type==FVIZ_CELL_QUAD) { local[0]=0u; local[1]=1u; local[2]=2u; local[3]=3u; face_count=4u; }
            else return NAN;
        }
        face_count=fviz_quality_face_corner_count(type,face_count);
        for (i=0u;i<face_count;++i)
        {
            const uint32_t prev=local[(i+face_count-1u)%face_count],cur=local[i],next=local[(i+1u)%face_count];
            const double angle=fviz_quality_angle_degrees(fviz_vec3_sub(p[prev],p[cur]),fviz_vec3_sub(p[next],p[cur]));
            if (!isfinite(angle)) continue;
            if ((maximum!=FVIZ_FALSE && angle>result) || (maximum==FVIZ_FALSE && angle<result)) result=angle;
            found=FVIZ_TRUE;
        }
        return found!=FVIZ_FALSE?result:NAN;
    }
    if (traits.dimension!=3u) return NAN;
    for (face=0u;face<traits.face_count;++face)
    {
        uint32_t local[9],face_count=0u,i;
        if (fviz_cell_type_face(type,face,local,9u,&face_count)!=FVIZ_OK) continue;
        face_count=fviz_quality_face_corner_count(type,face_count);
        for (i=0u;i<face_count;++i)
        {
            const uint32_t prev=local[(i+face_count-1u)%face_count],cur=local[i],next=local[(i+1u)%face_count];
            const double angle=fviz_quality_angle_degrees(fviz_vec3_sub(p[prev],p[cur]),fviz_vec3_sub(p[next],p[cur]));
            if (!isfinite(angle)) continue;
            if ((maximum!=FVIZ_FALSE && angle>result) || (maximum==FVIZ_FALSE && angle<result)) result=angle;
            found=FVIZ_TRUE;
        }
    }
    return found!=FVIZ_FALSE?result:NAN;
}

static double fviz_quality_warpage(FVizCellType type,const FVizVec3* p)
{
    const FVizCellTypeTraits traits=fviz_cell_type_traits(type);
    double maximum=0.0;
    FVizBool found=FVIZ_FALSE;
    uint32_t face;
    uint32_t face_total=traits.dimension==2u?1u:traits.face_count;
    for (face=0u;face<face_total;++face)
    {
        uint32_t local[9],count=0u;
        FVizVec3 n0,n1;
        double angle;
        if (traits.dimension==2u && type==FVIZ_CELL_QUAD)
        { local[0]=0u; local[1]=1u; local[2]=2u; local[3]=3u; count=4u; }
        else if (fviz_cell_type_face(type,face,local,9u,&count)!=FVIZ_OK) continue;
        count=fviz_quality_face_corner_count(type,count);
        if (count!=4u) continue;
        n0=fviz_vec3_cross(fviz_vec3_sub(p[local[1]],p[local[0]]),fviz_vec3_sub(p[local[2]],p[local[0]]));
        n1=fviz_vec3_cross(fviz_vec3_sub(p[local[2]],p[local[0]]),fviz_vec3_sub(p[local[3]],p[local[0]]));
        angle=fviz_quality_angle_degrees(n0,n1);
        if (isfinite(angle) && angle>maximum) maximum=angle;
        if (isfinite(angle)) found=FVIZ_TRUE;
    }
    return found!=FVIZ_FALSE?maximum:0.0;
}

static double fviz_quality_value(FVizCellType type,const FVizVec3* p,FVizSize count,FVizMeshQualityMetric metric)
{
    switch (metric)
    {
        case FVIZ_MESH_QUALITY_MEASURE: return fviz_quality_measure(type,p,count);
        case FVIZ_MESH_QUALITY_EDGE_RATIO: return fviz_quality_edge_ratio(type,p);
        case FVIZ_MESH_QUALITY_SCALED_JACOBIAN: return fviz_quality_scaled_jacobian(type,p);
        case FVIZ_MESH_QUALITY_MIN_CORNER_ANGLE: return fviz_quality_corner_angle(type,p,FVIZ_FALSE);
        case FVIZ_MESH_QUALITY_MAX_CORNER_ANGLE: return fviz_quality_corner_angle(type,p,FVIZ_TRUE);
        case FVIZ_MESH_QUALITY_WARPAGE: return fviz_quality_warpage(type,p);
        default: return NAN;
    }
}

static void fviz_mesh_quality_range(FVizSize begin,FVizSize end,void* user_data)
{
    FVizMeshQualityContext* context=(FVizMeshQualityContext*)user_data;
    FVizSize cell_id;
    for (cell_id=begin;cell_id<end;++cell_id)
    {
        FVizCellView view;
        FVizVec3 points[27];
        if (fviz_quality_cell_points(context->grid,cell_id,&view,points,27u)!=FVIZ_OK)
        {
            context->output[cell_id]=NAN;
            continue;
        }
        context->output[cell_id]=fviz_quality_value(view.type,points,view.point_count,context->metric);
    }
}

FVizResult fviz_mesh_quality_compute(
    const FVizUnstructuredGrid* input,FVizMeshQualityMetric metric,FVizDataArray** out_quality)
{
    FVizDataArray* quality=NULL;
    FVizMeshQualityContext context;
    const FVizSize cell_count=input!=NULL?fviz_unstructured_grid_cell_count(input):0u;
    if (out_quality==NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_quality=NULL;
    if (input==NULL || metric<FVIZ_MESH_QUALITY_MEASURE || metric>FVIZ_MESH_QUALITY_WARPAGE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"mesh quality input or metric is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_unstructured_grid_validate(input)!=FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64,1u,&quality)!=FVIZ_OK ||
        fviz_data_array_resize(quality,cell_count)!=FVIZ_OK)
    { fviz_release(quality); return fviz_last_error_code(); }
    context.grid=input; context.metric=metric; context.output=(double*)fviz_data_array_data(quality);
    if (cell_count>=4096u)
    {
        if (fviz_parallel_for(0u,cell_count,512u,fviz_mesh_quality_range,&context)!=FVIZ_OK)
        { fviz_release(quality); return fviz_last_error_code(); }
    }
    else fviz_mesh_quality_range(0u,cell_count,&context);
    *out_quality=quality;
    return FVIZ_OK;
}

const char* fviz_mesh_quality_metric_name(FVizMeshQualityMetric metric)
{
    switch (metric)
    {
        case FVIZ_MESH_QUALITY_MEASURE: return "Measure";
        case FVIZ_MESH_QUALITY_EDGE_RATIO: return "EdgeRatio";
        case FVIZ_MESH_QUALITY_SCALED_JACOBIAN: return "ScaledJacobian";
        case FVIZ_MESH_QUALITY_MIN_CORNER_ANGLE: return "MinCornerAngle";
        case FVIZ_MESH_QUALITY_MAX_CORNER_ANGLE: return "MaxCornerAngle";
        case FVIZ_MESH_QUALITY_WARPAGE: return "Warpage";
        default: return "Unknown";
    }
}

struct FVizMeshQualityFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizMeshQualityMetric metric;
    char result_name[96];
    FVizBool active_scalars;
};

static void fviz_mesh_quality_filter_destroy(FVizObject* object)
{
    FVizMeshQualityFilter* filter=(FVizMeshQualityFilter*)object;
    fviz_release(filter->algorithm);
}

static const FVizObjectClass g_fviz_mesh_quality_filter_class={
    FVIZ_TYPE_MESH_QUALITY_FILTER,"FVizMeshQualityFilter",&g_fviz_object_class,
    fviz_mesh_quality_filter_destroy,NULL
};

static FVizMTime fviz_mesh_quality_filter_state_mtime(const void* state)
{
    return state!=NULL?fviz_object_mtime((const FVizObject*)state):0u;
}

static FVizResult fviz_mesh_quality_filter_process(
    FVizAlgorithm* algorithm,const FVizPipelineRequestInfo* request,void* state)
{
    FVizMeshQualityFilter* filter=(FVizMeshQualityFilter*)state;
    FVizUnstructuredGrid* input;
    FVizUnstructuredGrid* output=NULL;
    FVizDataArray* quality=NULL;
    if (request->type!=FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input=(FVizUnstructuredGrid*)fviz_algorithm_resolved_input(algorithm,0u,0u);
    if (input==NULL)
    { fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"mesh quality filter has no input"); return FVIZ_ERROR_INVALID_STATE; }
    if (fviz_unstructured_grid_shallow_copy(input,&output)!=FVIZ_OK ||
        fviz_mesh_quality_compute(input,filter->metric,&quality)!=FVIZ_OK)
        goto fail;
    if (fviz_attribute_set_add(fviz_unstructured_grid_cell_data(output),filter->result_name,quality)!=FVIZ_OK)
        goto fail;
    if (filter->active_scalars!=FVIZ_FALSE &&
        fviz_attribute_set_set_active(fviz_unstructured_grid_cell_data(output),FVIZ_ATTRIBUTE_SCALARS,filter->result_name)!=FVIZ_OK)
        goto fail;
    if (fviz_algorithm_set_output_data(algorithm,request->requested_output_port,(FVizDataObject*)output)!=FVIZ_OK)
        goto fail;
    fviz_release(quality); fviz_release(output); return FVIZ_OK;
fail:
    fviz_release(quality); fviz_release(output); return fviz_last_error_code();
}

FVizResult fviz_mesh_quality_filter_create(FVizMeshQualityFilter** out_filter)
{
    FVizMeshQualityFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter==NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_filter=NULL;
    filter=(FVizMeshQualityFilter*)fviz_internal_object_allocate(sizeof(*filter),&g_fviz_mesh_quality_filter_class,NULL);
    if (filter==NULL) return fviz_last_error_code();
    filter->metric=FVIZ_MESH_QUALITY_SCALED_JACOBIAN;
    (void)memcpy(filter->result_name,"ScaledJacobian",15u);
    filter->active_scalars=FVIZ_TRUE;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request=fviz_mesh_quality_filter_process;
    callbacks.get_state_mtime=fviz_mesh_quality_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u,1u,&callbacks,filter,&filter->algorithm)!=FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm,0u,FVIZ_TYPE_UNSTRUCTURED_GRID,FVIZ_FALSE,FVIZ_FALSE)!=FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm,0u,FVIZ_TYPE_UNSTRUCTURED_GRID)!=FVIZ_OK)
    { fviz_release(filter); return fviz_last_error_code(); }
    *out_filter=filter; return FVIZ_OK;
}

FVizResult fviz_mesh_quality_filter_set_input_data(FVizMeshQualityFilter* filter,FVizUnstructuredGrid* input)
{ return filter!=NULL?fviz_algorithm_set_input_data(filter->algorithm,0u,(FVizDataObject*)input):FVIZ_ERROR_INVALID_ARGUMENT; }
FVizResult fviz_mesh_quality_filter_set_input_connection(FVizMeshQualityFilter* filter,FVizAlgorithmOutput* input)
{ return filter!=NULL?fviz_algorithm_set_input_connection(filter->algorithm,0u,input):FVIZ_ERROR_INVALID_ARGUMENT; }
void fviz_mesh_quality_filter_set_metric(FVizMeshQualityFilter* filter,FVizMeshQualityMetric metric)
{
    if (filter==NULL || metric<FVIZ_MESH_QUALITY_MEASURE || metric>FVIZ_MESH_QUALITY_WARPAGE || filter->metric==metric) return;
    filter->metric=metric;
    (void)snprintf(filter->result_name,sizeof(filter->result_name),"%s",fviz_mesh_quality_metric_name(metric));
    fviz_object_modified((FVizObject*)filter);
}
FVizMeshQualityMetric fviz_mesh_quality_filter_metric(const FVizMeshQualityFilter* filter)
{ return filter!=NULL?filter->metric:FVIZ_MESH_QUALITY_MEASURE; }
FVizResult fviz_mesh_quality_filter_set_result_name(FVizMeshQualityFilter* filter,const char* name)
{
    const FVizSize length=name!=NULL?(FVizSize)strlen(name):0u;
    if (filter==NULL || length==0u || length>=sizeof(filter->result_name))
    { fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"mesh quality result name is invalid"); return FVIZ_ERROR_INVALID_ARGUMENT; }
    if (strcmp(filter->result_name,name)==0) return FVIZ_OK;
    (void)memcpy(filter->result_name,name,length+1u); fviz_object_modified((FVizObject*)filter); return FVIZ_OK;
}
const char* fviz_mesh_quality_filter_result_name(const FVizMeshQualityFilter* filter)
{ return filter!=NULL?filter->result_name:NULL; }
void fviz_mesh_quality_filter_set_result_as_active_scalars(FVizMeshQualityFilter* filter,FVizBool enabled)
{
    if (filter==NULL) return;
    enabled=enabled!=FVIZ_FALSE?FVIZ_TRUE:FVIZ_FALSE;
    if (filter->active_scalars!=enabled)
    {
        filter->active_scalars=enabled;
        fviz_object_modified((FVizObject*)filter);
    }
}
FVizAlgorithm* fviz_mesh_quality_filter_algorithm(FVizMeshQualityFilter* filter){return filter!=NULL?filter->algorithm:NULL;}
FVizAlgorithmOutput* fviz_mesh_quality_filter_output_port(FVizMeshQualityFilter* filter){return filter!=NULL?fviz_algorithm_output_port(filter->algorithm,0u):NULL;}
FVizUnstructuredGrid* fviz_mesh_quality_filter_output(FVizMeshQualityFilter* filter){return filter!=NULL?(FVizUnstructuredGrid*)fviz_algorithm_output_data(filter->algorithm,0u):NULL;}
FVizResult fviz_mesh_quality_filter_update(FVizMeshQualityFilter* filter){return filter!=NULL?fviz_algorithm_update(filter->algorithm):FVIZ_ERROR_INVALID_ARGUMENT;}
