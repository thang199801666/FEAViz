#include <ctype.h>
#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizString.h>
#include <FViz/IO/FVizPVDReader.h>
#include <FViz/IO/FVizVTPReader.h>
#include <FViz/IO/FVizVTUReader.h>
#include <FViz/IO/FVizPVTUReader.h>
#include <FViz/Data/FVizPartitionedDataSet.h>
#include <FViz/Data/FVizDataObject.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

typedef struct FVizPVDCacheEntry
{
    FVizSize index;
    FVizDataObject* data;
    uint64_t stamp;
    FVizSize memory_bytes;
} FVizPVDCacheEntry;

struct FVizPVDReader
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizString* file_name;
    FVizPVDCollection* collection;
    FVizPVDCacheEntry* cache_entries;
    FVizSize cache_capacity;
    FVizSize cache_size;
    FVizSize cache_byte_capacity;
    FVizSize cache_bytes;
    uint64_t cache_clock;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t cache_evictions;
    uint64_t cache_oversize_skips;
    FVizPVTUReader* piece_reader;
    FVizSize piece_reader_entry_index;
    FVizBool piece_reader_valid;
    double selected_time;
    FVizBool collection_dirty;
};

static void fviz_pvd_reader_destroy(FVizObject* object)
{
    FVizPVDReader* reader = (FVizPVDReader*)object;
    fviz_release(reader->algorithm);
    fviz_release(reader->file_name);
    fviz_release(reader->collection);
    fviz_release(reader->piece_reader);
    reader->piece_reader = NULL;
    fviz_pvd_reader_clear_cache(reader);
}

static FVizMTime fviz_pvd_reader_mtime(const FVizObject* object)
{
    const FVizPVDReader* reader = (const FVizPVDReader*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    if (reader->file_name != NULL)
    {
        const FVizMTime child = fviz_object_mtime((const FVizObject*)reader->file_name);
        if (child > mtime) mtime = child;
    }
    return mtime;
}

static const FVizObjectClass g_fviz_pvd_reader_class = {FVIZ_TYPE_PVD_READER, "FVizPVDReader", &g_fviz_object_class,
                                                        fviz_pvd_reader_destroy, fviz_pvd_reader_mtime};

static FVizResult fviz_pvd_reader_ensure_collection(FVizPVDReader* reader)
{
    FVizPVDCollection* replacement = NULL;
    if (reader->collection_dirty == FVIZ_FALSE && reader->collection != NULL) return FVIZ_OK;
    if (reader->file_name == NULL || fviz_string_length(reader->file_name) == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "PVD reader has no file name");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_pvd_read(fviz_string_c_str(reader->file_name), &replacement) != FVIZ_OK) return fviz_last_error_code();
    fviz_release(reader->collection);
    reader->collection = replacement;
    reader->collection_dirty = FVIZ_FALSE;
    fviz_pvd_reader_clear_cache(reader);
    fviz_release(reader->piece_reader);
    reader->piece_reader = NULL;
    reader->piece_reader_valid = FVIZ_FALSE;
    return FVIZ_OK;
}

