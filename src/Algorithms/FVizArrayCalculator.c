#include <math.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Algorithms/FVizArrayCalculator.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Math/FVizTensor.h>
#include <FViz/Parallel/FVizParallel.h>

#include <FViz/Core/FVizErrorInternal.h>

typedef struct FVizArrayCalcContext
{
    const unsigned char* source;
    FVizSize source_stride;
    FVizDataType source_type;
    uint32_t source_components;
    FVizArrayCalculatorOptions options;
    double* output;
    uint32_t output_components;
} FVizArrayCalcContext;

static double fviz_array_calc_read(const unsigned char* p,FVizDataType type)
{
    switch (type)
    {
        case FVIZ_DATA_INT8: { int8_t v; memcpy(&v,p,sizeof(v)); return (double)v; }
        case FVIZ_DATA_UINT8: { uint8_t v; memcpy(&v,p,sizeof(v)); return (double)v; }
        case FVIZ_DATA_INT16: { int16_t v; memcpy(&v,p,sizeof(v)); return (double)v; }
        case FVIZ_DATA_UINT16: { uint16_t v; memcpy(&v,p,sizeof(v)); return (double)v; }
        case FVIZ_DATA_INT32: { int32_t v; memcpy(&v,p,sizeof(v)); return (double)v; }
        case FVIZ_DATA_UINT32: { uint32_t v; memcpy(&v,p,sizeof(v)); return (double)v; }
        case FVIZ_DATA_INT64: { int64_t v; memcpy(&v,p,sizeof(v)); return (double)v; }
        case FVIZ_DATA_UINT64: { uint64_t v; memcpy(&v,p,sizeof(v)); return (double)v; }
        case FVIZ_DATA_FLOAT32: { float v; memcpy(&v,p,sizeof(v)); return (double)v; }
        case FVIZ_DATA_FLOAT64: { double v; memcpy(&v,p,sizeof(v)); return v; }
        default: return 0.0;
    }
}

static double fviz_array_calc_component(const FVizArrayCalcContext* c,FVizSize tuple,uint32_t component)
{
    const FVizSize size=fviz_data_type_size(c->source_type);
    return fviz_array_calc_read(c->source+tuple*c->source_stride+(FVizSize)component*size,c->source_type);
}


static FVizSymmetricTensor3d fviz_array_calc_tensor(
    const FVizArrayCalcContext* context, FVizSize tuple)
{
    FVizSymmetricTensor3d tensor = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double components[9];
    uint32_t component;
    for (component = 0u; component < context->source_components; ++component)
        components[component] = fviz_array_calc_component(context, tuple, component);
    (void)fviz_symmetric_tensor3d_from_components(
        components, context->source_components, &tensor);
    return tensor;
}


