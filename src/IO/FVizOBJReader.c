#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/IO/FVizOBJReader.h>

#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_OBJ_LINE_CAPACITY 16384u

static const char* fviz_obj_skip_space(const char* text)
{
    while (*text != '\0' && isspace((unsigned char)*text) != 0) ++text;
    return text;
}

static FVizBool fviz_obj_parse_vertex_index(const char* token, FVizSize point_count, uint32_t* out_index)
{
    char* end = NULL;
    int64_t value;
    if (token == NULL || out_index == NULL) return FVIZ_FALSE;
    value = (int64_t)strtoll(token, &end, 10);
    if (end == token || value == 0) return FVIZ_FALSE;
    if (value > 0)
    {
        const uint64_t index = (uint64_t)(value - 1);
        if (index >= (uint64_t)point_count || index > UINT32_MAX) return FVIZ_FALSE;
        *out_index = (uint32_t)index;
    }
    else
    {
        int64_t resolved;
        if (sizeof(FVizSize) > sizeof(int64_t) && point_count > (FVizSize)INT64_MAX) return FVIZ_FALSE;
        resolved = (int64_t)point_count + value;
        if (resolved < 0 || (uint64_t)resolved >= (uint64_t)point_count || (uint64_t)resolved > UINT32_MAX) return FVIZ_FALSE;
        *out_index = (uint32_t)resolved;
    }
    return FVIZ_TRUE;
}

static FVizResult fviz_obj_parse_face(FVizPolyData* poly_data, char* text)
{
    uint32_t face[4096];
    FVizSize count = 0u;
    char* current = text;
    const FVizSize point_count = fviz_poly_data_point_count(poly_data);

    while (*current != '\0')
    {
        char* token;
        char saved;
        current = (char*)fviz_obj_skip_space(current);
        if (*current == '\0' || *current == '#') break;
        token = current;
        while (*current != '\0' && isspace((unsigned char)*current) == 0) ++current;
        saved = *current;
        *current = '\0';
        if (count >= FVIZ_ARRAY_COUNT(face) ||
            fviz_obj_parse_vertex_index(token, point_count, &face[count]) == FVIZ_FALSE)
        {
            *current = saved;
            fviz_internal_set_error(FVIZ_ERROR_PARSE, "OBJ face contains an invalid or excessively large vertex list");
            return FVIZ_ERROR_PARSE;
        }
        ++count;
        *current = saved;
        if (saved != '\0') ++current;
    }

    if (count < 3u)
    {
        fviz_internal_set_error(FVIZ_ERROR_PARSE, "OBJ face contains fewer than three vertices");
        return FVIZ_ERROR_PARSE;
    }

    {
        FVizSize i;
        for (i = 1u; i + 1u < count; ++i)
        {
            if (fviz_poly_data_add_triangle(poly_data, face[0], face[i], face[i + 1u]) != FVIZ_OK)
            {
                return fviz_last_error_code();
            }
        }
    }
    return FVIZ_OK;
}

FVizResult fviz_obj_read(const char* path, FVizPolyData** out_poly_data)
{
    FILE* file;
    char line[FVIZ_OBJ_LINE_CAPACITY];
    FVizPolyData* poly_data;
    FVizSize line_number = 0u;

    if (path == NULL || out_poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "OBJ path and output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_poly_data = NULL;
    file = fopen(path, "rb");
    if (file == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to open OBJ file");
        return FVIZ_ERROR_IO;
    }
    if (fviz_poly_data_create(&poly_data) != FVIZ_OK)
    {
        (void)fclose(file);
        return fviz_last_error_code();
    }

    while (fgets(line, (int)sizeof(line), file) != NULL)
    {
        const char* text;
        ++line_number;
        text = fviz_obj_skip_space(line);
        if (text[0] == 'v' && isspace((unsigned char)text[1]) != 0)
        {
            FVizVec3 point;
            if (sscanf(text + 1, "%f %f %f", &point.x, &point.y, &point.z) != 3)
            {
                FVIZ_UNUSED(line_number);
                fviz_release(poly_data);
                (void)fclose(file);
                fviz_internal_set_error(FVIZ_ERROR_PARSE, "failed to parse OBJ vertex");
                return FVIZ_ERROR_PARSE;
            }
            if (fviz_poly_data_add_point(poly_data, point, NULL) != FVIZ_OK)
            {
                fviz_release(poly_data);
                (void)fclose(file);
                return fviz_last_error_code();
            }
        }
        else if (text[0] == 'f' && isspace((unsigned char)text[1]) != 0)
        {
            FVizResult result = fviz_obj_parse_face(poly_data, (char*)(text + 1));
            if (result != FVIZ_OK)
            {
                fviz_release(poly_data);
                (void)fclose(file);
                return result;
            }
        }
    }
    if (ferror(file) != 0)
    {
        fviz_release(poly_data);
        (void)fclose(file);
        fviz_internal_set_error(FVIZ_ERROR_IO, "I/O failure while reading OBJ file");
        return FVIZ_ERROR_IO;
    }
    (void)fclose(file);

    if (fviz_poly_data_point_count(poly_data) == 0u || fviz_poly_data_triangle_count(poly_data) == 0u)
    {
        fviz_release(poly_data);
        fviz_internal_set_error(FVIZ_ERROR_PARSE, "OBJ contains no renderable triangle geometry");
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
