#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizFont.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizAtomic.h>
#include <FViz/Rendering/FVizFontPrivate.h>

#define FVIZ_FONT_CACHE_CAPACITY 8u
typedef struct FVizFontCacheEntry { FVizFont* font; char family[64]; float pixel_size; } FVizFontCacheEntry;
static FVizFontCacheEntry g_fviz_font_cache[FVIZ_FONT_CACHE_CAPACITY];
static FVizSpinLock g_fviz_font_cache_lock = {0};
static uint32_t g_fviz_font_cache_next = 0u;
#define FVIZ_FILE_FONT_CACHE_CAPACITY 4u
typedef struct FVizFileFontCacheEntry { FVizFont* font; char path[260]; char family[64]; float pixel_size; uint64_t file_stamp; } FVizFileFontCacheEntry;
static FVizFileFontCacheEntry g_fviz_file_font_cache[FVIZ_FILE_FONT_CACHE_CAPACITY];
static uint32_t g_fviz_file_font_cache_next = 0u;

static uint64_t fviz_font_file_stamp(const char* path)
{
#if defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA data;
    ULARGE_INTEGER value;
    if (path == NULL || GetFileAttributesExA(path, GetFileExInfoStandard, &data) == 0) return 0u;
    value.LowPart = data.ftLastWriteTime.dwLowDateTime;
    value.HighPart = data.ftLastWriteTime.dwHighDateTime;
    return value.QuadPart;
#else
    (void)path;
    return 0u;
#endif
}

