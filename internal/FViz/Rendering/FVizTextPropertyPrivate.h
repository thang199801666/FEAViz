#ifndef FVIZ_INTERNAL_RENDERING_TEXT_PROPERTY_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_TEXT_PROPERTY_PRIVATE_H
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizTextProperty.h>
struct FVizTextProperty
{
    FVizObject base;
    FVizFont* font;
    float font_size;
    float color[4];
    float background[4];
    FVizTextHorizontalAlignment horizontal_alignment;
    FVizTextVerticalAlignment vertical_alignment;
    float line_spacing;
    FVizBool shadow;
    float shadow_offset[2];
    float shadow_opacity;
};
#endif
