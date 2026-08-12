#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/IO/FVizSTLReader.h>

#include <FViz/Core/FVizErrorInternal.h>

static uint32_t fviz_u32_le(const unsigned char bytes[4])
{
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
}

static float fviz_f32_le(const unsigned char bytes[4])
{
    uint32_t bits = fviz_u32_le(bytes);
    float value;
    (void)memcpy(&value, &bits, sizeof(value));
    return value;
}

static FVizResult fviz_stl_add_triangle(FVizPolyData* poly_data, FVizVec3 a, FVizVec3 b, FVizVec3 c)
{
    uint32_t ia;
    uint32_t ib;
    uint32_t ic;
    if (fviz_poly_data_add_point(poly_data, a, &ia) != FVIZ_OK ||
        fviz_poly_data_add_point(poly_data, b, &ib) != FVIZ_OK ||
        fviz_poly_data_add_point(poly_data, c, &ic) != FVIZ_OK ||
        fviz_poly_data_add_triangle(poly_data, ia, ib, ic) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizBool fviz_stl_is_binary(FILE* file, uint32_t* out_triangles)
{
    unsigned char header[84];
    long size;
    uint64_t expected;
    if (fseek(file, 0, SEEK_END) != 0) return FVIZ_FALSE;
    size = ftell(file);
    if (size < 84 || fseek(file, 0, SEEK_SET) != 0) return FVIZ_FALSE;
    if (fread(header, 1u, sizeof(header), file) != sizeof(header)) return FVIZ_FALSE;
    *out_triangles = fviz_u32_le(header + 80u);
    expected = UINT64_C(84) + (uint64_t)(*out_triangles) * UINT64_C(50);
    (void)fseek(file, 0, SEEK_SET);
    return (uint64_t)size == expected ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_stl_read_binary(FILE* file, uint32_t triangle_count, FVizPolyData* poly_data)
{
    unsigned char header[84];
    uint32_t i;
    if (fread(header, 1u, sizeof(header), file) != sizeof(header))
    {
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to read STL binary header");
        return FVIZ_ERROR_IO;
    }
    if (fviz_poly_data_reserve(poly_data, (FVizSize)triangle_count * 3u, triangle_count) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    for (i = 0u; i < triangle_count; ++i)
    {
        unsigned char record[50];
        FVizVec3 a;
        FVizVec3 b;
        FVizVec3 c;
        if (fread(record, 1u, sizeof(record), file) != sizeof(record))
        {
            fviz_internal_set_error(FVIZ_ERROR_IO, "truncated binary STL triangle record");
            return FVIZ_ERROR_IO;
        }
        a = fviz_vec3(fviz_f32_le(record + 12u), fviz_f32_le(record + 16u), fviz_f32_le(record + 20u));
        b = fviz_vec3(fviz_f32_le(record + 24u), fviz_f32_le(record + 28u), fviz_f32_le(record + 32u));
        c = fviz_vec3(fviz_f32_le(record + 36u), fviz_f32_le(record + 40u), fviz_f32_le(record + 44u));
        if (fviz_stl_add_triangle(poly_data, a, b, c) != FVIZ_OK)
        {
            return fviz_last_error_code();
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_stl_read_ascii(FILE* file, FVizPolyData* poly_data)
{
    char line[2048];
    FVizVec3 vertices[3];
    unsigned vertex_count = 0u;
    while (fgets(line, (int)sizeof(line), file) != NULL)
    {
        char word[32];
        FVizVec3 v;
        if (sscanf(line, " %31s %f %f %f", word, &v.x, &v.y, &v.z) == 4 && strcmp(word, "vertex") == 0)
        {
            vertices[vertex_count++] = v;
            if (vertex_count == 3u)
            {
                if (fviz_stl_add_triangle(poly_data, vertices[0], vertices[1], vertices[2]) != FVIZ_OK)
                {
                    return fviz_last_error_code();
                }
                vertex_count = 0u;
            }
        }
    }
    if (ferror(file) != 0)
    {
        fviz_internal_set_error(FVIZ_ERROR_IO, "I/O failure while reading ASCII STL");
        return FVIZ_ERROR_IO;
    }
    if (vertex_count != 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_PARSE, "ASCII STL ended with an incomplete facet");
        return FVIZ_ERROR_PARSE;
    }
    return FVIZ_OK;
}

FVizResult fviz_stl_read(const char* path, FVizPolyData** out_poly_data)
{
    FILE* file;
    FVizPolyData* poly_data;
    uint32_t triangle_count = 0u;
    FVizBool binary;
    FVizResult result;

    if (path == NULL || out_poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "STL path and output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_poly_data = NULL;
    file = fopen(path, "rb");
    if (file == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to open STL file");
        return FVIZ_ERROR_IO;
    }
    binary = fviz_stl_is_binary(file, &triangle_count);
    if (fviz_poly_data_create(&poly_data) != FVIZ_OK)
    {
        (void)fclose(file);
        return fviz_last_error_code();
    }
    result = binary == FVIZ_TRUE ?
        fviz_stl_read_binary(file, triangle_count, poly_data) :
        fviz_stl_read_ascii(file, poly_data);
    (void)fclose(file);
    if (result != FVIZ_OK)
    {
        fviz_release(poly_data);
        return result;
    }
    if (fviz_poly_data_triangle_count(poly_data) == 0u)
    {
        fviz_release(poly_data);
        fviz_internal_set_error(FVIZ_ERROR_PARSE, "STL contains no triangles");
        return FVIZ_ERROR_PARSE;
    }
    if (fviz_poly_data_compute_normals(poly_data) != FVIZ_OK)
    {
        fviz_release(poly_data);
        return fviz_last_error_code();
    }
    *out_poly_data = poly_data;
    return FVIZ_OK;
}