static void fviz_array_calc_range(FVizSize begin,FVizSize end,void* user_data)
{
    FVizArrayCalcContext* c=(FVizArrayCalcContext*)user_data;
    FVizSize i;
    for (i=begin;i<end;++i)
    {
        if (c->options.operation==FVIZ_ARRAY_CALC_COMPONENT)
        {
            c->output[i]=fviz_array_calc_component(c,i,c->options.component);
        }
        else if (c->options.operation==FVIZ_ARRAY_CALC_MAGNITUDE)
        {
            double sum=0.0;
            uint32_t k;
            for (k=0u;k<c->source_components;++k)
            {
                const double v=fviz_array_calc_component(c,i,k);
                sum += v*v;
            }
            c->output[i]=sqrt(sum);
        }
        else if (c->options.operation==FVIZ_ARRAY_CALC_SCALE_OFFSET)
        {
            uint32_t k;
            double* tuple=c->output+i*c->output_components;
            for (k=0u;k<c->source_components;++k)
                tuple[k]=fviz_array_calc_component(c,i,k)*c->options.scale+c->options.offset;
        }
        else
        {
            const FVizSymmetricTensor3d t=fviz_array_calc_tensor(c,i);
            double principal[3];
            (void)fviz_symmetric_tensor3d_eigensystem(&t,principal,NULL);
            if (c->options.operation==FVIZ_ARRAY_CALC_EQUIVALENT_DEVIATORIC)
            {
                c->output[i]=sqrt(0.5*((t.xx-t.yy)*(t.xx-t.yy)+(t.yy-t.zz)*(t.yy-t.zz)+(t.zz-t.xx)*(t.zz-t.xx)) +
                                  3.0*(t.xy*t.xy+t.yz*t.yz+t.xz*t.xz));
            }
            else if (c->options.operation==FVIZ_ARRAY_CALC_TENSOR_MEAN)
                c->output[i]=fviz_symmetric_tensor3d_mean(&t);
            else if (c->options.operation==FVIZ_ARRAY_CALC_PRINCIPAL_MAX) c->output[i]=principal[0];
            else if (c->options.operation==FVIZ_ARRAY_CALC_PRINCIPAL_MID) c->output[i]=principal[1];
            else if (c->options.operation==FVIZ_ARRAY_CALC_PRINCIPAL_MIN) c->output[i]=principal[2];
            else if (c->options.operation==FVIZ_ARRAY_CALC_HALF_PRINCIPAL_SPAN) c->output[i]=0.5*(principal[0]-principal[2]);
            else if (c->options.operation==FVIZ_ARRAY_CALC_PRINCIPAL_SPAN) c->output[i]=principal[0]-principal[2];
            else if (c->options.operation==FVIZ_ARRAY_CALC_PRINCIPAL_VALUES)
            {
                double* tuple=c->output+i*3u;
                tuple[0]=principal[0]; tuple[1]=principal[1]; tuple[2]=principal[2];
            }
            else if (c->options.operation==FVIZ_ARRAY_CALC_DEVIATORIC_TENSOR)
            {
                FVizSymmetricTensor3d deviatoric;
                double* tuple=c->output+i*6u;
                (void)fviz_symmetric_tensor3d_deviatoric(&t,&deviatoric);
                tuple[0]=deviatoric.xx; tuple[1]=deviatoric.yy; tuple[2]=deviatoric.zz;
                tuple[3]=deviatoric.xy; tuple[4]=deviatoric.yz; tuple[5]=deviatoric.xz;
            }
            else if (c->options.operation==FVIZ_ARRAY_CALC_PRINCIPAL_DIRECTIONS)
            {
                double* tuple=c->output+i*9u;
                (void)fviz_symmetric_tensor3d_eigensystem(&t,principal,tuple);
            }
        }
    }
}

void fviz_array_calculator_options_initialize(FVizArrayCalculatorOptions* options)
{
    if (options==NULL) return;
    memset(options,0,sizeof(*options));
    options->struct_size=(uint32_t)sizeof(*options);
    options->operation=FVIZ_ARRAY_CALC_MAGNITUDE;
    options->scale=1.0;
    options->parallel_threshold=16384u;
}

FVizResult fviz_array_calculator_compute(
    const FVizDataArray* source,const FVizArrayCalculatorOptions* options,FVizDataArray** out_array)
{
    FVizArrayCalculatorOptions defaults;
    FVizArrayCalcContext context;
    FVizDataArray* output=NULL;
    const FVizSize tuple_count=source!=NULL?fviz_data_array_tuple_count(source):0u;
    uint32_t output_components;
    if (out_array==NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_array=NULL;
    if (source==NULL)
    { fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"array calculator source must not be NULL"); return FVIZ_ERROR_INVALID_ARGUMENT; }
    fviz_array_calculator_options_initialize(&defaults);
    if (options==NULL) options=&defaults;
    if (options->operation<FVIZ_ARRAY_CALC_COMPONENT || options->operation>FVIZ_ARRAY_CALC_PRINCIPAL_DIRECTIONS ||
        !isfinite(options->scale) || !isfinite(options->offset))
    { fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"array calculator options are invalid"); return FVIZ_ERROR_INVALID_ARGUMENT; }
    if (options->operation==FVIZ_ARRAY_CALC_COMPONENT && options->component>=fviz_data_array_components(source))
    { fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"array calculator component is out of range"); return FVIZ_ERROR_INVALID_ARGUMENT; }
    if (options->operation>=FVIZ_ARRAY_CALC_EQUIVALENT_DEVIATORIC && options->operation!=FVIZ_ARRAY_CALC_SCALE_OFFSET &&
        fviz_data_array_components(source)!=6u && fviz_data_array_components(source)!=9u)
    { fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"tensor calculation requires 6 or 9 tensor components"); return FVIZ_ERROR_INVALID_ARGUMENT; }
    output_components=1u;
    if (options->operation==FVIZ_ARRAY_CALC_SCALE_OFFSET) output_components=fviz_data_array_components(source);
    else if (options->operation==FVIZ_ARRAY_CALC_PRINCIPAL_VALUES) output_components=3u;
    else if (options->operation==FVIZ_ARRAY_CALC_DEVIATORIC_TENSOR) output_components=6u;
    else if (options->operation==FVIZ_ARRAY_CALC_PRINCIPAL_DIRECTIONS) output_components=9u;
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64,output_components,&output)!=FVIZ_OK ||
        fviz_data_array_resize(output,tuple_count)!=FVIZ_OK)
    { fviz_release(output); return fviz_last_error_code(); }
    memset(&context,0,sizeof(context));
    context.source=(const unsigned char*)fviz_data_array_const_data(source);
    context.source_stride=fviz_data_array_tuple_stride(source);
    context.source_type=fviz_data_array_type(source);
    context.source_components=fviz_data_array_components(source);
    context.options=*options;
    context.output=(double*)fviz_data_array_data(output);
    context.output_components=output_components;
    if (tuple_count>=options->parallel_threshold && options->parallel_threshold!=0u)
    {
        if (fviz_parallel_for(0u,tuple_count,4096u,fviz_array_calc_range,&context)!=FVIZ_OK)
        { fviz_release(output); return fviz_last_error_code(); }
    }
    else fviz_array_calc_range(0u,tuple_count,&context);
    *out_array=output;
    return FVIZ_OK;
}

