#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/FEA/FVizScalarBarActor.h>
#include <FViz/FEA/FVizVisualization.h>
#include <FViz/Rendering/FVizFont.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizFEAScalarBarActor
{
    FVizObject base;
    FVizScalarLegend* legend;
};

static void fviz_fea_scalar_bar_actor_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_fea_scalar_bar_actor_class = {
    FVIZ_TYPE_FEA_SCALAR_BAR_ACTOR, "FVizFEAScalarBarActor", NULL, fviz_fea_scalar_bar_actor_destroy, NULL};

static void fviz_fea_scalar_bar_actor_destroy(FVizObject* object)
{
    FVizFEAScalarBarActor* actor = (FVizFEAScalarBarActor*)object;
    fviz_release(actor->legend);
    actor->legend = NULL;
}

void fviz_fea_scalar_bar_options_initialize(FVizFEAScalarBarOptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->range_minimum = 0.0f;
    options->range_maximum = 1.0f;
    options->interval_count = 12u;
    options->tick_count = 13u;
    options->position = FVIZ_LEGEND_TOP_LEFT;
    options->padding_horizontal = 0.02f;
    options->padding_vertical = 0.05f;
    options->title = "S, Mises";
    options->units = "MPa";
    options->label_format = "%+.4f";
    options->title_font_size = 13.0f;
    options->label_font_size = 9.0f;
    options->title_color[0] = 0.96f;
    options->title_color[1] = 0.96f;
    options->title_color[2] = 0.96f;
    options->title_color[3] = 1.0f;
    options->label_color[0] = 0.96f;
    options->label_color[1] = 0.96f;
    options->label_color[2] = 0.96f;
    options->label_color[3] = 1.0f;
    options->title_shadow = FVIZ_FALSE;
    options->label_shadow = FVIZ_FALSE;
    options->visible = FVIZ_TRUE;
    options->use_abaqus_lookup_table = FVIZ_TRUE;
    options->bar_width_pixels = 18.0f;
    options->bar_height_pixels = 180.0f;
    options->panel_color[0] = 0.05f;
    options->panel_color[1] = 0.08f;
    options->panel_color[2] = 0.14f;
    options->panel_color[3] = 0.0f;
    options->border_color[0] = 0.92f;
    options->border_color[1] = 0.92f;
    options->border_color[2] = 0.92f;
    options->border_color[3] = 1.0f;
    options->discrete = FVIZ_TRUE;
    options->ticks_visible = FVIZ_TRUE;
    options->tick_length_pixels = 4.0f;
}

static FVizResult fviz_fea_scalar_bar_validate_options(const FVizFEAScalarBarOptions* options)
{
    if (options == NULL || options->struct_size < sizeof(*options) || options->interval_count < 2u ||
        options->tick_count < 2u || !(options->range_maximum > options->range_minimum) || options->title == NULL ||
        options->units == NULL || options->label_format == NULL || options->title_font_size <= 0.0f ||
        options->label_font_size <= 0.0f)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA scalar bar options are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return FVIZ_OK;
}

