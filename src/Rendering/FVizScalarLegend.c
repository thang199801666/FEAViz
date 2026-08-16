#include <ctype.h>
#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Rendering/FVizScalarLegend.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizScalarLegendPrivate.h>

static void fviz_scalar_legend_destroy(FVizObject* object);
static FVizMTime fviz_scalar_legend_mtime(const FVizObject* object);

static FVizBool fviz_scalar_legend_dependency_modified(FVizObject* caller, FVizEventId event_id, void* call_data,
                                                       void* client_data)
{
    FVizScalarLegend* legend = (FVizScalarLegend*)client_data;
    FVIZ_UNUSED(caller);
    FVIZ_UNUSED(event_id);
    FVIZ_UNUSED(call_data);
    if (legend != NULL) fviz_object_modified((FVizObject*)legend);
    return FVIZ_FALSE;
}

static FVizResult fviz_scalar_legend_observe_dependency(FVizScalarLegend* legend, FVizObject* dependency,
                                                        FVizObserverTag* out_tag)
{
    if (legend == NULL || dependency == NULL || out_tag == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_tag = FVIZ_OBSERVER_TAG_INVALID;
    return fviz_object_add_observer(dependency, FVIZ_EVENT_MODIFIED, 0.0f, fviz_scalar_legend_dependency_modified,
                                    legend, out_tag);
}

static const FVizObjectClass g_fviz_scalar_legend_class = {FVIZ_TYPE_SCALAR_LEGEND, "FVizScalarLegend",
                                                           &g_fviz_object_class, fviz_scalar_legend_destroy,
                                                           fviz_scalar_legend_mtime};

static FVizMTime fviz_scalar_legend_mtime(const FVizObject* object)
{
    const FVizScalarLegend* legend = (const FVizScalarLegend*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    FVizMTime child;
#define FVIZ_LEGEND_MAX_CHILD(child_object)                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        child = fviz_object_mtime((const FVizObject*)(child_object));                                                  \
        if (child > mtime) mtime = child;                                                                              \
    } while (0)
    FVIZ_LEGEND_MAX_CHILD(legend->lookup_table);
    FVIZ_LEGEND_MAX_CHILD(legend->title);
    FVIZ_LEGEND_MAX_CHILD(legend->units);
    FVIZ_LEGEND_MAX_CHILD(legend->label_format);
    FVIZ_LEGEND_MAX_CHILD(legend->title_text_property);
    FVIZ_LEGEND_MAX_CHILD(legend->label_text_property);
#undef FVIZ_LEGEND_MAX_CHILD
    return mtime;
}

static void fviz_scalar_legend_destroy(FVizObject* object)
{
    FVizScalarLegend* legend = (FVizScalarLegend*)object;
    if (legend->lookup_table != NULL && legend->lookup_table_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)legend->lookup_table, legend->lookup_table_modified_tag);
    if (legend->title_text_property != NULL && legend->title_text_property_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)legend->title_text_property,
                                          legend->title_text_property_modified_tag);
    if (legend->label_text_property != NULL && legend->label_text_property_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)legend->label_text_property,
                                          legend->label_text_property_modified_tag);
    fviz_release(legend->lookup_table);
    fviz_release(legend->title);
    fviz_release(legend->units);
    fviz_release(legend->label_format);
    fviz_release(legend->title_text_property);
    fviz_release(legend->label_text_property);
    legend->lookup_table = NULL;
    legend->title = NULL;
    legend->units = NULL;
    legend->label_format = NULL;
    legend->title_text_property = NULL;
    legend->label_text_property = NULL;
    legend->lookup_table_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    legend->title_text_property_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    legend->label_text_property_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
}