#include <FViz/Core/FVizObject.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Pipeline/FVizExecutive.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizArrayCalculatorFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizArrayCalculatorAssociation association;
    char array_name[128];
    char result_name[128];
    FVizArrayCalculatorOptions options;
    FVizBool active_scalars;
};

static void fviz_array_calculator_filter_destroy(FVizObject* object)
{
    FVizArrayCalculatorFilter* filter=(FVizArrayCalculatorFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm=NULL;
}

static const FVizObjectClass g_fviz_array_calculator_filter_class={
    FVIZ_TYPE_ARRAY_CALCULATOR_FILTER,"FVizArrayCalculatorFilter",&g_fviz_object_class,
    fviz_array_calculator_filter_destroy,NULL
};

static FVizMTime fviz_array_calculator_filter_state_mtime(const void* state)
{
    return state!=NULL?fviz_object_mtime((const FVizObject*)state):0u;
}

static FVizResult fviz_array_calculator_filter_copy_name(char* dst,FVizSize capacity,const char* src,const char* what)
{
    const FVizSize length=src!=NULL?(FVizSize)strlen(src):0u;
    if (src==NULL || length==0u || length>=capacity)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,what);
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    memcpy(dst,src,length+1u);
    return FVIZ_OK;
}

static FVizResult fviz_array_calculator_filter_process(
    FVizAlgorithm* algorithm,const FVizPipelineRequestInfo* request,void* state)
{
    FVizArrayCalculatorFilter* filter=(FVizArrayCalculatorFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output=NULL;
    FVizAttributeSet* destination_set;
    const FVizAttributeSet* source_set;
    const FVizDataArray* source_array;
    FVizDataArray* result_array=NULL;
    if (request->type!=FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input=(FVizPolyData*)fviz_algorithm_resolved_input(algorithm,0u,0u);
    if (input==NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"array calculator filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (filter->array_name[0]=='\0' || filter->result_name[0]=='\0')
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,"array calculator filter requires input and result array names");
        return FVIZ_ERROR_INVALID_STATE;
    }
    source_set=filter->association==FVIZ_ARRAY_CALC_POINT_DATA?
        fviz_poly_data_const_point_data(input):fviz_poly_data_const_cell_data(input);
    source_array=fviz_attribute_set_const_get(source_set,filter->array_name);
    if (source_array==NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND,"array calculator source array was not found");
        return FVIZ_ERROR_NOT_FOUND;
    }
    if (fviz_poly_data_deep_copy(input,&output)!=FVIZ_OK ||
        fviz_array_calculator_compute(source_array,&filter->options,&result_array)!=FVIZ_OK)
        goto fail;
    destination_set=filter->association==FVIZ_ARRAY_CALC_POINT_DATA?
        fviz_poly_data_point_data(output):fviz_poly_data_cell_data(output);
    if (fviz_attribute_set_add(destination_set,filter->result_name,result_array)!=FVIZ_OK) goto fail;
    if (filter->active_scalars!=FVIZ_FALSE && fviz_data_array_components(result_array)==1u &&
        fviz_attribute_set_set_active(destination_set,FVIZ_ATTRIBUTE_SCALARS,filter->result_name)!=FVIZ_OK) goto fail;
    if (fviz_algorithm_set_output_data(algorithm,request->requested_output_port,(FVizDataObject*)output)!=FVIZ_OK) goto fail;
    fviz_release(result_array);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_release(result_array);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_array_calculator_filter_create(FVizArrayCalculatorFilter** out_filter)
{
    FVizArrayCalculatorFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter==NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"array calculator filter output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter=NULL;
    filter=(FVizArrayCalculatorFilter*)fviz_internal_object_allocate(
        sizeof(*filter),&g_fviz_array_calculator_filter_class,NULL);
    if (filter==NULL) return fviz_last_error_code();
    filter->association=FVIZ_ARRAY_CALC_POINT_DATA;
    filter->array_name[0]='\0';
    memcpy(filter->result_name,"Result",7u);
    fviz_array_calculator_options_initialize(&filter->options);
    filter->active_scalars=FVIZ_TRUE;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request=fviz_array_calculator_filter_process;
    callbacks.get_state_mtime=fviz_array_calculator_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u,1u,&callbacks,filter,&filter->algorithm)!=FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm,0u,FVIZ_TYPE_POLY_DATA,FVIZ_FALSE,FVIZ_FALSE)!=FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm,0u,FVIZ_TYPE_POLY_DATA)!=FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter=filter;
    return FVIZ_OK;
}