static FVizBool fviz_path_is_absolute(const char* path)
{
    if (path == NULL || path[0] == '\0') return FVIZ_FALSE;
    if (path[0] == '/' || path[0] == '\\') return FVIZ_TRUE;
    return isalpha((unsigned char)path[0]) && path[1] == ':' ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_pvd_resolve_path(const char* pvd_path, const char* child, FVizString** out_path)
{
    const char* slash1;
    const char* slash2;
    const char* slash;
    FVizString* result = NULL;
    if (out_path == NULL || pvd_path == NULL || child == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_path = NULL;
    if (fviz_path_is_absolute(child) != FVIZ_FALSE) return fviz_string_create_from(child, out_path);
    if (fviz_string_create(&result) != FVIZ_OK) return fviz_last_error_code();
    slash1 = strrchr(pvd_path, '/');
    slash2 = strrchr(pvd_path, '\\');
    slash = slash1 != NULL && (slash2 == NULL || slash1 > slash2) ? slash1 : slash2;
    if (slash != NULL)
    {
        const FVizSize prefix = (FVizSize)(slash - pvd_path) + 1u;
        char* buffer = (char*)fviz_alloc(prefix + 1u);
        if (buffer == NULL)
        {
            fviz_release(result);
            return fviz_last_error_code();
        }
        (void)memcpy(buffer, pvd_path, prefix);
        buffer[prefix] = '\0';
        if (fviz_string_set(result, buffer) != FVIZ_OK)
        {
            fviz_free(buffer);
            fviz_release(result);
            return fviz_last_error_code();
        }
        fviz_free(buffer);
    }
    if (fviz_string_append(result, child) != FVIZ_OK)
    {
        fviz_release(result);
        return fviz_last_error_code();
    }
    *out_path = result;
    return FVIZ_OK;
}

static FVizResult fviz_pvd_load_entry(FVizPVDReader* reader, FVizSize index, FVizDataObject** out_data);

static const char* fviz_extension(const char* path)
{
    const char* dot = strrchr(path, '.');
    return dot != NULL ? dot : "";
}

static FVizSize fviz_pvd_time_group_end(const FVizPVDReader* reader, FVizSize first_index)
{
    const FVizSize count =
        reader != NULL && reader->collection != NULL ? fviz_pvd_collection_count(reader->collection) : 0u;
    const double time = first_index < count ? fviz_pvd_collection_time(reader->collection, first_index) : 0.0;
    FVizSize end = first_index;
    while (end < count && fviz_pvd_collection_time(reader->collection, end) == time)
        ++end;
    return end;
}

static FVizResult fviz_pvd_ensure_piece_reader(FVizPVDReader* reader, FVizSize entry_index,
                                               FVizPVTUReader** out_piece_reader)
{
    FVizString* resolved = NULL;
    FVizPVTUReader* replacement = NULL;
    const char* file;
    const char* ext;
    FVizResult result;
    if (out_piece_reader != NULL) *out_piece_reader = NULL;
    if (reader == NULL || out_piece_reader == NULL || reader->collection == NULL ||
        entry_index >= fviz_pvd_collection_count(reader->collection))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (reader->piece_reader_valid != FVIZ_FALSE && reader->piece_reader != NULL &&
        reader->piece_reader_entry_index == entry_index)
    {
        *out_piece_reader = reader->piece_reader;
        return FVIZ_OK;
    }
    file = fviz_pvd_collection_file(reader->collection, entry_index);
    if (fviz_pvd_resolve_path(fviz_string_c_str(reader->file_name), file, &resolved) != FVIZ_OK)
        return fviz_last_error_code();
    ext = fviz_extension(fviz_string_c_str(resolved));
    if (strcmp(ext, ".pvtu") != 0 && strcmp(ext, ".PVTU") != 0)
    {
        fviz_release(resolved);
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "requested PVD entry is not a PVTU manifest");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    result = fviz_pvtu_reader_create(&replacement);
    if (result == FVIZ_OK) result = fviz_pvtu_reader_set_file_name(replacement, fviz_string_c_str(resolved));
    if (result == FVIZ_OK && reader->cache_byte_capacity != 0u)
        result = fviz_pvtu_reader_set_cache_byte_capacity(replacement, reader->cache_byte_capacity);
    fviz_release(resolved);
    if (result != FVIZ_OK)
    {
        fviz_release(replacement);
        return result;
    }
    fviz_release(reader->piece_reader);
    reader->piece_reader = replacement;
    reader->piece_reader_entry_index = entry_index;
    reader->piece_reader_valid = FVIZ_TRUE;
    *out_piece_reader = reader->piece_reader;
    return FVIZ_OK;
}

static FVizResult fviz_pvd_piece_count_for_group(FVizPVDReader* reader, FVizSize first_index, uint32_t* out_piece_count)
{
    const FVizSize end = fviz_pvd_time_group_end(reader, first_index);
    const FVizSize group_count = end >= first_index ? end - first_index : 0u;
    const char* file;
    const char* ext;
    if (out_piece_count != NULL) *out_piece_count = 0u;
    if (reader == NULL || out_piece_count == NULL || group_count == 0u) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (group_count > 1u)
    {
        if (group_count > UINT32_MAX) return FVIZ_ERROR_OVERFLOW;
        *out_piece_count = (uint32_t)group_count;
        return FVIZ_OK;
    }
    file = fviz_pvd_collection_file(reader->collection, first_index);
    ext = fviz_extension(file);
    if (strcmp(ext, ".pvtu") == 0 || strcmp(ext, ".PVTU") == 0)
    {
        FVizPVTUReader* piece_reader = NULL;
        const FVizSize piece_count = fviz_pvd_ensure_piece_reader(reader, first_index, &piece_reader) == FVIZ_OK
                                         ? fviz_pvtu_reader_piece_count(piece_reader)
                                         : 0u;
        if (piece_reader == NULL) return fviz_last_error_code();
        if (piece_count == 0u || piece_count > UINT32_MAX) return FVIZ_ERROR_OVERFLOW;
        *out_piece_count = (uint32_t)piece_count;
        return FVIZ_OK;
    }
    *out_piece_count = 1u;
    return FVIZ_OK;
}

static FVizResult fviz_pvd_load_time_piece(FVizPVDReader* reader, FVizSize first_index, uint32_t piece,
                                           uint32_t number_of_pieces, uint32_t ghost_levels, FVizDataObject** out_data)
{
    const FVizSize end = fviz_pvd_time_group_end(reader, first_index);
    const FVizSize group_count = end >= first_index ? end - first_index : 0u;
    if (out_data != NULL) *out_data = NULL;
    if (reader == NULL || out_data == NULL || number_of_pieces == 0u || piece >= number_of_pieces || group_count == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (group_count > 1u)
    {
        FVizSize selected = first_index + (FVizSize)piece;
        FVizSize i;
        FVizBool found_part = FVIZ_FALSE;
        if (number_of_pieces != group_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "PVD piece request does not match timestep part count");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        if (ghost_levels != 0u)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                    "PVD multi-entry piece requests cannot synthesize additional ghost layers");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        for (i = first_index; i < end; ++i)
        {
            if (fviz_pvd_collection_part(reader->collection, i) == piece)
            {
                if (found_part == FVIZ_FALSE)
                {
                    selected = i;
                    found_part = FVIZ_TRUE;
                }
                else
                {
                    /* Duplicate part ids are ambiguous; fall back to stable positional mapping. */
                    selected = first_index + (FVizSize)piece;
                    break;
                }
            }
        }
        return fviz_pvd_load_entry(reader, selected, out_data);
    }
    {
        FVizPVTUReader* piece_reader = NULL;
        FVizUnstructuredGrid* grid = NULL;
        FVizSize available;
        FVizResult result = fviz_pvd_ensure_piece_reader(reader, first_index, &piece_reader);
        if (result != FVIZ_OK) return result;
        available = fviz_pvtu_reader_piece_count(piece_reader);
        if (available > UINT32_MAX || number_of_pieces != (uint32_t)available || piece >= available)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "PVD/PVTU piece request does not match manifest piece count");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        if (ghost_levels > fviz_pvtu_reader_ghost_level(piece_reader))
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                    "PVTU manifest does not contain the requested ghost depth");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        result = fviz_pvtu_reader_load_piece(piece_reader, piece, &grid);
        if (result != FVIZ_OK) return result;
        *out_data = (FVizDataObject*)grid;
        return FVIZ_OK;
    }
}

