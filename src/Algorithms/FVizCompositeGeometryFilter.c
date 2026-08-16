#include <string.h>
#include <FViz/Algorithms/FVizCompositeGeometryFilter.h>
#include <FViz/Algorithms/FVizImageDataGeometryFilter.h>
#include <FViz/Algorithms/FVizRectilinearGridGeometryFilter.h>
#include <FViz/Algorithms/FVizStructuredGridGeometryFilter.h>
#include <FViz/Algorithms/FVizUnstructuredGridGeometryFilter.h>
#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Data/FVizImageData.h>
#include <FViz/Data/FVizDataObject.h>
#include <FViz/Data/FVizPartitionedDataSet.h>
#include <FViz/Data/FVizRectilinearGrid.h>
#include <FViz/Data/FVizStructuredGrid.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Parallel/FVizParallel.h>
#include <FViz/Pipeline/FVizExecutive.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

typedef struct FVizCompositeGeometryCacheEntry
{
    FVizDataObject* input;
    FVizMTime input_mtime;
    FVizDataObject* output;
    uint64_t generation;
    uint64_t last_use;
    FVizSize memory_bytes;
} FVizCompositeGeometryCacheEntry;

struct FVizCompositeGeometryFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizArray* cache_entries; /* FVizCompositeGeometryCacheEntry* */
    FVizHashMap* cache_index; /* input pointer -> cache entry */
    uint64_t cache_generation;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t cache_pruned;
    uint64_t cache_evictions;
    uint64_t cache_oversize_skips;
    uint64_t cache_clock;
    FVizSize cache_byte_capacity;
    FVizSize cache_bytes;
    FVizBool parallel_enabled;
    FVizSize parallel_threshold;
    uint64_t parallel_batches;
    uint64_t parallel_leaf_conversions;
};

typedef struct FVizCompositeGeometryJob
{
    const FVizDataObject* input;
    FVizDataObject* output_parent;
    FVizSize index;
} FVizCompositeGeometryJob;

typedef struct FVizCompositeGeometryPendingLeaf
{
    const FVizDataObject* input;
    FVizDataObject* output_parent;
    FVizSize index;
    FVizMTime input_mtime;
    FVizCompositeGeometryCacheEntry* cache_entry;
    FVizDataObject* converted;
} FVizCompositeGeometryPendingLeaf;

typedef struct FVizCompositeGeometryParallelContext
{
    FVizCompositeGeometryPendingLeaf* leaves;
} FVizCompositeGeometryParallelContext;

static void fviz_composite_geometry_cache_entry_destroy(FVizCompositeGeometryCacheEntry* entry)
{
    if (entry == NULL) return;
    fviz_release(entry->input);
    fviz_release(entry->output);
    fviz_free(entry);
}

void fviz_composite_geometry_filter_clear_cache(FVizCompositeGeometryFilter* filter)
{
    FVizSize i;
    if (filter == NULL) return;
    if (filter->cache_entries != NULL)
    {
        for (i = 0u; i < fviz_array_count(filter->cache_entries); ++i)
        {
            FVizCompositeGeometryCacheEntry* entry =
                *(FVizCompositeGeometryCacheEntry**)fviz_array_at(filter->cache_entries, i);
            fviz_composite_geometry_cache_entry_destroy(entry);
        }
        fviz_array_clear(filter->cache_entries);
    }
    if (filter->cache_index != NULL) fviz_hash_map_clear(filter->cache_index);
    filter->cache_bytes = 0u;
}

static void fviz_composite_geometry_destroy(FVizObject* object)
{
    FVizCompositeGeometryFilter* filter = (FVizCompositeGeometryFilter*)object;
    fviz_composite_geometry_filter_clear_cache(filter);
    fviz_release(filter->cache_entries); filter->cache_entries = NULL;
    fviz_release(filter->cache_index); filter->cache_index = NULL;
    fviz_release(filter->algorithm); filter->algorithm = NULL;
}
static const FVizObjectClass g_fviz_composite_geometry_class = {
    FVIZ_TYPE_COMPOSITE_GEOMETRY_FILTER, "FVizCompositeGeometryFilter", &g_fviz_object_class,
    fviz_composite_geometry_destroy, NULL
};
static FVizMTime fviz_composite_geometry_state_mtime(const void* state)
{ return fviz_object_mtime((const FVizObject*)state); }

