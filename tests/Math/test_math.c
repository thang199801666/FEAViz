#include <math.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizVec3 x = fviz_vec3(1,0,0);
    FVizVec3 y = fviz_vec3(0,1,0);
    FVizVec3 z = fviz_vec3_cross(x, y);
    FVizBounds bounds = fviz_bounds_empty();
    FVizMat4 identity = fviz_mat4_identity();
    CHECK(fabsf(z.z - 1.0f) < 1.0e-6f);
    fviz_bounds_include_point(&bounds, fviz_vec3(-1,-2,-3));
    fviz_bounds_include_point(&bounds, fviz_vec3(3,2,1));
    CHECK(bounds.valid == FVIZ_TRUE);
    CHECK(fabsf(fviz_bounds_center(&bounds).x - 1.0f) < 1.0e-6f);
    CHECK(identity.m[0] == 1.0f && identity.m[15] == 1.0f);
    return 0;
}