static FVizResult fviz_pvd_load_entry(FVizPVDReader* reader, FVizSize index, FVizDataObject** out_data)
{
    FVizString* resolved = NULL;
    FVizDataObject* data = NULL;
    const char* file;
    const char* ext;
    if (out_data != NULL) *out_data = NULL;
    if (reader == NULL || out_data == NULL || reader->collection == NULL ||
        index >= fviz_pvd_collection_count(reader->collection))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    file = fviz_pvd_collection_file(reader->collection, index);
    if (fviz_pvd_resolve_path(fviz_string_c_str(reader->file_name), file, &resolved) != FVIZ_OK)
        return fviz_last_error_code();
    ext = fviz_extension(fviz_string_c_str(resolved));
    if (strcmp(ext, ".vtp") == 0 || strcmp(ext, ".VTP") == 0)
    {
        FVizPolyData* poly = NULL;
        if (fviz_vtp_read(fviz_string_c_str(resolved), &poly) != FVIZ_OK)
        {
            fviz_release(resolved);
            return fviz_last_error_code();
        }
        data = (FVizDataObject*)poly;
    }
    else if (strcmp(ext, ".vtu") == 0 || strcmp(ext, ".VTU") == 0)
    {
        FVizUnstructuredGrid* grid = NULL;
        if (fviz_vtu_read(fviz_string_c_str(resolved), &grid) != FVIZ_OK)
        {
            fviz_release(resolved);
            return fviz_last_error_code();
        }
        data = (FVizDataObject*)grid;
    }
    else if (strcmp(ext, ".pvtu") == 0 || strcmp(ext, ".PVTU") == 0)
    {
        FVizPartitionedDataSet* partitions = NULL;
        if (fviz_pvtu_read(fviz_string_c_str(resolved), &partitions) != FVIZ_OK)
        {
            fviz_release(resolved);
            return fviz_last_error_code();
        }
        data = (FVizDataObject*)partitions;
    }
    else
    {
        fviz_release(resolved);
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                "PVD frame extension is not supported; expected .vtp, .vtu, or .pvtu");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    fviz_release(resolved);
    *out_data = data;
    return FVIZ_OK;
}

