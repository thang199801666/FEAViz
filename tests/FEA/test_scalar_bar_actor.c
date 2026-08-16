#include <stdio.h>
#include <string.h>

#include <FViz/FEA/FVizFEA.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void)
{
    FVizFEAScalarBarOptions options;
    FVizFEAScalarBarActor* actor = NULL;
    FVizScalarLegend* legend;
    float minimum = 0.0f;
    float maximum = 0.0f;
    float px = 0.0f;
    float py = 0.0f;
    fviz_fea_scalar_bar_options_initialize(&options);
    CHECK(options.interval_count == 12u && options.tick_count == 13u);
    CHECK(options.position == FVIZ_LEGEND_TOP_LEFT);
    CHECK(options.padding_horizontal == 0.02f && options.padding_vertical == 0.05f);
    CHECK(options.discrete == FVIZ_TRUE && options.ticks_visible == FVIZ_TRUE);
    CHECK(options.bar_width_pixels == 18.0f && options.bar_height_pixels == 180.0f);
    CHECK(fviz_fea_scalar_bar_actor_create(&options, &actor) == FVIZ_OK);
    legend = fviz_fea_scalar_bar_actor_legend(actor);
    CHECK(legend != NULL);
    CHECK(strcmp(fviz_scalar_legend_title(legend), "S, Mises") == 0);
    CHECK(strcmp(fviz_scalar_legend_units(legend), "MPa") == 0);
    CHECK(fviz_scalar_legend_tick_count(legend) == 13u);
    CHECK(fviz_scalar_legend_is_discrete(legend) == FVIZ_TRUE);
    fviz_scalar_legend_get_range(legend, &minimum, &maximum);
    CHECK(minimum == 0.0f && maximum == 1.0f);
    fviz_scalar_legend_get_viewport_padding(legend, &px, &py);
    CHECK(px == 0.02f && py == 0.05f);
    options.range_minimum = -120.0f;
    options.range_maximum = 340.0f;
    options.title = "U, Magnitude";
    options.units = "mm";
    options.interval_count = 16u;
    options.tick_count = 17u;
    options.label_format = "%+.2f";
    options.title_shadow = FVIZ_TRUE;
    options.label_shadow = FVIZ_TRUE;
    CHECK(fviz_fea_scalar_bar_actor_apply(actor, &options) == FVIZ_OK);
    CHECK(strcmp(fviz_scalar_legend_title(legend), "U, Magnitude") == 0);
    CHECK(strcmp(fviz_scalar_legend_label_format(legend), "%+.2f") == 0);
    CHECK(fviz_scalar_legend_tick_count(legend) == 17u);
    fviz_scalar_legend_get_range(legend, &minimum, &maximum);
    CHECK(minimum == -120.0f && maximum == 340.0f);
    fviz_release(actor);
    return 0;
}
