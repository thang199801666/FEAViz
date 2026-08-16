#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Rendering/FVizFontAtlas.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizFontAtlasPrivate.h>

static void fviz_font_atlas_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_font_atlas_class = {FVIZ_TYPE_FONT_ATLAS, "FVizFontAtlas", &g_fviz_object_class,
                                                        fviz_font_atlas_destroy, NULL};

static const uint8_t g_digits[10][7] = {
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},    {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30}, {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};
static const uint8_t g_upper[26][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 2, 18, 12},      {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31}};

static void fviz_builtin_rows(uint32_t cp, uint8_t rows[7])
{
    static const uint8_t question[7] = {14, 17, 1, 2, 4, 0, 4};
    unsigned i;
    (void)memset(rows, 0, 7u);
    if (cp >= '0' && cp <= '9')
    {
        (void)memcpy(rows, g_digits[cp - '0'], 7u);
        return;
    }
    if (cp >= 'a' && cp <= 'z') cp = cp - 'a' + 'A';
    if (cp >= 'A' && cp <= 'Z')
    {
        (void)memcpy(rows, g_upper[cp - 'A'], 7u);
        return;
    }
    switch (cp)
    {
        case ' ':
            break;
        case '.':
            rows[6] = 4;
            break;
        case ',':
            rows[5] = 4;
            rows[6] = 8;
            break;
        case ':':
            rows[2] = 4;
            rows[5] = 4;
            break;
        case ';':
            rows[2] = 4;
            rows[5] = 4;
            rows[6] = 8;
            break;
        case '-':
            rows[3] = 14;
            break;
        case '_':
            rows[6] = 31;
            break;
        case '+':
            rows[2] = 4;
            rows[3] = 14;
            rows[4] = 4;
            break;
        case '=':
            rows[2] = 14;
            rows[4] = 14;
            break;
        case '/':
            rows[0] = 1;
            rows[1] = 2;
            rows[2] = 2;
            rows[3] = 4;
            rows[4] = 8;
            rows[5] = 8;
            rows[6] = 16;
            break;
        case '\\':
            rows[0] = 16;
            rows[1] = 8;
            rows[2] = 8;
            rows[3] = 4;
            rows[4] = 2;
            rows[5] = 2;
            rows[6] = 1;
            break;
        case '(':
            rows[0] = 2;
            rows[1] = 4;
            rows[2] = 8;
            rows[3] = 8;
            rows[4] = 8;
            rows[5] = 4;
            rows[6] = 2;
            break;
        case ')':
            rows[0] = 8;
            rows[1] = 4;
            rows[2] = 2;
            rows[3] = 2;
            rows[4] = 2;
            rows[5] = 4;
            rows[6] = 8;
            break;
        case '[':
            rows[0] = 14;
            rows[1] = 8;
            rows[2] = 8;
            rows[3] = 8;
            rows[4] = 8;
            rows[5] = 8;
            rows[6] = 14;
            break;
        case ']':
            rows[0] = 14;
            rows[1] = 2;
            rows[2] = 2;
            rows[3] = 2;
            rows[4] = 2;
            rows[5] = 2;
            rows[6] = 14;
            break;
        case '<':
            rows[1] = 2;
            rows[2] = 4;
            rows[3] = 8;
            rows[4] = 4;
            rows[5] = 2;
            break;
        case '>':
            rows[1] = 8;
            rows[2] = 4;
            rows[3] = 2;
            rows[4] = 4;
            rows[5] = 8;
            break;
        case '!':
            rows[0] = 4;
            rows[1] = 4;
            rows[2] = 4;
            rows[3] = 4;
            rows[5] = 4;
            break;
        case '?':
            (void)memcpy(rows, question, 7u);
            break;
        case '#':
            rows[1] = 10;
            rows[2] = 31;
            rows[3] = 10;
            rows[4] = 31;
            rows[5] = 10;
            break;
        case '%':
            rows[0] = 17;
            rows[1] = 2;
            rows[2] = 4;
            rows[3] = 4;
            rows[4] = 8;
            rows[5] = 17;
            break;
        case '*':
            rows[1] = 21;
            rows[2] = 14;
            rows[3] = 31;
            rows[4] = 14;
            rows[5] = 21;
            break;
        case '|':
            for (i = 0; i < 7u; ++i)
                rows[i] = 4;
            break;
        case '\'':
            rows[0] = 4;
            rows[1] = 4;
            break;
        case '"':
            rows[0] = 10;
            rows[1] = 10;
            break;
        default:
            (void)memcpy(rows, question, 7u);
            break;
    }
}

