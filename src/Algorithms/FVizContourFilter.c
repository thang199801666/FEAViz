#include <string.h>

#include <FViz/Algorithms/FVizContourFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Parallel/FVizParallel.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Algorithms/FVizContourFilterPrivate.h>

static void fviz_contour_filter_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_contour_filter_class = {FVIZ_TYPE_CONTOUR_FILTER, "FVizContourFilter",
                                                            &g_fviz_object_class, fviz_contour_filter_destroy, NULL};

static void fviz_contour_copy_scalar_name(char destination[128], const char* source)
{
    FVizSize i = 0u;
    while (i + 1u < 128u && source[i] != '\0')
    {
        destination[i] = source[i];
        ++i;
    }
    destination[i] = '\0';
}

static void fviz_contour_filter_destroy(FVizObject* object)
{
    FVizContourFilter* filter = (FVizContourFilter*)object;
    fviz_release(filter->input);
    fviz_release(filter->output);
    fviz_free(filter->levels);
    filter->input = NULL;
    filter->output = NULL;
    filter->levels = NULL;
}

typedef struct FVizContourSegment
{
    FVizBool valid;
    FVizVec3 a;
    FVizVec3 b;
} FVizContourSegment;

typedef struct FVizContourRangeContext
{
    const FVizVec3* points;
    const uint32_t* indices;
    const void* scalars;
    FVizDataType scalar_type;
    const float* levels;
    FVizSize triangle_count;
    FVizContourSegment* segments;
} FVizContourRangeContext;

static float fviz_contour_scalar_value(const FVizContourRangeContext* context, FVizSize point_id)
{
    switch (context->scalar_type)
    {
        case FVIZ_DATA_INT8:
            return (float)((const int8_t*)context->scalars)[point_id];
        case FVIZ_DATA_UINT8:
            return (float)((const uint8_t*)context->scalars)[point_id];
        case FVIZ_DATA_INT16:
            return (float)((const int16_t*)context->scalars)[point_id];
        case FVIZ_DATA_UINT16:
            return (float)((const uint16_t*)context->scalars)[point_id];
        case FVIZ_DATA_INT32:
            return (float)((const int32_t*)context->scalars)[point_id];
        case FVIZ_DATA_UINT32:
            return (float)((const uint32_t*)context->scalars)[point_id];
        case FVIZ_DATA_INT64:
            return (float)((const int64_t*)context->scalars)[point_id];
        case FVIZ_DATA_UINT64:
            return (float)((const uint64_t*)context->scalars)[point_id];
        case FVIZ_DATA_FLOAT32:
            return ((const float*)context->scalars)[point_id];
        case FVIZ_DATA_FLOAT64:
            return (float)((const double*)context->scalars)[point_id];
        default:
            return 0.0f;
    }
}

static void fviz_contour_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizContourRangeContext* context = (FVizContourRangeContext*)user_data;
    FVizSize index;
    static const uint32_t edges[3][2] = {{0u, 1u}, {1u, 2u}, {2u, 0u}};
    for (index = begin; index < end; ++index)
    {
        const FVizSize level_id = index / context->triangle_count;
        const FVizSize triangle_id = index % context->triangle_count;
        FVizContourSegment* segment = &context->segments[index];
        const uint32_t ia = context->indices[triangle_id * 3u + 0u];
        const uint32_t ib = context->indices[triangle_id * 3u + 1u];
        const uint32_t ic = context->indices[triangle_id * 3u + 2u];
        const float values[3] = {fviz_contour_scalar_value(context, ia), fviz_contour_scalar_value(context, ib),
                                 fviz_contour_scalar_value(context, ic)};
        const FVizVec3 corners[3] = {context->points[ia], context->points[ib], context->points[ic]};
        uint32_t vertex_count = 0u;
        uint32_t edge;
        for (edge = 0u; edge < 3u; ++edge)
        {
            const uint32_t a = edges[edge][0];
            const uint32_t b = edges[edge][1];
            if ((values[a] >= context->levels[level_id]) != (values[b] >= context->levels[level_id]))
            {
                const float t = (context->levels[level_id] - values[a]) / (values[b] - values[a]);
                const FVizVec3 point =
                    fviz_vec3_add(corners[a], fviz_vec3_scale(fviz_vec3_sub(corners[b], corners[a]), t));
                if (vertex_count++ == 0u) segment->a = point;
                else
                    segment->b = point;
            }
        }
        segment->valid = vertex_count == 2u ? FVIZ_TRUE : FVIZ_FALSE;
    }
}

