#include <limits.h>
#include <string.h>

#include <FViz/Algorithms/FVizRectilinearGridExtractFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizRectilinearGridExtractFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
};

static void fviz_rectilinear_extract_destroy(FVizObject* object)
{
    FVizRectilinearGridExtractFilter* filter = (FVizRectilinearGridExtractFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_rectilinear_extract_class = {
    FVIZ_TYPE_RECTILINEAR_GRID_EXTRACT_FILTER, "FVizRectilinearGridExtractFilter", &g_fviz_object_class,
    fviz_rectilinear_extract_destroy, NULL};

static FVizMTime fviz_rectilinear_extract_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static int64_t fviz_rectilinear_extract_saturating_sub(int64_t value, uint32_t amount)
{
    if (value < INT64_MIN + (int64_t)amount) return INT64_MIN;
    return value - (int64_t)amount;
}

static int64_t fviz_rectilinear_extract_saturating_add(int64_t value, uint32_t amount)
{
    if (value > INT64_MAX - (int64_t)amount) return INT64_MAX;
    return value + (int64_t)amount;
}

static FVizResult fviz_rectilinear_extract_map_request(FVizAlgorithm* algorithm, uint32_t input_port,
                                                       uint32_t connection, const FVizPipelineRequestInfo* downstream,
                                                       FVizPipelineRequestInfo* upstream, void* state)
{
    uint32_t axis;
    (void)algorithm;
    (void)connection;
    (void)state;
    if (input_port != 0u) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (downstream->has_extent == FVIZ_FALSE || downstream->ghost_levels == 0u) return FVIZ_OK;
    for (axis = 0u; axis < 3u; ++axis)
    {
        upstream->extent[axis * 2u] =
            fviz_rectilinear_extract_saturating_sub(downstream->extent[axis * 2u], downstream->ghost_levels);
        upstream->extent[axis * 2u + 1u] =
            fviz_rectilinear_extract_saturating_add(downstream->extent[axis * 2u + 1u], downstream->ghost_levels);
    }
    upstream->has_extent = FVIZ_TRUE;
    return FVIZ_OK;
}

static FVizBool fviz_rectilinear_extract_intersect_extent(const int64_t requested[6], const int64_t available[6],
                                                          int64_t out_extent[6])
{
    uint32_t axis;
    for (axis = 0u; axis < 3u; ++axis)
    {
        const int64_t minimum =
            requested[axis * 2u] > available[axis * 2u] ? requested[axis * 2u] : available[axis * 2u];
        const int64_t maximum = requested[axis * 2u + 1u] < available[axis * 2u + 1u] ? requested[axis * 2u + 1u]
                                                                                      : available[axis * 2u + 1u];
        out_extent[axis * 2u] = minimum;
        out_extent[axis * 2u + 1u] = maximum;
        if (maximum < minimum) return FVIZ_FALSE;
    }
    return FVIZ_TRUE;
}

static FVizResult fviz_rectilinear_extract_copy_field_attributes(const FVizAttributeSet* source,
                                                                 FVizAttributeSet* destination)
{
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        const FVizDataArray* array = fviz_attribute_set_const_array_at(source, i);
        FVizDataArray* copy = NULL;
        FVizAttributeRole role;
        if (fviz_data_array_deep_copy(array, &copy) != FVIZ_OK ||
            fviz_attribute_set_add(destination, name, copy) != FVIZ_OK)
        {
            fviz_release(copy);
            return fviz_last_error_code();
        }
        fviz_release(copy);
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(destination, role, name);
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_rectilinear_extract_copy_indexed_attributes(const FVizAttributeSet* source,
                                                                   FVizAttributeSet* destination,
                                                                   const FVizId* source_ids, FVizSize output_count,
                                                                   FVizSize expected_source_count)
{
    FVizSize array_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source, array_index);
        const FVizDataArray* in_array = fviz_attribute_set_const_array_at(source, array_index);
        FVizDataArray* out_array = NULL;
        FVizAttributeRole role;
        FVizSize i;
        if (fviz_data_array_tuple_count(in_array) != expected_source_count) continue;
        if (fviz_data_array_create(fviz_data_array_type(in_array), fviz_data_array_components(in_array), &out_array) !=
                FVIZ_OK ||
            fviz_data_array_resize(out_array, output_count) != FVIZ_OK)
            goto fail;
        for (i = 0u; i < output_count; ++i)
        {
            const FVizSize source_id = (FVizSize)source_ids[i];
            const void* in_tuple = fviz_data_array_const_tuple(in_array, source_id);
            void* out_tuple = fviz_data_array_tuple(out_array, i);
            if (in_tuple == NULL || out_tuple == NULL) goto fail;
            (void)memcpy(out_tuple, in_tuple, fviz_data_array_tuple_stride(in_array));
        }
        if (fviz_attribute_set_add(destination, name, out_array) != FVIZ_OK) goto fail;
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(destination, role, name);
        }
        fviz_release(out_array);
        continue;
    fail:
        fviz_release(out_array);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_rectilinear_extract_copy_axis(const FVizRectilinearGrid* input, FVizRectilinearGrid* output,
                                                     uint32_t axis, const int64_t input_extent[6],
                                                     const int64_t output_extent[6])
{
    const FVizDataArray* input_coordinates = fviz_rectilinear_grid_const_coordinates(input, axis);
    FVizDataArray* output_coordinates = NULL;
    const int64_t first = output_extent[axis * 2u] - input_extent[axis * 2u];
    const int64_t last = output_extent[axis * 2u + 1u] - input_extent[axis * 2u];
    FVizSize count;
    FVizSize tuple_stride;
    const unsigned char* source;
    FVizResult result;
    if (input_coordinates == NULL || first < 0 || last < first) return FVIZ_ERROR_INVALID_STATE;
    count = (FVizSize)(last - first + 1);
    tuple_stride = fviz_data_array_tuple_stride(input_coordinates);
    source = (const unsigned char*)fviz_data_array_const_data(input_coordinates) + (FVizSize)first * tuple_stride;
    result = fviz_data_array_create(fviz_data_array_type(input_coordinates), 1u, &output_coordinates);
    if (result == FVIZ_OK) result = fviz_data_array_resize(output_coordinates, count);
    if (result == FVIZ_OK && count != 0u) result = fviz_data_array_set_tuples(output_coordinates, 0u, source, count);
    if (result == FVIZ_OK) result = fviz_rectilinear_grid_set_coordinates(output, axis, output_coordinates);
    fviz_release(output_coordinates);
    return result;
}

static FVizResult fviz_rectilinear_extract_process_request(FVizAlgorithm* algorithm,
                                                           const FVizPipelineRequestInfo* request, void* state)
{
    FVizRectilinearGrid* input;
    FVizRectilinearGrid* output = NULL;
    int64_t input_extent[6], output_extent[6];
    FVizSize point_count, cell_count, p = 0u, c = 0u;
    FVizId* point_source_ids = NULL;
    FVizId* cell_source_ids = NULL;
    int64_t i, j, k;
    FVizResult result = FVIZ_OK;
    (void)state;
    input = (FVizRectilinearGrid*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (request->type == FVIZ_PIPELINE_REQUEST_INFORMATION)
    {
        if (input != NULL)
        {
            fviz_rectilinear_grid_extent(input, input_extent);
            if (fviz_rectilinear_grid_point_count(input) != 0u)
                (void)fviz_algorithm_set_output_whole_extent(algorithm, request->requested_output_port, input_extent);
        }
        return FVIZ_OK;
    }
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (input == NULL || fviz_rectilinear_grid_validate(input) != FVIZ_OK)
    {
        if (input == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "rectilinear extract filter has no input");
        return input == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
    }
    fviz_rectilinear_grid_extent(input, input_extent);
    if (request->has_extent != FVIZ_FALSE)
    {
        uint32_t axis;
        if ((request->flags & FVIZ_PIPELINE_REQUEST_FLAG_EXACT_EXTENT) != 0u)
        {
            for (axis = 0u; axis < 3u; ++axis)
            {
                if (request->extent[axis * 2u] < input_extent[axis * 2u] ||
                    request->extent[axis * 2u + 1u] > input_extent[axis * 2u + 1u])
                {
                    fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                            "exact rectilinear extent lies outside available input");
                    return FVIZ_ERROR_INVALID_ARGUMENT;
                }
            }
        }
        if (fviz_rectilinear_extract_intersect_extent(request->extent, input_extent, output_extent) == FVIZ_FALSE)
        {
            static const int64_t empty[6] = {0, -1, 0, -1, 0, -1};
            (void)memcpy(output_extent, empty, sizeof(empty));
        }
    }
    else
    {
        (void)memcpy(output_extent, input_extent, sizeof(output_extent));
    }

    if (fviz_rectilinear_grid_create(&output) != FVIZ_OK ||
        fviz_rectilinear_grid_set_extent(output, output_extent) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto done;
    }
    point_count = fviz_rectilinear_grid_point_count(output);
    cell_count = fviz_rectilinear_grid_cell_count(output);
    if (point_count != 0u)
    {
        FVizSize bytes;
        if (fviz_rectilinear_extract_copy_axis(input, output, 0u, input_extent, output_extent) != FVIZ_OK ||
            fviz_rectilinear_extract_copy_axis(input, output, 1u, input_extent, output_extent) != FVIZ_OK ||
            fviz_rectilinear_extract_copy_axis(input, output, 2u, input_extent, output_extent) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
        if (fviz_size_multiply(point_count, sizeof(FVizId), &bytes) != FVIZ_OK)
        {
            result = FVIZ_ERROR_OVERFLOW;
            goto done;
        }
        point_source_ids = (FVizId*)fviz_alloc(bytes);
        if (point_source_ids == NULL)
        {
            result = fviz_last_error_code();
            goto done;
        }
        for (k = output_extent[4]; k <= output_extent[5]; ++k)
            for (j = output_extent[2]; j <= output_extent[3]; ++j)
                for (i = output_extent[0]; i <= output_extent[1]; ++i)
                {
                    if (fviz_rectilinear_grid_point_id(input, i, j, k, &point_source_ids[p++]) != FVIZ_OK)
                    {
                        result = fviz_last_error_code();
                        goto done;
                    }
                }
    }
    if (cell_count != 0u)
    {
        FVizSize bytes;
        if (fviz_size_multiply(cell_count, sizeof(FVizId), &bytes) != FVIZ_OK)
        {
            result = FVIZ_ERROR_OVERFLOW;
            goto done;
        }
        cell_source_ids = (FVizId*)fviz_alloc(bytes);
        if (cell_source_ids == NULL)
        {
            result = fviz_last_error_code();
            goto done;
        }
        for (k = output_extent[4]; k < (output_extent[5] > output_extent[4] ? output_extent[5] : output_extent[4] + 1);
             ++k)
            for (j = output_extent[2];
                 j < (output_extent[3] > output_extent[2] ? output_extent[3] : output_extent[2] + 1); ++j)
                for (i = output_extent[0];
                     i < (output_extent[1] > output_extent[0] ? output_extent[1] : output_extent[0] + 1); ++i)
                {
                    if (fviz_rectilinear_grid_cell_id(input, i, j, k, &cell_source_ids[c++]) != FVIZ_OK)
                    {
                        result = fviz_last_error_code();
                        goto done;
                    }
                }
    }
    if (fviz_rectilinear_extract_copy_indexed_attributes(
            fviz_rectilinear_grid_const_point_data(input), fviz_rectilinear_grid_point_data(output), point_source_ids,
            point_count, fviz_rectilinear_grid_point_count(input)) != FVIZ_OK ||
        fviz_rectilinear_extract_copy_indexed_attributes(
            fviz_rectilinear_grid_const_cell_data(input), fviz_rectilinear_grid_cell_data(output), cell_source_ids,
            cell_count, fviz_rectilinear_grid_cell_count(input)) != FVIZ_OK ||
        fviz_rectilinear_extract_copy_field_attributes(fviz_rectilinear_grid_const_field_data(input),
                                                       fviz_rectilinear_grid_field_data(output)) != FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto done;
    }
    if (fviz_rectilinear_grid_point_count(input) != 0u)
        (void)fviz_algorithm_set_output_whole_extent(algorithm, request->requested_output_port, input_extent);
done:
    fviz_free(cell_source_ids);
    fviz_free(point_source_ids);
    fviz_release(output);
    return result;
}

FVizResult fviz_rectilinear_grid_extract_filter_create(FVizRectilinearGridExtractFilter** out_filter)
{
    FVizRectilinearGridExtractFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizRectilinearGridExtractFilter*)fviz_internal_object_allocate(sizeof(*filter),
                                                                              &g_fviz_rectilinear_extract_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_rectilinear_extract_process_request;
    callbacks.get_state_mtime = fviz_rectilinear_extract_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    callbacks.map_input_request = fviz_rectilinear_extract_map_request;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_RECTILINEAR_GRID, FVIZ_FALSE,
                                            FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_RECTILINEAR_GRID) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_rectilinear_grid_extract_filter_set_input_data(FVizRectilinearGridExtractFilter* filter,
                                                               FVizRectilinearGrid* input)
{
    int64_t extent[6];
    FVizResult result;
    if (filter == NULL || input == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input);
    if (result == FVIZ_OK && fviz_rectilinear_grid_point_count(input) != 0u)
    {
        fviz_rectilinear_grid_extent(input, extent);
        (void)fviz_algorithm_set_output_whole_extent(filter->algorithm, 0u, extent);
    }
    return result;
}

FVizResult fviz_rectilinear_grid_extract_filter_set_input_connection(FVizRectilinearGridExtractFilter* filter,
                                                                     FVizAlgorithmOutput* input)
{
    return filter != NULL && input != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                                           : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_rectilinear_grid_extract_filter_algorithm(FVizRectilinearGridExtractFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_rectilinear_grid_extract_filter_output_port(FVizRectilinearGridExtractFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizRectilinearGrid* fviz_rectilinear_grid_extract_filter_output(FVizRectilinearGridExtractFilter* filter)
{
    return filter != NULL ? (FVizRectilinearGrid*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_rectilinear_grid_extract_filter_update(FVizRectilinearGridExtractFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_rectilinear_grid_extract_filter_update_extent(FVizRectilinearGridExtractFilter* filter,
                                                              const int64_t extent[6], uint32_t ghost_levels)
{
    return filter != NULL && extent != NULL
               ? fviz_executive_update_extent(fviz_algorithm_executive(filter->algorithm), 0u, extent, ghost_levels)
               : FVIZ_ERROR_INVALID_ARGUMENT;
}
