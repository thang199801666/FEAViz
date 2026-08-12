#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static long file_size(const char* path)
{
    FILE* file = NULL;
    long size;
#if defined(_MSC_VER)
    (void)fopen_s(&file, path, "rb");
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL) return -1;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return -1; }
    size = ftell(file);
    fclose(file);
    return size;
}

int main(void)
{
    FVizPolyData* data = NULL;
    uint32_t a, b, c;
    char header[256] = {0};
    FILE* file;
    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0, 0, 0), &a) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(1, 0, 0), &b) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0, 1, 0), &c) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data, a, b, c) == FVIZ_OK);
    CHECK(fviz_ply_write("FVizWriterAscii.ply", data, FVIZ_PLY_OUTPUT_ASCII) == FVIZ_OK);
    CHECK(fviz_ply_write("FVizWriterBinary.ply", data,
        FVIZ_PLY_OUTPUT_BINARY_LITTLE_ENDIAN) == FVIZ_OK);
#if defined(_MSC_VER)
    (void)fopen_s(&file, "FVizWriterAscii.ply", "rb");
#else
    file = fopen("FVizWriterAscii.ply", "rb");
#endif
    CHECK(file != NULL);
    CHECK(fread(header, 1u, sizeof(header) - 1u, file) != 0u);
    fclose(file);
    CHECK(strstr(header, "format ascii 1.0") != NULL);
    CHECK(strstr(header, "element vertex 3") != NULL);
    CHECK(strstr(header, "element face 1") != NULL);
    CHECK(file_size("FVizWriterBinary.ply") > 100);
    CHECK(remove("FVizWriterAscii.ply") == 0);
    CHECK(remove("FVizWriterBinary.ply") == 0);
    fviz_release(data);
    return 0;
}