static FVizDataObject* fviz_pvd_cache_lookup(FVizPVDReader* reader, FVizSize index)
{
    FVizSize i;
    if (reader == NULL || reader->cache_capacity == 0u) return NULL;
    for (i = 0u; i < reader->cache_size; ++i)
    {
        FVizPVDCacheEntry* entry = &reader->cache_entries[i];
        if (entry->data != NULL && entry->index == index)
        {
            entry->stamp = ++reader->cache_clock;
            ++reader->cache_hits;
            return (FVizDataObject*)fviz_retain(entry->data);
        }
    }
    ++reader->cache_misses;
    return NULL;
}

static void fviz_pvd_cache_remove(FVizPVDReader* reader, FVizSize index)
{
    if (reader == NULL || index >= reader->cache_size) return;
    if (reader->cache_entries[index].memory_bytes <= reader->cache_bytes)
        reader->cache_bytes -= reader->cache_entries[index].memory_bytes;
    else
        reader->cache_bytes = 0u;
    fviz_release(reader->cache_entries[index].data);
    if (index + 1u < reader->cache_size)
        (void)memmove(&reader->cache_entries[index], &reader->cache_entries[index + 1u],
                      (size_t)(reader->cache_size - index - 1u) * sizeof(*reader->cache_entries));
    --reader->cache_size;
}

static FVizSize fviz_pvd_cache_lru_index(const FVizPVDReader* reader)
{
    FVizSize i;
    FVizSize index = 0u;
    if (reader == NULL || reader->cache_size == 0u) return 0u;
    for (i = 1u; i < reader->cache_size; ++i)
        if (reader->cache_entries[i].stamp < reader->cache_entries[index].stamp) index = i;
    return index;
}