static void fviz_font_atlas_destroy(FVizObject* object)
{
    FVizFontAtlas* atlas = (FVizFontAtlas*)object;
    fviz_free(atlas->pixels);
    fviz_free(atlas->glyphs);
    atlas->pixels = NULL;
    atlas->glyphs = NULL;
    atlas->glyph_count = 0u;
}

static int fviz_font_glyph_compare(const void* left, const void* right)
{
    const FVizFontGlyph* a = (const FVizFontGlyph*)left;
    const FVizFontGlyph* b = (const FVizFontGlyph*)right;
    return a->codepoint < b->codepoint ? -1 : (a->codepoint > b->codepoint ? 1 : 0);
}

static const FVizFontGlyph* fviz_font_atlas_find_exact(const FVizFontAtlas* atlas, uint32_t codepoint)
{
    FVizSize lo = 0u;
    FVizSize hi;
    if (atlas == NULL || atlas->glyphs == NULL) return NULL;
    hi = atlas->glyph_count;
    while (lo < hi)
    {
        const FVizSize mid = lo + (hi - lo) / 2u;
        const uint32_t cp = atlas->glyphs[mid].codepoint;
        if (cp < codepoint) lo = mid + 1u;
        else
            hi = mid;
    }
    return lo < atlas->glyph_count && atlas->glyphs[lo].codepoint == codepoint ? &atlas->glyphs[lo] : NULL;
}

