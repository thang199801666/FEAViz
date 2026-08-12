#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Rendering/FVizLookupTable.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizLookupTablePrivate.h>

static void fviz_lookup_table_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_lookup_table_class = {
    FVIZ_TYPE_LOOKUP_TABLE,
    "FVizLookupTable",
    &g_fviz_object_class,
    fviz_lookup_table_destroy
};

static void fviz_lookup_table_destroy(FVizObject* object)
{
    FVizLookupTable* table = (FVizLookupTable*)object;
    fviz_free(table->colors);
    table->colors = NULL;
    table->size = 0u;
}

FVizResult fviz_lookup_table_create(FVizSize table_size, FVizLookupTable** out_table)
{
    FVizLookupTable* table;
    FVizSize bytes;
    if (out_table == NULL || table_size == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "lookup table requires size and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_table = NULL;
    if (fviz_size_multiply(table_size, 3u * sizeof(float), &bytes) != FVIZ_OK)
    {
        return FVIZ_ERROR_OVERFLOW;
    }
    table = (FVizLookupTable*)fviz_internal_object_allocate(sizeof(FVizLookupTable), &g_fviz_lookup_table_class, NULL);
    if (table == NULL)
    {
        return fviz_last_error_code();
    }
    table->colors = (float*)fviz_alloc(bytes);
    if (table->colors == NULL)
    {
        fviz_release(table);
        return fviz_last_error_code();
    }
    table->size = table_size;
    table->range_min = 0.0f;
    table->range_max = 1.0f;
    fviz_lookup_table_build(table);
    *out_table = table;
    return FVIZ_OK;
}

FVizSize fviz_lookup_table_size(const FVizLookupTable* table)
{
    return table != NULL ? table->size : 0u;
}

void fviz_lookup_table_set_range(FVizLookupTable* table, float minimum, float maximum)
{
    if (table == NULL) return;
    if (maximum <= minimum)
    {
        maximum = minimum + 1.0f;
    }
    table->range_min = minimum;
    table->range_max = maximum;
}

void fviz_lookup_table_get_range(const FVizLookupTable* table, float* minimum, float* maximum)
{
    if (table == NULL) return;
    if (minimum != NULL) *minimum = table->range_min;
    if (maximum != NULL) *maximum = table->range_max;
}

FVizResult fviz_lookup_table_set_color(FVizLookupTable* table, FVizSize index, float red, float green, float blue)
{
    if (table == NULL || index >= table->size)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "lookup table color index out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    table->colors[index * 3u + 0u] = red;
    table->colors[index * 3u + 1u] = green;
    table->colors[index * 3u + 2u] = blue;
    return FVIZ_OK;
}

void fviz_lookup_table_get_color(const FVizLookupTable* table, FVizSize index, float* red, float* green, float* blue)
{
    if (table == NULL || index >= table->size) return;
    if (red != NULL) *red = table->colors[index * 3u + 0u];
    if (green != NULL) *green = table->colors[index * 3u + 1u];
    if (blue != NULL) *blue = table->colors[index * 3u + 2u];
}

void fviz_lookup_table_build(FVizLookupTable* table)
{
    static const float control_points[5][3] = {
        {0.230f, 0.299f, 0.754f},
        {0.086f, 0.627f, 0.521f},
        {0.387f, 0.404f, 0.322f},
        {0.831f, 0.550f, 0.145f},
        {0.706f, 0.015f, 0.150f}
    };
    FVizSize i;
    if (table == NULL || table->size == 0u) return;
    for (i = 0u; i < table->size; ++i)
    {
        const float position = table->size == 1u ? 0.0f : (float)i / (float)(table->size - 1u);
        const float segment = position * 4.0f;
        const FVizSize segment_index = (FVizSize)segment < 4u ? (FVizSize)segment : 3u;
        const float fraction = segment - (float)segment_index;
        const float* start = control_points[segment_index];
        const float* end = control_points[segment_index + 1u];
        table->colors[i * 3u + 0u] = start[0] + (end[0] - start[0]) * fraction;
        table->colors[i * 3u + 1u] = start[1] + (end[1] - start[1]) * fraction;
        table->colors[i * 3u + 2u] = start[2] + (end[2] - start[2]) * fraction;
    }
}

void fviz_lookup_table_map_scalar(const FVizLookupTable* table, float value, float* red, float* green, float* blue)
{
    float position;
    FVizSize index;
    float fraction;
    if (table == NULL || table->size == 0u) return;
    if (value <= table->range_min)
    {
        position = 0.0f;
    }
    else if (value >= table->range_max)
    {
        position = 1.0f;
    }
    else
    {
        position = (value - table->range_min) / (table->range_max - table->range_min);
    }
    if (table->size == 1u)
    {
        if (red != NULL) *red = table->colors[0];
        if (green != NULL) *green = table->colors[1];
        if (blue != NULL) *blue = table->colors[2];
        return;
    }
    position *= (float)(table->size - 1u);
    index = (FVizSize)position;
    if (index >= table->size - 1u)
    {
        index = table->size - 2u;
    }
    fraction = position - (float)index;
    if (red != NULL)
    {
        *red = table->colors[index * 3u + 0u] +
            (table->colors[(index + 1u) * 3u + 0u] - table->colors[index * 3u + 0u]) * fraction;
    }
    if (green != NULL)
    {
        *green = table->colors[index * 3u + 1u] +
            (table->colors[(index + 1u) * 3u + 1u] - table->colors[index * 3u + 1u]) * fraction;
    }
    if (blue != NULL)
    {
        *blue = table->colors[index * 3u + 2u] +
            (table->colors[(index + 1u) * 3u + 2u] - table->colors[index * 3u + 2u]) * fraction;
    }
}