FVizResult fviz_contour_filter_create(const char* scalar_name, const float* levels, FVizSize level_count,
                                      FVizContourFilter** out_filter)
{
    FVizContourFilter* filter;
    if (out_filter == NULL || scalar_name == NULL || level_count == 0u || levels == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "contour filter requires scalar name and levels");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizContourFilter*)fviz_internal_object_allocate(sizeof(FVizContourFilter), &g_fviz_contour_filter_class,
                                                               NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->levels = (float*)fviz_alloc(level_count * sizeof(float));
    if (filter->levels == NULL)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    (void)memcpy(filter->levels, levels, level_count * sizeof(float));
    filter->level_count = level_count;
    fviz_contour_copy_scalar_name(filter->scalar_name, scalar_name);
    filter->input = NULL;
    filter->output = NULL;
    filter->input_mtime = 0u;
    filter->updated = FVIZ_FALSE;
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_contour_filter_set_input(FVizContourFilter* filter, const FVizPolyData* poly_data)
{
    if (filter == NULL || poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "filter and input must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (filter->input == poly_data) return FVIZ_OK;
    if (fviz_retain((FVizPolyData*)poly_data) == NULL) return fviz_last_error_code();
    fviz_release(filter->input);
    filter->input = (FVizPolyData*)poly_data;
    filter->updated = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

const FVizPolyData* fviz_contour_filter_const_input(const FVizContourFilter* filter)
{
    return filter != NULL ? filter->input : NULL;
}

FVizPolyData* fviz_contour_filter_output(FVizContourFilter* filter)
{
    return filter != NULL ? filter->output : NULL;
}

FVizSize fviz_contour_filter_level_count(const FVizContourFilter* filter)
{
    return filter != NULL ? filter->level_count : 0u;
}

static FVizResult fviz_contour_execute(FVizContourFilter* filter)
{
    const FVizPolyData* input = filter->input;
    const FVizDataArray* scalars;
    const FVizVec3* points;
    const uint32_t* indices;
    const void* scalar_data;
    FVizDataType scalar_type;
    FVizPolyData* output = NULL;
    FVizSize triangle_count;
    FVizSize level_id;
    FVizSize i;
    FVizContourSegment* segments = NULL;
    FVizContourRangeContext range_context;

    if (input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "contour filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (filter->scalar_name[0] == '\0')
    {
        scalars = fviz_poly_data_const_scalars(input);
    }
    else
    {
        scalars = fviz_attribute_set_const_get(fviz_poly_data_point_data((FVizPolyData*)input), filter->scalar_name);
    }
    if (scalars == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "contour scalar field not found on input");
        return FVIZ_ERROR_NOT_FOUND;
    }
    if (fviz_data_array_tuple_count(scalars) != fviz_poly_data_point_count(input) ||
        fviz_data_array_components(scalars) != 1u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                "contour scalar field must contain one component per input point");
        return FVIZ_ERROR_INVALID_STATE;
    }
    scalar_type = fviz_data_array_type(scalars);
    if (scalar_type < FVIZ_DATA_INT8 || scalar_type > FVIZ_DATA_FLOAT64)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "contour scalar type is unsupported");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    scalar_data = fviz_data_array_const_data((FVizDataArray*)scalars);
    points = fviz_poly_data_points(input);
    indices = fviz_poly_data_triangle_indices(input);
    triangle_count = fviz_poly_data_triangle_count(input);
    if (scalar_data == NULL || points == NULL || indices == NULL) return FVIZ_OK;

    if (fviz_poly_data_create(&output) != FVIZ_OK) return fviz_last_error_code();

    if (triangle_count > 0u && filter->level_count > 0u)
    {
        segments = (FVizContourSegment*)fviz_alloc(triangle_count * filter->level_count * sizeof(*segments));
        if (segments == NULL) goto fail;
        (void)memset(segments, 0, triangle_count * filter->level_count * sizeof(*segments));
        range_context.points = points;
        range_context.indices = indices;
        range_context.scalars = scalar_data;
        range_context.scalar_type = scalar_type;
        range_context.levels = filter->levels;
        range_context.triangle_count = triangle_count;
        range_context.segments = segments;
        if (fviz_parallel_for(0u, filter->level_count * triangle_count, 256u, fviz_contour_range, &range_context) !=
            FVIZ_OK)
            goto fail;
    }
    if (segments != NULL)
    {
        const FVizSize work_count = filter->level_count * triangle_count;
        FVizSize valid_count = 0u;
        FVizVec3* output_points = NULL;
        uint32_t* output_lines = NULL;
        float* output_levels = NULL;
        FVizDataArray* level_array = NULL;
        FVizSize point_bytes = 0u;
        FVizSize line_bytes = 0u;
        FVizSize level_bytes = 0u;
        FVizSize cursor = 0u;
        for (i = 0u; i < work_count; ++i)
            if (segments[i].valid != FVIZ_FALSE) ++valid_count;
        if (valid_count > (FVizSize)UINT32_MAX / 2u)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "contour output exceeds uint32 render-index capacity");
            goto fail;
        }
        if (valid_count != 0u)
        {
            if (fviz_size_multiply(valid_count * 2u, sizeof(*output_points), &point_bytes) != FVIZ_OK ||
                fviz_size_multiply(valid_count * 2u, sizeof(*output_lines), &line_bytes) != FVIZ_OK ||
                fviz_size_multiply(valid_count * 2u, sizeof(*output_levels), &level_bytes) != FVIZ_OK)
                goto fail;
            output_points = (FVizVec3*)fviz_alloc(point_bytes);
            output_lines = (uint32_t*)fviz_alloc(line_bytes);
            output_levels = (float*)fviz_alloc(level_bytes);
            if (output_points == NULL || output_lines == NULL || output_levels == NULL)
            {
                fviz_free(output_levels);
                fviz_free(output_lines);
                fviz_free(output_points);
                goto fail;
            }
            for (level_id = 0u; level_id < filter->level_count; ++level_id)
                for (i = 0u; i < triangle_count; ++i)
                {
                    const FVizContourSegment* segment = &segments[level_id * triangle_count + i];
                    if (segment->valid == FVIZ_FALSE) continue;
                    output_points[cursor * 2u + 0u] = segment->a;
                    output_points[cursor * 2u + 1u] = segment->b;
                    output_lines[cursor * 2u + 0u] = (uint32_t)(cursor * 2u + 0u);
                    output_lines[cursor * 2u + 1u] = (uint32_t)(cursor * 2u + 1u);
                    output_levels[cursor * 2u + 0u] = filter->levels[level_id];
                    output_levels[cursor * 2u + 1u] = filter->levels[level_id];
                    ++cursor;
                }
            if (fviz_poly_data_reserve(output, valid_count * 2u, 0u) != FVIZ_OK ||
                fviz_poly_data_add_points(output, output_points, valid_count * 2u, NULL) != FVIZ_OK ||
                fviz_poly_data_add_lines(output, output_lines, valid_count) != FVIZ_OK ||
                fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &level_array) != FVIZ_OK ||
                fviz_data_array_append_tuples(level_array, output_levels, valid_count * 2u) != FVIZ_OK ||
                fviz_attribute_set_add(fviz_poly_data_point_data(output), "contour_level", level_array) != FVIZ_OK)
            {
                fviz_release(level_array);
                fviz_free(output_levels);
                fviz_free(output_lines);
                fviz_free(output_points);
                goto fail;
            }
            fviz_release(level_array);
            fviz_free(output_levels);
            fviz_free(output_lines);
            fviz_free(output_points);
        }
        fviz_free(segments);
        segments = NULL;
    }

    fviz_release(filter->output);
    filter->output = output;
    filter->updated = FVIZ_TRUE;
    return FVIZ_OK;
fail:
    fviz_free(segments);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_contour_filter_update(FVizContourFilter* filter)
{
    FVizMTime input_mtime;
    if (filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (filter->input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    input_mtime = fviz_object_mtime((const FVizObject*)filter->input);
    if (filter->updated == FVIZ_TRUE && filter->input_mtime == input_mtime)
    {
        return FVIZ_OK;
    }
    {
        FVizResult result = fviz_contour_execute(filter);
        if (result != FVIZ_OK) return result;
    }
    filter->input_mtime = input_mtime;
    return FVIZ_OK;
}
