#include <math.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Algorithms/FVizProbeFilter.h>
#include <FViz/Algorithms/FVizResampleWithDataSet.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizImageData.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizResampleWithDataSet
{
    FVizObject base;
    FVizAlgorithm* algorithm;
};

typedef struct FVizImageSampleArray
{
    const char* name;
    const FVizDataArray* source;
    const unsigned char* source_data;
    FVizSize source_stride;
    FVizDataType source_type;
    FVizSize source_type_size;
    uint32_t components;
    FVizDataArray* destination;
} FVizImageSampleArray;

typedef struct FVizImageStencil
{
    FVizId ids[8];
    double weights[8];
    uint32_t count;
} FVizImageStencil;

static void fviz_resample_destroy(FVizObject* object)
{
    FVizResampleWithDataSet* filter=(FVizResampleWithDataSet*)object;
    fviz_release(filter->algorithm);
}

static const FVizObjectClass g_fviz_resample_class={
    FVIZ_TYPE_RESAMPLE_WITH_DATA_SET,"FVizResampleWithDataSet",&g_fviz_object_class,
    fviz_resample_destroy,NULL
};

static FVizMTime fviz_resample_state_mtime(const void* state)
{
    return state!=NULL?fviz_object_mtime((const FVizObject*)state):0u;
}


static double fviz_resample_read_value(const unsigned char* source,FVizDataType type)
{
    switch (type)
    {
        case FVIZ_DATA_INT8: { int8_t v; memcpy(&v,source,sizeof(v)); return (double)v; }
        case FVIZ_DATA_UINT8: { uint8_t v; memcpy(&v,source,sizeof(v)); return (double)v; }
        case FVIZ_DATA_INT16: { int16_t v; memcpy(&v,source,sizeof(v)); return (double)v; }
        case FVIZ_DATA_UINT16: { uint16_t v; memcpy(&v,source,sizeof(v)); return (double)v; }
        case FVIZ_DATA_INT32: { int32_t v; memcpy(&v,source,sizeof(v)); return (double)v; }
        case FVIZ_DATA_UINT32: { uint32_t v; memcpy(&v,source,sizeof(v)); return (double)v; }
        case FVIZ_DATA_INT64: { int64_t v; memcpy(&v,source,sizeof(v)); return (double)v; }
        case FVIZ_DATA_UINT64: { uint64_t v; memcpy(&v,source,sizeof(v)); return (double)v; }
        case FVIZ_DATA_FLOAT32: { float v; memcpy(&v,source,sizeof(v)); return (double)v; }
        case FVIZ_DATA_FLOAT64: { double v; memcpy(&v,source,sizeof(v)); return v; }
        default: return 0.0;
    }
}

