#include <math.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int test_legend_api(void)
{
    FVizScalarLegend* legend = NULL;
    FVizLookupTable* table = NULL;
    FVizLookupTable* returned;
    float minimum;
    float maximum;
    CHECK(fviz_scalar_legend_create(&legend) == FVIZ_OK);
    CHECK(legend != NULL);
    CHECK(fviz_scalar_legend_is_visible(legend) == FVIZ_TRUE);
    CHECK(fviz_scalar_legend_lookup_table(legend) != NULL);
    fviz_scalar_legend_set_range(legend, -5.0f, 25.0f);
    fviz_scalar_legend_get_range(legend, &minimum, &maximum);
    CHECK(minimum == -5.0f && maximum == 25.0f);
    fviz_scalar_legend_set_position(legend, FVIZ_LEGEND_BOTTOM_LEFT);
    CHECK(fviz_scalar_legend_position(legend) == FVIZ_LEGEND_BOTTOM_LEFT);
    fviz_scalar_legend_set_visible(legend, FVIZ_FALSE);
    CHECK(fviz_scalar_legend_is_visible(legend) == FVIZ_FALSE);
    fviz_scalar_legend_set_visible(legend, FVIZ_TRUE);
    fviz_scalar_legend_set_title(legend, "Stress [MPa]");
    CHECK(strcmp(fviz_scalar_legend_title(legend), "Stress [MPa]") == 0);
    CHECK(fviz_lookup_table_create(128u, &table) == FVIZ_OK);
    fviz_scalar_legend_set_lookup_table(legend, table);
    returned = fviz_scalar_legend_lookup_table(legend);
    CHECK(returned == table);
    CHECK(fviz_object_type_id((const FVizObject*)legend) == FVIZ_TYPE_SCALAR_LEGEND);
    fviz_release(table);
    fviz_release(legend);
    return 0;
}

static int test_legend_renderer_hook(void)
{
    FVizRenderer* renderer = NULL;
    FVizScalarLegend* legend = NULL;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_scalar_legend_create(&legend) == FVIZ_OK);
    CHECK(fviz_renderer_scalar_legend(renderer) == NULL);
    fviz_renderer_set_scalar_legend(renderer, legend);
    CHECK(fviz_renderer_scalar_legend(renderer) == legend);
    fviz_release(legend);
    CHECK(fviz_renderer_scalar_legend(renderer) == legend);
    fviz_release(renderer);
    return 0;
}

int main(void)
{
    CHECK(test_legend_api() == 0);
    CHECK(test_legend_renderer_hook() == 0);
    return 0;
}
