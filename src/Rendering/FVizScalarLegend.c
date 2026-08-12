#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Rendering/FVizScalarLegend.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizScalarLegendPrivate.h>

static void fviz_scalar_legend_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_scalar_legend_class = {
    FVIZ_TYPE_SCALAR_LEGEND, "FVizScalarLegend", &g_fviz_object_class, fviz_scalar_legend_destroy
};

static void fviz_scalar_legend_destroy(FVizObject* object)
{
    FVizScalarLegend* legend = (FVizScalarLegend*)object;
    fviz_release(legend->lookup_table);
    fviz_release(legend->title);
    legend->lookup_table = NULL;
    legend->title = NULL;
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
    legend = (FVizScalarLegend*)fviz_internal_object_allocate(sizeof(FVizScalarLegend), &g_fviz_scalar_legend_class, NULL);
    if (legend == NULL) return fviz_last_error_code();
    if (fviz_lookup_table_create(256u, &legend->lookup_table) != FVIZ_OK ||
        fviz_string_create_from("", &legend->title) != FVIZ_OK)
    {
        fviz_release(legend);
        return fviz_last_error_code();
    }
    legend->range_min = 0.0f;
    legend->range_max = 1.0f;
    legend->position = FVIZ_LEGEND_TOP_RIGHT;
    legend->visible = FVIZ_TRUE;
    *out_legend = legend;
    return FVIZ_OK;
}

void fviz_scalar_legend_set_lookup_table(FVizScalarLegend* legend, FVizLookupTable* table)
{
    if (legend == NULL) return;
    if (table != NULL && fviz_retain(table) == NULL) return;
    fviz_release(legend->lookup_table);
    legend->lookup_table = table;
    if (table != NULL)
    {
        fviz_lookup_table_get_range(table, &legend->range_min, &legend->range_max);
    }
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
}

void fviz_scalar_legend_get_range(const FVizScalarLegend* legend, float* minimum, float* maximum)
{
    if (legend == NULL) return;
    if (minimum != NULL) *minimum = legend->range_min;
    if (maximum != NULL) *maximum = legend->range_max;
}

void fviz_scalar_legend_set_position(FVizScalarLegend* legend, FVizLegendPosition position)
{
    if (legend != NULL) legend->position = position;
}

FVizLegendPosition fviz_scalar_legend_position(const FVizScalarLegend* legend)
{
    return legend != NULL ? legend->position : FVIZ_LEGEND_TOP_RIGHT;
}

void fviz_scalar_legend_set_visible(FVizScalarLegend* legend, FVizBool visible)
{
    if (legend != NULL) legend->visible = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_scalar_legend_is_visible(const FVizScalarLegend* legend)
{
    return legend != NULL ? legend->visible : FVIZ_FALSE;
}

void fviz_scalar_legend_set_title(FVizScalarLegend* legend, const char* title)
{
    FVizString* new_title;
    if (legend == NULL || title == NULL) return;
    if (fviz_string_create_from(title, &new_title) != FVIZ_OK) return;
    fviz_release(legend->title);
    legend->title = new_title;
}

const char* fviz_scalar_legend_title(const FVizScalarLegend* legend)
{
    return legend != NULL && legend->title != NULL ? fviz_string_c_str(legend->title) : "";
}