static FVizResult fviz_composite_geometry_assign(
    FVizDataObject* parent, FVizSize index, FVizDataObject* child)
{
    if (fviz_object_is_type((const FVizObject*)parent, FVIZ_TYPE_MULTI_BLOCK_DATA_SET) != FVIZ_FALSE)
        return fviz_multi_block_data_set_set_block((FVizMultiBlockDataSet*)parent, index, child);
    if (fviz_object_is_type((const FVizObject*)parent, FVIZ_TYPE_PARTITIONED_DATA_SET) != FVIZ_FALSE)
        return fviz_partitioned_data_set_set_partition((FVizPartitionedDataSet*)parent, index, child);
    return FVIZ_ERROR_INVALID_STATE;
}

static FVizId fviz_composite_geometry_cache_key(const FVizDataObject* input)
{
    return (FVizId)(uintptr_t)input;
}

static FVizBool fviz_composite_geometry_cache_lookup(
    FVizCompositeGeometryFilter* filter,
    const FVizDataObject* input,
    FVizMTime input_mtime,
    FVizCompositeGeometryCacheEntry** out_entry,
    FVizDataObject** out_data)
{
    FVizCompositeGeometryCacheEntry* entry = NULL;
    void* value = NULL;
    if (out_entry != NULL) *out_entry = NULL;
    if (out_data != NULL) *out_data = NULL;
    if (filter == NULL || input == NULL || out_data == NULL) return FVIZ_FALSE;
    if (fviz_hash_map_get(filter->cache_index, fviz_composite_geometry_cache_key(input), &value) == FVIZ_FALSE)
        return FVIZ_FALSE;
    entry = (FVizCompositeGeometryCacheEntry*)value;
    if (out_entry != NULL) *out_entry = entry;
    if (entry == NULL || entry->input != input || entry->input_mtime != input_mtime || entry->output == NULL)
        return FVIZ_FALSE;
    entry->generation = filter->cache_generation;
    entry->last_use = ++filter->cache_clock;
    filter->cache_hits += 1u;
    *out_data = (FVizDataObject*)fviz_retain(entry->output);
    return *out_data != NULL ? FVIZ_TRUE : FVIZ_FALSE;
}

static void fviz_composite_geometry_cache_remove_at(
    FVizCompositeGeometryFilter* filter, FVizSize index, FVizBool eviction)
{
    FVizCompositeGeometryCacheEntry** entries;
    FVizSize count;
    FVizCompositeGeometryCacheEntry* entry;
    if (filter == NULL || filter->cache_entries == NULL || filter->cache_index == NULL) return;
    count = fviz_array_count(filter->cache_entries);
    if (index >= count) return;
    entries = (FVizCompositeGeometryCacheEntry**)fviz_array_data(filter->cache_entries);
    entry = entries[index];
    if (entry != NULL)
    {
        (void)fviz_hash_map_erase(filter->cache_index, fviz_composite_geometry_cache_key(entry->input));
        if (entry->memory_bytes <= filter->cache_bytes) filter->cache_bytes -= entry->memory_bytes;
        else filter->cache_bytes = 0u;
        fviz_composite_geometry_cache_entry_destroy(entry);
    }
    /* Entry order is not part of the cache contract. Move the tail into the
     * removed slot instead of shifting all following entries with memmove. */
    if (index + 1u < count) entries[index] = entries[count - 1u];
    entries[count - 1u] = NULL;
    (void)fviz_array_resize(filter->cache_entries, count - 1u);
    if (eviction != FVIZ_FALSE) filter->cache_evictions += 1u;
}