static FVizResult fviz_pvd_cache_store(FVizPVDReader* reader, FVizSize index, FVizDataObject* data)
{
    FVizSize memory_bytes;
    FVizPVDCacheEntry* entry;
    if (reader == NULL || data == NULL || reader->cache_capacity == 0u) return FVIZ_OK;
    memory_bytes = fviz_data_object_memory_size(data);
    if (memory_bytes == 0u && fviz_last_error_code() != FVIZ_OK) return fviz_last_error_code();
    if (reader->cache_byte_capacity != 0u && memory_bytes > reader->cache_byte_capacity)
    {
        ++reader->cache_oversize_skips;
        return FVIZ_OK;
    }
    if (reader->cache_entries == NULL)
    {
        FVizSize allocation_bytes;
        if (fviz_size_multiply(reader->cache_capacity, sizeof(FVizPVDCacheEntry), &allocation_bytes) != FVIZ_OK)
            return fviz_last_error_code();
        reader->cache_entries = (FVizPVDCacheEntry*)fviz_alloc(allocation_bytes);
        if (reader->cache_entries == NULL) return fviz_last_error_code();
        (void)memset(reader->cache_entries, 0, allocation_bytes);
    }
    while (reader->cache_size != 0u &&
           (reader->cache_size >= reader->cache_capacity ||
            (reader->cache_byte_capacity != 0u && (memory_bytes > (FVizSize)-1 - reader->cache_bytes ||
                                                   reader->cache_bytes + memory_bytes > reader->cache_byte_capacity))))
    {
        fviz_pvd_cache_remove(reader, fviz_pvd_cache_lru_index(reader));
        ++reader->cache_evictions;
    }
    if (reader->cache_size >= reader->cache_capacity) return FVIZ_OK;
    entry = &reader->cache_entries[reader->cache_size++];
    (void)memset(entry, 0, sizeof(*entry));
    entry->index = index;
    entry->data = (FVizDataObject*)fviz_retain(data);
    if (entry->data == NULL)
    {
        --reader->cache_size;
        return fviz_last_error_code();
    }
    entry->memory_bytes = memory_bytes;
    entry->stamp = ++reader->cache_clock;
    if (memory_bytes > (FVizSize)-1 - reader->cache_bytes)
    {
        fviz_release(entry->data);
        --reader->cache_size;
        return FVIZ_ERROR_OVERFLOW;
    }
    reader->cache_bytes += memory_bytes;
    return FVIZ_OK;
}

static FVizResult fviz_pvd_load_time_group(FVizPVDReader* reader, FVizSize first_index, FVizDataObject** out_data)
{
    const FVizSize count = fviz_pvd_collection_count(reader->collection);
    const double selected = fviz_pvd_collection_time(reader->collection, first_index);
    FVizSize last_index = first_index + 1u;
    FVizDataObject* result = NULL;
    if (out_data != NULL) *out_data = NULL;
    if (reader == NULL || out_data == NULL || first_index >= count) return FVIZ_ERROR_INVALID_ARGUMENT;
    while (last_index < count && fviz_pvd_collection_time(reader->collection, last_index) == selected)
        ++last_index;
    result = fviz_pvd_cache_lookup(reader, first_index);
    if (result != NULL)
    {
        *out_data = result;
        return FVIZ_OK;
    }
    if (last_index - first_index == 1u)
    {
        if (fviz_pvd_load_entry(reader, first_index, &result) != FVIZ_OK) return fviz_last_error_code();
    }
    else
    {
        FVizPartitionedDataSet* partitions = NULL;
        FVizSize i;
        if (fviz_partitioned_data_set_create(&partitions) != FVIZ_OK ||
            fviz_partitioned_data_set_reserve(partitions, last_index - first_index) != FVIZ_OK)
        {
            fviz_release(partitions);
            return fviz_last_error_code();
        }
        for (i = first_index; i < last_index; ++i)
        {
            FVizDataObject* part = NULL;
            const char* group = fviz_pvd_collection_group(reader->collection, i);
            const char* file = fviz_pvd_collection_file(reader->collection, i);
            const char* name = group != NULL && group[0] != '\0' ? group : file;
            if (fviz_pvd_load_entry(reader, i, &part) != FVIZ_OK ||
                fviz_partitioned_data_set_add_partition(partitions, part, name, NULL) != FVIZ_OK)
            {
                fviz_release(part);
                fviz_release(partitions);
                return fviz_last_error_code();
            }
            fviz_release(part);
        }
        result = (FVizDataObject*)partitions;
    }
    if (fviz_pvd_cache_store(reader, first_index, result) != FVIZ_OK)
    {
        fviz_release(result);
        return fviz_last_error_code();
    }
    *out_data = result;
    return FVIZ_OK;
}

