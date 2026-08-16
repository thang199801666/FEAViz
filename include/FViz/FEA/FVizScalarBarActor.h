#ifndef FVIZ_FEA_SCALAR_BAR_ACTOR_H
#define FVIZ_FEA_SCALAR_BAR_ACTOR_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/FEA/FVizFEAApi.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizScalarLegend.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizFEAScalarBarActor FVizFEAScalarBarActor;
#define FVIZ_TYPE_FEA_SCALAR_BAR_ACTOR UINT64_C(0xD6B7A20C6D8841F3)

/* The strings and lookup_table are borrowed only for the duration of create/
 * apply. The actor copies strings and retains the resulting lookup table. */
typedef struct FVizFEAScalarBarOptions
{
    uint32_t struct_size;
    float range_minimum;
    float range_maximum;
    uint32_t interval_count;
    uint32_t tick_count;
    FVizLegendPosition position;
    float padding_horizontal;
    float padding_vertical;
    const char* title;
    const char* units;
    const char* label_format;
    float title_font_size;
    float label_font_size;
    float title_color[4];
    float label_color[4];
    FVizBool title_shadow;
    FVizBool label_shadow;
    FVizBool visible;
    FVizBool use_abaqus_lookup_table;
    FVizLookupTable* lookup_table;
    float bar_width_pixels;
    float bar_height_pixels;
    float panel_color[4];
    float border_color[4];
    FVizBool discrete;
    FVizBool ticks_visible;
    float tick_length_pixels;
} FVizFEAScalarBarOptions;

FVIZ_FEA_API void fviz_fea_scalar_bar_options_initialize(FVizFEAScalarBarOptions* options);
FVIZ_FEA_API FVizResult fviz_fea_scalar_bar_actor_create(
    const FVizFEAScalarBarOptions* options,
    FVizFEAScalarBarActor** out_actor);
FVIZ_FEA_API FVizResult fviz_fea_scalar_bar_actor_apply(
    FVizFEAScalarBarActor* actor,
    const FVizFEAScalarBarOptions* options);
FVIZ_FEA_API FVizScalarLegend* fviz_fea_scalar_bar_actor_legend(
    FVizFEAScalarBarActor* actor);
FVIZ_FEA_API const FVizScalarLegend* fviz_fea_scalar_bar_actor_const_legend(
    const FVizFEAScalarBarActor* actor);
/* Convenience attachment; renderer retains the legend independently. */
FVIZ_FEA_API FVizResult fviz_fea_scalar_bar_actor_attach(
    FVizFEAScalarBarActor* actor, FVizRenderer* renderer);

FVIZ_EXTERN_C_END

#endif /* FVIZ_FEA_SCALAR_BAR_ACTOR_H */
