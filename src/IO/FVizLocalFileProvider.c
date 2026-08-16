#include <ctype.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/IO/FVizLocalFileProvider.h>
#include <FViz/IO/FVizMeshReader.h>
#include <FViz/IO/FVizVTKLegacyReader.h>
#include <FViz/IO/FVizVTPReader.h>
#include <FViz/IO/FVizVTUReader.h>

#include <FViz/Core/FVizErrorInternal.h>

typedef struct FVizLocalFileEntry
{
    uint64_t key;
    char* path;
} FVizLocalFileEntry;

typedef struct FVizLocalFileState
{
    FVizLocalFileEntry* entries;
    FVizSize count;
    FVizSize capacity;
} FVizLocalFileState;

struct FVizLocalFileProvider
{
    FVizDataProvider* provider;
    FVizLocalFileState* state;
};

static const char* fviz_local_extension(const char* path)
{
    const char* dot;
    const char* slash;
    if (path == NULL) return NULL;
    dot = strrchr(path, '.');
    slash = strrchr(path, '/');
#if defined(_WIN32)
    {
        const char* backslash = strrchr(path, '\\');
        if (backslash != NULL && (slash == NULL || backslash > slash)) slash = backslash;
    }
#endif
    return dot != NULL && (slash == NULL || dot > slash) ? dot + 1 : NULL;
}

static FVizBool fviz_local_extension_equals(const char* extension, const char* expected)
{
    if (extension == NULL) return FVIZ_FALSE;
    while (*extension != '\0' && *expected != '\0')
    {
        if (tolower((unsigned char)*extension) != tolower((unsigned char)*expected)) return FVIZ_FALSE;
        ++extension;
        ++expected;
    }
    return *extension == '\0' && *expected == '\0' ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_local_file_provider_format_supported(const char* path)
{
    const char* extension = fviz_local_extension(path);
    return fviz_mesh_format_supported(path) != FVIZ_FALSE ||
                   fviz_local_extension_equals(extension, "vtu") != FVIZ_FALSE ||
                   fviz_local_extension_equals(extension, "vtp") != FVIZ_FALSE ||
                   fviz_local_extension_equals(extension, "vtk") != FVIZ_FALSE
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

static FVizResult fviz_local_file_fetch(const FVizDataProviderRequest* request, void* user_data,
                                        FVizDataObject** out_data)
{
    FVizLocalFileState* state = (FVizLocalFileState*)user_data;
    const char* path = NULL;
    const char* extension;
    FVizSize i;
    *out_data = NULL;
    if (request->pipeline.type != FVIZ_PIPELINE_REQUEST_DATA || request->pipeline.has_extent != FVIZ_FALSE ||
        request->pipeline.has_time != FVIZ_FALSE || request->pipeline.piece != 0u ||
        request->pipeline.number_of_pieces != 1u || request->pipeline.ghost_levels != 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "local file provider requires a whole-resource DATA request");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    for (i = 0u; i < state->count; ++i)
        if (state->entries[i].key == request->resource_key)
        {
            path = state->entries[i].path;
            break;
        }
    if (path == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "local provider resource key is not registered");
        return FVIZ_ERROR_NOT_FOUND;
    }
    extension = fviz_local_extension(path);
    if (fviz_mesh_format_supported(path) != FVIZ_FALSE)
    {
        FVizPolyData* mesh = NULL;
        FVizResult result = fviz_mesh_read(path, &mesh);
        *out_data = (FVizDataObject*)mesh;
        return result;
    }
    if (fviz_local_extension_equals(extension, "vtu") != FVIZ_FALSE)
    {
        FVizUnstructuredGrid* grid = NULL;
        FVizResult result = fviz_vtu_read(path, &grid);
        *out_data = (FVizDataObject*)grid;
        return result;
    }
    if (fviz_local_extension_equals(extension, "vtp") != FVIZ_FALSE)
    {
        FVizPolyData* mesh = NULL;
        FVizResult result = fviz_vtp_read(path, &mesh);
        *out_data = (FVizDataObject*)mesh;
        return result;
    }
    if (fviz_local_extension_equals(extension, "vtk") != FVIZ_FALSE)
    {
        FVizUnstructuredGrid* grid = NULL;
        FVizResult result = fviz_vtk_legacy_read(path, &grid);
        *out_data = (FVizDataObject*)grid;
        return result;
    }
    fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "local provider file format is unsupported");
    return FVIZ_ERROR_NOT_SUPPORTED;
}

static void fviz_local_file_state_destroy(void* user_data)
{
    FVizLocalFileState* state = (FVizLocalFileState*)user_data;
    FVizSize i;
    if (state == NULL) return;
    for (i = 0u; i < state->count; ++i)
        fviz_free(state->entries[i].path);
    fviz_free(state->entries);
    fviz_free(state);
}

