#ifndef FVIZ_INTERNAL_RENDERING_FONT_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_FONT_PRIVATE_H
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Rendering/FVizFont.h>

struct FVizFont
{
    FVizObject base;
    FVizString* family;
    FVizFontAtlas* atlas;
};
#endif