FVizResult fviz_scalar_legend_create(FVizScalarLegend** out_legend)
{
    FVizScalarLegend* legend;
    if (out_legend == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_legend must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_legend = NULL;
    legend =
        (FVizScalarLegend*)fviz_internal_object_allocate(sizeof(FVizScalarLegend), &g_fviz_scalar_legend_class, NULL);
    if (legend == NULL) return fviz_last_error_code();
    if (fviz_lookup_table_create(256u, &legend->lookup_table) != FVIZ_OK ||
        fviz_string_create_from("", &legend->title) != FVIZ_OK ||
        fviz_string_create_from("", &legend->units) != FVIZ_OK ||
        fviz_string_create_from("%.4g", &legend->label_format) != FVIZ_OK ||
        fviz_text_property_create(&legend->title_text_property) != FVIZ_OK ||
        fviz_text_property_create(&legend->label_text_property) != FVIZ_OK)
    {
        fviz_release(legend);
        return fviz_last_error_code();
    }
    legend->range_min = 0.0f;
    legend->range_max = 1.0f;
    legend->viewport_padding_x = -1.0f;
    legend->viewport_padding_y = -1.0f;
    legend->position = FVIZ_LEGEND_TOP_RIGHT;
    legend->visible = FVIZ_TRUE;
    legend->tick_count = 5u;
    legend->bar_width_pixels = 0.0f;
    legend->bar_height_pixels = 0.0f;
    legend->discrete = FVIZ_FALSE;
    legend->tick_visible = FVIZ_FALSE;
    legend->tick_length_pixels = 5.0f;
    legend->title_subtitle_spacing = 2.0f;
    legend->subtitle_bar_spacing = 4.0f;
    legend->bar_label_spacing = 8.0f;
    legend->bar_statistics_spacing = 16.0f;
    legend->panel_color[0] = 0.10f;
    legend->panel_color[1] = 0.11f;
    legend->panel_color[2] = 0.14f;
    legend->panel_color[3] = 1.0f;
    legend->border_color[0] = 0.85f;
    legend->border_color[1] = 0.85f;
    legend->border_color[2] = 0.90f;
    legend->border_color[3] = 1.0f;
    fviz_text_property_set_font_size(legend->title_text_property, 14.0f);
    fviz_text_property_set_font_size(legend->label_text_property, 12.0f);
    legend->lookup_table_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    legend->title_text_property_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    legend->label_text_property_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (fviz_scalar_legend_observe_dependency(legend, (FVizObject*)legend->lookup_table,
                                              &legend->lookup_table_modified_tag) != FVIZ_OK ||
        fviz_scalar_legend_observe_dependency(legend, (FVizObject*)legend->title_text_property,
                                              &legend->title_text_property_modified_tag) != FVIZ_OK ||
        fviz_scalar_legend_observe_dependency(legend, (FVizObject*)legend->label_text_property,
                                              &legend->label_text_property_modified_tag) != FVIZ_OK)
    {
        fviz_release(legend);
        return fviz_last_error_code();
    }
    *out_legend = legend;
    return FVIZ_OK;
}

void fviz_scalar_legend_set_lookup_table(FVizScalarLegend* legend, FVizLookupTable* table)
{
    FVizObserverTag new_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (legend == NULL || legend->lookup_table == table) return;
    if (table != NULL)
    {
        if (fviz_retain(table) == NULL) return;
        if (fviz_scalar_legend_observe_dependency(legend, (FVizObject*)table, &new_tag) != FVIZ_OK)
        {
            fviz_release(table);
            return;
        }
    }
    if (legend->lookup_table != NULL && legend->lookup_table_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)legend->lookup_table, legend->lookup_table_modified_tag);
    fviz_release(legend->lookup_table);
    legend->lookup_table = table;
    legend->lookup_table_modified_tag = new_tag;
    if (table != NULL) fviz_lookup_table_get_range(table, &legend->range_min, &legend->range_max);
    fviz_object_modified((FVizObject*)legend);
}

FVizLookupTable* fviz_scalar_legend_lookup_table(FVizScalarLegend* legend)
{
    return legend != NULL ? legend->lookup_table : NULL;
}

void fviz_scalar_legend_set_range(FVizScalarLegend* legend, float minimum, float maximum)
{
    if (legend == NULL) return;
    if (maximum <= minimum) maximum = minimum + 1.0f;
    legend->range_min = minimum;
    legend->range_max = maximum;
    if (legend->lookup_table != NULL)
    {
        fviz_lookup_table_set_range(legend->lookup_table, minimum, maximum);
    }
    fviz_object_modified((FVizObject*)legend);
}

