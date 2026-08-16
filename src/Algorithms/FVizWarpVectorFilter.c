#include <math.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Algorithms/FVizWarpVectorFilter.h>
#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Parallel/FVizParallel.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Mesh/FVizPolyDataPrivate.h>

typedef struct FVizWarpVectorContext
{
    const FVizVec3* input;
    FVizVec3* output;
    const unsigned char* vectors;
    FVizSize stride;
    FVizDataType type;
    FVizSize type_size;
    double scale;
} FVizWarpVectorContext;

static double fviz_warp_vector_read(const unsigned char* p, FVizDataType type)
{
    switch (type)
    {
        case FVIZ_DATA_INT8:
            {
                int8_t v;
                memcpy(&v, p, sizeof(v));
                return (double)v;
            }
        case FVIZ_DATA_UINT8:
            {
                uint8_t v;
                memcpy(&v, p, sizeof(v));
                return (double)v;
            }
        case FVIZ_DATA_INT16:
            {
                int16_t v;
                memcpy(&v, p, sizeof(v));
                return (double)v;
            }
        case FVIZ_DATA_UINT16:
            {
                uint16_t v;
                memcpy(&v, p, sizeof(v));
                return (double)v;
            }
        case FVIZ_DATA_INT32:
            {
                int32_t v;
                memcpy(&v, p, sizeof(v));
                return (double)v;
            }
        case FVIZ_DATA_UINT32:
            {
                uint32_t v;
                memcpy(&v, p, sizeof(v));
                return (double)v;
            }
        case FVIZ_DATA_INT64:
            {
                int64_t v;
                memcpy(&v, p, sizeof(v));
                return (double)v;
            }
        case FVIZ_DATA_UINT64:
            {
                uint64_t v;
                memcpy(&v, p, sizeof(v));
                return (double)v;
            }
        case FVIZ_DATA_FLOAT32:
            {
                float v;
                memcpy(&v, p, sizeof(v));
                return (double)v;
            }
        case FVIZ_DATA_FLOAT64:
            {
                double v;
                memcpy(&v, p, sizeof(v));
                return v;
            }
        default:
            return 0.0;
    }
}

static void fviz_warp_vector_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizWarpVectorContext* c = (FVizWarpVectorContext*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i)
    {
        const unsigned char* tuple = c->vectors + i * c->stride;
        const double x = fviz_warp_vector_read(tuple, c->type);
        const double y = fviz_warp_vector_read(tuple + c->type_size, c->type);
        const double z = fviz_warp_vector_read(tuple + 2u * c->type_size, c->type);
        c->output[i] = fviz_vec3(c->input[i].x + (float)(x * c->scale), c->input[i].y + (float)(y * c->scale),
                                 c->input[i].z + (float)(z * c->scale));
    }
}

struct FVizWarpVectorFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    char vector_name[128];
    double scale;
    FVizBool recompute_normals;
};

static void fviz_warp_vector_filter_destroy(FVizObject* object)
{
    FVizWarpVectorFilter* filter = (FVizWarpVectorFilter*)object;
    fviz_release(filter->algorithm);
}

static const FVizObjectClass g_fviz_warp_vector_filter_class = {
    FVIZ_TYPE_WARP_VECTOR_FILTER, "FVizWarpVectorFilter", &g_fviz_object_class, fviz_warp_vector_filter_destroy, NULL};

static FVizMTime fviz_warp_vector_filter_state_mtime(const void* state)
{
    return state != NULL ? fviz_object_mtime((const FVizObject*)state) : 0u;
}

