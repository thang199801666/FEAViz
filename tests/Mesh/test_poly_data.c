#include <math.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizPolyData* data = NULL;
    uint32_t a,b,c;
    const FVizVec3* normals;
    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0,0,0), &a) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(1,0,0), &b) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0,1,0), &c) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, a,b,c) == FVIZ_OK);
    CHECK(fviz_poly_data_validate(data) == FVIZ_OK);
    CHECK(fviz_poly_data_compute_normals(data) == FVIZ_OK);
    normals = fviz_poly_data_normals(data);
    CHECK(normals != NULL);
    CHECK(fabsf(normals[0].z - 1.0f) < 1.0e-6f);
    CHECK(fviz_poly_data_triangle_count(data) == 1u);
    fviz_release(data);
    return 0;
}
