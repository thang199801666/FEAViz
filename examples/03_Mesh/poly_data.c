#include <stdio.h>
#include <FViz/FViz.h>

int main(void)
{
    FVizPolyData* data = NULL;
    uint32_t a, b, c;
    FVizBounds bounds;
    if (fviz_poly_data_create(&data) != FVIZ_OK) return 1;
    (void)fviz_poly_data_add_point(data, fviz_vec3(0,0,0), &a);
    (void)fviz_poly_data_add_point(data, fviz_vec3(1,0,0), &b);
    (void)fviz_poly_data_add_point(data, fviz_vec3(0,1,0), &c);
    (void)fviz_poly_data_add_triangle(data, a, b, c);
    (void)fviz_poly_data_compute_normals(data);
    bounds = fviz_poly_data_bounds(data);
    printf("triangles=%zu bounds-valid=%u\n", (size_t)fviz_poly_data_triangle_count(data), (unsigned)bounds.valid);
    fviz_release(data);
    return 0;
}