static FVizResult fviz_warp_vector_filter_process(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                  void* state)
{
    FVizWarpVectorFilter* filter = (FVizWarpVectorFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    const FVizDataArray* vectors;
    FVizArray* new_points = NULL;
    FVizArray* new_normals = NULL;
    FVizWarpVectorContext context;
    const FVizSize point_count =
        request->type == FVIZ_PIPELINE_REQUEST_DATA
            ? fviz_poly_data_point_count((FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u))
            : 0u;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "warp vector filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    vectors = fviz_attribute_set_const_get(fviz_poly_data_const_point_data(input), filter->vector_name);
    if (vectors == NULL || fviz_data_array_components(vectors) < 3u ||
        fviz_data_array_tuple_count(vectors) != point_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "warp vector array is missing or has an invalid shape");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_poly_data_shallow_copy(input, &output) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizVec3), &new_points) != FVIZ_OK ||
        fviz_array_resize(new_points, point_count) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizVec3), &new_normals) != FVIZ_OK)
        goto fail;
    memset(&context, 0, sizeof(context));
    context.input = fviz_poly_data_points(input);
    context.output = (FVizVec3*)fviz_array_data(new_points);
    context.vectors = (const unsigned char*)fviz_data_array_const_data(vectors);
    context.stride = fviz_data_array_tuple_stride(vectors);
    context.type = fviz_data_array_type(vectors);
    context.type_size = fviz_data_type_size(context.type);
    context.scale = filter->scale;
    if (point_count >= 16384u)
    {
        if (fviz_parallel_for(0u, point_count, 4096u, fviz_warp_vector_range, &context) != FVIZ_OK) goto fail;
    }
    else
        fviz_warp_vector_range(0u, point_count, &context);
    fviz_release(output->points);
    output->points = new_points;
    new_points = NULL;
    fviz_release(output->normals);
    output->normals = new_normals;
    new_normals = NULL;
    output->bounds = fviz_bounds_empty();
    output->bounds_dirty = FVIZ_TRUE;
    output->normals_dirty = FVIZ_TRUE;
    if (filter->recompute_normals != FVIZ_FALSE && fviz_poly_data_triangle_count(output) > 0u &&
        fviz_poly_data_compute_normals(output) != FVIZ_OK)
        goto fail;
    if (fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_release(new_points);
    fviz_release(new_normals);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_warp_vector_filter_create(FVizWarpVectorFilter** out_filter)
{
    FVizWarpVectorFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_filter = NULL;
    filter =
        (FVizWarpVectorFilter*)fviz_internal_object_allocate(sizeof(*filter), &g_fviz_warp_vector_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    (void)memcpy(filter->vector_name, "Vectors", 8u);
    filter->scale = 1.0;
    filter->recompute_normals = FVIZ_TRUE;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_warp_vector_filter_process;
    callbacks.get_state_mtime = fviz_warp_vector_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) !=
            FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_warp_vector_filter_set_input_data(FVizWarpVectorFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_warp_vector_filter_set_input_connection(FVizWarpVectorFilter* filter, FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_warp_vector_filter_set_vector_name(FVizWarpVectorFilter* filter, const char* name)
{
    const FVizSize length = name != NULL ? (FVizSize)strlen(name) : 0u;
    if (filter == NULL || length == 0u || length >= sizeof(filter->vector_name))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "warp vector name is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (strcmp(filter->vector_name, name) != 0)
    {
        memcpy(filter->vector_name, name, length + 1u);
        fviz_object_modified((FVizObject*)filter);
    }
    return FVIZ_OK;
}

const char* fviz_warp_vector_filter_vector_name(const FVizWarpVectorFilter* filter)
{
    return filter != NULL ? filter->vector_name : NULL;
}

void fviz_warp_vector_filter_set_scale(FVizWarpVectorFilter* filter, double scale)
{
    if (filter != NULL && isfinite(scale) && filter->scale != scale)
    {
        filter->scale = scale;
        fviz_object_modified((FVizObject*)filter);
    }
}

double fviz_warp_vector_filter_scale(const FVizWarpVectorFilter* filter)
{
    return filter != NULL ? filter->scale : 0.0;
}

void fviz_warp_vector_filter_set_recompute_normals(FVizWarpVectorFilter* filter, FVizBool enabled)
{
    if (filter != NULL)
    {
        enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (filter->recompute_normals != enabled)
        {
            filter->recompute_normals = enabled;
            fviz_object_modified((FVizObject*)filter);
        }
    }
}

FVizBool fviz_warp_vector_filter_recompute_normals(const FVizWarpVectorFilter* filter)
{
    return filter != NULL ? filter->recompute_normals : FVIZ_FALSE;
}

FVizAlgorithm* fviz_warp_vector_filter_algorithm(FVizWarpVectorFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_warp_vector_filter_output_port(FVizWarpVectorFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_warp_vector_filter_output(FVizWarpVectorFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_warp_vector_filter_update(FVizWarpVectorFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