FVizResult fviz_array_calculator_filter_set_input_data(FVizArrayCalculatorFilter* filter,FVizPolyData* input)
{
    return filter!=NULL?fviz_algorithm_set_input_data(filter->algorithm,0u,(FVizDataObject*)input):FVIZ_ERROR_INVALID_ARGUMENT;
}
FVizResult fviz_array_calculator_filter_set_input_connection(FVizArrayCalculatorFilter* filter,FVizAlgorithmOutput* input)
{
    return filter!=NULL?fviz_algorithm_set_input_connection(filter->algorithm,0u,input):FVIZ_ERROR_INVALID_ARGUMENT;
}
FVizResult fviz_array_calculator_filter_set_array(FVizArrayCalculatorFilter* filter,FVizArrayCalculatorAssociation association,const char* array_name)
{
    if (filter==NULL || (association!=FVIZ_ARRAY_CALC_POINT_DATA && association!=FVIZ_ARRAY_CALC_CELL_DATA))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"array calculator filter association is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_array_calculator_filter_copy_name(filter->array_name,sizeof(filter->array_name),array_name,
        "array calculator input array name is invalid")!=FVIZ_OK) return fviz_last_error_code();
    filter->association=association;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}
FVizResult fviz_array_calculator_filter_set_result_name(FVizArrayCalculatorFilter* filter,const char* result_name)
{
    if (filter==NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_array_calculator_filter_copy_name(filter->result_name,sizeof(filter->result_name),result_name,
        "array calculator result array name is invalid")!=FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}
FVizResult fviz_array_calculator_filter_set_options(FVizArrayCalculatorFilter* filter,const FVizArrayCalculatorOptions* options)
{
    if (filter==NULL || options==NULL || options->struct_size<sizeof(FVizArrayCalculatorOptions))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"array calculator filter options are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    filter->options=*options;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}
void fviz_array_calculator_filter_set_result_as_active_scalars(FVizArrayCalculatorFilter* filter,FVizBool enabled)
{
    if (filter==NULL) return;
    filter->active_scalars=enabled!=FVIZ_FALSE?FVIZ_TRUE:FVIZ_FALSE;
    fviz_object_modified((FVizObject*)filter);
}
FVizAlgorithm* fviz_array_calculator_filter_algorithm(FVizArrayCalculatorFilter* filter){return filter!=NULL?filter->algorithm:NULL;}
FVizAlgorithmOutput* fviz_array_calculator_filter_output_port(FVizArrayCalculatorFilter* filter){return filter!=NULL?fviz_algorithm_output_port(filter->algorithm,0u):NULL;}
FVizPolyData* fviz_array_calculator_filter_output(FVizArrayCalculatorFilter* filter){return filter!=NULL?(FVizPolyData*)fviz_algorithm_output_data(filter->algorithm,0u):NULL;}
FVizResult fviz_array_calculator_filter_update(FVizArrayCalculatorFilter* filter){return filter!=NULL?fviz_algorithm_update(filter->algorithm):FVIZ_ERROR_INVALID_ARGUMENT;}
