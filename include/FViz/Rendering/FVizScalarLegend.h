#ifndef FVIZ_RENDERING_SCALAR_LEGEND_H
#define FVIZ_RENDERING_SCALAR_LEGEND_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Rendering/FVizLookupTable.h>
#include <FViz/Rendering/FVizTextProperty.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizScalarLegend FVizScalarLegend;
#define FVIZ_TYPE_SCALAR_LEGEND UINT64_C(0x9F3C7A51E8B2D406)

typedef enum FVizLegendPosition
{
    FVIZ_LEGEND_TOP_RIGHT = 0,
    FVIZ_LEGEND_TOP_LEFT = 1,
    FVIZ_LEGEND_BOTTOM_RIGHT = 2,
    FVIZ_LEGEND_BOTTOM_LEFT = 3
} FVizLegendPosition;

FVIZ_API FVizResult fviz_scalar_legend_create(FVizScalarLegend** out_legend);
FVIZ_API void fviz_scalar_legend_set_lookup_table(FVizScalarLegend* legend, FVizLookupTable* table);
FVIZ_API FVizLookupTable* fviz_scalar_legend_lookup_table(FVizScalarLegend* legend);
FVIZ_API void fviz_scalar_legend_set_range(FVizScalarLegend* legend, float minimum, float maximum);
FVIZ_API void fviz_scalar_legend_get_range(const FVizScalarLegend* legend, float* minimum, float* maximum);
FVIZ_API void fviz_scalar_legend_set_position(FVizScalarLegend* legend, FVizLegendPosition position);
FVIZ_API FVizLegendPosition fviz_scalar_legend_position(const FVizScalarLegend* legend);
/* Normalized viewport padding (0..1). Negative values restore the renderer's
 * DPI-scaled default margin for the corresponding axis. */
FVIZ_API void fviz_scalar_legend_set_viewport_padding(
    FVizScalarLegend* legend, float horizontal, float vertical);
FVIZ_API void fviz_scalar_legend_get_viewport_padding(
    const FVizScalarLegend* legend, float* horizontal, float* vertical);
FVIZ_API void fviz_scalar_legend_set_visible(FVizScalarLegend* legend, FVizBool visible);
FVIZ_API FVizBool fviz_scalar_legend_is_visible(const FVizScalarLegend* legend);
FVIZ_API void fviz_scalar_legend_set_title(FVizScalarLegend* legend, const char* title);
FVIZ_API const char* fviz_scalar_legend_title(const FVizScalarLegend* legend);
FVIZ_API void fviz_scalar_legend_set_units(FVizScalarLegend* legend, const char* units);
FVIZ_API const char* fviz_scalar_legend_units(const FVizScalarLegend* legend);
FVIZ_API void fviz_scalar_legend_set_tick_count(FVizScalarLegend* legend, uint32_t tick_count);
FVIZ_API uint32_t fviz_scalar_legend_tick_count(const FVizScalarLegend* legend);
FVIZ_API FVizResult fviz_scalar_legend_set_label_format(FVizScalarLegend* legend, const char* printf_style_format);
FVIZ_API const char* fviz_scalar_legend_label_format(const FVizScalarLegend* legend);
FVIZ_API FVizTextProperty* fviz_scalar_legend_title_text_property(FVizScalarLegend* legend);
FVIZ_API FVizTextProperty* fviz_scalar_legend_label_text_property(FVizScalarLegend* legend);
/* Presentation controls. A zero bar dimension keeps renderer defaults. */
FVIZ_API void fviz_scalar_legend_set_bar_size(
    FVizScalarLegend* legend, float width_pixels, float height_pixels);
FVIZ_API void fviz_scalar_legend_get_bar_size(
    const FVizScalarLegend* legend, float* width_pixels, float* height_pixels);
FVIZ_API void fviz_scalar_legend_set_discrete(FVizScalarLegend* legend, FVizBool discrete);
FVIZ_API FVizBool fviz_scalar_legend_is_discrete(const FVizScalarLegend* legend);
FVIZ_API void fviz_scalar_legend_set_panel_color(
    FVizScalarLegend* legend, float red, float green, float blue, float alpha);
FVIZ_API void fviz_scalar_legend_get_panel_color(
    const FVizScalarLegend* legend, float* red, float* green, float* blue, float* alpha);
FVIZ_API void fviz_scalar_legend_set_border_color(
    FVizScalarLegend* legend, float red, float green, float blue, float alpha);
FVIZ_API void fviz_scalar_legend_get_border_color(
    const FVizScalarLegend* legend, float* red, float* green, float* blue, float* alpha);
FVIZ_API void fviz_scalar_legend_set_tick_style(
    FVizScalarLegend* legend, FVizBool visible, float length_pixels);
FVIZ_API void fviz_scalar_legend_get_tick_style(
    const FVizScalarLegend* legend, FVizBool* visible, float* length_pixels);
/* Core layout spacing in logical pixels: title/subtitle, subtitle/bar,
 * bar/labels and bar/statistics respectively. */
FVIZ_API void fviz_scalar_legend_set_layout_spacing(
    FVizScalarLegend* legend, float title_subtitle, float subtitle_bar,
    float bar_label, float bar_statistics);
FVIZ_API void fviz_scalar_legend_get_layout_spacing(
    const FVizScalarLegend* legend, float* title_subtitle, float* subtitle_bar,
    float* bar_label, float* bar_statistics);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_SCALAR_LEGEND_H */