FVizResult fviz_font_atlas_create_from_coverage(uint32_t width, uint32_t height, const uint8_t* coverage_pixels,
                                                const FVizFontGlyph* glyphs, FVizSize glyph_count,
                                                float nominal_pixel_size, uint32_t fallback_codepoint,
                                                FVizFontAtlas** out_atlas)
{
    FVizFontAtlas* atlas;
    FVizSize pixel_count;
    FVizSize glyph_bytes;
    FVizSize i;
    if (out_atlas == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_atlas = NULL;
    if (coverage_pixels == NULL || glyphs == NULL || width == 0u || height == 0u || width > UINT16_MAX ||
        height > UINT16_MAX || glyph_count == 0u || !isfinite(nominal_pixel_size) || nominal_pixel_size <= 0.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if ((FVizSize)width > SIZE_MAX / (FVizSize)height || glyph_count > SIZE_MAX / sizeof(FVizFontGlyph))
        return FVIZ_ERROR_OVERFLOW;
    pixel_count = (FVizSize)width * (FVizSize)height;
    glyph_bytes = glyph_count * sizeof(FVizFontGlyph);
    for (i = 0u; i < glyph_count; ++i)
    {
        const FVizFontGlyph* glyph = &glyphs[i];
        if ((uint32_t)glyph->x + (uint32_t)glyph->width > width ||
            (uint32_t)glyph->y + (uint32_t)glyph->height > height || !isfinite(glyph->advance_x) ||
            !isfinite(glyph->bearing_x) || !isfinite(glyph->bearing_y))
            return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    atlas = (FVizFontAtlas*)fviz_internal_object_allocate(sizeof(*atlas), &g_fviz_font_atlas_class, NULL);
    if (atlas == NULL) return fviz_last_error_code();
    atlas->pixels = (uint8_t*)fviz_alloc(pixel_count);
    atlas->glyphs = (FVizFontGlyph*)fviz_alloc(glyph_bytes);
    if (atlas->pixels == NULL || atlas->glyphs == NULL)
    {
        fviz_release(atlas);
        return fviz_last_error_code();
    }
    (void)memcpy(atlas->pixels, coverage_pixels, pixel_count);
    (void)memcpy(atlas->glyphs, glyphs, glyph_bytes);
    qsort(atlas->glyphs, (size_t)glyph_count, sizeof(FVizFontGlyph), fviz_font_glyph_compare);
    for (i = 1u; i < glyph_count; ++i)
    {
        if (atlas->glyphs[i - 1u].codepoint == atlas->glyphs[i].codepoint)
        {
            fviz_release(atlas);
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    atlas->width = width;
    atlas->height = height;
    atlas->nominal_pixel_size = nominal_pixel_size;
    atlas->glyph_count = glyph_count;
    atlas->fallback_codepoint = fallback_codepoint;
    if (fviz_font_atlas_find_exact(atlas, fallback_codepoint) == NULL)
        atlas->fallback_codepoint = atlas->glyphs[0].codepoint;
    *out_atlas = atlas;
    return FVIZ_OK;
}

FVizResult fviz_font_atlas_create_builtin(FVizFontAtlas** out_atlas)
{
    FVizFontAtlas* atlas;
    uint32_t cp;
    if (out_atlas == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_atlas must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_atlas = NULL;
    atlas = (FVizFontAtlas*)fviz_internal_object_allocate(sizeof(*atlas), &g_fviz_font_atlas_class, NULL);
    if (atlas == NULL) return fviz_last_error_code();
    atlas->width = 16u * 8u;
    atlas->height = 6u * 8u;
    atlas->nominal_pixel_size = 8.0f;
    atlas->glyph_count = 95u;
    atlas->fallback_codepoint = '?';
    atlas->pixels = (uint8_t*)fviz_alloc((FVizSize)atlas->width * (FVizSize)atlas->height);
    atlas->glyphs = (FVizFontGlyph*)fviz_alloc(atlas->glyph_count * sizeof(FVizFontGlyph));
    if (atlas->pixels == NULL || atlas->glyphs == NULL)
    {
        fviz_release(atlas);
        return fviz_last_error_code();
    }
    (void)memset(atlas->pixels, 0, (size_t)atlas->width * (size_t)atlas->height);
    for (cp = 32u; cp <= 126u; ++cp)
    {
        const uint32_t glyph_index = cp - 32u;
        const uint32_t cell_x = (glyph_index % 16u) * 8u;
        const uint32_t cell_y = (glyph_index / 16u) * 8u;
        uint8_t rows[7];
        uint32_t y;
        FVizFontGlyph* glyph = &atlas->glyphs[glyph_index];
        fviz_builtin_rows(cp, rows);
        glyph->codepoint = cp;
        glyph->x = (uint16_t)cell_x;
        glyph->y = (uint16_t)cell_y;
        glyph->width = 8u;
        glyph->height = 8u;
        glyph->advance_x = cp == ' ' ? 4.0f : 6.0f;
        glyph->bearing_x = 0.0f;
        glyph->bearing_y = 7.0f;
        for (y = 0u; y < 7u; ++y)
        {
            uint32_t x;
            for (x = 0u; x < 5u; ++x)
            {
                if ((rows[y] & (uint8_t)(1u << (4u - x))) != 0u)
                {
                    const uint32_t px = cell_x + 1u + x;
                    const uint32_t py = cell_y + (6u - y);
                    atlas->pixels[(FVizSize)py * atlas->width + px] = 255u;
                }
            }
        }
    }
    *out_atlas = atlas;
    return FVIZ_OK;
}

FVizResult fviz_font_atlas_create_system(const char* family, float pixel_size, FVizFontAtlas** out_atlas)
{
#ifdef _WIN32
    HDC dc = NULL;
    HFONT font = NULL;
    HGDIOBJ old_font = NULL;
    FVizFontGlyph glyphs[96];
    uint8_t* pixels = NULL;
    FVizSize glyph_count = 0u;
    uint32_t cell_w;
    uint32_t cell_h;
    uint32_t atlas_w;
    uint32_t atlas_h;
    uint32_t cp;
    FVizResult result;
    if (family == NULL || out_atlas == NULL || !isfinite(pixel_size) || pixel_size <= 0.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_atlas = NULL;
    cell_w = (uint32_t)(pixel_size + 8.0f);
    if (cell_w < 32u) cell_w = 32u;
    cell_h = cell_w;
    if (cell_w > 1024u) return FVIZ_ERROR_INVALID_ARGUMENT;
    atlas_w = cell_w * 16u;
    atlas_h = cell_h * 6u;
    pixels = (uint8_t*)fviz_alloc((FVizSize)atlas_w * atlas_h);
    if (pixels == NULL) return fviz_last_error_code();
    (void)memset(pixels, 0, (FVizSize)atlas_w * atlas_h);
    dc = CreateCompatibleDC(NULL);
    font =
        CreateFontA(-(LONG)(pixel_size + 0.5f), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family);
    if (dc == NULL || font == NULL)
    {
        if (font != NULL) DeleteObject(font);
        if (dc != NULL) DeleteDC(dc);
        fviz_free(pixels);
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    old_font = SelectObject(dc, font);
    for (cp = 32u; cp <= 127u; ++cp)
    {
        WCHAR wc = (WCHAR)cp;
        MAT2 mat = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
        GLYPHMETRICS gm;
        DWORD bytes = GetGlyphOutlineW(dc, wc, GGO_GRAY8_BITMAP, &gm, 0u, NULL, &mat);
        uint8_t* bitmap;
        uint32_t gx;
        uint32_t gy;
        uint32_t pitch;
        uint32_t row;
        if (bytes == GDI_ERROR) continue;
        gx = (uint32_t)(glyph_count % 16u) * cell_w;
        gy = (uint32_t)(glyph_count / 16u) * cell_h;
        if ((uint32_t)gm.gmBlackBoxX > cell_w || (uint32_t)gm.gmBlackBoxY > cell_h) continue;
        bitmap = (uint8_t*)fviz_alloc(bytes != 0u ? bytes : 1u);
        if (bitmap == NULL) break;
        (void)GetGlyphOutlineW(dc, wc, GGO_GRAY8_BITMAP, &gm, bytes, bitmap, &mat);
        pitch = ((uint32_t)gm.gmBlackBoxX + 3u) & ~3u;
        for (row = 0u; row < (uint32_t)gm.gmBlackBoxY; ++row)
        {
            uint32_t col;
            for (col = 0u; col < (uint32_t)gm.gmBlackBoxX; ++col)
                /* GDI returns grayscale glyph rows bottom-up; FViz texture
                 * coordinates are top-down, so flip the row while packing. */
                pixels[(gy + row) * atlas_w + gx + col] =
                    (uint8_t)(((uint32_t)bitmap[((uint32_t)gm.gmBlackBoxY - 1u - row) * pitch + col] * 255u) / 64u);
        }
        glyphs[glyph_count].codepoint = cp;
        glyphs[glyph_count].x = (uint16_t)gx;
        glyphs[glyph_count].y = (uint16_t)gy;
        glyphs[glyph_count].width = (uint16_t)gm.gmBlackBoxX;
        glyphs[glyph_count].height = (uint16_t)gm.gmBlackBoxY;
        glyphs[glyph_count].advance_x = (float)gm.gmCellIncX;
        glyphs[glyph_count].bearing_x = (float)gm.gmptGlyphOrigin.x;
        glyphs[glyph_count].bearing_y = (float)gm.gmptGlyphOrigin.y;
        ++glyph_count;
        fviz_free(bitmap);
    }
    SelectObject(dc, old_font);
    DeleteObject(font);
    DeleteDC(dc);
    if (glyph_count == 0u)
    {
        fviz_free(pixels);
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    result = fviz_font_atlas_create_from_coverage(atlas_w, atlas_h, pixels, glyphs, glyph_count, pixel_size,
                                                  (uint32_t)'?', out_atlas);
    fviz_free(pixels);
    return result;
#else
    (void)family;
    (void)pixel_size;
    (void)out_atlas;
    return FVIZ_ERROR_NOT_SUPPORTED;
#endif
}

FVizResult fviz_font_atlas_create_from_file(const char* file_path, const char* family, float pixel_size,
                                            FVizFontAtlas** out_atlas)
{
#ifdef _WIN32
    int added;
    FVizResult result;
    if (file_path == NULL || family == NULL || out_atlas == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    added = AddFontResourceExA(file_path, FR_PRIVATE, NULL);
    if (added == 0) return FVIZ_ERROR_NOT_SUPPORTED;
    result = fviz_font_atlas_create_system(family, pixel_size, out_atlas);
    (void)RemoveFontResourceExA(file_path, FR_PRIVATE, NULL);
    return result;
#else
    (void)file_path;
    (void)family;
    (void)pixel_size;
    (void)out_atlas;
    return FVIZ_ERROR_NOT_SUPPORTED;
#endif
}

uint32_t fviz_font_default_family_count(void)
{
    return 4u;
}

const char* fviz_font_default_family(uint32_t index)
{
    static const char* families[] = {"Arial", "Times New Roman", "Segoe UI", "Consolas"};
    return index < 4u ? families[index] : NULL;
}

uint32_t fviz_font_atlas_width(const FVizFontAtlas* atlas)
{
    return atlas != NULL ? atlas->width : 0u;
}

uint32_t fviz_font_atlas_height(const FVizFontAtlas* atlas)
{
    return atlas != NULL ? atlas->height : 0u;
}

const uint8_t* fviz_font_atlas_pixels(const FVizFontAtlas* atlas)
{
    return atlas != NULL ? atlas->pixels : NULL;
}

FVizSize fviz_font_atlas_glyph_count(const FVizFontAtlas* atlas)
{
    return atlas != NULL ? atlas->glyph_count : 0u;
}

const FVizFontGlyph* fviz_font_atlas_glyph_at(const FVizFontAtlas* atlas, FVizSize index)
{
    return atlas != NULL && index < atlas->glyph_count ? &atlas->glyphs[index] : NULL;
}

const FVizFontGlyph* fviz_font_atlas_find_glyph(const FVizFontAtlas* atlas, uint32_t codepoint)
{
    const FVizFontGlyph* glyph;
    if (atlas == NULL) return NULL;
    glyph = fviz_font_atlas_find_exact(atlas, codepoint);
    if (glyph != NULL) return glyph;
    return fviz_font_atlas_find_exact(atlas, atlas->fallback_codepoint);
}

float fviz_font_atlas_nominal_pixel_size(const FVizFontAtlas* atlas)
{
    return atlas != NULL ? atlas->nominal_pixel_size : 0.0f;
}
