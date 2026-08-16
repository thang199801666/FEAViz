#ifndef FVIZ_INTERNAL_RENDERING_FONT_ATLAS_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_FONT_ATLAS_PRIVATE_H
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizFontAtlas.h>
struct FVizFontAtlas
{
    FVizObject base;
    uint32_t width;
    uint32_t height;
    float nominal_pixel_size;
    uint8_t* pixels;
    FVizFontGlyph* glyphs;
    FVizSize glyph_count;
    uint32_t fallback_codepoint;
};
#endif