static FVizMTime fviz_pvd_reader_state_mtime(const void* state)
{
    return state != NULL ? fviz_object_mtime((const FVizObject*)state) : 0u;
}

static FVizResult fviz_pvd_reader_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                  void* state)
{
    FVizPVDReader* reader = (FVizPVDReader*)state;
    FVizSize count;
    if (fviz_pvd_reader_ensure_collection(reader) != FVIZ_OK) return fviz_last_error_code();
    count = fviz_pvd_collection_count(reader->collection);
    if (request->type == FVIZ_PIPELINE_REQUEST_INFORMATION)
    {
        double* unique_times;
        FVizSize unique_count = 0u;
        FVizSize i;
        unique_times = (double*)fviz_alloc(count * sizeof(double));
        if (unique_times == NULL && count != 0u) return fviz_last_error_code();
        for (i = 0u; i < count; ++i)
        {
            const double time = fviz_pvd_collection_time(reader->collection, i);
            if (unique_count == 0u || unique_times[unique_count - 1u] != time) unique_times[unique_count++] = time;
        }
        if (fviz_algorithm_set_output_time_steps(algorithm, request->requested_output_port, unique_times,
                                                 unique_count) != FVIZ_OK)
        {
            fviz_free(unique_times);
            return fviz_last_error_code();
        }
        fviz_free(unique_times);
        return FVIZ_OK;
    }
    if (request->type == FVIZ_PIPELINE_REQUEST_DATA)
    {
        FVizSize index = 0u;
        FVizDataObject* data = NULL;
        FVizResult load_result;
        const double requested =
            request->has_time != FVIZ_FALSE ? request->time : fviz_pvd_collection_time(reader->collection, 0u);
        if (fviz_pvd_collection_find_nearest(reader->collection, requested, &index) != FVIZ_OK)
            return fviz_last_error_code();
        if (request->number_of_pieces > 1u)
            load_result = fviz_pvd_load_time_piece(reader, index, request->piece, request->number_of_pieces,
                                                   request->ghost_levels, &data);
        else
            load_result = fviz_pvd_load_time_group(reader, index, &data);
        if (load_result != FVIZ_OK) return load_result;
        reader->selected_time = fviz_pvd_collection_time(reader->collection, index);
        if (fviz_algorithm_set_output_data(algorithm, request->requested_output_port, data) != FVIZ_OK)
        {
            fviz_release(data);
            return fviz_last_error_code();
        }
        fviz_release(data);
    }
    return FVIZ_OK;
}

FVizResult fviz_pvd_reader_create(FVizPVDReader** out_reader)
{
    FVizPVDReader* reader;
    FVizAlgorithmCallbacks callbacks;
    if (out_reader == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_reader must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_reader = NULL;
    reader = (FVizPVDReader*)fviz_internal_object_allocate(sizeof(*reader), &g_fviz_pvd_reader_class, NULL);
    if (reader == NULL) return fviz_last_error_code();
    reader->cache_capacity = 3u;
    reader->collection_dirty = FVIZ_TRUE;
    if (fviz_string_create(&reader->file_name) != FVIZ_OK)
    {
        fviz_release(reader);
        return fviz_last_error_code();
    }
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_pvd_reader_process_request;
    callbacks.get_state_mtime = fviz_pvd_reader_state_mtime;
    callbacks.state_object = (FVizObject*)reader;
    if (fviz_algorithm_create(0u, 1u, &callbacks, reader, &reader->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(reader->algorithm, 0u, FVIZ_TYPE_DATA_OBJECT) != FVIZ_OK)
    {
        fviz_release(reader);
        return fviz_last_error_code();
    }
    *out_reader = reader;
    return FVIZ_OK;
}

FVizResult fviz_pvd_reader_set_file_name(FVizPVDReader* reader, const char* file_path)
{
    if (reader == NULL || file_path == NULL || file_path[0] == '\0')
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PVD file name must not be empty");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (strcmp(fviz_string_c_str(reader->file_name), file_path) == 0) return FVIZ_OK;
    if (fviz_string_set(reader->file_name, file_path) != FVIZ_OK) return fviz_last_error_code();
    reader->collection_dirty = FVIZ_TRUE;
    fviz_release(reader->collection);
    reader->collection = NULL;
    fviz_pvd_reader_clear_cache(reader);
    fviz_release(reader->piece_reader);
    reader->piece_reader = NULL;
    reader->piece_reader_valid = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)reader);
    return FVIZ_OK;
}