FVizResult fviz_fea_scalar_bar_actor_apply(FVizFEAScalarBarActor* actor, const FVizFEAScalarBarOptions* options)
{
    FVizLookupTable* table = NULL;
    FVizResult result;
    if (actor == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_fea_scalar_bar_validate_options(options);
    if (result != FVIZ_OK) return result;
    if (options->lookup_table != NULL)
    {
        table = (FVizLookupTable*)fviz_retain(options->lookup_table);
        if (table == NULL) return fviz_last_error_code();
    }
    else
    {
        if (fviz_lookup_table_create(256u, &table) != FVIZ_OK) return fviz_last_error_code();
        if (options->use_abaqus_lookup_table != FVIZ_FALSE &&
            fviz_fea_configure_abaqus_contour_lut(table, options->interval_count) != FVIZ_OK)
        {
            fviz_release(table);
            return fviz_last_error_code();
        }
    }
    fviz_lookup_table_set_range(table, options->range_minimum, options->range_maximum);
    fviz_scalar_legend_set_lookup_table(actor->legend, table);
    fviz_release(table);
    fviz_scalar_legend_set_range(actor->legend, options->range_minimum, options->range_maximum);
    fviz_scalar_legend_set_position(actor->legend, options->position);
    fviz_scalar_legend_set_viewport_padding(actor->legend, options->padding_horizontal, options->padding_vertical);
    fviz_scalar_legend_set_title(actor->legend, options->title);
    fviz_scalar_legend_set_units(actor->legend, options->units);
    if (fviz_scalar_legend_set_label_format(actor->legend, options->label_format) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_scalar_legend_set_tick_count(actor->legend, options->tick_count);
    fviz_scalar_legend_set_visible(actor->legend, options->visible);
    fviz_scalar_legend_set_bar_size(actor->legend, options->bar_width_pixels, options->bar_height_pixels);
    fviz_scalar_legend_set_discrete(actor->legend, options->discrete);
    fviz_scalar_legend_set_panel_color(actor->legend, options->panel_color[0], options->panel_color[1],
                                       options->panel_color[2], options->panel_color[3]);
    fviz_scalar_legend_set_border_color(actor->legend, options->border_color[0], options->border_color[1],
                                        options->border_color[2], options->border_color[3]);
    fviz_scalar_legend_set_tick_style(actor->legend, options->ticks_visible, options->tick_length_pixels);
    fviz_scalar_legend_set_layout_spacing(actor->legend, 2.0f, 4.0f, 8.0f, 16.0f);
    {
        FVizTextProperty* title_property = fviz_scalar_legend_title_text_property(actor->legend);
        FVizFontAtlas* arial_atlas = NULL;
        FVizFont* arial_font = NULL;
        if (fviz_font_atlas_create_system("Arial", options->title_font_size, &arial_atlas) == FVIZ_OK &&
            fviz_font_create_from_atlas("Arial", arial_atlas, &arial_font) == FVIZ_OK)
        {
            (void)fviz_text_property_set_font(title_property, arial_font);
            (void)fviz_text_property_set_font(fviz_scalar_legend_label_text_property(actor->legend), arial_font);
            fviz_release(arial_font);
        }
        fviz_release(arial_atlas);
    }
    fviz_text_property_set_font_size(fviz_scalar_legend_title_text_property(actor->legend), options->title_font_size);
    fviz_text_property_set_font_size(fviz_scalar_legend_label_text_property(actor->legend), options->label_font_size);
    fviz_text_property_set_color(fviz_scalar_legend_title_text_property(actor->legend), options->title_color[0],
                                 options->title_color[1], options->title_color[2], options->title_color[3]);
    fviz_text_property_set_color(fviz_scalar_legend_label_text_property(actor->legend), options->label_color[0],
                                 options->label_color[1], options->label_color[2], options->label_color[3]);
    fviz_text_property_set_shadow(fviz_scalar_legend_title_text_property(actor->legend), options->title_shadow, 1.0f,
                                  -1.0f, 0.65f);
    fviz_text_property_set_shadow(fviz_scalar_legend_label_text_property(actor->legend), options->label_shadow, 1.0f,
                                  -1.0f, 0.65f);
    fviz_object_modified((FVizObject*)actor);
    return FVIZ_OK;
}

FVizResult fviz_fea_scalar_bar_actor_create(const FVizFEAScalarBarOptions* options, FVizFEAScalarBarActor** out_actor)
{
    FVizFEAScalarBarOptions defaults;
    FVizFEAScalarBarActor* actor;
    FVizResult result;
    if (out_actor == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_actor = NULL;
    if (options == NULL)
    {
        fviz_fea_scalar_bar_options_initialize(&defaults);
        options = &defaults;
    }
    result = fviz_fea_scalar_bar_validate_options(options);
    if (result != FVIZ_OK) return result;
    actor =
        (FVizFEAScalarBarActor*)fviz_internal_object_allocate(sizeof(*actor), &g_fviz_fea_scalar_bar_actor_class, NULL);
    if (actor == NULL) return fviz_last_error_code();
    if (fviz_scalar_legend_create(&actor->legend) != FVIZ_OK)
    {
        fviz_release(actor);
        return fviz_last_error_code();
    }
    result = fviz_fea_scalar_bar_actor_apply(actor, options);
    if (result != FVIZ_OK)
    {
        fviz_release(actor);
        return result;
    }
    *out_actor = actor;
    return FVIZ_OK;
}

FVizScalarLegend* fviz_fea_scalar_bar_actor_legend(FVizFEAScalarBarActor* actor)
{
    return actor != NULL ? actor->legend : NULL;
}

const FVizScalarLegend* fviz_fea_scalar_bar_actor_const_legend(const FVizFEAScalarBarActor* actor)
{
    return actor != NULL ? actor->legend : NULL;
}

FVizResult fviz_fea_scalar_bar_actor_attach(FVizFEAScalarBarActor* actor, FVizRenderer* renderer)
{
    if (actor == NULL || renderer == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_renderer_set_scalar_legend(renderer, actor->legend);
    return FVIZ_OK;
}
