#ifndef FVIZ_RENDERING_FONT_ATLAS_H
#define FVIZ_RENDERING_FONT_ATLAS_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizFontAtlas FVizFontAtlas;
#define FVIZ_TYPE_FONT_ATLAS UINT64_C(0xE42391AD7B50A101)

typedef struct FVizFontGlyph
{
    uint32_t codepoint;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    float advance_x;
    float bearing_x;
    float bearing_y;
} FVizFontGlyph;

/* Creates FEAViz's dependency-free built-in monochrome atlas. The built-in
   face is intentionally small and is a fallback; external rasterizers can
   populate future atlas backends without changing text actor APIs. */
FVIZ_API FVizResult fviz_font_atlas_create_builtin(FVizFontAtlas** out_atlas);
/* Creates an atlas from an installed system font. On Windows this uses the
   native GDI rasterizer; other platforms return FVIZ_ERROR_NOT_SUPPORTED. */
FVIZ_API FVizResult fviz_font_atlas_create_system(
    const char* family, float pixel_size, FVizFontAtlas** out_atlas);
FVIZ_API FVizResult fviz_font_atlas_create_from_file(
    const char* file_path, const char* family, float pixel_size, FVizFontAtlas** out_atlas);
FVIZ_API uint32_t fviz_font_default_family_count(void);
FVIZ_API const char* fviz_font_default_family(uint32_t index);
/* Creates an owned 8-bit coverage atlas from caller-provided pixels and glyphs.
   This is the extension point for FreeType/DirectWrite/custom rasterizers without
   making those libraries dependencies of the FEAViz core. Glyphs are copied and
   sorted by Unicode codepoint. */
FVIZ_API FVizResult fviz_font_atlas_create_from_coverage(
    uint32_t width,
    uint32_t height,
    const uint8_t* coverage_pixels,
    const FVizFontGlyph* glyphs,
    FVizSize glyph_count,
    float nominal_pixel_size,
    uint32_t fallback_codepoint,
    FVizFontAtlas** out_atlas);
FVIZ_API uint32_t fviz_font_atlas_width(const FVizFontAtlas* atlas);
FVIZ_API uint32_t fviz_font_atlas_height(const FVizFontAtlas* atlas);
FVIZ_API const uint8_t* fviz_font_atlas_pixels(const FVizFontAtlas* atlas);
FVIZ_API FVizSize fviz_font_atlas_glyph_count(const FVizFontAtlas* atlas);
FVIZ_API const FVizFontGlyph* fviz_font_atlas_glyph_at(const FVizFontAtlas* atlas, FVizSize index);
FVIZ_API const FVizFontGlyph* fviz_font_atlas_find_glyph(const FVizFontAtlas* atlas, uint32_t codepoint);
FVIZ_API float fviz_font_atlas_nominal_pixel_size(const FVizFontAtlas* atlas);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_FONT_ATLAS_H */
