#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { \
    (void)fprintf(stderr, "geometry sources check failed at line %d: %s\n", __LINE__, #expr); \
    return __LINE__; \
} } while (0)

static FVizResult test_cone(void)
{
    FVizConeSource* source = NULL;
    FVizPolyData* output = NULL;
    CHECK(fviz_cone_source_create(&source) == FVIZ_OK);
    fviz_cone_source_set_height(source, 2.0);
    fviz_cone_source_set_radius(source, 1.0);
    fviz_cone_source_set_resolution(source, 8u);
    fviz_cone_source_set_capping(source, FVIZ_TRUE);
    CHECK(fviz_cone_source_update(source) == FVIZ_OK);
    output = fviz_cone_source_output(source);
    CHECK(output != NULL);
    /* 8 base ring points + apex = 9 points; 8 side + 6 cap triangles = 14. */
    CHECK(fviz_poly_data_point_count(output) == 9u);
    CHECK(fviz_poly_data_triangle_count(output) == 14u);
    CHECK(fviz_cone_source_resolution(source) == 8u);
    CHECK(fviz_cone_source_capping(source) == FVIZ_TRUE);
    fviz_release(source);
    return 0;
}

static FVizResult test_cylinder(void)
{
    FVizCylinderSource* source = NULL;
    FVizPolyData* output = NULL;
    CHECK(fviz_cylinder_source_create(&source) == FVIZ_OK);
    fviz_cylinder_source_set_height(source, 3.0);
    fviz_cylinder_source_set_radius(source, 0.5);
    fviz_cylinder_source_set_resolution(source, 6u);
    CHECK(fviz_cylinder_source_update(source) == FVIZ_OK);
    output = fviz_cylinder_source_output(source);
    CHECK(output != NULL);
    /* 6 top + 6 base = 12 points; 12 side + 8 cap = 20 triangles. */
    CHECK(fviz_poly_data_point_count(output) == 12u);
    CHECK(fviz_poly_data_triangle_count(output) == 20u);
    fviz_release(source);
    return 0;
}

static FVizResult test_disk(void)
{
    FVizDiskSource* source = NULL;
    FVizPolyData* output = NULL;
    CHECK(fviz_disk_source_create(&source) == FVIZ_OK);
    fviz_disk_source_set_outer_radius(source, 1.0);
    fviz_disk_source_set_radial_resolution(source, 2u);
    fviz_disk_source_set_circumferential_resolution(source, 8u);
    CHECK(fviz_disk_source_update(source) == FVIZ_OK);
    output = fviz_disk_source_output(source);
    CHECK(output != NULL);
    /* (2+1) rings * 8 points = 24 points; 2 rings * 8 quads * 2 = 32 triangles. */
    CHECK(fviz_poly_data_point_count(output) == 24u);
    CHECK(fviz_poly_data_triangle_count(output) == 32u);
    fviz_release(source);
    return 0;
}

static FVizResult test_line(void)
{
    FVizLineSource* source = NULL;
    FVizPolyData* output = NULL;
    CHECK(fviz_line_source_create(&source) == FVIZ_OK);
    CHECK(fviz_line_source_set_points(source, fviz_vec3(0.0f, 0.0f, 0.0f), fviz_vec3(1.0f, 0.0f, 0.0f)) == FVIZ_OK);
    CHECK(fviz_line_source_set_resolution(source, 4u) == FVIZ_OK);
    CHECK(fviz_line_source_update(source) == FVIZ_OK);
    output = fviz_line_source_output(source);
    CHECK(output != NULL);
    /* 5 points, 4 line segments. */
    CHECK(fviz_poly_data_point_count(output) == 5u);
    CHECK(fviz_poly_data_line_count(output) == 4u);
    fviz_release(source);
    return 0;
}

int main(void)
{
    int result = 0;
    if ((result = test_cone()) != 0) { fprintf(stderr, "test_cone failed at %d\n", result); return result; }
    if ((result = test_cylinder()) != 0) { fprintf(stderr, "test_cylinder failed at %d\n", result); return result; }
    if ((result = test_disk()) != 0) { fprintf(stderr, "test_disk failed at %d\n", result); return result; }
    if ((result = test_line()) != 0) { fprintf(stderr, "test_line failed at %d\n", result); return result; }
    return 0;
}
