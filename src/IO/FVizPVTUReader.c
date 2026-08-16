#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataObject.h>
#include <FViz/IO/FVizPVTUReader.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

#define FVIZ_PVTU_DEFAULT_MAX_FILE_BYTES ((FVizSize)16u * 1024u * 1024u)
#define FVIZ_PVTU_DEFAULT_MAX_PIECES ((FVizSize)65536u)

#define FVIZ_PVTU_DEFAULT_CACHE_CAPACITY ((FVizSize)4u)

typedef struct FVizPVTUCacheEntry
{
    FVizSize index;
    FVizUnstructuredGrid* data;
    uint64_t stamp;
    FVizSize memory_bytes;
} FVizPVTUCacheEntry;

struct FVizPVTUReader
{
    FVizObject base;
    FVizPVTUReaderOptions options;
    char* file_name;
    char** piece_sources;
    char** piece_paths;
    FVizSize piece_count;
    uint32_t ghost_level;
    FVizPVTUCacheEntry* cache_entries;
    FVizSize cache_capacity;
    FVizSize cache_size;
    FVizSize cache_byte_capacity;
    FVizSize cache_bytes;
    uint64_t cache_clock;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t cache_evictions;
    uint64_t cache_oversize_skips;
};

static void fviz_pvtu_reader_clear_manifest(FVizPVTUReader* reader);

static void fviz_pvtu_reader_destroy(FVizObject* object)
{
    FVizPVTUReader* reader = (FVizPVTUReader*)object;
    fviz_pvtu_reader_clear_cache(reader);
    fviz_pvtu_reader_clear_manifest(reader);
}

static const FVizObjectClass g_fviz_pvtu_reader_class = {FVIZ_TYPE_PVTU_READER, "FVizPVTUReader", &g_fviz_object_class,
                                                         fviz_pvtu_reader_destroy, NULL};

static FVizBool fviz_pvtu_attr_string(const char* tag_begin, const char* tag_end, const char* attribute,
                                      char* out_value, FVizSize out_capacity)
{
    char pattern[64];
    const char* found;
    const char* value_end;
    FVizSize length;
    if (tag_begin == NULL || tag_end == NULL || attribute == NULL || out_value == NULL || out_capacity == 0u ||
        tag_begin >= tag_end)
        return FVIZ_FALSE;
    if (snprintf(pattern, sizeof(pattern), "%s=\"", attribute) < 0) return FVIZ_FALSE;
    found = strstr(tag_begin, pattern);
    if (found == NULL || found >= tag_end) return FVIZ_FALSE;
    found += strlen(pattern);
    value_end = strchr(found, '"');
    if (value_end == NULL || value_end > tag_end) return FVIZ_FALSE;
    length = (FVizSize)(value_end - found);
    if (length >= out_capacity) return FVIZ_FALSE;
    (void)memcpy(out_value, found, length);
    out_value[length] = '\0';
    return FVIZ_TRUE;
}

