#ifndef FVIZ_INTERNAL_RENDERING_SCALAR_LEGEND_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_SCALAR_LEGEND_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Rendering/FVizScalarLegend.h>

struct FVizScalarLegend
{
    FVizObject base;
    FVizLookupTable* lookup_table;
    FVizString* title;
    FVizString* units;
    FVizString* label_format;
    FVizTextProperty* title_text_property;
    FVizTextProperty* label_text_property;
    FVizObserverTag lookup_table_modified_tag;
    FVizObserverTag title_text_property_modified_tag;
    FVizObserverTag label_text_property_modified_tag;
    float range_min;
    float range_max;
    float viewport_padding_x;
    float viewport_padding_y;
    FVizLegendPosition position;
    FVizBool visible;
    uint32_t tick_count;
    float bar_width_pixels;
    float bar_height_pixels;
    FVizBool discrete;
    FVizBool tick_visible;
    float tick_length_pixels;
    float panel_color[4];
    float border_color[4];
    float title_subtitle_spacing;
    float subtitle_bar_spacing;
    float bar_label_spacing;
    float bar_statistics_spacing;
};

#endif /* FVIZ_INTERNAL_RENDERING_SCALAR_LEGEND_PRIVATE_H */