static void fviz_composite_geometry_cache_prune(FVizCompositeGeometryFilter* filter)
{
    FVizCompositeGeometryCacheEntry** entries;
    FVizSize count;
    FVizSize read_index;
    FVizSize write_index = 0u;
    if (filter == NULL || filter->cache_entries == NULL || filter->cache_index == NULL) return;
    count = fviz_array_count(filter->cache_entries);
    entries = (FVizCompositeGeometryCacheEntry**)fviz_array_data(filter->cache_entries);
    for (read_index = 0u; read_index < count; ++read_index)
    {
        FVizCompositeGeometryCacheEntry* entry = entries[read_index];
        if (entry != NULL && entry->generation == filter->cache_generation)
        {
            entries[write_index++] = entry;
            continue;
        }
        if (entry != NULL)
        {
            (void)fviz_hash_map_erase(filter->cache_index, fviz_composite_geometry_cache_key(entry->input));
            if (entry->memory_bytes <= filter->cache_bytes) filter->cache_bytes -= entry->memory_bytes;
            else filter->cache_bytes = 0u;
            fviz_composite_geometry_cache_entry_destroy(entry);
            filter->cache_pruned += 1u;
        }
    }
    if (write_index != count) (void)fviz_array_resize(filter->cache_entries, write_index);
}

static void fviz_composite_geometry_cache_enforce_budget(FVizCompositeGeometryFilter* filter)
{
    while (filter != NULL && filter->cache_byte_capacity != 0u &&
           filter->cache_bytes > filter->cache_byte_capacity &&
           filter->cache_entries != NULL && fviz_array_count(filter->cache_entries) != 0u)
    {
        FVizCompositeGeometryCacheEntry** entries =
            (FVizCompositeGeometryCacheEntry**)fviz_array_data(filter->cache_entries);
        FVizSize count = fviz_array_count(filter->cache_entries);
        FVizSize lru_index = 0u;
        FVizSize i;
        uint64_t lru_stamp = entries[0] != NULL ? entries[0]->last_use : 0u;
        for (i = 1u; i < count; ++i)
        {
            const uint64_t stamp = entries[i] != NULL ? entries[i]->last_use : 0u;
            if (stamp < lru_stamp)
            {
                lru_stamp = stamp;
                lru_index = i;
            }
        }
        fviz_composite_geometry_cache_remove_at(filter, lru_index, FVIZ_TRUE);
    }
}