static FVizBool fviz_pvtu_path_is_absolute(const char* path)
{
    if (path == NULL || path[0] == '\0') return FVIZ_FALSE;
    if (path[0] == '/' || path[0] == '\\') return FVIZ_TRUE;
    return isalpha((unsigned char)path[0]) && path[1] == ':' ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_pvtu_resolve_path(const char* manifest_path, const char* child_path, char** out_path)
{
    const char* slash_a;
    const char* slash_b;
    const char* slash;
    FVizSize prefix = 0u;
    FVizSize child_length;
    FVizSize total;
    char* result;
    if (manifest_path == NULL || child_path == NULL || out_path == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_path = NULL;
    child_length = strlen(child_path);
    if (fviz_pvtu_path_is_absolute(child_path) != FVIZ_FALSE)
    {
        result = (char*)fviz_alloc(child_length + 1u);
        if (result == NULL) return fviz_last_error_code();
        (void)memcpy(result, child_path, child_length + 1u);
        *out_path = result;
        return FVIZ_OK;
    }
    slash_a = strrchr(manifest_path, '/');
    slash_b = strrchr(manifest_path, '\\');
    slash = slash_a != NULL && (slash_b == NULL || slash_a > slash_b) ? slash_a : slash_b;
    if (slash != NULL) prefix = (FVizSize)(slash - manifest_path) + 1u;
    if (fviz_size_add(prefix, child_length, &total) != FVIZ_OK || fviz_size_add(total, 1u, &total) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    result = (char*)fviz_alloc(total);
    if (result == NULL) return fviz_last_error_code();
    if (prefix != 0u) (void)memcpy(result, manifest_path, prefix);
    (void)memcpy(result + prefix, child_path, child_length + 1u);
    *out_path = result;
    return FVIZ_OK;
}

static FVizResult fviz_pvtu_read_text(const char* file_path, FVizSize maximum_bytes, char** out_text,
                                      FVizSize* out_size)
{
    FILE* file;
    long file_size_long;
    FVizSize file_size;
    char* text;
    if (file_path == NULL || out_text == NULL || out_size == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_text = NULL;
    *out_size = 0u;
    file = fopen(file_path, "rb");
    if (file == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to open PVTU manifest");
        return FVIZ_ERROR_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (file_size_long = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to determine PVTU manifest size");
        return FVIZ_ERROR_IO;
    }
    file_size = (FVizSize)file_size_long;
    if (file_size > maximum_bytes)
    {
        fclose(file);
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "PVTU manifest exceeds configured size limit");
        return FVIZ_ERROR_OVERFLOW;
    }
    text = (char*)fviz_alloc(file_size + 1u);
    if (text == NULL)
    {
        fclose(file);
        return fviz_last_error_code();
    }
    if (file_size != 0u && fread(text, 1u, file_size, file) != file_size)
    {
        fviz_free(text);
        fclose(file);
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to read PVTU manifest");
        return FVIZ_ERROR_IO;
    }
    fclose(file);
    text[file_size] = '\0';
    *out_text = text;
    *out_size = file_size;
    return FVIZ_OK;
}

static char* fviz_pvtu_copy_string(const char* value)
{
    const FVizSize length = value != NULL ? strlen(value) : 0u;
    char* copy;
    if (value == NULL) return NULL;
    copy = (char*)fviz_alloc(length + 1u);
    if (copy != NULL) (void)memcpy(copy, value, length + 1u);
    return copy;
}

static void fviz_pvtu_reader_clear_manifest(FVizPVTUReader* reader)
{
    FVizSize i;
    if (reader == NULL) return;
    for (i = 0u; i < reader->piece_count; ++i)
    {
        fviz_free(reader->piece_sources != NULL ? reader->piece_sources[i] : NULL);
        fviz_free(reader->piece_paths != NULL ? reader->piece_paths[i] : NULL);
    }
    fviz_free(reader->piece_sources);
    fviz_free(reader->piece_paths);
    fviz_free(reader->file_name);
    reader->piece_sources = NULL;
    reader->piece_paths = NULL;
    reader->file_name = NULL;
    reader->piece_count = 0u;
    reader->ghost_level = 0u;
}

void fviz_pvtu_reader_clear_cache(FVizPVTUReader* reader)
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
}

static FVizUnstructuredGrid* fviz_pvtu_reader_cache_lookup(FVizPVTUReader* reader, FVizSize piece_index)
{
    FVizSize i;
    if (reader == NULL || reader->cache_capacity == 0u) return NULL;
    for (i = 0u; i < reader->cache_size; ++i)
    {
        FVizPVTUCacheEntry* entry = &reader->cache_entries[i];
        if (entry->index == piece_index)
        {
            entry->stamp = ++reader->cache_clock;
            ++reader->cache_hits;
            return (FVizUnstructuredGrid*)fviz_retain(entry->data);
        }
    }
    ++reader->cache_misses;
    return NULL;
}

static void fviz_pvtu_reader_cache_remove(FVizPVTUReader* reader, FVizSize index)
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

static FVizSize fviz_pvtu_reader_lru_index(const FVizPVTUReader* reader)
{
    FVizSize i;
    FVizSize index = 0u;
    if (reader == NULL || reader->cache_size == 0u) return 0u;
    for (i = 1u; i < reader->cache_size; ++i)
        if (reader->cache_entries[i].stamp < reader->cache_entries[index].stamp) index = i;
    return index;
}

static FVizResult fviz_pvtu_reader_cache_store(FVizPVTUReader* reader, FVizSize piece_index, FVizUnstructuredGrid* data)
{
    FVizSize memory_bytes;
    FVizPVTUCacheEntry* entry;
    if (reader == NULL || data == NULL || reader->cache_capacity == 0u) return FVIZ_OK;
    memory_bytes = fviz_data_object_memory_size((const FVizDataObject*)data);
    if (memory_bytes == 0u && fviz_last_error_code() != FVIZ_OK) return fviz_last_error_code();
    if (reader->cache_byte_capacity != 0u && memory_bytes > reader->cache_byte_capacity)
    {
        ++reader->cache_oversize_skips;
        return FVIZ_OK;
    }
    if (reader->cache_entries == NULL)
    {
        FVizSize bytes;
        if (fviz_size_multiply(reader->cache_capacity, sizeof(FVizPVTUCacheEntry), &bytes) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        reader->cache_entries = (FVizPVTUCacheEntry*)fviz_alloc(bytes);
        if (reader->cache_entries == NULL) return fviz_last_error_code();
        (void)memset(reader->cache_entries, 0, bytes);
    }
    while (reader->cache_size != 0u &&
           (reader->cache_size >= reader->cache_capacity ||
            (reader->cache_byte_capacity != 0u && (memory_bytes > (FVizSize)-1 - reader->cache_bytes ||
                                                   reader->cache_bytes + memory_bytes > reader->cache_byte_capacity))))
    {
        fviz_pvtu_reader_cache_remove(reader, fviz_pvtu_reader_lru_index(reader));
        ++reader->cache_evictions;
    }
    if (reader->cache_size >= reader->cache_capacity) return FVIZ_OK;
    entry = &reader->cache_entries[reader->cache_size++];
    (void)memset(entry, 0, sizeof(*entry));
    entry->index = piece_index;
    entry->data = (FVizUnstructuredGrid*)fviz_retain(data);
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

void fviz_pvtu_reader_options_initialize(FVizPVTUReaderOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->maximum_file_bytes = FVIZ_PVTU_DEFAULT_MAX_FILE_BYTES;
    options->maximum_pieces = FVIZ_PVTU_DEFAULT_MAX_PIECES;
    fviz_vtu_reader_options_initialize(&options->piece_options);
}

FVizResult fviz_pvtu_reader_create(FVizPVTUReader** out_reader)
{
    FVizPVTUReaderOptions options;
    fviz_pvtu_reader_options_initialize(&options);
    return fviz_pvtu_reader_create_with_options(&options, out_reader);
}

FVizResult fviz_pvtu_reader_create_with_options(const FVizPVTUReaderOptions* options, FVizPVTUReader** out_reader)
{
    FVizPVTUReader* reader;
    if (out_reader == NULL || options == NULL || options->struct_size < sizeof(*options) ||
        options->maximum_file_bytes == 0u || options->maximum_pieces == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_reader = NULL;
    reader = (FVizPVTUReader*)fviz_internal_object_allocate(sizeof(*reader), &g_fviz_pvtu_reader_class, NULL);
    if (reader == NULL) return fviz_last_error_code();
    reader->options = *options;
    reader->cache_capacity = FVIZ_PVTU_DEFAULT_CACHE_CAPACITY;
    *out_reader = reader;
    return FVIZ_OK;
}

FVizResult fviz_pvtu_reader_set_file_name(FVizPVTUReader* reader, const char* file_path)
{
    char* text = NULL;
    FVizSize text_size = 0u;
    const char* grid_begin;
    const char* grid_end;
    const char* cursor;
    FVizSize piece_count = 0u;
    char** sources = NULL;
    char** paths = NULL;
    char* file_copy = NULL;
    uint32_t ghost_level = 0u;
    FVizSize index = 0u;
    FVizResult result;
    if (reader == NULL || file_path == NULL || file_path[0] == '\0') return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_pvtu_read_text(file_path, reader->options.maximum_file_bytes, &text, &text_size);
    if (result != FVIZ_OK) return result;
    (void)text_size;
    grid_begin = strstr(text, "<PUnstructuredGrid");
    if (grid_begin == NULL || (grid_end = strstr(grid_begin, "</PUnstructuredGrid>")) == NULL)
    {
        result = FVIZ_ERROR_PARSE;
        fviz_internal_set_error(FVIZ_ERROR_PARSE, "PVTU manifest has no PUnstructuredGrid element");
        goto cleanup;
    }
    {
        const char* tag_end = strchr(grid_begin, '>');
        char ghost_text[32];
        if (tag_end == NULL || tag_end > grid_end)
        {
            result = FVIZ_ERROR_PARSE;
            goto cleanup;
        }
        if (fviz_pvtu_attr_string(grid_begin, tag_end, "GhostLevel", ghost_text, sizeof(ghost_text)) != FVIZ_FALSE)
        {
            char* end = NULL;
            const unsigned long value = strtoul(ghost_text, &end, 10);
            if (end != ghost_text && *end == '\0' && value <= UINT32_MAX) ghost_level = (uint32_t)value;
        }
    }
    cursor = grid_begin;
    while ((cursor = strstr(cursor, "<Piece")) != NULL && cursor < grid_end)
    {
        const char* tag_end = strchr(cursor, '>');
        if (tag_end == NULL || tag_end > grid_end)
        {
            result = FVIZ_ERROR_PARSE;
            fviz_internal_set_error(FVIZ_ERROR_PARSE, "PVTU Piece tag is malformed");
            goto cleanup;
        }
        ++piece_count;
        if (piece_count > reader->options.maximum_pieces)
        {
            result = FVIZ_ERROR_OVERFLOW;
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "PVTU piece count exceeds configured limit");
            goto cleanup;
        }
        cursor = tag_end + 1;
    }
    if (piece_count == 0u)
    {
        result = FVIZ_ERROR_PARSE;
        fviz_internal_set_error(FVIZ_ERROR_PARSE, "PVTU manifest contains no Piece elements");
        goto cleanup;
    }
    sources = (char**)fviz_alloc(piece_count * sizeof(*sources));
    paths = (char**)fviz_alloc(piece_count * sizeof(*paths));
    if (sources == NULL || paths == NULL)
    {
        result = fviz_last_error_code();
        goto cleanup;
    }
    (void)memset(sources, 0, piece_count * sizeof(*sources));
    (void)memset(paths, 0, piece_count * sizeof(*paths));
    file_copy = fviz_pvtu_copy_string(file_path);
    if (file_copy == NULL)
    {
        result = fviz_last_error_code();
        goto cleanup;
    }
    cursor = grid_begin;
    while ((cursor = strstr(cursor, "<Piece")) != NULL && cursor < grid_end)
    {
        const char* tag_end = strchr(cursor, '>');
        char source[2048];
        if (tag_end == NULL || tag_end > grid_end ||
            fviz_pvtu_attr_string(cursor, tag_end, "Source", source, sizeof(source)) == FVIZ_FALSE || source[0] == '\0')
        {
            result = FVIZ_ERROR_PARSE;
            fviz_internal_set_error(FVIZ_ERROR_PARSE, "PVTU Piece is missing a valid Source attribute");
            goto cleanup;
        }
        sources[index] = fviz_pvtu_copy_string(source);
        if (sources[index] == NULL)
        {
            result = fviz_last_error_code();
            goto cleanup;
        }
        result = fviz_pvtu_resolve_path(file_path, source, &paths[index]);
        if (result != FVIZ_OK) goto cleanup;
        ++index;
        cursor = tag_end + 1;
    }
    fviz_pvtu_reader_clear_cache(reader);
    fviz_pvtu_reader_clear_manifest(reader);
    reader->file_name = file_copy;
    reader->piece_sources = sources;
    reader->piece_paths = paths;
    reader->piece_count = piece_count;
    reader->ghost_level = ghost_level;
    file_copy = NULL;
    sources = NULL;
    paths = NULL;
    fviz_object_modified((FVizObject*)reader);
    result = FVIZ_OK;

cleanup:
    if (sources != NULL || paths != NULL)
    {
        for (index = 0u; index < piece_count; ++index)
        {
            fviz_free(sources != NULL ? sources[index] : NULL);
            fviz_free(paths != NULL ? paths[index] : NULL);
        }
    }
    fviz_free(sources);
    fviz_free(paths);
    fviz_free(file_copy);
    fviz_free(text);
    return result;
}

const char* fviz_pvtu_reader_file_name(const FVizPVTUReader* reader)
{
    return reader != NULL ? reader->file_name : NULL;
}

FVizSize fviz_pvtu_reader_piece_count(const FVizPVTUReader* reader)
{
    return reader != NULL ? reader->piece_count : 0u;
}

const char* fviz_pvtu_reader_piece_source(const FVizPVTUReader* reader, FVizSize piece_index)
{
    return reader != NULL && piece_index < reader->piece_count ? reader->piece_sources[piece_index] : NULL;
}

uint32_t fviz_pvtu_reader_ghost_level(const FVizPVTUReader* reader)
{
    return reader != NULL ? reader->ghost_level : 0u;
}

FVizResult fviz_pvtu_reader_load_piece(FVizPVTUReader* reader, FVizSize piece_index, FVizUnstructuredGrid** out_piece)
{
    FVizUnstructuredGrid* piece;
    FVizResult result;
    if (reader == NULL || out_piece == NULL || reader->file_name == NULL || piece_index >= reader->piece_count)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_piece = NULL;
    piece = fviz_pvtu_reader_cache_lookup(reader, piece_index);
    if (piece != NULL)
    {
        *out_piece = piece;
        return FVIZ_OK;
    }
    result = fviz_vtu_read_with_options(reader->piece_paths[piece_index], &reader->options.piece_options, &piece);
    if (result != FVIZ_OK) return result;
    result = fviz_pvtu_reader_cache_store(reader, piece_index, piece);
    if (result != FVIZ_OK)
    {
        fviz_release(piece);
        return result;
    }
    *out_piece = piece;
    return FVIZ_OK;
}

FVizResult fviz_pvtu_reader_prefetch_piece(FVizPVTUReader* reader, FVizSize piece_index)
{
    FVizUnstructuredGrid* piece = NULL;
    const FVizResult result = fviz_pvtu_reader_load_piece(reader, piece_index, &piece);
    fviz_release(piece);
    return result;
}

FVizResult fviz_pvtu_reader_materialize(FVizPVTUReader* reader, FVizPartitionedDataSet** out_data_set)
{
    FVizPartitionedDataSet* output = NULL;
    FVizSize i;
    FVizResult result;
    if (reader == NULL || out_data_set == NULL || reader->file_name == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_data_set = NULL;
    result = fviz_partitioned_data_set_create(&output);
    if (result == FVIZ_OK) result = fviz_partitioned_data_set_reserve(output, reader->piece_count);
    for (i = 0u; result == FVIZ_OK && i < reader->piece_count; ++i)
    {
        FVizUnstructuredGrid* piece = NULL;
        result = fviz_pvtu_reader_load_piece(reader, i, &piece);
        if (result == FVIZ_OK)
            result =
                fviz_partitioned_data_set_add_partition(output, (FVizDataObject*)piece, reader->piece_sources[i], NULL);
        fviz_release(piece);
    }
    if (result != FVIZ_OK)
    {
        fviz_release(output);
        return result;
    }
    *out_data_set = output;
    return FVIZ_OK;
}

FVizResult fviz_pvtu_reader_set_cache_capacity(FVizPVTUReader* reader, FVizSize capacity)
{
    if (reader == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (reader->cache_capacity == capacity) return FVIZ_OK;
    fviz_pvtu_reader_clear_cache(reader);
    reader->cache_capacity = capacity;
    return FVIZ_OK;
}

FVizSize fviz_pvtu_reader_cache_capacity(const FVizPVTUReader* reader)
{
    return reader != NULL ? reader->cache_capacity : 0u;
}

FVizResult fviz_pvtu_reader_set_cache_byte_capacity(FVizPVTUReader* reader, FVizSize byte_capacity)
{
    if (reader == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (reader->cache_byte_capacity == byte_capacity) return FVIZ_OK;
    reader->cache_byte_capacity = byte_capacity;
    while (reader->cache_size != 0u && byte_capacity != 0u && reader->cache_bytes > byte_capacity)
    {
        fviz_pvtu_reader_cache_remove(reader, fviz_pvtu_reader_lru_index(reader));
        ++reader->cache_evictions;
    }
    fviz_object_modified((FVizObject*)reader);
    return FVIZ_OK;
}

FVizSize fviz_pvtu_reader_cache_byte_capacity(const FVizPVTUReader* reader)
{
    return reader != NULL ? reader->cache_byte_capacity : 0u;
}

FVizPVTUCacheStatistics fviz_pvtu_reader_cache_statistics(const FVizPVTUReader* reader)
{
    FVizPVTUCacheStatistics stats;
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

FVizResult fviz_pvtu_read(const char* file_path, FVizPartitionedDataSet** out_data_set)
{
    FVizPVTUReaderOptions options;
    fviz_pvtu_reader_options_initialize(&options);
    return fviz_pvtu_read_with_options(file_path, &options, out_data_set);
}

FVizResult fviz_pvtu_read_with_options(const char* file_path, const FVizPVTUReaderOptions* options,
                                       FVizPartitionedDataSet** out_data_set)
{
    FVizPVTUReader* reader = NULL;
    FVizResult result = fviz_pvtu_reader_create_with_options(options, &reader);
    if (result == FVIZ_OK) result = fviz_pvtu_reader_set_file_name(reader, file_path);
    if (result == FVIZ_OK) result = fviz_pvtu_reader_materialize(reader, out_data_set);
    fviz_release(reader);
    return result;
}
