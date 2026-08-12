#include <string.h>

#include <FViz/Algorithms/FVizContourFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Algorithms/FVizContourFilterPrivate.h>

static void fviz_contour_filter_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_contour_filter_class = {
    FVIZ_TYPE_CONTOUR_FILTER, "FVizContourFilter", &g_fviz_object_class,
    fviz_contour_filter_destroy, NULL
};

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

FVizResult fviz_contour_filter_create(
    const char* scalar_name,
    const float* levels,
    FVizSize level_count,
    FVizContourFilter** out_filter)
{
    FVizContourFilter* filter;
    if (out_filter == NULL || scalar_name == NULL ||
        level_count == 0u || levels == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "contour filter requires scalar name and levels");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizContourFilter*)fviz_internal_object_allocate(sizeof(FVizContourFilter), &g_fviz_contour_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->levels = (float*)fviz_alloc(level_count * sizeof(float));
    if (filter->levels == NULL)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    (void)memcpy(filter->levels, levels, level_count * sizeof(float));
    filter->level_count = level_count;
    (void)strncpy(filter->scalar_name, scalar_name, sizeof(filter->scalar_name) - 1u);
    filter->scalar_name[sizeof(filter->scalar_name) - 1u] = '\0';
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
    const float* scalar_data;
    FVizPolyData* output = NULL;
    FVizSize triangle_count;
    FVizSize level_id;
    FVizSize i;

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
    if (fviz_data_array_tuple_count(scalars) != fviz_poly_data_point_count(input))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "contour scalar count does not match point count");
        return FVIZ_ERROR_INVALID_STATE;
    }
    scalar_data = (const float*)fviz_data_array_const_data((FVizDataArray*)scalars);
    points = fviz_poly_data_points(input);
    indices = fviz_poly_data_triangle_indices(input);
    triangle_count = fviz_poly_data_triangle_count(input);
    if (scalar_data == NULL || points == NULL || indices == NULL) return FVIZ_OK;

    if (fviz_poly_data_create(&output) != FVIZ_OK) return fviz_last_error_code();

    for (level_id = 0u; level_id < filter->level_count; ++level_id)
    {
        const float level = filter->levels[level_id];
        for (i = 0u; i < triangle_count; ++i)
        {
            const uint32_t ia = indices[i * 3u + 0u];
            const uint32_t ib = indices[i * 3u + 1u];
            const uint32_t ic = indices[i * 3u + 2u];
            const float sa = scalar_data[ia];
            const float sb = scalar_data[ib];
            const float sc = scalar_data[ic];
            uint32_t vertex_ids[4];
            FVizSize vertex_count = 0u;
            const FVizVec3 corners[3] = {points[ia], points[ib], points[ic]};
            const float values[3] = {sa, sb, sc};
            const uint32_t ids[3] = {ia, ib, ic};
            FVizSize edge;
            static const FVizSize edges[3][2] = {{0, 1}, {1, 2}, {2, 0}};

            for (edge = 0u; edge < 3u; ++edge)
            {
                const FVizSize a = edges[edge][0];
                const FVizSize b = edges[edge][1];
                const float va = values[a];
                const float vb = values[b];
                const FVizBool sa_above = va >= level;
                const FVizBool sb_above = vb >= level;
                if (sa_above != sb_above)
                {
                    const float t = (level - va) / (vb - va);
                    FVizVec3 intersection;
                    uint32_t id;
                    intersection = fviz_vec3_add(corners[a], fviz_vec3_scale(fviz_vec3_sub(corners[b], corners[a]), t));
                    if (fviz_poly_data_add_point(output, intersection, &id) != FVIZ_OK) goto fail;
                    vertex_ids[vertex_count++] = id;
                    (void)ids;
                }
            }
            if (vertex_count == 2u)
            {
                if (fviz_poly_data_add_line(output, vertex_ids[0], vertex_ids[1]) != FVIZ_OK) goto fail;
            }
            else if (vertex_count == 4u)
            {
                if (fviz_poly_data_add_line(output, vertex_ids[0], vertex_ids[1]) != FVIZ_OK ||
                    fviz_poly_data_add_line(output, vertex_ids[2], vertex_ids[3]) != FVIZ_OK) goto fail;
            }
        }
    }

    fviz_release(filter->output);
    filter->output = output;
    filter->updated = FVIZ_TRUE;
    return FVIZ_OK;
fail:
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