static FVizResult fviz_composite_geometry_cache_commit(
    FVizCompositeGeometryFilter* filter,
    const FVizDataObject* input,
    FVizMTime input_mtime,
    FVizCompositeGeometryCacheEntry* entry,
    FVizDataObject* converted)
{
    FVizDataObjectMemoryInfo memory_info;
    FVizSize memory_bytes;
    FVizResult result;
    if (filter == NULL || input == NULL || converted == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_data_object_memory_info(converted, &memory_info);
    if (result != FVIZ_OK) return result;
    memory_bytes = memory_info.total_bytes;
    if (filter->cache_byte_capacity != 0u && memory_bytes > filter->cache_byte_capacity)
    {
        FVizSize i;
        filter->cache_oversize_skips += 1u;
        if (entry != NULL && filter->cache_entries != NULL)
        {
            FVizCompositeGeometryCacheEntry** entries =
                (FVizCompositeGeometryCacheEntry**)fviz_array_data(filter->cache_entries);
            for (i = 0u; i < fviz_array_count(filter->cache_entries); ++i)
            {
                if (entries[i] == entry)
                {
                    fviz_composite_geometry_cache_remove_at(filter, i, FVIZ_FALSE);
                    break;
                }
            }
        }
        return FVIZ_OK;
    }
    if (entry != NULL && entry->input == input)
    {
        FVizDataObject* retained = (FVizDataObject*)fviz_retain(converted);
        if (retained == NULL) return fviz_last_error_code();
        if (entry->memory_bytes <= filter->cache_bytes) filter->cache_bytes -= entry->memory_bytes;
        else filter->cache_bytes = 0u;
        fviz_release(entry->output);
        entry->output = retained;
        entry->input_mtime = input_mtime;
        entry->generation = filter->cache_generation;
        entry->last_use = ++filter->cache_clock;
        entry->memory_bytes = memory_bytes;
        filter->cache_bytes += memory_bytes;
        return FVIZ_OK;
    }
    entry = (FVizCompositeGeometryCacheEntry*)fviz_alloc(sizeof(*entry));
    if (entry == NULL) return fviz_last_error_code();
    (void)memset(entry, 0, sizeof(*entry));
    entry->input = (FVizDataObject*)fviz_retain((void*)input);
    entry->output = (FVizDataObject*)fviz_retain(converted);
    entry->input_mtime = input_mtime;
    entry->generation = filter->cache_generation;
    entry->last_use = ++filter->cache_clock;
    entry->memory_bytes = memory_bytes;
    if (entry->input == NULL || entry->output == NULL)
    {
        result = fviz_last_error_code();
        fviz_composite_geometry_cache_entry_destroy(entry);
        return result;
    }
    result = fviz_hash_map_set(filter->cache_index, fviz_composite_geometry_cache_key(input), entry);
    if (result != FVIZ_OK)
    {
        fviz_composite_geometry_cache_entry_destroy(entry);
        return result;
    }
    result = fviz_array_push(filter->cache_entries, &entry);
    if (result != FVIZ_OK)
    {
        (void)fviz_hash_map_erase(filter->cache_index, fviz_composite_geometry_cache_key(input));
        fviz_composite_geometry_cache_entry_destroy(entry);
        return result;
    }
    filter->cache_bytes += memory_bytes;
    return FVIZ_OK;
}

static FVizResult fviz_composite_geometry_convert_leaf(
    const FVizDataObject* input, FVizDataObject** out_data)
{
    *out_data = NULL;
    if (fviz_object_is_type((const FVizObject*)input, FVIZ_TYPE_POLY_DATA) != FVIZ_FALSE)
    {
        *out_data = (FVizDataObject*)fviz_retain((void*)input);
        return *out_data != NULL ? FVIZ_OK : fviz_last_error_code();
    }
#define FVIZ_CONVERT_WITH_FILTER(TypeMacro, FilterType, create_fn, set_fn, update_fn, output_fn) \
    if (fviz_object_is_type((const FVizObject*)input, TypeMacro) != FVIZ_FALSE) \
    { \
        FilterType* leaf_filter = NULL; FVizPolyData* output = NULL; FVizResult result; \
        result = create_fn(&leaf_filter); \
        if (result == FVIZ_OK) result = set_fn(leaf_filter, (void*)input); \
        if (result == FVIZ_OK) result = update_fn(leaf_filter); \
        if (result == FVIZ_OK) { output = output_fn(leaf_filter); if (output == NULL) result = FVIZ_ERROR_INVALID_STATE; } \
        if (result == FVIZ_OK) *out_data = (FVizDataObject*)fviz_retain(output); \
        fviz_release(leaf_filter); \
        return result; \
    }
    FVIZ_CONVERT_WITH_FILTER(FVIZ_TYPE_UNSTRUCTURED_GRID, FVizUnstructuredGridGeometryFilter,
        fviz_unstructured_grid_geometry_filter_create, fviz_unstructured_grid_geometry_filter_set_input_data,
        fviz_unstructured_grid_geometry_filter_update, fviz_unstructured_grid_geometry_filter_output)
    FVIZ_CONVERT_WITH_FILTER(FVIZ_TYPE_STRUCTURED_GRID, FVizStructuredGridGeometryFilter,
        fviz_structured_grid_geometry_filter_create, fviz_structured_grid_geometry_filter_set_input_data,
        fviz_structured_grid_geometry_filter_update, fviz_structured_grid_geometry_filter_output)
    FVIZ_CONVERT_WITH_FILTER(FVIZ_TYPE_RECTILINEAR_GRID, FVizRectilinearGridGeometryFilter,
        fviz_rectilinear_grid_geometry_filter_create, fviz_rectilinear_grid_geometry_filter_set_input_data,
        fviz_rectilinear_grid_geometry_filter_update, fviz_rectilinear_grid_geometry_filter_output)
    FVIZ_CONVERT_WITH_FILTER(FVIZ_TYPE_IMAGE_DATA, FVizImageDataGeometryFilter,
        fviz_image_data_geometry_filter_create, fviz_image_data_geometry_filter_set_input_data,
        fviz_image_data_geometry_filter_update, fviz_image_data_geometry_filter_output)
#undef FVIZ_CONVERT_WITH_FILTER
    fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "composite geometry contains an unsupported leaf data type");
    return FVIZ_ERROR_NOT_SUPPORTED;
}