const char* fviz_pvd_reader_file_name(const FVizPVDReader* reader)
{
    return reader != NULL && reader->file_name != NULL ? fviz_string_c_str(reader->file_name) : NULL;
}

const FVizPVDCollection* fviz_pvd_reader_collection(const FVizPVDReader* reader)
{
    return reader != NULL ? reader->collection : NULL;
}

FVizAlgorithm* fviz_pvd_reader_algorithm(FVizPVDReader* reader)
{
    return reader != NULL ? reader->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_pvd_reader_output_port(FVizPVDReader* reader)
{
    return reader != NULL ? fviz_algorithm_output_port(reader->algorithm, 0u) : NULL;
}

FVizDataObject* fviz_pvd_reader_output(FVizPVDReader* reader)
{
    return reader != NULL ? fviz_algorithm_output_data(reader->algorithm, 0u) : NULL;
}

FVizResult fviz_pvd_reader_update(FVizPVDReader* reader)
{
    return reader != NULL ? fviz_algorithm_update(reader->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_pvd_reader_update_time(FVizPVDReader* reader, double time)
{
    FVizPipelineRequestInfo request;
    if (reader == NULL || !isfinite(time)) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_pipeline_request_initialize(&request);
    if (fviz_pipeline_request_set_time(&request, time) != FVIZ_OK) return fviz_last_error_code();
    return fviz_executive_update_request(fviz_algorithm_executive(reader->algorithm), &request);
}

FVizResult fviz_pvd_reader_update_piece_time(FVizPVDReader* reader, double time, uint32_t piece,
                                             uint32_t number_of_pieces, uint32_t ghost_levels)
{
    FVizPipelineRequestInfo request;
    if (reader == NULL || !isfinite(time)) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_pipeline_request_initialize(&request);
    if (fviz_pipeline_request_set_time(&request, time) != FVIZ_OK ||
        fviz_pipeline_request_set_piece(&request, piece, number_of_pieces, ghost_levels) != FVIZ_OK)
        return fviz_last_error_code();
    return fviz_executive_update_request(fviz_algorithm_executive(reader->algorithm), &request);
}

FVizResult fviz_pvd_reader_piece_count_at_time(FVizPVDReader* reader, double time, uint32_t* out_piece_count)
{
    FVizSize index = 0u;
    if (out_piece_count != NULL) *out_piece_count = 0u;
    if (reader == NULL || out_piece_count == NULL || !isfinite(time)) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_pvd_reader_ensure_collection(reader) != FVIZ_OK ||
        fviz_pvd_collection_find_nearest(reader->collection, time, &index) != FVIZ_OK)
        return fviz_last_error_code();
    return fviz_pvd_piece_count_for_group(reader, index, out_piece_count);
}

double fviz_pvd_reader_selected_time(const FVizPVDReader* reader)
{
    return reader != NULL ? reader->selected_time : 0.0;
}

FVizResult fviz_pvd_reader_prefetch_time(FVizPVDReader* reader, double time)
{
    FVizSize index = 0u;
    FVizDataObject* data = NULL;
    if (reader == NULL || !isfinite(time))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PVD prefetch requires a finite time");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_pvd_reader_ensure_collection(reader) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_pvd_collection_find_nearest(reader->collection, time, &index) != FVIZ_OK ||
        fviz_pvd_load_time_group(reader, index, &data) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_release(data);
    return FVIZ_OK;
}

FVizResult fviz_pvd_reader_prefetch_piece_time(FVizPVDReader* reader, double time, uint32_t piece,
                                               uint32_t number_of_pieces, uint32_t ghost_levels)
{
    FVizSize index = 0u;
    FVizDataObject* data = NULL;
    FVizResult result;
    if (reader == NULL || !isfinite(time) || number_of_pieces == 0u || piece >= number_of_pieces)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_pvd_reader_ensure_collection(reader) != FVIZ_OK ||
        fviz_pvd_collection_find_nearest(reader->collection, time, &index) != FVIZ_OK)
        return fviz_last_error_code();
    result = fviz_pvd_load_time_piece(reader, index, piece, number_of_pieces, ghost_levels, &data);
    fviz_release(data);
    return result;
}

FVizResult fviz_pvd_reader_set_cache_capacity(FVizPVDReader* reader, FVizSize capacity)
{
    if (reader == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (reader->cache_capacity == capacity) return FVIZ_OK;
    fviz_pvd_reader_clear_cache(reader);
    reader->cache_capacity = capacity;
    fviz_object_modified((FVizObject*)reader);
    return FVIZ_OK;
}

FVizSize fviz_pvd_reader_cache_capacity(const FVizPVDReader* reader)
{
    return reader != NULL ? reader->cache_capacity : 0u;
}

FVizResult fviz_pvd_reader_set_cache_byte_capacity(FVizPVDReader* reader, FVizSize byte_capacity)
{
    if (reader == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (reader->cache_byte_capacity == byte_capacity) return FVIZ_OK;
    reader->cache_byte_capacity = byte_capacity;
    while (reader->cache_size != 0u && byte_capacity != 0u && reader->cache_bytes > byte_capacity)
    {
        fviz_pvd_cache_remove(reader, fviz_pvd_cache_lru_index(reader));
        ++reader->cache_evictions;
    }
    if (reader->piece_reader != NULL)
        (void)fviz_pvtu_reader_set_cache_byte_capacity(reader->piece_reader, byte_capacity);
    fviz_object_modified((FVizObject*)reader);
    return FVIZ_OK;
}

FVizSize fviz_pvd_reader_cache_byte_capacity(const FVizPVDReader* reader)
{
    return reader != NULL ? reader->cache_byte_capacity : 0u;
}

void fviz_pvd_reader_clear_cache(FVizPVDReader* reader)
{
    FVizSize i;
    if (reader == NULL) return;
    for (i = 0u; i < reader->cache_size; ++i)
        fviz_release(reader->cache_entries[i].data);
    fviz_free(reader->cache_entries);
    reader->cache_entries = NULL;
    reader->cache_size = 0u;
    reader->cache_bytes = 0u;
    reader->cache_clock = 0u;
    if (reader->piece_reader != NULL) fviz_pvtu_reader_clear_cache(reader->piece_reader);
}

FVizPVDCacheStatistics fviz_pvd_reader_cache_statistics(const FVizPVDReader* reader)
{
    FVizPVDCacheStatistics stats;
    (void)memset(&stats, 0, sizeof(stats));
    if (reader != NULL)
    {
        stats.capacity = reader->cache_capacity;
        stats.size = reader->cache_size;
        stats.hits = reader->cache_hits;
        stats.misses = reader->cache_misses;
        stats.evictions = reader->cache_evictions;
        stats.byte_capacity = reader->cache_byte_capacity;
        stats.bytes = reader->cache_bytes;
        stats.oversize_skips = reader->cache_oversize_skips;
    }
    return stats;
}
