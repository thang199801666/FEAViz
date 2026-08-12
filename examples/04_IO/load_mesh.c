#include <stdio.h>
#include <FViz/FViz.h>

int main(int argc, char** argv)
{
    FVizPolyData* data = NULL;
    if (argc < 2)
    {
        fprintf(stderr, "usage: FVizExampleLoadMesh model.obj|model.stl\n");
        return 2;
    }
    if (fviz_mesh_read(argv[1], &data) != FVIZ_OK)
    {
        fprintf(stderr, "%s\n", fviz_last_error_message());
        return 1;
    }
    printf("points=%zu triangles=%zu\n", (size_t)fviz_poly_data_point_count(data), (size_t)fviz_poly_data_triangle_count(data));
    fviz_release(data);
    return 0;
}