static FVizResult fviz_composite_geometry_parallel_convert(
    FVizSize begin, FVizSize end, void* user_data)
{
    FVizCompositeGeometryParallelContext* context = (FVizCompositeGeometryParallelContext*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i)
    {
        FVizResult result = fviz_composite_geometry_convert_leaf(
            context->leaves[i].input, &context->leaves[i].converted);
        if (result != FVIZ_OK) return result;
    }
    return FVIZ_OK;
}

static FVizResult fviz_composite_geometry_prepare_composite(
    const FVizDataObject* input, FVizDataObject** out_data, FVizArray* jobs)
{
    FVizSize count, i;
    *out_data = NULL;
    if (fviz_object_is_type((const FVizObject*)input, FVIZ_TYPE_MULTI_BLOCK_DATA_SET) != FVIZ_FALSE)
    {
        const FVizMultiBlockDataSet* source = (const FVizMultiBlockDataSet*)input;
        FVizMultiBlockDataSet* dest = NULL;
        if (fviz_multi_block_data_set_create(&dest) != FVIZ_OK) return fviz_last_error_code();
        count = fviz_multi_block_data_set_count(source);
        if (fviz_multi_block_data_set_resize(dest, count) != FVIZ_OK) { fviz_release(dest); return fviz_last_error_code(); }
        for (i = 0u; i < count; ++i)
        {
            const char* name = fviz_multi_block_data_set_block_name(source, i);
            FVizCompositeGeometryJob job = {fviz_multi_block_data_set_const_block(source, i), (FVizDataObject*)dest, i};
            if (name != NULL && fviz_multi_block_data_set_set_block_name(dest, i, name) != FVIZ_OK) { fviz_release(dest); return fviz_last_error_code(); }
            if (job.input != NULL && fviz_array_push(jobs, &job) != FVIZ_OK) { fviz_release(dest); return fviz_last_error_code(); }
        }
        *out_data = (FVizDataObject*)dest; return FVIZ_OK;
    }
    if (fviz_object_is_type((const FVizObject*)input, FVIZ_TYPE_PARTITIONED_DATA_SET) != FVIZ_FALSE)
    {
        const FVizPartitionedDataSet* source = (const FVizPartitionedDataSet*)input;
        FVizPartitionedDataSet* dest = NULL;
        if (fviz_partitioned_data_set_create(&dest) != FVIZ_OK) return fviz_last_error_code();
        count = fviz_partitioned_data_set_count(source);
        if (fviz_partitioned_data_set_resize(dest, count) != FVIZ_OK) { fviz_release(dest); return fviz_last_error_code(); }
        for (i = 0u; i < count; ++i)
        {
            const char* name = fviz_partitioned_data_set_partition_name(source, i);
            FVizCompositeGeometryJob job = {fviz_partitioned_data_set_const_partition(source, i), (FVizDataObject*)dest, i};
            if (name != NULL && fviz_partitioned_data_set_set_partition_name(dest, i, name) != FVIZ_OK) { fviz_release(dest); return fviz_last_error_code(); }
            if (job.input != NULL && fviz_array_push(jobs, &job) != FVIZ_OK) { fviz_release(dest); return fviz_last_error_code(); }
        }
        *out_data = (FVizDataObject*)dest; return FVIZ_OK;
    }
    return FVIZ_ERROR_NOT_FOUND;
}