void fviz_scalar_legend_get_range(const FVizScalarLegend* legend, float* minimum, float* maximum)
{
    if (legend == NULL) return;
    if (minimum != NULL) *minimum = legend->range_min;
    if (maximum != NULL) *maximum = legend->range_max;
}

void fviz_scalar_legend_set_position(FVizScalarLegend* legend, FVizLegendPosition position)
{
    if (legend != NULL && legend->position != position)
    {
        legend->position = position;
        fviz_object_modified((FVizObject*)legend);
    }
}

FVizLegendPosition fviz_scalar_legend_position(const FVizScalarLegend* legend)
{
    return legend != NULL ? legend->position : FVIZ_LEGEND_TOP_RIGHT;
}

void fviz_scalar_legend_set_viewport_padding(FVizScalarLegend* legend, float horizontal, float vertical)
{
    float normalized_x;
    float normalized_y;
    if (legend == NULL) return;
    normalized_x = isfinite(horizontal) && horizontal >= 0.0f ? (horizontal > 1.0f ? 1.0f : horizontal) : -1.0f;
    normalized_y = isfinite(vertical) && vertical >= 0.0f ? (vertical > 1.0f ? 1.0f : vertical) : -1.0f;
    if (legend->viewport_padding_x == normalized_x && legend->viewport_padding_y == normalized_y) return;
    legend->viewport_padding_x = normalized_x;
    legend->viewport_padding_y = normalized_y;
    fviz_object_modified((FVizObject*)legend);
}

void fviz_scalar_legend_get_viewport_padding(const FVizScalarLegend* legend, float* horizontal, float* vertical)
{
    if (legend == NULL) return;
    if (horizontal != NULL) *horizontal = legend->viewport_padding_x;
    if (vertical != NULL) *vertical = legend->viewport_padding_y;
}

void fviz_scalar_legend_set_visible(FVizScalarLegend* legend, FVizBool visible)
{
    if (legend != NULL)
    {
        const FVizBool normalized = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (legend->visible != normalized)
        {
            legend->visible = normalized;
            fviz_object_modified((FVizObject*)legend);
        }
    }
}

FVizBool fviz_scalar_legend_is_visible(const FVizScalarLegend* legend)
{
    return legend != NULL ? legend->visible : FVIZ_FALSE;
}

void fviz_scalar_legend_set_title(FVizScalarLegend* legend, const char* title)
{
    if (legend == NULL || title == NULL || legend->title == NULL) return;
    if (strcmp(fviz_string_c_str(legend->title), title) == 0) return;
    if (fviz_string_set(legend->title, title) == FVIZ_OK) fviz_object_modified((FVizObject*)legend);
}

const char* fviz_scalar_legend_title(const FVizScalarLegend* legend)
{
    return legend != NULL && legend->title != NULL ? fviz_string_c_str(legend->title) : "";
}

void fviz_scalar_legend_set_units(FVizScalarLegend* legend, const char* units)
{
    if (legend == NULL || units == NULL || legend->units == NULL) return;
    if (strcmp(fviz_string_c_str(legend->units), units) == 0) return;
    if (fviz_string_set(legend->units, units) == FVIZ_OK) fviz_object_modified((FVizObject*)legend);
}

const char* fviz_scalar_legend_units(const FVizScalarLegend* legend)
{
    return legend != NULL && legend->units != NULL ? fviz_string_c_str(legend->units) : "";
}

void fviz_scalar_legend_set_tick_count(FVizScalarLegend* legend, uint32_t tick_count)
{
    if (legend == NULL) return;
    if (tick_count < 2u) tick_count = 2u;
    if (tick_count > 32u) tick_count = 32u;
    if (legend->tick_count != tick_count)
    {
        legend->tick_count = tick_count;
        fviz_object_modified((FVizObject*)legend);
    }
}