static void fviz_resample_write_value(unsigned char* destination,FVizDataType type,double value)
{
    switch (type)
    {
        case FVIZ_DATA_INT8: { const int8_t v=(int8_t)value; memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_UINT8: { const uint8_t v=(uint8_t)value; memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_INT16: { const int16_t v=(int16_t)value; memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_UINT16: { const uint16_t v=(uint16_t)value; memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_INT32: { const int32_t v=(int32_t)value; memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_UINT32: { const uint32_t v=(uint32_t)value; memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_INT64: { const int64_t v=(int64_t)value; memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_UINT64: { const uint64_t v=(uint64_t)value; memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_FLOAT32: { const float v=(float)value; memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_FLOAT64: memcpy(destination,&value,sizeof(value)); break;
        default: break;
    }
}

static FVizBool fviz_image_stencil(const FVizImageData* image,const FVizVec3 point,FVizImageStencil* out)
{
    const double physical[3]={(double)point.x,(double)point.y,(double)point.z};
    double index[3];
    FVizSize dims[3];
    int64_t extent[6];
    int64_t lower[3];
    double t[3];
    uint32_t active[3];
    uint32_t active_count=0u;
    uint32_t axis,corner;
    if (fviz_image_data_physical_to_continuous_index(image,physical,index)!=FVIZ_OK) return FVIZ_FALSE;
    fviz_image_data_dimensions(image,dims);
    fviz_image_data_extent(image,extent);
    for (axis=0u;axis<3u;++axis)
    {
        const double minimum=(double)extent[axis*2u];
        const double maximum=(double)extent[axis*2u+1u];
        const double epsilon=1.0e-10*(fabs(maximum-minimum)+1.0);
        if (index[axis]<minimum-epsilon || index[axis]>maximum+epsilon) return FVIZ_FALSE;
        if (dims[axis]<=1u) { lower[axis]=extent[axis*2u]; t[axis]=0.0; }
        else if (index[axis]>=maximum) { lower[axis]=extent[axis*2u+1u]-1; t[axis]=1.0; active[active_count++]=axis; }
        else { const double floor_value=floor(index[axis]); lower[axis]=(int64_t)floor_value; t[axis]=index[axis]-floor_value; active[active_count++]=axis; }
    }
    out->count=0u;
    for (corner=0u;corner<(1u<<active_count);++corner)
    {
        int64_t ijk[3]={lower[0],lower[1],lower[2]};
        double weight=1.0;
        FVizId point_id;
        uint32_t bit;
        for (bit=0u;bit<active_count;++bit)
        {
            const uint32_t a=active[bit];
            if ((corner&(1u<<bit))!=0u) { ++ijk[a]; weight*=t[a]; }
            else weight*=1.0-t[a];
        }
        if (weight==0.0) continue;
        if (fviz_image_data_point_id(image,ijk[0],ijk[1],ijk[2],&point_id)!=FVIZ_OK) return FVIZ_FALSE;
        out->ids[out->count]=point_id;
        out->weights[out->count]=weight;
        ++out->count;
    }
    return FVIZ_TRUE;
}

static FVizResult fviz_resample_image(
    FVizAlgorithm* algorithm,FVizPolyData* input,FVizImageData* source,FVizPolyData** out_output)
{
    FVizPolyData* output=NULL;
    FVizDataArray* valid_mask=NULL;
    FVizImageSampleArray* arrays=NULL;
    const FVizAttributeSet* source_set=fviz_image_data_const_point_data(source);
    const FVizSize candidate_count=fviz_attribute_set_count(source_set);
    const FVizSize source_points=fviz_image_data_point_count(source);
    const FVizSize point_count=fviz_poly_data_point_count(input);
    FVizSize array_count=0u;
    FVizSize i;
    FVizSize bytes=0u;
    if (out_output!=NULL) *out_output=NULL;
    if (fviz_poly_data_deep_copy(input,&output)!=FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT8,1u,&valid_mask)!=FVIZ_OK ||
        fviz_data_array_resize(valid_mask,point_count)!=FVIZ_OK) goto fail;
    if (candidate_count!=0u)
    {
        if (fviz_size_multiply(candidate_count,sizeof(*arrays),&bytes)!=FVIZ_OK) goto fail;
        arrays=(FVizImageSampleArray*)fviz_alloc(bytes);
        if (arrays==NULL) goto fail;
        memset(arrays,0,bytes);
    }
    for (i=0u;i<candidate_count;++i)
    {
        const FVizDataArray* source_array=fviz_attribute_set_const_array_at(source_set,i);
        const char* name=fviz_attribute_set_name_at(source_set,i);
        FVizDataArray* destination=NULL;
        FVizAttributeRole role;
        if (source_array==NULL || name==NULL || fviz_data_array_tuple_count(source_array)!=source_points) continue;
        if (fviz_data_array_create(fviz_data_array_type(source_array),fviz_data_array_components(source_array),&destination)!=FVIZ_OK ||
            fviz_data_array_resize(destination,point_count)!=FVIZ_OK) { fviz_release(destination); goto fail; }
        memset(fviz_data_array_data(destination),0,fviz_data_array_tuple_stride(destination)*point_count);
        if (fviz_attribute_set_add(fviz_poly_data_point_data(output),name,destination)!=FVIZ_OK) { fviz_release(destination); goto fail; }
        arrays[array_count].name=name;
        arrays[array_count].source=source_array;
        arrays[array_count].source_data=(const unsigned char*)fviz_data_array_const_data(source_array);
        arrays[array_count].source_stride=fviz_data_array_tuple_stride(source_array);
        arrays[array_count].source_type=fviz_data_array_type(source_array);
        arrays[array_count].source_type_size=fviz_data_type_size(arrays[array_count].source_type);
        arrays[array_count].components=fviz_data_array_components(source_array);
        arrays[array_count].destination=destination;
        for (role=FVIZ_ATTRIBUTE_SCALARS;role<FVIZ_ATTRIBUTE_ROLE_COUNT;++role)
        {
            const char* active=fviz_attribute_set_active_name(source_set,role);
            if (active!=NULL && strcmp(active,name)==0)
                (void)fviz_attribute_set_set_active(fviz_poly_data_point_data(output),role,name);
        }
        ++array_count;
    }
    {
        uint8_t* mask=(uint8_t*)fviz_data_array_data(valid_mask);
        for (i=0u;i<point_count;++i)
        {
            FVizImageStencil stencil;
            FVizSize a;
            if (fviz_image_stencil(source,fviz_poly_data_points(input)[i],&stencil)==FVIZ_FALSE) { mask[i]=0u; continue; }
            mask[i]=1u;
            for (a=0u;a<array_count;++a)
            {
                const FVizImageSampleArray* sample_array=&arrays[a];
                FVizDataArray* dst=sample_array->destination;
                unsigned char* tuple=(unsigned char*)fviz_data_array_data(dst)+i*fviz_data_array_tuple_stride(dst);
                uint32_t component;
                for (component=0u;component<sample_array->components;++component)
                {
                    double value=0.0;
                    uint32_t k;
                    for (k=0u;k<stencil.count;++k)
                    {
                        const unsigned char* source_value=sample_array->source_data+
                            (FVizSize)stencil.ids[k]*sample_array->source_stride+
                            (FVizSize)component*sample_array->source_type_size;
                        value += stencil.weights[k]*fviz_resample_read_value(source_value,sample_array->source_type);
                    }
                    fviz_resample_write_value(tuple+(FVizSize)component*sample_array->source_type_size,sample_array->source_type,value);
                }
            }
            if ((i&4095u)==4095u)
            {
                if (fviz_algorithm_abort_requested(algorithm)!=FVIZ_FALSE)
                { fviz_internal_set_error(FVIZ_ERROR_CANCELLED,"resample filter was aborted"); goto fail; }
                if (point_count!=0u) (void)fviz_algorithm_report_progress(algorithm,(double)(i+1u)/(double)point_count);
            }
        }
    }
    if (fviz_attribute_set_add(fviz_poly_data_point_data(output),"FVizValidPointMask",valid_mask)!=FVIZ_OK) goto fail;
    for (i=0u;i<array_count;++i) fviz_release(arrays[i].destination);
    fviz_free(arrays); fviz_release(valid_mask);
    *out_output=output;
    return FVIZ_OK;
fail:
    for (i=0u;i<array_count;++i) fviz_release(arrays[i].destination);
    fviz_free(arrays); fviz_release(valid_mask); fviz_release(output);
    return fviz_last_error_code();
}

static FVizResult fviz_resample_process(FVizAlgorithm* algorithm,const FVizPipelineRequestInfo* request,void* state)
{
    FVizPolyData* input;
    FVizDataObject* source;
    FVizPolyData* output=NULL;
    (void)state;
    if (request->type!=FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input=(FVizPolyData*)fviz_algorithm_resolved_input(algorithm,0u,0u);
    source=fviz_algorithm_resolved_input(algorithm,1u,0u);
    if (input==NULL || source==NULL)
    { fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"resample filter requires sampling geometry and source data"); return FVIZ_ERROR_INVALID_STATE; }
    if (fviz_object_is_type((FVizObject*)source,FVIZ_TYPE_IMAGE_DATA)!=FVIZ_FALSE)
    {
        if (fviz_resample_image(algorithm,input,(FVizImageData*)source,&output)!=FVIZ_OK) return fviz_last_error_code();
    }
    else if (fviz_object_is_type((FVizObject*)source,FVIZ_TYPE_UNSTRUCTURED_GRID)!=FVIZ_FALSE)
    {
        FVizProbeFilter* probe=NULL;
        if (fviz_probe_filter_create(&probe)!=FVIZ_OK ||
            fviz_probe_filter_set_input_data(probe,input)!=FVIZ_OK ||
            fviz_probe_filter_set_source_data(probe,(FVizUnstructuredGrid*)source)!=FVIZ_OK ||
            fviz_probe_filter_update(probe)!=FVIZ_OK)
        { fviz_release(probe); return fviz_last_error_code(); }
        output=(FVizPolyData*)fviz_retain(fviz_probe_filter_output(probe));
        fviz_release(probe);
        if (output==NULL) return fviz_last_error_code();
    }
    else
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,"resample source must be ImageData or UnstructuredGrid");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    if (fviz_algorithm_set_output_data(algorithm,request->requested_output_port,(FVizDataObject*)output)!=FVIZ_OK)
    { fviz_release(output); return fviz_last_error_code(); }
    fviz_release(output);
    (void)fviz_algorithm_report_progress(algorithm,1.0);
    return FVIZ_OK;
}

FVizResult fviz_resample_with_data_set_create(FVizResampleWithDataSet** out_filter)
{
    FVizResampleWithDataSet* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter==NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_filter=NULL;
    filter=(FVizResampleWithDataSet*)fviz_internal_object_allocate(sizeof(*filter),&g_fviz_resample_class,NULL);
    if (filter==NULL) return fviz_last_error_code();
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request=fviz_resample_process;
    callbacks.get_state_mtime=fviz_resample_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(2u,1u,&callbacks,filter,&filter->algorithm)!=FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm,0u,FVIZ_TYPE_POLY_DATA,FVIZ_FALSE,FVIZ_FALSE)!=FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm,1u,FVIZ_TYPE_DATA_OBJECT,FVIZ_FALSE,FVIZ_FALSE)!=FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm,0u,FVIZ_TYPE_POLY_DATA)!=FVIZ_OK)
    { fviz_release(filter); return fviz_last_error_code(); }
    *out_filter=filter;
    return FVIZ_OK;
}
FVizResult fviz_resample_with_data_set_set_input_data(FVizResampleWithDataSet* f,FVizPolyData* d){return f!=NULL?fviz_algorithm_set_input_data(f->algorithm,0u,(FVizDataObject*)d):FVIZ_ERROR_INVALID_ARGUMENT;}
FVizResult fviz_resample_with_data_set_set_input_connection(FVizResampleWithDataSet* f,FVizAlgorithmOutput* d){return f!=NULL?fviz_algorithm_set_input_connection(f->algorithm,0u,d):FVIZ_ERROR_INVALID_ARGUMENT;}
FVizResult fviz_resample_with_data_set_set_source_data(FVizResampleWithDataSet* f,FVizDataObject* d){return f!=NULL?fviz_algorithm_set_input_data(f->algorithm,1u,d):FVIZ_ERROR_INVALID_ARGUMENT;}
FVizResult fviz_resample_with_data_set_set_source_connection(FVizResampleWithDataSet* f,FVizAlgorithmOutput* d){return f!=NULL?fviz_algorithm_set_input_connection(f->algorithm,1u,d):FVIZ_ERROR_INVALID_ARGUMENT;}
FVizAlgorithm* fviz_resample_with_data_set_algorithm(FVizResampleWithDataSet* f){return f!=NULL?f->algorithm:NULL;}
FVizAlgorithmOutput* fviz_resample_with_data_set_output_port(FVizResampleWithDataSet* f){return f!=NULL?fviz_algorithm_output_port(f->algorithm,0u):NULL;}
FVizPolyData* fviz_resample_with_data_set_output(FVizResampleWithDataSet* f){return f!=NULL?(FVizPolyData*)fviz_algorithm_output_data(f->algorithm,0u):NULL;}
FVizResult fviz_resample_with_data_set_update(FVizResampleWithDataSet* f){return f!=NULL?fviz_algorithm_update(f->algorithm):FVIZ_ERROR_INVALID_ARGUMENT;}