static FVizResult fviz_composite_geometry_process_request(
    FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request, void* state)
{
    FVizMultiBlockDataSet* input;
    FVizMultiBlockDataSet* output = NULL;
    FVizArray* jobs = NULL;
    FVizArray* pending = NULL;
    FVizSize i;
    FVizResult result = FVIZ_OK;
    FVizCompositeGeometryFilter* filter = (FVizCompositeGeometryFilter*)state;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (filter == NULL) return FVIZ_ERROR_INVALID_STATE;
    if (filter->cache_generation == UINT64_MAX)
    {
        fviz_composite_geometry_filter_clear_cache(filter);
        filter->cache_generation = 1u;
    }
    else
    {
        filter->cache_generation += 1u;
        if (filter->cache_generation == 0u) filter->cache_generation = 1u;
    }
    input = (FVizMultiBlockDataSet*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL) return FVIZ_ERROR_INVALID_STATE;
    if (fviz_array_create(sizeof(FVizCompositeGeometryJob), &jobs) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizCompositeGeometryPendingLeaf), &pending) != FVIZ_OK ||
        fviz_multi_block_data_set_create(&output) != FVIZ_OK)
    { result = fviz_last_error_code(); goto done; }
    if (fviz_multi_block_data_set_resize(output, fviz_multi_block_data_set_count(input)) != FVIZ_OK)
    { result = fviz_last_error_code(); goto done; }
    for (i = 0u; i < fviz_multi_block_data_set_count(input); ++i)
    {
        const char* name = fviz_multi_block_data_set_block_name(input, i);
        FVizCompositeGeometryJob job = {
            fviz_multi_block_data_set_const_block(input, i), (FVizDataObject*)output, i};
        if (name != NULL && fviz_multi_block_data_set_set_block_name(output, i, name) != FVIZ_OK)
        { result = fviz_last_error_code(); goto done; }
        if (job.input != NULL && fviz_array_push(jobs, &job) != FVIZ_OK)
        { result = fviz_last_error_code(); goto done; }
    }

    /* Build the output hierarchy serially, resolve cache hits immediately, and
     * collect only dirty/new leaves for the parallel conversion phase.  Cache
     * mutation and composite assignment remain serialized, so observer/refcount
     * semantics do not become dependent on worker scheduling. */
    while (fviz_array_count(jobs) != 0u)
    {
        FVizCompositeGeometryJob job =
            *(const FVizCompositeGeometryJob*)fviz_array_const_at(
                jobs, fviz_array_count(jobs) - 1u);
        FVizDataObject* converted = NULL;
        if (fviz_array_resize(jobs, fviz_array_count(jobs) - 1u) != FVIZ_OK)
        { result = fviz_last_error_code(); goto done; }
        if (request->cancellation != NULL &&
            fviz_cancellation_token_is_cancelled(request->cancellation) != FVIZ_FALSE)
        { result = FVIZ_ERROR_CANCELLED; goto done; }
        result = fviz_composite_geometry_prepare_composite(job.input, &converted, jobs);
        if (result == FVIZ_OK)
        {
            result = fviz_composite_geometry_assign(job.output_parent, job.index, converted);
            fviz_release(converted);
            if (result != FVIZ_OK) goto done;
            continue;
        }
        if (result != FVIZ_ERROR_NOT_FOUND) goto done;
        {
            FVizCompositeGeometryPendingLeaf leaf;
            (void)memset(&leaf, 0, sizeof(leaf));
            leaf.input = job.input;
            leaf.output_parent = job.output_parent;
            leaf.index = job.index;
            leaf.input_mtime = fviz_object_mtime((const FVizObject*)job.input);
            if (fviz_composite_geometry_cache_lookup(
                    filter, job.input, leaf.input_mtime, &leaf.cache_entry, &converted) != FVIZ_FALSE)
            {
                result = fviz_composite_geometry_assign(job.output_parent, job.index, converted);
                fviz_release(converted);
                if (result != FVIZ_OK) goto done;
                continue;
            }
            filter->cache_misses += 1u;
            if (fviz_array_push(pending, &leaf) != FVIZ_OK)
            { result = fviz_last_error_code(); goto done; }
        }
    }

    if (fviz_array_count(pending) != 0u)
    {
        FVizCompositeGeometryPendingLeaf* leaves =
            (FVizCompositeGeometryPendingLeaf*)fviz_array_data(pending);
        const FVizSize count = fviz_array_count(pending);
        const FVizBool use_parallel =
            filter->parallel_enabled != FVIZ_FALSE && count >= filter->parallel_threshold &&
            fviz_parallel_context_thread_count(fviz_parallel_default_context()) > 1u
                ? FVIZ_TRUE : FVIZ_FALSE;
        if (use_parallel != FVIZ_FALSE)
        {
            FVizCompositeGeometryParallelContext parallel_context;
            parallel_context.leaves = leaves;
            filter->parallel_batches += 1u;
            filter->parallel_leaf_conversions += (uint64_t)count;
            result = fviz_parallel_context_for(
                fviz_parallel_default_context(), 0u, count, 1u,
                fviz_composite_geometry_parallel_convert, &parallel_context,
                request->cancellation);
        }
        else
        {
            for (i = 0u; i < count; ++i)
            {
                if (request->cancellation != NULL &&
                    fviz_cancellation_token_is_cancelled(request->cancellation) != FVIZ_FALSE)
                { result = FVIZ_ERROR_CANCELLED; break; }
                result = fviz_composite_geometry_convert_leaf(leaves[i].input, &leaves[i].converted);
                if (result != FVIZ_OK) break;
            }
        }
        if (result != FVIZ_OK) goto done;
        for (i = 0u; i < count; ++i)
        {
            result = fviz_composite_geometry_cache_commit(
                filter, leaves[i].input, leaves[i].input_mtime,
                leaves[i].cache_entry, leaves[i].converted);
            if (result == FVIZ_OK)
                result = fviz_composite_geometry_assign(
                    leaves[i].output_parent, leaves[i].index, leaves[i].converted);
            if (result != FVIZ_OK) goto done;
        }
    }
    result = fviz_algorithm_set_output_data(
        algorithm, request->requested_output_port, (FVizDataObject*)output);
    if (result == FVIZ_OK)
    {
        fviz_composite_geometry_cache_prune(filter);
        fviz_composite_geometry_cache_enforce_budget(filter);
    }