uint32_t fviz_scalar_legend_tick_count(const FVizScalarLegend* legend)
{
    return legend != NULL ? legend->tick_count : 0u;
}

static FVizBool fviz_scalar_legend_format_valid(const char* format)
{
    const char* p;
    unsigned conversions = 0u;
    if (format == NULL || *format == '\0') return FVIZ_FALSE;
    for (p = format; *p != '\0'; ++p)
    {
        if (*p != '%') continue;
        ++p;
        if (*p == '%') continue;
        ++conversions;
        if (conversions > 1u) return FVIZ_FALSE;
        while (*p == '+' || *p == '-' || *p == ' ' || *p == '#' || *p == '0')
            ++p;
        while (isdigit((unsigned char)*p))
            ++p;
        if (*p == '.')
        {
            ++p;
            if (*p == '*') return FVIZ_FALSE;
            while (isdigit((unsigned char)*p))
                ++p;
        }
        if (*p == '*' || *p == 'l' || *p == 'h' || *p == 'L' || *p == 'j' || *p == 'z' || *p == 't') return FVIZ_FALSE;
        if (*p != 'f' && *p != 'F' && *p != 'e' && *p != 'E' && *p != 'g' && *p != 'G') return FVIZ_FALSE;
    }
    return conversions == 1u ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizResult fviz_scalar_legend_set_label_format(FVizScalarLegend* legend, const char* format)
{
    if (legend == NULL || fviz_scalar_legend_format_valid(format) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "scalar legend label format must contain one floating-point conversion");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (strcmp(fviz_string_c_str(legend->label_format), format) == 0) return FVIZ_OK;
    if (fviz_string_set(legend->label_format, format) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)legend);
    return FVIZ_OK;
}

const char* fviz_scalar_legend_label_format(const FVizScalarLegend* legend)
{
    return legend != NULL && legend->label_format != NULL ? fviz_string_c_str(legend->label_format) : "%.4g";
}

FVizTextProperty* fviz_scalar_legend_title_text_property(FVizScalarLegend* legend)
{
    return legend != NULL ? legend->title_text_property : NULL;
}

FVizTextProperty* fviz_scalar_legend_label_text_property(FVizScalarLegend* legend)
{
    return legend != NULL ? legend->label_text_property : NULL;
}

void fviz_scalar_legend_set_bar_size(FVizScalarLegend* legend, float width_pixels, float height_pixels)
{
    if (legend == NULL) return;
    if (!isfinite(width_pixels) || width_pixels < 0.0f) width_pixels = 0.0f;
    if (!isfinite(height_pixels) || height_pixels < 0.0f) height_pixels = 0.0f;
    if (legend->bar_width_pixels == width_pixels && legend->bar_height_pixels == height_pixels) return;
    legend->bar_width_pixels = width_pixels;
    legend->bar_height_pixels = height_pixels;
    fviz_object_modified((FVizObject*)legend);
}

void fviz_scalar_legend_get_bar_size(const FVizScalarLegend* legend, float* width_pixels, float* height_pixels)
{
    if (legend == NULL) return;
    if (width_pixels != NULL) *width_pixels = legend->bar_width_pixels;
    if (height_pixels != NULL) *height_pixels = legend->bar_height_pixels;
}

void fviz_scalar_legend_set_discrete(FVizScalarLegend* legend, FVizBool discrete)
{
    if (legend == NULL) return;
    discrete = discrete != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (legend->discrete == discrete) return;
    legend->discrete = discrete;
    fviz_object_modified((FVizObject*)legend);
}

FVizBool fviz_scalar_legend_is_discrete(const FVizScalarLegend* legend)
{
    return legend != NULL ? legend->discrete : FVIZ_FALSE;
}

static void fviz_scalar_legend_set_rgba(float target[4], float red, float green, float blue, float alpha)
{
    target[0] = isfinite(red) ? red : 0.0f;
    target[1] = isfinite(green) ? green : 0.0f;
    target[2] = isfinite(blue) ? blue : 0.0f;
    target[3] = isfinite(alpha) ? alpha : 1.0f;
}

