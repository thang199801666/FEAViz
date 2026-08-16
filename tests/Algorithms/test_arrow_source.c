#include <math.h>
#include <stdio.h>
#include <FViz/FViz.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
int main(void)
{
    FVizArrowSource* arrow = NULL;
    FVizPolyData* output;
    FVizBounds b;
    CHECK(fviz_arrow_source_create(&arrow) == FVIZ_OK);
    CHECK(fviz_arrow_source_set_radial_resolution(arrow, 16u) == FVIZ_OK);
    CHECK(fviz_arrow_source_set_shaft_radius(arrow, 0.04) == FVIZ_OK);
    CHECK(fviz_arrow_source_set_tip_radius(arrow, 0.10) == FVIZ_OK);
    CHECK(fviz_arrow_source_set_tip_length(arrow, 0.30) == FVIZ_OK);
    CHECK(fviz_arrow_source_update(arrow) == FVIZ_OK);
    output = fviz_arrow_source_output(arrow);
    CHECK(output != NULL);
    CHECK(fviz_poly_data_point_count(output) == 50u);
    CHECK(fviz_poly_data_triangle_count(output) == 96u);
    CHECK(fviz_poly_data_has_normals(output) == FVIZ_TRUE);
    CHECK(fviz_poly_data_validate(output) == FVIZ_OK);
    b = fviz_poly_data_bounds(output);
    CHECK(b.valid == FVIZ_TRUE);
    CHECK(fabsf(b.min.x) < 1.0e-5f && fabsf(b.max.x - 1.0f) < 1.0e-5f);
    CHECK(b.max.y >= 0.099f && b.min.y <= -0.099f);
    CHECK(fviz_arrow_source_set_tip_length(arrow, 1.0) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_arrow_source_set_radial_resolution(arrow, 2u) == FVIZ_ERROR_INVALID_ARGUMENT);
    fviz_release(arrow);
    return 0;
}