done:
    if (pending != NULL)
    {
        FVizCompositeGeometryPendingLeaf* leaves =
            (FVizCompositeGeometryPendingLeaf*)fviz_array_data(pending);
        for (i = 0u; i < fviz_array_count(pending); ++i)
            fviz_release(leaves[i].converted);
    }
    fviz_release(pending);
    fviz_release(jobs);
    fviz_release(output);
    return result;
}

FVizResult fviz_composite_geometry_filter_create(FVizCompositeGeometryFilter** out_filter)
{
    FVizCompositeGeometryFilter* filter; FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizCompositeGeometryFilter*)fviz_internal_object_allocate(sizeof(*filter), &g_fviz_composite_geometry_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->parallel_enabled = FVIZ_TRUE;
    filter->parallel_threshold = 4u;
    if (fviz_array_create(sizeof(FVizCompositeGeometryCacheEntry*), &filter->cache_entries) != FVIZ_OK ||
        fviz_hash_map_create(&filter->cache_index) != FVIZ_OK)
    { fviz_release(filter); return fviz_last_error_code(); }
    fviz_algorithm_callbacks_initialize(&callbacks); callbacks.process_request = fviz_composite_geometry_process_request;
    callbacks.get_state_mtime = fviz_composite_geometry_state_mtime; callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u,1u,&callbacks,filter,&filter->algorithm)!=FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm,0u,FVIZ_TYPE_MULTI_BLOCK_DATA_SET,FVIZ_FALSE,FVIZ_FALSE)!=FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm,0u,FVIZ_TYPE_MULTI_BLOCK_DATA_SET)!=FVIZ_OK)
    { fviz_release(filter); return fviz_last_error_code(); }
    *out_filter=filter; return FVIZ_OK;
}
FVizResult fviz_composite_geometry_filter_set_input_data(FVizCompositeGeometryFilter* f,FVizMultiBlockDataSet* in)
{ return f!=NULL&&in!=NULL ? fviz_algorithm_set_input_data(f->algorithm,0u,(FVizDataObject*)in) : FVIZ_ERROR_INVALID_ARGUMENT; }
FVizResult fviz_composite_geometry_filter_set_input_connection(FVizCompositeGeometryFilter* f,FVizAlgorithmOutput* in)
{ return f!=NULL&&in!=NULL ? fviz_algorithm_set_input_connection(f->algorithm,0u,in) : FVIZ_ERROR_INVALID_ARGUMENT; }
FVizAlgorithm* fviz_composite_geometry_filter_algorithm(FVizCompositeGeometryFilter* f){return f?f->algorithm:NULL;}
FVizAlgorithmOutput* fviz_composite_geometry_filter_output_port(FVizCompositeGeometryFilter* f){return f?fviz_algorithm_output_port(f->algorithm,0u):NULL;}
FVizMultiBlockDataSet* fviz_composite_geometry_filter_output(FVizCompositeGeometryFilter* f){return f?(FVizMultiBlockDataSet*)fviz_algorithm_output_data(f->algorithm,0u):NULL;}
FVizResult fviz_composite_geometry_filter_update(FVizCompositeGeometryFilter* f){return f?fviz_algorithm_update(f->algorithm):FVIZ_ERROR_INVALID_ARGUMENT;}

