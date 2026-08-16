#ifndef FVIZ_INTERNAL_RENDERING_TEXT_LAYOUT_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_TEXT_LAYOUT_PRIVATE_H
#include <FViz/Rendering/FVizTextActor.h>
#include <FViz/Rendering/FVizFontAtlas.h>
typedef FVizResult (*FVizTextGlyphVisitor)(
    const FVizFontGlyph* glyph,
    uint32_t codepoint,
    float x0, float y0, float x1, float y1,
    void* user_data);
FVizResult fviz_internal_text_layout_visit(
    const FVizTextProperty* property,
    const char* utf8,
    FVizTextGlyphVisitor visitor,
    void* user_data,
    FVizTextMetrics* out_metrics);
#endif
