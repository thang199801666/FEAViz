#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Algorithms/FVizMeshProcessingFilters.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Parallel/FVizParallel.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizSmoothPolyDataFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    uint32_t iterations;
    double relaxation_factor;
    FVizBool boundary_smoothing;
};

typedef struct FVizSmoothRangeContext
{
    const FVizVec3* input;
    FVizVec3* output;
    const FVizSize* offsets;
    const uint32_t* neighbors;
    const uint8_t* boundary;
    float relaxation;
    FVizBool boundary_smoothing;
} FVizSmoothRangeContext;

static FVizMTime fviz_mesh_processing_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static void fviz_smooth_poly_data_filter_destroy(FVizObject* object)
{
    FVizSmoothPolyDataFilter* filter = (FVizSmoothPolyDataFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_smooth_poly_data_filter_class = {
    FVIZ_TYPE_SMOOTH_POLY_DATA_FILTER,
    "FVizSmoothPolyDataFilter",
    &g_fviz_object_class,
    fviz_smooth_poly_data_filter_destroy,
    NULL
};

static uint64_t fviz_mesh_edge_key(uint32_t a, uint32_t b)
{
    const uint32_t lo = a < b ? a : b;
    const uint32_t hi = a < b ? b : a;
    return ((uint64_t)lo << 32u) | (uint64_t)hi;
}

static int fviz_compare_u64(const void* lhs, const void* rhs)
{
    const uint64_t a = *(const uint64_t*)lhs;
    const uint64_t b = *(const uint64_t*)rhs;
    return a < b ? -1 : (a > b ? 1 : 0);
}

static FVizResult fviz_smooth_build_adjacency(
    const FVizPolyData* input,
    FVizSize** out_offsets,
    uint32_t** out_neighbors,
    uint8_t** out_boundary)
{
    const FVizSize point_count = fviz_poly_data_point_count(input);
    const FVizSize triangle_count = fviz_poly_data_triangle_count(input);
    const uint32_t* triangles = fviz_poly_data_triangle_indices(input);
    FVizSize* degree = NULL;
    FVizSize* offsets = NULL;
    FVizSize* cursor = NULL;
    uint32_t* neighbors = NULL;
    uint8_t* boundary = NULL;
    uint64_t* edges = NULL;
    FVizSize adjacency_count = 0u;
    FVizSize edge_count = 0u;
    FVizSize i;
    FVizSize bytes;
    if (out_offsets == NULL || out_neighbors == NULL || out_boundary == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_offsets = NULL;
    *out_neighbors = NULL;
    *out_boundary = NULL;
    if (triangle_count == 0u || triangles == NULL)
    {
        fviz_internal_set_error(
            FVIZ_ERROR_INVALID_STATE,
            "smooth poly data requires render-ready triangle topology; run TriangleFilter first for general PolyData");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_size_multiply(point_count, sizeof(*degree), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    degree = (FVizSize*)fviz_alloc(bytes);
    cursor = (FVizSize*)fviz_alloc(bytes);
    boundary = (uint8_t*)fviz_alloc(point_count);
    if (point_count == (FVizSize)-1 ||
        fviz_size_multiply(point_count + 1u, sizeof(*offsets), &bytes) != FVIZ_OK) goto overflow;
    offsets = (FVizSize*)fviz_alloc(bytes);
    if (degree == NULL || cursor == NULL || boundary == NULL || offsets == NULL) goto oom;
    memset(degree, 0, point_count * sizeof(*degree));
    memset(boundary, 0, point_count);
    if (triangle_count > (FVizSize)-1 / 3u) goto overflow;
    edge_count = triangle_count * 3u;
    if (fviz_size_multiply(edge_count, sizeof(*edges), &bytes) != FVIZ_OK) goto overflow;
    edges = (uint64_t*)fviz_alloc(bytes);
    if (edges == NULL) goto oom;
    for (i = 0u; i < triangle_count; ++i)
    {
        const uint32_t a = triangles[i * 3u + 0u];
        const uint32_t b = triangles[i * 3u + 1u];
        const uint32_t c = triangles[i * 3u + 2u];
        if (a >= point_count || b >= point_count || c >= point_count) goto invalid;
        if (degree[a] > (FVizSize)-3 || degree[b] > (FVizSize)-3 || degree[c] > (FVizSize)-3)
            goto overflow;
        degree[a] += 2u;
        degree[b] += 2u;
        degree[c] += 2u;
        edges[i * 3u + 0u] = fviz_mesh_edge_key(a, b);
        edges[i * 3u + 1u] = fviz_mesh_edge_key(b, c);
        edges[i * 3u + 2u] = fviz_mesh_edge_key(c, a);
    }
    offsets[0] = 0u;
    for (i = 0u; i < point_count; ++i)
    {
        if (degree[i] > (FVizSize)-1 - offsets[i]) goto overflow;
        offsets[i + 1u] = offsets[i] + degree[i];
    }
    adjacency_count = offsets[point_count];
    if (fviz_size_multiply(adjacency_count, sizeof(*neighbors), &bytes) != FVIZ_OK) goto overflow;
    neighbors = (uint32_t*)fviz_alloc(bytes);
    if (neighbors == NULL && adjacency_count != 0u) goto oom;
    memcpy(cursor, offsets, point_count * sizeof(*cursor));
    for (i = 0u; i < triangle_count; ++i)
    {
        const uint32_t a = triangles[i * 3u + 0u];
        const uint32_t b = triangles[i * 3u + 1u];
        const uint32_t c = triangles[i * 3u + 2u];
        neighbors[cursor[a]++] = b;
        neighbors[cursor[a]++] = c;
        neighbors[cursor[b]++] = a;
        neighbors[cursor[b]++] = c;
        neighbors[cursor[c]++] = a;
        neighbors[cursor[c]++] = b;
    }
    qsort(edges, edge_count, sizeof(*edges), fviz_compare_u64);
    for (i = 0u; i < edge_count;)
    {
        FVizSize j = i + 1u;
        while (j < edge_count && edges[j] == edges[i]) ++j;
        if (j - i == 1u)
        {
            const uint32_t a = (uint32_t)(edges[i] >> 32u);
            const uint32_t b = (uint32_t)(edges[i] & UINT32_MAX);
            boundary[a] = 1u;
            boundary[b] = 1u;
        }
        i = j;
    }
    fviz_free(edges);
    fviz_free(cursor);
    fviz_free(degree);
    *out_offsets = offsets;
    *out_neighbors = neighbors;
    *out_boundary = boundary;
    return FVIZ_OK;
invalid:
    fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "triangle topology references a missing point");
    goto fail;
overflow:
    fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "smooth adjacency size overflow");
    goto fail;
oom:
    fviz_internal_set_error(FVIZ_ERROR_OUT_OF_MEMORY, "could not allocate smoothing adjacency");
fail:
    fviz_free(edges);
    fviz_free(neighbors);
    fviz_free(offsets);
    fviz_free(boundary);
    fviz_free(cursor);
    fviz_free(degree);
    return fviz_last_error_code();
}

static void fviz_smooth_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizSmoothRangeContext* context = (FVizSmoothRangeContext*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i)
    {
        const FVizSize start = context->offsets[i];
        const FVizSize finish = context->offsets[i + 1u];
        FVizVec3 sum = fviz_vec3(0.0f, 0.0f, 0.0f);
        FVizSize j;
        if ((context->boundary_smoothing == FVIZ_FALSE && context->boundary[i] != 0u) || start == finish)
        {
            context->output[i] = context->input[i];
            continue;
        }
        for (j = start; j < finish; ++j)
            sum = fviz_vec3_add(sum, context->input[context->neighbors[j]]);
        sum = fviz_vec3_scale(sum, 1.0f / (float)(finish - start));
        context->output[i] = fviz_vec3_add(
            context->input[i],
            fviz_vec3_scale(fviz_vec3_sub(sum, context->input[i]), context->relaxation));
    }
}

static FVizResult fviz_smooth_poly_data_filter_process_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* state)
{
    FVizSmoothPolyDataFilter* filter = (FVizSmoothPolyDataFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    FVizSize* offsets = NULL;
    uint32_t* neighbors = NULL;
    uint8_t* boundary = NULL;
    FVizVec3* current = NULL;
    FVizVec3* next = NULL;
    FVizSize point_count;
    FVizSize bytes;
    uint32_t iteration;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL || fviz_poly_data_validate(input) != FVIZ_OK)
    {
        if (input == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "smooth poly data filter has no input");
        return input == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
    }
    point_count = fviz_poly_data_point_count(input);
    if (fviz_poly_data_deep_copy(input, &output) != FVIZ_OK) goto fail;
    if (point_count == 0u || filter->iterations == 0u)
    {
        if (fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
            goto fail;
        fviz_release(output);
        return FVIZ_OK;
    }
    if (fviz_smooth_build_adjacency(input, &offsets, &neighbors, &boundary) != FVIZ_OK) goto fail;
    if (fviz_size_multiply(point_count, sizeof(*current), &bytes) != FVIZ_OK) goto fail;
    current = (FVizVec3*)fviz_alloc(bytes);
    next = (FVizVec3*)fviz_alloc(bytes);
    if (current == NULL || next == NULL) goto fail;
    memcpy(current, fviz_poly_data_points(input), bytes);
    for (iteration = 0u; iteration < filter->iterations; ++iteration)
    {
        FVizSmoothRangeContext context;
        FVizVec3* swap;
        context.input = current;
        context.output = next;
        context.offsets = offsets;
        context.neighbors = neighbors;
        context.boundary = boundary;
        context.relaxation = (float)filter->relaxation_factor;
        context.boundary_smoothing = filter->boundary_smoothing;
        if (fviz_parallel_for(0u, point_count, 4096u, fviz_smooth_range, &context) != FVIZ_OK) goto fail;
        swap = current;
        current = next;
        next = swap;
        if (fviz_algorithm_report_progress(
                algorithm, (double)(iteration + 1u) / (double)filter->iterations) != FVIZ_OK)
            goto fail;
        if (fviz_algorithm_abort_requested(algorithm) != FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_CANCELLED, "smooth poly data filter was aborted");
            goto fail;
        }
    }
    if (fviz_poly_data_set_points(output, current, point_count) != FVIZ_OK) goto fail;
    if (fviz_poly_data_triangle_count(output) != 0u && fviz_poly_data_compute_normals(output) != FVIZ_OK) goto fail;
    if (fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK) goto fail;
    fviz_free(next);
    fviz_free(current);
    fviz_free(boundary);
    fviz_free(neighbors);
    fviz_free(offsets);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_free(next);
    fviz_free(current);
    fviz_free(boundary);
    fviz_free(neighbors);
    fviz_free(offsets);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_smooth_poly_data_filter_create(FVizSmoothPolyDataFilter** out_filter)
{
    FVizSmoothPolyDataFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizSmoothPolyDataFilter*)fviz_internal_object_allocate(
        sizeof(*filter), &g_fviz_smooth_poly_data_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->iterations = 20u;
    filter->relaxation_factor = 0.01;
    filter->boundary_smoothing = FVIZ_TRUE;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_smooth_poly_data_filter_process_request;
    callbacks.get_state_mtime = fviz_mesh_processing_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_smooth_poly_data_filter_set_iterations(FVizSmoothPolyDataFilter* filter, uint32_t iterations)
{
    if (filter == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (iterations > 100000u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "smoothing iteration count is unreasonably large");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (filter->iterations != iterations)
    {
        filter->iterations = iterations;
        fviz_object_modified((FVizObject*)filter);
    }
    return FVIZ_OK;
}

uint32_t fviz_smooth_poly_data_filter_iterations(const FVizSmoothPolyDataFilter* filter)
{
    return filter != NULL ? filter->iterations : 0u;
}

FVizResult fviz_smooth_poly_data_filter_set_relaxation_factor(FVizSmoothPolyDataFilter* filter, double factor)
{
    if (filter == NULL || factor < 0.0 || factor > 1.0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "relaxation factor must be in [0, 1]");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (filter->relaxation_factor != factor)
    {
        filter->relaxation_factor = factor;
        fviz_object_modified((FVizObject*)filter);
    }
    return FVIZ_OK;
}

double fviz_smooth_poly_data_filter_relaxation_factor(const FVizSmoothPolyDataFilter* filter)
{
    return filter != NULL ? filter->relaxation_factor : 0.0;
}

void fviz_smooth_poly_data_filter_set_boundary_smoothing(FVizSmoothPolyDataFilter* filter, FVizBool enabled)
{
    if (filter != NULL && filter->boundary_smoothing != enabled)
    {
        filter->boundary_smoothing = enabled;
        fviz_object_modified((FVizObject*)filter);
    }
}

FVizBool fviz_smooth_poly_data_filter_boundary_smoothing(const FVizSmoothPolyDataFilter* filter)
{
    return filter != NULL ? filter->boundary_smoothing : FVIZ_FALSE;
}

FVizResult fviz_smooth_poly_data_filter_set_input_data(FVizSmoothPolyDataFilter* filter, FVizPolyData* input)
{
    return filter != NULL
        ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
        : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_smooth_poly_data_filter_set_input_connection(FVizSmoothPolyDataFilter* filter, FVizAlgorithmOutput* input)
{
    return filter != NULL
        ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
        : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_smooth_poly_data_filter_algorithm(FVizSmoothPolyDataFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_smooth_poly_data_filter_output_port(FVizSmoothPolyDataFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_smooth_poly_data_filter_output(FVizSmoothPolyDataFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_smooth_poly_data_filter_update(FVizSmoothPolyDataFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}

/* ------------------------------------------------------------------------- */
/* DecimatePolyData: deterministic vertex clustering                         */

#include <math.h>

typedef struct FVizDecimateTriRecord
{
    uint32_t a, b, c;
    uint32_t k0, k1, k2;
    FVizSize source;
} FVizDecimateTriRecord;

struct FVizDecimatePolyDataFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    double target_reduction;
};

static void fviz_decimate_poly_data_filter_destroy(FVizObject* object)
{
    FVizDecimatePolyDataFilter* filter = (FVizDecimatePolyDataFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_decimate_poly_data_filter_class = {
    FVIZ_TYPE_DECIMATE_POLY_DATA_FILTER,
    "FVizDecimatePolyDataFilter",
    &g_fviz_object_class,
    fviz_decimate_poly_data_filter_destroy,
    NULL
};

static int fviz_decimate_compare_tri(const void* lhs, const void* rhs)
{
    const FVizDecimateTriRecord* a = (const FVizDecimateTriRecord*)lhs;
    const FVizDecimateTriRecord* b = (const FVizDecimateTriRecord*)rhs;
    if (a->k0 != b->k0) return a->k0 < b->k0 ? -1 : 1;
    if (a->k1 != b->k1) return a->k1 < b->k1 ? -1 : 1;
    if (a->k2 != b->k2) return a->k2 < b->k2 ? -1 : 1;
    return a->source < b->source ? -1 : (a->source > b->source ? 1 : 0);
}

static void fviz_decimate_sort3(uint32_t a, uint32_t b, uint32_t c, uint32_t* k0, uint32_t* k1, uint32_t* k2)
{
    uint32_t x = a, y = b, z = c, t;
    if (x > y) { t = x; x = y; y = t; }
    if (y > z) { t = y; y = z; z = t; }
    if (x > y) { t = x; x = y; y = t; }
    *k0 = x; *k1 = y; *k2 = z;
}

static FVizResult fviz_decimate_copy_clustered_point_data(
    const FVizPolyData* input,
    FVizPolyData* output,
    const uint32_t* mapping,
    const FVizSize* cluster_counts,
    FVizSize cluster_count)
{
    const FVizAttributeSet* source_set = fviz_poly_data_const_point_data(input);
    FVizSize array_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source_set); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source_set, array_index);
        const FVizDataArray* source = fviz_attribute_set_const_array_at(source_set, array_index);
        const uint32_t components = fviz_data_array_components(source);
        FVizDataArray* destination = NULL;
        double* sums = NULL;
        FVizSize sum_count;
        FVizSize sum_bytes;
        FVizSize i;
        FVizAttributeRole role;
        if (fviz_data_array_tuple_count(source) != fviz_poly_data_point_count(input)) continue;
        if (cluster_count != 0u && components > (FVizSize)-1 / cluster_count) return FVIZ_ERROR_OVERFLOW;
        sum_count = cluster_count * components;
        if (fviz_size_multiply(sum_count, sizeof(*sums), &sum_bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
        sums = (double*)fviz_alloc(sum_bytes);
        if (sums == NULL && sum_count != 0u) return fviz_last_error_code();
        if (sum_bytes != 0u) memset(sums, 0, sum_bytes);
        for (i = 0u; i < fviz_poly_data_point_count(input); ++i)
        {
            uint32_t component;
            const FVizSize cluster = mapping[i];
            for (component = 0u; component < components; ++component)
            {
                double value = 0.0;
                if (fviz_data_array_get_component(source, i, component, &value) != FVIZ_OK)
                {
                    fviz_free(sums);
                    return fviz_last_error_code();
                }
                sums[cluster * components + component] += value;
            }
        }
        if (fviz_data_array_create(fviz_data_array_type(source), components, &destination) != FVIZ_OK ||
            fviz_data_array_resize(destination, cluster_count) != FVIZ_OK)
        {
            fviz_free(sums);
            fviz_release(destination);
            return fviz_last_error_code();
        }
        for (i = 0u; i < cluster_count; ++i)
        {
            uint32_t component;
            for (component = 0u; component < components; ++component)
            {
                const double value = sums[i * components + component] / (double)cluster_counts[i];
                if (fviz_data_array_set_component(destination, i, component, value) != FVIZ_OK)
                {
                    fviz_free(sums);
                    fviz_release(destination);
                    return fviz_last_error_code();
                }
            }
        }
        fviz_free(sums);
        if (fviz_attribute_set_add(fviz_poly_data_point_data(output), name, destination) != FVIZ_OK)
        {
            fviz_release(destination);
            return fviz_last_error_code();
        }
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source_set, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(fviz_poly_data_point_data(output), role, name);
        }
        fviz_release(destination);
    }
    return FVIZ_OK;
}

static FVizResult fviz_decimate_copy_cell_data(
    const FVizPolyData* input,
    FVizPolyData* output,
    const FVizSize* source_ids,
    FVizSize output_triangle_count)
{
    const FVizAttributeSet* source_set = fviz_poly_data_const_cell_data(input);
    const FVizSize input_triangle_count = fviz_poly_data_triangle_count(input);
    FVizSize array_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source_set); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source_set, array_index);
        const FVizDataArray* source = fviz_attribute_set_const_array_at(source_set, array_index);
        FVizDataArray* destination = NULL;
        FVizAttributeRole role;
        FVizSize i;
        if (fviz_data_array_tuple_count(source) != input_triangle_count) continue;
        if (fviz_data_array_create(fviz_data_array_type(source), fviz_data_array_components(source), &destination) != FVIZ_OK ||
            fviz_data_array_reserve(destination, output_triangle_count) != FVIZ_OK)
        {
            fviz_release(destination);
            return fviz_last_error_code();
        }
        for (i = 0u; i < output_triangle_count; ++i)
        {
            if (fviz_data_array_append_tuple(destination, fviz_data_array_const_tuple(source, source_ids[i])) != FVIZ_OK)
            {
                fviz_release(destination);
                return fviz_last_error_code();
            }
        }
        if (fviz_attribute_set_add(fviz_poly_data_cell_data(output), name, destination) != FVIZ_OK)
        {
            fviz_release(destination);
            return fviz_last_error_code();
        }
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source_set, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(fviz_poly_data_cell_data(output), role, name);
        }
        fviz_release(destination);
    }
    if (fviz_attribute_set_const_get(fviz_poly_data_const_cell_data(output), "FVizOriginalCellIds") == NULL)
    {
        FVizDataArray* provenance = NULL;
        FVizSize i;
        if (fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &provenance) != FVIZ_OK ||
            fviz_data_array_reserve(provenance, output_triangle_count) != FVIZ_OK)
        {
            fviz_release(provenance);
            return fviz_last_error_code();
        }
        for (i = 0u; i < output_triangle_count; ++i)
        {
            const uint64_t id = (uint64_t)source_ids[i];
            if (fviz_data_array_append_tuple(provenance, &id) != FVIZ_OK)
            {
                fviz_release(provenance);
                return fviz_last_error_code();
            }
        }
        if (fviz_attribute_set_add(fviz_poly_data_cell_data(output), "FVizOriginalCellIds", provenance) != FVIZ_OK)
        {
            fviz_release(provenance);
            return fviz_last_error_code();
        }
        fviz_release(provenance);
    }
    return FVIZ_OK;
}

static FVizResult fviz_decimate_poly_data_filter_process_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* state)
{
    FVizDecimatePolyDataFilter* filter = (FVizDecimatePolyDataFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    FVizHashMap* clusters = NULL;
    FVizVec3* sums = NULL;
    FVizSize* counts = NULL;
    uint32_t* mapping = NULL;
    FVizVec3* centroids = NULL;
    FVizDecimateTriRecord* records = NULL;
    uint32_t* output_triangles = NULL;
    FVizSize* source_ids = NULL;
    FVizSize cluster_count = 0u;
    FVizSize record_count = 0u;
    FVizSize unique_count = 0u;
    FVizSize point_count;
    FVizSize triangle_count;
    const FVizVec3* points;
    const uint32_t* triangles;
    FVizBounds bounds;
    double max_extent;
    double target_points;
    uint32_t resolution;
    FVizSize i;
    FVizSize bytes;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL || fviz_poly_data_validate(input) != FVIZ_OK)
    {
        if (input == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "decimate poly data filter has no input");
        return input == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
    }
    point_count = fviz_poly_data_point_count(input);
    triangle_count = fviz_poly_data_triangle_count(input);
    points = fviz_poly_data_points(input);
    triangles = fviz_poly_data_triangle_indices(input);
    if (triangle_count == 0u || triangles == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "decimation requires render-ready triangles; run TriangleFilter first");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (filter->target_reduction <= 0.0)
    {
        FVizResult result = fviz_poly_data_deep_copy(input, &output);
        if (result == FVIZ_OK)
            result = fviz_algorithm_set_output_data(
                algorithm, request->requested_output_port, (FVizDataObject*)output);
        fviz_release(output);
        return result;
    }

    bounds = fviz_poly_data_bounds(input);
    max_extent = (double)(bounds.max.x - bounds.min.x);
    if ((double)(bounds.max.y - bounds.min.y) > max_extent) max_extent = (double)(bounds.max.y - bounds.min.y);
    if ((double)(bounds.max.z - bounds.min.z) > max_extent) max_extent = (double)(bounds.max.z - bounds.min.z);
    target_points = (double)point_count * (1.0 - filter->target_reduction);
    if (target_points < 4.0) target_points = 4.0;
    resolution = (uint32_t)ceil(sqrt(target_points));
    if (resolution < 1u) resolution = 1u;
    if (resolution > 2097151u) resolution = 2097151u;
    if (max_extent <= 0.0) resolution = 1u;

    if (fviz_size_multiply(point_count, sizeof(*sums), &bytes) != FVIZ_OK) goto fail;
    sums = (FVizVec3*)fviz_alloc(bytes);
    if (fviz_size_multiply(point_count, sizeof(*counts), &bytes) != FVIZ_OK) goto fail;
    counts = (FVizSize*)fviz_alloc(bytes);
    if (fviz_size_multiply(point_count, sizeof(*mapping), &bytes) != FVIZ_OK) goto fail;
    mapping = (uint32_t*)fviz_alloc(bytes);
    if (sums == NULL || counts == NULL || mapping == NULL) goto fail;
    memset(sums, 0, point_count * sizeof(*sums));
    memset(counts, 0, point_count * sizeof(*counts));
    if (point_count > ((FVizSize)-1) / 2u)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "decimate hash capacity overflow");
        goto fail;
    }
    if (fviz_hash_map_create_reserve(point_count < 8u ? 8u : point_count * 2u, &clusters) != FVIZ_OK) goto fail;
    for (i = 0u; i < point_count; ++i)
    {
        const double ex = max_extent > 0.0 ? ((double)points[i].x - bounds.min.x) / max_extent : 0.0;
        const double ey = max_extent > 0.0 ? ((double)points[i].y - bounds.min.y) / max_extent : 0.0;
        const double ez = max_extent > 0.0 ? ((double)points[i].z - bounds.min.z) / max_extent : 0.0;
        uint32_t ix = (uint32_t)(ex <= 0.0 ? 0.0 : ex >= 1.0 ? resolution - 1u : floor(ex * resolution));
        uint32_t iy = (uint32_t)(ey <= 0.0 ? 0.0 : ey >= 1.0 ? resolution - 1u : floor(ey * resolution));
        uint32_t iz = (uint32_t)(ez <= 0.0 ? 0.0 : ez >= 1.0 ? resolution - 1u : floor(ez * resolution));
        const uint64_t key = ((uint64_t)ix << 42u) | ((uint64_t)iy << 21u) | (uint64_t)iz;
        void* value = NULL;
        FVizSize cluster;
        if (fviz_hash_map_get(clusters, key, &value) != FVIZ_FALSE)
        {
            cluster = (FVizSize)((uintptr_t)value - 1u);
        }
        else
        {
            cluster = cluster_count++;
            if (cluster > UINT32_MAX || fviz_hash_map_set(clusters, key, (void*)(uintptr_t)(cluster + 1u)) != FVIZ_OK)
                goto fail;
        }
        mapping[i] = (uint32_t)cluster;
        sums[cluster] = fviz_vec3_add(sums[cluster], points[i]);
        counts[cluster] += 1u;
    }
    if (fviz_size_multiply(cluster_count, sizeof(*centroids), &bytes) != FVIZ_OK) goto fail;
    centroids = (FVizVec3*)fviz_alloc(bytes);
    if (centroids == NULL && cluster_count != 0u) goto fail;
    for (i = 0u; i < cluster_count; ++i)
        centroids[i] = fviz_vec3_scale(sums[i], 1.0f / (float)counts[i]);

    if (fviz_size_multiply(triangle_count, sizeof(*records), &bytes) != FVIZ_OK) goto fail;
    records = (FVizDecimateTriRecord*)fviz_alloc(bytes);
    if (records == NULL) goto fail;
    for (i = 0u; i < triangle_count; ++i)
    {
        FVizDecimateTriRecord record;
        record.a = mapping[triangles[i * 3u + 0u]];
        record.b = mapping[triangles[i * 3u + 1u]];
        record.c = mapping[triangles[i * 3u + 2u]];
        if (record.a == record.b || record.b == record.c || record.c == record.a) continue;
        fviz_decimate_sort3(record.a, record.b, record.c, &record.k0, &record.k1, &record.k2);
        record.source = i;
        records[record_count++] = record;
    }
    qsort(records, record_count, sizeof(*records), fviz_decimate_compare_tri);
    for (i = 0u; i < record_count; ++i)
    {
        if (i == 0u || records[i].k0 != records[i - 1u].k0 ||
            records[i].k1 != records[i - 1u].k1 || records[i].k2 != records[i - 1u].k2)
            ++unique_count;
    }
    if (unique_count > (FVizSize)-1 / 3u ||
        fviz_size_multiply(unique_count * 3u, sizeof(*output_triangles), &bytes) != FVIZ_OK) goto fail;
    output_triangles = (uint32_t*)fviz_alloc(bytes);
    if (fviz_size_multiply(unique_count, sizeof(*source_ids), &bytes) != FVIZ_OK) goto fail;
    source_ids = (FVizSize*)fviz_alloc(bytes);
    if ((output_triangles == NULL || source_ids == NULL) && unique_count != 0u) goto fail;
    unique_count = 0u;
    for (i = 0u; i < record_count; ++i)
    {
        if (i != 0u && records[i].k0 == records[i - 1u].k0 &&
            records[i].k1 == records[i - 1u].k1 && records[i].k2 == records[i - 1u].k2)
            continue;
        output_triangles[unique_count * 3u + 0u] = records[i].a;
        output_triangles[unique_count * 3u + 1u] = records[i].b;
        output_triangles[unique_count * 3u + 2u] = records[i].c;
        source_ids[unique_count] = records[i].source;
        ++unique_count;
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_reserve(output, cluster_count, unique_count) != FVIZ_OK ||
        fviz_poly_data_add_points(output, centroids, cluster_count, NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangles(output, output_triangles, unique_count) != FVIZ_OK ||
        fviz_decimate_copy_clustered_point_data(input, output, mapping, counts, cluster_count) != FVIZ_OK ||
        fviz_decimate_copy_cell_data(input, output, source_ids, unique_count) != FVIZ_OK ||
        fviz_poly_data_compute_normals(output) != FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;

    fviz_release(output);
    fviz_release(clusters);
    fviz_free(source_ids);
    fviz_free(output_triangles);
    fviz_free(records);
    fviz_free(centroids);
    fviz_free(mapping);
    fviz_free(counts);
    fviz_free(sums);
    return FVIZ_OK;
fail:
    fviz_release(output);
    fviz_release(clusters);
    fviz_free(source_ids);
    fviz_free(output_triangles);
    fviz_free(records);
    fviz_free(centroids);
    fviz_free(mapping);
    fviz_free(counts);
    fviz_free(sums);
    return fviz_last_error_code();
}

FVizResult fviz_decimate_poly_data_filter_create(FVizDecimatePolyDataFilter** out_filter)
{
    FVizDecimatePolyDataFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_filter = NULL;
    filter = (FVizDecimatePolyDataFilter*)fviz_internal_object_allocate(
        sizeof(*filter), &g_fviz_decimate_poly_data_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->target_reduction = 0.5;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_decimate_poly_data_filter_process_request;
    callbacks.get_state_mtime = fviz_mesh_processing_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_decimate_poly_data_filter_set_target_reduction(FVizDecimatePolyDataFilter* filter, double reduction)
{
    if (filter == NULL || reduction < 0.0 || reduction > 0.99)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "target reduction must be in [0, 0.99]");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (filter->target_reduction != reduction)
    {
        filter->target_reduction = reduction;
        fviz_object_modified((FVizObject*)filter);
    }
    return FVIZ_OK;
}

double fviz_decimate_poly_data_filter_target_reduction(const FVizDecimatePolyDataFilter* filter)
{
    return filter != NULL ? filter->target_reduction : 0.0;
}

FVizResult fviz_decimate_poly_data_filter_set_input_data(FVizDecimatePolyDataFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input) : FVIZ_ERROR_INVALID_ARGUMENT;
}
FVizResult fviz_decimate_poly_data_filter_set_input_connection(FVizDecimatePolyDataFilter* filter, FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input) : FVIZ_ERROR_INVALID_ARGUMENT;
}
FVizAlgorithm* fviz_decimate_poly_data_filter_algorithm(FVizDecimatePolyDataFilter* filter) { return filter != NULL ? filter->algorithm : NULL; }
FVizAlgorithmOutput* fviz_decimate_poly_data_filter_output_port(FVizDecimatePolyDataFilter* filter) { return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL; }
FVizPolyData* fviz_decimate_poly_data_filter_output(FVizDecimatePolyDataFilter* filter) { return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL; }
FVizResult fviz_decimate_poly_data_filter_update(FVizDecimatePolyDataFilter* filter) { return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT; }