FVizResult fviz_composite_geometry_filter_set_parallel_enabled(
    FVizCompositeGeometryFilter* filter, FVizBool enabled)
{
    if (filter == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (filter->parallel_enabled == enabled) return FVIZ_OK;
    filter->parallel_enabled = enabled;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizBool fviz_composite_geometry_filter_parallel_enabled(
    const FVizCompositeGeometryFilter* filter)
{ return filter != NULL ? filter->parallel_enabled : FVIZ_FALSE; }

FVizResult fviz_composite_geometry_filter_set_parallel_threshold(
    FVizCompositeGeometryFilter* filter, FVizSize leaf_count)
{
    if (filter == NULL || leaf_count == 0u) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (filter->parallel_threshold == leaf_count) return FVIZ_OK;
    filter->parallel_threshold = leaf_count;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizSize fviz_composite_geometry_filter_parallel_threshold(
    const FVizCompositeGeometryFilter* filter)
{ return filter != NULL ? filter->parallel_threshold : 0u; }

FVizResult fviz_composite_geometry_filter_set_cache_byte_capacity(
    FVizCompositeGeometryFilter* filter, FVizSize byte_capacity)
{
    if (filter == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (filter->cache_byte_capacity == byte_capacity) return FVIZ_OK;
    filter->cache_byte_capacity = byte_capacity;
    fviz_composite_geometry_cache_enforce_budget(filter);
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

FVizSize fviz_composite_geometry_filter_cache_byte_capacity(
    const FVizCompositeGeometryFilter* filter)
{ return filter != NULL ? filter->cache_byte_capacity : 0u; }

FVizCompositeGeometryCacheStatistics fviz_composite_geometry_filter_cache_statistics(
    const FVizCompositeGeometryFilter* filter)
{
    FVizCompositeGeometryCacheStatistics stats;
    (void)memset(&stats, 0, sizeof(stats));
    if (filter != NULL)
    {
        stats.entries = filter->cache_entries != NULL ? fviz_array_count(filter->cache_entries) : 0u;
        stats.hits = filter->cache_hits;
        stats.misses = filter->cache_misses;
        stats.pruned = filter->cache_pruned;
        stats.byte_capacity = filter->cache_byte_capacity;
        stats.bytes = filter->cache_bytes;
        stats.evictions = filter->cache_evictions;
        stats.oversize_skips = filter->cache_oversize_skips;
        stats.parallel_batches = filter->parallel_batches;
        stats.parallel_leaf_conversions = filter->parallel_leaf_conversions;
    }
    return stats;
}
