#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

#ifndef FVIZ_TESTDATA_DIR
#error FVIZ_TESTDATA_DIR must be defined
#endif

int main(void)
{
    FVizPolyData* data = NULL;
    CHECK(fviz_mesh_read(FVIZ_TESTDATA_DIR "/cube.obj", &data) == FVIZ_OK);
    CHECK(fviz_poly_data_point_count(data) == 8u);
    CHECK(fviz_poly_data_triangle_count(data) == 12u);
    CHECK(fviz_poly_data_has_normals(data) == FVIZ_TRUE);
    fviz_release(data);
    data = NULL;
    CHECK(fviz_mesh_read(FVIZ_TESTDATA_DIR "/triangle.stl", &data) == FVIZ_OK);
    CHECK(fviz_poly_data_triangle_count(data) == 1u);
    fviz_release(data);
    return 0;
}