void fviz_scalar_legend_set_panel_color(FVizScalarLegend* legend, float red, float green, float blue, float alpha)
{
    float color[4];
    if (legend == NULL) return;
    fviz_scalar_legend_set_rgba(color, red, green, blue, alpha);
    if (memcmp(legend->panel_color, color, sizeof(color)) == 0) return;
    (void)memcpy(legend->panel_color, color, sizeof(color));
    fviz_object_modified((FVizObject*)legend);
}

void fviz_scalar_legend_get_panel_color(const FVizScalarLegend* legend, float* red, float* green, float* blue,
                                        float* alpha)
{
    if (legend == NULL) return;
    if (red != NULL) *red = legend->panel_color[0];
    if (green != NULL) *green = legend->panel_color[1];
    if (blue != NULL) *blue = legend->panel_color[2];
    if (alpha != NULL) *alpha = legend->panel_color[3];
}

void fviz_scalar_legend_set_border_color(FVizScalarLegend* legend, float red, float green, float blue, float alpha)
{
    float color[4];
    if (legend == NULL) return;
    fviz_scalar_legend_set_rgba(color, red, green, blue, alpha);
    if (memcmp(legend->border_color, color, sizeof(color)) == 0) return;
    (void)memcpy(legend->border_color, color, sizeof(color));
    fviz_object_modified((FVizObject*)legend);
}

void fviz_scalar_legend_get_border_color(const FVizScalarLegend* legend, float* red, float* green, float* blue,
                                         float* alpha)
{
    if (legend == NULL) return;
    if (red != NULL) *red = legend->border_color[0];
    if (green != NULL) *green = legend->border_color[1];
    if (blue != NULL) *blue = legend->border_color[2];
    if (alpha != NULL) *alpha = legend->border_color[3];
}

void fviz_scalar_legend_set_tick_style(FVizScalarLegend* legend, FVizBool visible, float length_pixels)
{
    if (legend == NULL) return;
    visible = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (!isfinite(length_pixels) || length_pixels < 0.0f) length_pixels = 0.0f;
    if (legend->tick_visible == visible && legend->tick_length_pixels == length_pixels) return;
    legend->tick_visible = visible;
    legend->tick_length_pixels = length_pixels;
    fviz_object_modified((FVizObject*)legend);
}

void fviz_scalar_legend_get_tick_style(const FVizScalarLegend* legend, FVizBool* visible, float* length_pixels)
{
    if (legend == NULL) return;
    if (visible != NULL) *visible = legend->tick_visible;
    if (length_pixels != NULL) *length_pixels = legend->tick_length_pixels;
}

void fviz_scalar_legend_set_layout_spacing(FVizScalarLegend* legend, float title_subtitle, float subtitle_bar,
                                           float bar_label, float bar_statistics)
{
    if (legend == NULL) return;
    if (isfinite(title_subtitle) && title_subtitle >= 0.0f) legend->title_subtitle_spacing = title_subtitle;
    if (isfinite(subtitle_bar) && subtitle_bar >= 0.0f) legend->subtitle_bar_spacing = subtitle_bar;
    if (isfinite(bar_label) && bar_label >= 0.0f) legend->bar_label_spacing = bar_label;
    if (isfinite(bar_statistics) && bar_statistics >= 0.0f) legend->bar_statistics_spacing = bar_statistics;
    fviz_object_modified((FVizObject*)legend);
}

void fviz_scalar_legend_get_layout_spacing(const FVizScalarLegend* legend, float* title_subtitle, float* subtitle_bar,
                                           float* bar_label, float* bar_statistics)
{
    if (legend == NULL) return;
    if (title_subtitle != NULL) *title_subtitle = legend->title_subtitle_spacing;
    if (subtitle_bar != NULL) *subtitle_bar = legend->subtitle_bar_spacing;
    if (bar_label != NULL) *bar_label = legend->bar_label_spacing;
    if (bar_statistics != NULL) *bar_statistics = legend->bar_statistics_spacing;
}