static void fviz_font_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_font_class = {
    FVIZ_TYPE_FONT, "FVizFont", &g_fviz_object_class,
    fviz_font_destroy, NULL
};
static void fviz_font_destroy(FVizObject* object)
{
    FVizFont* font = (FVizFont*)object;
    fviz_release(font->family);
    fviz_release(font->atlas);
    font->family = NULL;
    font->atlas = NULL;
}
FVizResult fviz_font_create_from_atlas(const char* family, FVizFontAtlas* atlas, FVizFont** out_font)
{
    FVizFont* font;
    if (out_font == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_font must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_font = NULL;
    if (family == NULL || atlas == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "family and atlas must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    font = (FVizFont*)fviz_internal_object_allocate(sizeof(*font), &g_fviz_font_class, NULL);
    if (font == NULL) return fviz_last_error_code();
    if (fviz_string_create_from(family, &font->family) != FVIZ_OK || fviz_retain(atlas) == NULL)
    {
        fviz_release(font);
        return fviz_last_error_code();
    }
    font->atlas = atlas;
    *out_font = font;
    return FVIZ_OK;
}

FVizResult fviz_font_create_builtin(FVizFont** out_font)
{
    FVizFontAtlas* atlas = NULL;
    FVizResult result;
    if (out_font == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_font must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_font = NULL;
    if (fviz_font_atlas_create_builtin(&atlas) != FVIZ_OK) return fviz_last_error_code();
    result = fviz_font_create_from_atlas("FEAViz Builtin Mono", atlas, out_font);
    fviz_release(atlas);
    return result;
}

FVizResult fviz_font_create_system(const char* family, float pixel_size, FVizFont** out_font)
{
    FVizFontAtlas* atlas = NULL;
    FVizFont* cached;
    FVizResult result;
    if (family == NULL || out_font == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_font = NULL;
    fviz_spin_lock(&g_fviz_font_cache_lock);
    {
        uint32_t i;
        for (i = 0u; i < FVIZ_FONT_CACHE_CAPACITY; ++i)
        {
            if (g_fviz_font_cache[i].font != NULL &&
                g_fviz_font_cache[i].pixel_size == pixel_size &&
                strcmp(g_fviz_font_cache[i].family, family) == 0)
            {
                cached = (FVizFont*)fviz_retain(g_fviz_font_cache[i].font);
                fviz_spin_unlock(&g_fviz_font_cache_lock);
                if (cached == NULL) return fviz_last_error_code();
                *out_font = cached;
                return FVIZ_OK;
            }
        }
    }
    result = fviz_font_atlas_create_system(family, pixel_size, &atlas);
    if (result != FVIZ_OK) { fviz_spin_unlock(&g_fviz_font_cache_lock); return result; }
    result = fviz_font_create_from_atlas(family, atlas, out_font);
    fviz_release(atlas);
    if (result == FVIZ_OK)
    {
        const uint32_t slot = g_fviz_font_cache_next++ % FVIZ_FONT_CACHE_CAPACITY;
        fviz_release(g_fviz_font_cache[slot].font);
        g_fviz_font_cache[slot].font = (FVizFont*)fviz_retain(*out_font);
        (void)strncpy(g_fviz_font_cache[slot].family, family,
            sizeof(g_fviz_font_cache[slot].family) - 1u);
        g_fviz_font_cache[slot].family[sizeof(g_fviz_font_cache[slot].family) - 1u] = '\0';
        g_fviz_font_cache[slot].pixel_size = pixel_size;
    }
    fviz_spin_unlock(&g_fviz_font_cache_lock);
    return result;
}

FVizResult fviz_font_create_from_file(
    const char* file_path, const char* family, float pixel_size, FVizFont** out_font)
{
    FVizFontAtlas* atlas = NULL;
    FVizFont* cached;
    uint64_t file_stamp;
    FVizResult result;
    if (file_path == NULL || family == NULL || out_font == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    file_stamp = fviz_font_file_stamp(file_path);
    *out_font = NULL;
    fviz_spin_lock(&g_fviz_font_cache_lock);
    {
        uint32_t i;
        for (i = 0u; i < FVIZ_FILE_FONT_CACHE_CAPACITY; ++i)
        {
            if (g_fviz_file_font_cache[i].font != NULL &&
                g_fviz_file_font_cache[i].pixel_size == pixel_size &&
                g_fviz_file_font_cache[i].file_stamp == file_stamp &&
                strcmp(g_fviz_file_font_cache[i].path, file_path) == 0 &&
                strcmp(g_fviz_file_font_cache[i].family, family) == 0)
            {
                cached = (FVizFont*)fviz_retain(g_fviz_file_font_cache[i].font);
                fviz_spin_unlock(&g_fviz_font_cache_lock);
                if (cached == NULL) return fviz_last_error_code();
                *out_font = cached;
                return FVIZ_OK;
            }
        }
    }
    result = fviz_font_atlas_create_from_file(file_path, family, pixel_size, &atlas);
    if (result != FVIZ_OK) { fviz_spin_unlock(&g_fviz_font_cache_lock); return result; }
    result = fviz_font_create_from_atlas(family, atlas, out_font);
    fviz_release(atlas);
    if (result == FVIZ_OK)
    {
        const uint32_t slot = g_fviz_file_font_cache_next++ % FVIZ_FILE_FONT_CACHE_CAPACITY;
        fviz_release(g_fviz_file_font_cache[slot].font);
        g_fviz_file_font_cache[slot].font = (FVizFont*)fviz_retain(*out_font);
        (void)strncpy(g_fviz_file_font_cache[slot].path, file_path,
            sizeof(g_fviz_file_font_cache[slot].path) - 1u);
        g_fviz_file_font_cache[slot].path[sizeof(g_fviz_file_font_cache[slot].path) - 1u] = '\0';
        (void)strncpy(g_fviz_file_font_cache[slot].family, family,
            sizeof(g_fviz_file_font_cache[slot].family) - 1u);
        g_fviz_file_font_cache[slot].family[sizeof(g_fviz_file_font_cache[slot].family) - 1u] = '\0';
        g_fviz_file_font_cache[slot].pixel_size = pixel_size;
        g_fviz_file_font_cache[slot].file_stamp = file_stamp;
    }
    fviz_spin_unlock(&g_fviz_font_cache_lock);
    return result;
}

void fviz_font_cache_clear(void)
{
    uint32_t i;
    fviz_spin_lock(&g_fviz_font_cache_lock);
    for (i = 0u; i < FVIZ_FONT_CACHE_CAPACITY; ++i)
    {
        fviz_release(g_fviz_font_cache[i].font);
        memset(&g_fviz_font_cache[i], 0, sizeof(g_fviz_font_cache[i]));
    }
    for (i = 0u; i < FVIZ_FILE_FONT_CACHE_CAPACITY; ++i)
    {
        fviz_release(g_fviz_file_font_cache[i].font);
        memset(&g_fviz_file_font_cache[i], 0, sizeof(g_fviz_file_font_cache[i]));
    }
    g_fviz_font_cache_next = 0u;
    g_fviz_file_font_cache_next = 0u;
    fviz_spin_unlock(&g_fviz_font_cache_lock);
}
const char* fviz_font_family(const FVizFont* font)
{
    return font != NULL && font->family != NULL ? fviz_string_c_str(font->family) : "";
}
FVizFontAtlas* fviz_font_atlas(FVizFont* font) { return font != NULL ? font->atlas : NULL; }
const FVizFontAtlas* fviz_font_const_atlas(const FVizFont* font) { return font != NULL ? font->atlas : NULL; }
float fviz_font_nominal_pixel_size(const FVizFont* font)
{
    return font != NULL ? fviz_font_atlas_nominal_pixel_size(font->atlas) : 0.0f;
}