void fviz_local_file_provider_options_initialize(FVizLocalFileProviderOptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    fviz_data_provider_options_initialize(&options->provider);
}

FVizResult fviz_local_file_provider_create(const FVizLocalFileProviderOptions* options,
                                           FVizLocalFileProvider** out_provider)
{
    FVizLocalFileProviderOptions defaults;
    FVizDataProviderCallbacks callbacks;
    FVizLocalFileProvider* local;
    FVizLocalFileState* state;
    FVizResult result;
    if (out_provider == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_provider = NULL;
    if (options == NULL)
    {
        fviz_local_file_provider_options_initialize(&defaults);
        options = &defaults;
    }
    if (options->struct_size < sizeof(*options) || options->provider.struct_size < sizeof(options->provider))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    local = (FVizLocalFileProvider*)fviz_alloc(sizeof(*local));
    state = (FVizLocalFileState*)fviz_alloc(sizeof(*state));
    if (local == NULL || state == NULL)
    {
        fviz_free(local);
        fviz_free(state);
        return fviz_last_error_code();
    }
    memset(local, 0, sizeof(*local));
    memset(state, 0, sizeof(*state));
    fviz_data_provider_callbacks_initialize(&callbacks);
    callbacks.fetch = fviz_local_file_fetch;
    callbacks.destroy = fviz_local_file_state_destroy;
    result = fviz_data_provider_create(&callbacks, state, &options->provider, &local->provider);
    if (result != FVIZ_OK)
    {
        fviz_free(local);
        fviz_local_file_state_destroy(state);
        return result;
    }
    local->state = state;
    *out_provider = local;
    return FVIZ_OK;
}

void fviz_local_file_provider_destroy(FVizLocalFileProvider* provider)
{
    if (provider == NULL) return;
    fviz_data_provider_destroy(provider->provider);
    provider->provider = NULL;
    provider->state = NULL;
    fviz_free(provider);
}

FVizResult fviz_local_file_provider_register(FVizLocalFileProvider* provider, uint64_t resource_key, const char* path)
{
    FVizSize i;
    FVizSize length;
    char* copy;
    if (provider == NULL || path == NULL || path[0] == '\0') return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_local_file_provider_format_supported(path) == FVIZ_FALSE) return FVIZ_ERROR_NOT_SUPPORTED;
    length = (FVizSize)strlen(path) + 1u;
    copy = (char*)fviz_alloc(length);
    if (copy == NULL) return fviz_last_error_code();
    (void)memcpy(copy, path, length);
    for (i = 0u; i < provider->state->count; ++i)
        if (provider->state->entries[i].key == resource_key)
        {
            fviz_free(provider->state->entries[i].path);
            provider->state->entries[i].path = copy;
            fviz_data_provider_clear_cache(provider->provider);
            return FVIZ_OK;
        }
    if (provider->state->count == provider->state->capacity)
    {
        const FVizSize capacity = provider->state->capacity == 0u ? 8u : provider->state->capacity * 2u;
        FVizLocalFileEntry* entries;
        if (capacity < provider->state->capacity || capacity > (FVizSize)-1 / sizeof(*entries))
        {
            fviz_free(copy);
            return FVIZ_ERROR_OVERFLOW;
        }
        entries = (FVizLocalFileEntry*)fviz_realloc(provider->state->entries, capacity * sizeof(*entries));
        if (entries == NULL)
        {
            fviz_free(copy);
            return fviz_last_error_code();
        }
        provider->state->entries = entries;
        provider->state->capacity = capacity;
    }
    provider->state->entries[provider->state->count].key = resource_key;
    provider->state->entries[provider->state->count].path = copy;
    ++provider->state->count;
    return FVIZ_OK;
}

FVizResult fviz_local_file_provider_unregister(FVizLocalFileProvider* provider, uint64_t resource_key)
{
    FVizSize i;
    if (provider == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < provider->state->count; ++i)
        if (provider->state->entries[i].key == resource_key)
        {
            fviz_free(provider->state->entries[i].path);
            if (i + 1u < provider->state->count)
                memmove(&provider->state->entries[i], &provider->state->entries[i + 1u],
                        (provider->state->count - i - 1u) * sizeof(*provider->state->entries));
            --provider->state->count;
            fviz_data_provider_clear_cache(provider->provider);
            return FVIZ_OK;
        }
    return FVIZ_ERROR_NOT_FOUND;
}

FVizDataProvider* fviz_local_file_provider_data_provider(FVizLocalFileProvider* provider)
{
    return provider != NULL ? provider->provider : NULL;
}
