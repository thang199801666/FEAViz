#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataType.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/IO/FVizVTKLegacyReader.h>

#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_VTK_MAX_LINE 4096u

typedef enum FVizVTKEncoding
{
    FVIZ_VTK_ASCII = 0,
    FVIZ_VTK_BINARY = 1
} FVizVTKEncoding;

typedef struct FVizVTKCellDraft
{
    uint32_t* ids;
    FVizSize point_count;
} FVizVTKCellDraft;

static FVizResult fviz_vtk_io_error(const char* message)
{
    fviz_internal_set_error(FVIZ_ERROR_IO, message);
    return FVIZ_ERROR_IO;
}

static FVizResult fviz_vtk_format_error(const char* message)
{
    fviz_internal_set_error(FVIZ_ERROR_PARSE, message);
    return FVIZ_ERROR_PARSE;
}

static char* fviz_vtk_trim(char* line)
{
    char* start = line;
    char* end;
    while (*start != '\0' && isspace((unsigned char)*start)) ++start;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) --end;
    *end = '\0';
    return start;
}

static FVizBool fviz_vtk_read_line(FILE* file, char* line, FVizSize capacity)
{
    while (fgets(line, (int)capacity, file) != NULL)
    {
        char* trimmed = fviz_vtk_trim(line);
        if (trimmed[0] == '\0') continue;
        if (trimmed != line) (void)memmove(line, trimmed, strlen(trimmed) + 1u);
        return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

static FVizBool fviz_vtk_keyword(const char* line, const char* keyword)
{
    const FVizSize length = strlen(keyword);
    return strncmp(line, keyword, length) == 0 &&
        (line[length] == '\0' || isspace((unsigned char)line[length]));
}

static FVizBool fviz_vtk_read_token(FILE* file, char* out_token, FVizSize max_size)
{
    int c;
    FVizSize index = 0u;
    for (;;)
    {
        c = fgetc(file);
        if (c == EOF) break;
        if (isspace((unsigned char)c) || c == ',')
        {
            if (index > 0u) break;
            continue;
        }
        if (index + 1u < max_size) out_token[index++] = (char)c;
    }
    out_token[index] = '\0';
    return index > 0u ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_vtk_little_endian(void)
{
    const uint16_t value = 1u;
    return (*(const uint8_t*)&value == 1u) ? FVIZ_TRUE : FVIZ_FALSE;
}

static void fviz_vtk_swap_bytes(void* value, FVizSize size)
{
    uint8_t* bytes = (uint8_t*)value;
    FVizSize i;
    for (i = 0u; i < size / 2u; ++i)
    {
        const uint8_t temporary = bytes[i];
        bytes[i] = bytes[size - i - 1u];
        bytes[size - i - 1u] = temporary;
    }
}

static FVizBool fviz_vtk_data_type(const char* name, FVizDataType* out_type)
{
    if (strcmp(name, "char") == 0 || strcmp(name, "signed_char") == 0) *out_type = FVIZ_DATA_INT8;
    else if (strcmp(name, "unsigned_char") == 0) *out_type = FVIZ_DATA_UINT8;
    else if (strcmp(name, "short") == 0) *out_type = FVIZ_DATA_INT16;
    else if (strcmp(name, "unsigned_short") == 0) *out_type = FVIZ_DATA_UINT16;
    else if (strcmp(name, "int") == 0) *out_type = FVIZ_DATA_INT32;
    else if (strcmp(name, "unsigned_int") == 0) *out_type = FVIZ_DATA_UINT32;
    else if (strcmp(name, "long") == 0 || strcmp(name, "long_long") == 0) *out_type = FVIZ_DATA_INT64;
    else if (strcmp(name, "unsigned_long") == 0 || strcmp(name, "unsigned_long_long") == 0) *out_type = FVIZ_DATA_UINT64;
    else if (strcmp(name, "float") == 0) *out_type = FVIZ_DATA_FLOAT32;
    else if (strcmp(name, "double") == 0) *out_type = FVIZ_DATA_FLOAT64;
    else return FVIZ_FALSE;
    return FVIZ_TRUE;
}

static FVizResult fviz_vtk_parse_ascii_value(const char* token, FVizDataType type, void* destination)
{
    char* end = NULL;
    switch (type)
    {
        case FVIZ_DATA_INT8: *(int8_t*)destination = (int8_t)strtol(token, &end, 10); break;
        case FVIZ_DATA_UINT8: *(uint8_t*)destination = (uint8_t)strtoul(token, &end, 10); break;
        case FVIZ_DATA_INT16: *(int16_t*)destination = (int16_t)strtol(token, &end, 10); break;
        case FVIZ_DATA_UINT16: *(uint16_t*)destination = (uint16_t)strtoul(token, &end, 10); break;
        case FVIZ_DATA_INT32: *(int32_t*)destination = (int32_t)strtol(token, &end, 10); break;
        case FVIZ_DATA_UINT32: *(uint32_t*)destination = (uint32_t)strtoul(token, &end, 10); break;
        case FVIZ_DATA_INT64: *(int64_t*)destination = (int64_t)strtoll(token, &end, 10); break;
        case FVIZ_DATA_UINT64: *(uint64_t*)destination = (uint64_t)strtoull(token, &end, 10); break;
        case FVIZ_DATA_FLOAT32: *(float*)destination = strtof(token, &end); break;
        case FVIZ_DATA_FLOAT64: *(double*)destination = strtod(token, &end); break;
        default: return fviz_vtk_format_error("unsupported VTK numeric data type");
    }
    if (end == token || *end != '\0') return fviz_vtk_format_error("invalid numeric value in VTK file");
    return FVIZ_OK;
}

static FVizResult fviz_vtk_read_values(
    FILE* file,
    FVizVTKEncoding encoding,
    FVizDataType type,
    FVizSize value_count,
    void* destination)
{
    const FVizSize item_size = fviz_data_type_size(type);
    uint8_t* bytes = (uint8_t*)destination;
    FVizSize i;
    if (item_size == 0u || (value_count > 0u && destination == NULL))
        return fviz_vtk_format_error("invalid VTK numeric array");
    if (encoding == FVIZ_VTK_BINARY)
    {
        if (value_count > ((FVizSize)-1) / item_size)
            return fviz_vtk_format_error("VTK numeric array is too large");
        if (fread(destination, item_size, value_count, file) != value_count)
            return fviz_vtk_io_error("unexpected end of binary VTK data");
        if (item_size > 1u && fviz_vtk_little_endian())
            for (i = 0u; i < value_count; ++i) fviz_vtk_swap_bytes(bytes + i * item_size, item_size);
        return FVIZ_OK;
    }
    for (i = 0u; i < value_count; ++i)
    {
        char token[128];
        if (!fviz_vtk_read_token(file, token, sizeof(token)))
            return fviz_vtk_io_error("unexpected end of ASCII VTK data");
        if (fviz_vtk_parse_ascii_value(token, type, bytes + i * item_size) != FVIZ_OK)
            return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static double fviz_vtk_value_as_double(const void* value, FVizDataType type)
{
    switch (type)
    {
        case FVIZ_DATA_INT8: return (double)*(const int8_t*)value;
        case FVIZ_DATA_UINT8: return (double)*(const uint8_t*)value;
        case FVIZ_DATA_INT16: return (double)*(const int16_t*)value;
        case FVIZ_DATA_UINT16: return (double)*(const uint16_t*)value;
        case FVIZ_DATA_INT32: return (double)*(const int32_t*)value;
        case FVIZ_DATA_UINT32: return (double)*(const uint32_t*)value;
        case FVIZ_DATA_INT64: return (double)*(const int64_t*)value;
        case FVIZ_DATA_UINT64: return (double)*(const uint64_t*)value;
        case FVIZ_DATA_FLOAT32: return (double)*(const float*)value;
        case FVIZ_DATA_FLOAT64: return *(const double*)value;
        default: return 0.0;
    }
}

static FVizCellType fviz_vtk_cell_type(int32_t type)
{
    switch (type)
    {
        case 5: return FVIZ_CELL_TRIANGLE;
        case 8: return FVIZ_CELL_PIXEL;
        case 9: return FVIZ_CELL_QUAD;
        case 10: return FVIZ_CELL_TETRA;
        case 11: return FVIZ_CELL_VOXEL;
        case 12: return FVIZ_CELL_HEXAHEDRON;
        case 13: return FVIZ_CELL_WEDGE;
        case 14: return FVIZ_CELL_PYRAMID;
        case 15: return FVIZ_CELL_PENTAGONAL_PRISM;
        case 16: return FVIZ_CELL_HEXAGONAL_PRISM;
        case 21: return FVIZ_CELL_QUADRATIC_EDGE;
        case 22: return FVIZ_CELL_QUADRATIC_TRIANGLE;
        case 23: return FVIZ_CELL_QUADRATIC_QUAD;
        case 24: return FVIZ_CELL_QUADRATIC_TETRA;
        case 25: return FVIZ_CELL_QUADRATIC_HEXAHEDRON;
        case 26: return FVIZ_CELL_QUADRATIC_WEDGE;
        case 27: return FVIZ_CELL_QUADRATIC_PYRAMID;
        case 28: return FVIZ_CELL_BIQUADRATIC_QUAD;
        case 29: return FVIZ_CELL_TRIQUADRATIC_HEXAHEDRON;
        case 30: return FVIZ_CELL_QUADRATIC_LINEAR_QUAD;
        case 31: return FVIZ_CELL_QUADRATIC_LINEAR_WEDGE;
        case 32: return FVIZ_CELL_BIQUADRATIC_QUADRATIC_WEDGE;
        case 33: return FVIZ_CELL_BIQUADRATIC_QUADRATIC_HEXAHEDRON;
        case 34: return FVIZ_CELL_BIQUADRATIC_TRIANGLE;
        case 36: return FVIZ_CELL_CUBIC_LINE;
        case 37: return FVIZ_CELL_QUADRATIC_POLYGON;
        case 41: return FVIZ_CELL_CONVEX_POINT_SET;
        case 42: return FVIZ_CELL_POLYHEDRON;
        default: return (FVizCellType)0;
    }
}

static void fviz_vtk_free_cells(FVizVTKCellDraft* cells, FVizSize count)
{
    FVizSize i;
    if (cells == NULL) return;
    for (i = 0u; i < count; ++i) fviz_free(cells[i].ids);
    fviz_free(cells);
}

static FVizResult fviz_vtk_parse_points(
    FVizUnstructuredGrid* grid,
    FILE* file,
    FVizVTKEncoding encoding,
    FVizSize count,
    FVizDataType type)
{
    const FVizSize item_size = fviz_data_type_size(type);
    uint8_t tuple[3u * sizeof(double)];
    FVizSize i;
    for (i = 0u; i < count; ++i)
    {
        FVizVec3 point;
        FVizResult result = fviz_vtk_read_values(file, encoding, type, 3u, tuple);
        if (result != FVIZ_OK) return result;
        point = fviz_vec3(
            (float)fviz_vtk_value_as_double(tuple, type),
            (float)fviz_vtk_value_as_double(tuple + item_size, type),
            (float)fviz_vtk_value_as_double(tuple + 2u * item_size, type));
        if (fviz_unstructured_grid_add_point(grid, point, NULL) != FVIZ_OK) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_vtk_parse_cells(
    FILE* file,
    FVizVTKEncoding encoding,
    FVizSize cell_count,
    FVizSize integer_count,
    FVizVTKCellDraft** out_cells)
{
    int32_t* values = NULL;
    FVizVTKCellDraft* cells = NULL;
    FVizSize cursor = 0u;
    FVizSize i;
    if (integer_count < cell_count) return fviz_vtk_format_error("invalid VTK CELLS size");
    if (integer_count > ((FVizSize)-1) / sizeof(int32_t) ||
        cell_count > ((FVizSize)-1) / sizeof(FVizVTKCellDraft))
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "VTK CELLS allocation size overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    values = (int32_t*)fviz_alloc(integer_count * sizeof(int32_t));
    cells = (FVizVTKCellDraft*)fviz_alloc(cell_count * sizeof(FVizVTKCellDraft));
    if ((integer_count > 0u && values == NULL) || (cell_count > 0u && cells == NULL))
    {
        fviz_free(values);
        fviz_free(cells);
        return fviz_last_error_code();
    }
    if (cell_count > 0u) (void)memset(cells, 0, cell_count * sizeof(FVizVTKCellDraft));
    if (fviz_vtk_read_values(file, encoding, FVIZ_DATA_INT32, integer_count, values) != FVIZ_OK)
    {
        fviz_free(values);
        fviz_free(cells);
        return fviz_last_error_code();
    }
    for (i = 0u; i < cell_count; ++i)
    {
        FVizSize k;
        if (cursor >= integer_count || values[cursor] < 0)
        {
            fviz_free(values);
            fviz_vtk_free_cells(cells, cell_count);
            return fviz_vtk_format_error("invalid cell record in VTK file");
        }
        cells[i].point_count = (FVizSize)values[cursor++];
        if (cells[i].point_count > integer_count - cursor)
        {
            fviz_free(values);
            fviz_vtk_free_cells(cells, cell_count);
            return fviz_vtk_format_error("VTK cell connectivity exceeds CELLS size");
        }
        cells[i].ids = (uint32_t*)fviz_alloc(cells[i].point_count * sizeof(uint32_t));
        if (cells[i].point_count > 0u && cells[i].ids == NULL)
        {
            fviz_free(values);
            fviz_vtk_free_cells(cells, cell_count);
            return fviz_last_error_code();
        }
        for (k = 0u; k < cells[i].point_count; ++k)
        {
            if (values[cursor] < 0)
            {
                fviz_free(values);
                fviz_vtk_free_cells(cells, cell_count);
                return fviz_vtk_format_error("negative point id in VTK cell connectivity");
            }
            cells[i].ids[k] = (uint32_t)values[cursor++];
        }
    }
    fviz_free(values);
    if (cursor != integer_count)
    {
        fviz_vtk_free_cells(cells, cell_count);
        return fviz_vtk_format_error("VTK CELLS size does not match its records");
    }
    *out_cells = cells;
    return FVIZ_OK;
}

static FVizResult fviz_vtk_parse_cell_types(
    FILE* file,
    FVizVTKEncoding encoding,
    FVizSize count,
    FVizCellType** out_types)
{
    int32_t* vtk_types = NULL;
    FVizCellType* types = NULL;
    FVizSize i;
    if (count > ((FVizSize)-1) / sizeof(int32_t) || count > ((FVizSize)-1) / sizeof(FVizCellType))
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "VTK CELL_TYPES allocation size overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    vtk_types = (int32_t*)fviz_alloc(count * sizeof(int32_t));
    types = (FVizCellType*)fviz_alloc(count * sizeof(FVizCellType));
    if ((count > 0u && vtk_types == NULL) || (count > 0u && types == NULL))
    {
        fviz_free(vtk_types);
        fviz_free(types);
        return fviz_last_error_code();
    }
    if (fviz_vtk_read_values(file, encoding, FVIZ_DATA_INT32, count, vtk_types) != FVIZ_OK)
    {
        fviz_free(vtk_types);
        fviz_free(types);
        return fviz_last_error_code();
    }
    for (i = 0u; i < count; ++i)
    {
        types[i] = fviz_vtk_cell_type(vtk_types[i]);
        if (types[i] == (FVizCellType)0)
        {
            fviz_free(vtk_types);
            fviz_free(types);
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported VTK cell type in legacy file");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
    }
    fviz_free(vtk_types);
    *out_types = types;
    return FVIZ_OK;
}

static FVizResult fviz_vtk_read_array(
    FILE* file,
    FVizVTKEncoding encoding,
    FVizAttributeSet* destination,
    const char* name,
    FVizDataType type,
    uint32_t components,
    FVizSize tuples)
{
    FVizDataArray* array = NULL;
    FVizResult result;
    if (components == 0u || tuples > ((FVizSize)-1) / components)
        return fviz_vtk_format_error("invalid VTK array dimensions");
    result = fviz_data_array_create(type, components, &array);
    if (result == FVIZ_OK) result = fviz_data_array_resize(array, tuples);
    if (result == FVIZ_OK)
        result = fviz_vtk_read_values(
            file, encoding, type, tuples * (FVizSize)components, fviz_data_array_data(array));
    if (result == FVIZ_OK) result = fviz_attribute_set_add(destination, name, array);
    fviz_release(array);
    return result;
}

static FVizResult fviz_vtk_parse_field(
    FILE* file,
    FVizVTKEncoding encoding,
    FVizAttributeSet* destination,
    FVizSize array_count)
{
    char line[FVIZ_VTK_MAX_LINE];
    FVizSize i;
    for (i = 0u; i < array_count; ++i)
    {
        char name[256];
        char type_name[64];
        unsigned long components;
        unsigned long long tuples;
        FVizDataType type;
        if (!fviz_vtk_read_line(file, line, sizeof(line)))
            return fviz_vtk_io_error("missing VTK FIELD array header");
        if (sscanf(line, "%255s %lu %llu %63s", name, &components, &tuples, type_name) != 4)
            return fviz_vtk_format_error("invalid VTK FIELD array header");
        if (!fviz_vtk_data_type(type_name, &type))
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported VTK FIELD data type");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        if (components == 0ul || components > UINT32_MAX)
            return fviz_vtk_format_error("invalid VTK FIELD component count");
        if (fviz_vtk_read_array(
                file, encoding, destination, name, type, (uint32_t)components, (FVizSize)tuples) != FVIZ_OK)
            return fviz_last_error_code();
    }
    return FVIZ_OK;
}

FVizResult fviz_vtk_legacy_read(const char* file_path, FVizUnstructuredGrid** out_grid)
{
    FILE* file = NULL;
    FVizUnstructuredGrid* grid = NULL;
    FVizVTKCellDraft* cells = NULL;
    FVizCellType* cell_types = NULL;
    FVizAttributeSet* active_attributes = NULL;
    FVizSize active_tuple_count = 0u;
    FVizSize point_count = 0u;
    FVizSize cell_count = 0u;
    FVizSize cell_type_count = 0u;
    FVizVTKEncoding encoding;
    FVizResult result = FVIZ_OK;
    char line[FVIZ_VTK_MAX_LINE];

    if (out_grid == NULL || file_path == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "file path and output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    file = fopen(file_path, "rb");
    if (file == NULL) return fviz_vtk_io_error("failed to open VTK file");

    if (!fviz_vtk_read_line(file, line, sizeof(line)) || strncmp(line, "# vtk DataFile Version", 22u) != 0)
    {
        result = fviz_vtk_format_error("not a VTK legacy file");
        goto cleanup;
    }
    if (!fviz_vtk_read_line(file, line, sizeof(line)))
    {
        result = fviz_vtk_io_error("missing VTK title");
        goto cleanup;
    }
    if (!fviz_vtk_read_line(file, line, sizeof(line)))
    {
        result = fviz_vtk_io_error("missing VTK encoding");
        goto cleanup;
    }
    if (strcmp(line, "ASCII") == 0) encoding = FVIZ_VTK_ASCII;
    else if (strcmp(line, "BINARY") == 0) encoding = FVIZ_VTK_BINARY;
    else
    {
        result = fviz_vtk_format_error("VTK encoding must be ASCII or BINARY");
        goto cleanup;
    }
    if (!fviz_vtk_read_line(file, line, sizeof(line)) || strcmp(line, "DATASET UNSTRUCTURED_GRID") != 0)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "only VTK legacy UNSTRUCTURED_GRID is supported");
        result = FVIZ_ERROR_NOT_SUPPORTED;
        goto cleanup;
    }
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto cleanup;
    }

    while (result == FVIZ_OK && fviz_vtk_read_line(file, line, sizeof(line)))
    {
        if (fviz_vtk_keyword(line, "POINTS"))
        {
            char type_name[64];
            unsigned long long count;
            FVizDataType type;
            if (sscanf(line, "POINTS %llu %63s", &count, type_name) != 2)
            {
                result = fviz_vtk_format_error("invalid VTK POINTS header");
                break;
            }
            if (!fviz_vtk_data_type(type_name, &type))
            {
                fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported VTK point data type");
                result = FVIZ_ERROR_NOT_SUPPORTED;
                break;
            }
            point_count = (FVizSize)count;
            result = fviz_vtk_parse_points(grid, file, encoding, point_count, type);
        }
        else if (fviz_vtk_keyword(line, "CELLS"))
        {
            unsigned long long count;
            unsigned long long size;
            if (sscanf(line, "CELLS %llu %llu", &count, &size) != 2)
            {
                result = fviz_vtk_format_error("invalid VTK CELLS header");
                break;
            }
            if (cells != NULL)
            {
                result = fviz_vtk_format_error("duplicate VTK CELLS section");
                break;
            }
            cell_count = (FVizSize)count;
            result = fviz_vtk_parse_cells(file, encoding, cell_count, (FVizSize)size, &cells);
        }
        else if (fviz_vtk_keyword(line, "CELL_TYPES"))
        {
            unsigned long long count;
            if (sscanf(line, "CELL_TYPES %llu", &count) != 1)
            {
                result = fviz_vtk_format_error("invalid VTK CELL_TYPES header");
                break;
            }
            cell_type_count = (FVizSize)count;
            result = fviz_vtk_parse_cell_types(file, encoding, cell_type_count, &cell_types);
        }
        else if (fviz_vtk_keyword(line, "POINT_DATA"))
        {
            unsigned long long count;
            if (sscanf(line, "POINT_DATA %llu", &count) != 1 || (FVizSize)count != point_count)
            {
                result = fviz_vtk_format_error("VTK POINT_DATA count does not match POINTS");
                break;
            }
            active_tuple_count = (FVizSize)count;
            active_attributes = fviz_unstructured_grid_point_data(grid);
        }
        else if (fviz_vtk_keyword(line, "CELL_DATA"))
        {
            unsigned long long count;
            if (sscanf(line, "CELL_DATA %llu", &count) != 1 || (FVizSize)count != cell_count)
            {
                result = fviz_vtk_format_error("VTK CELL_DATA count does not match CELLS");
                break;
            }
            active_tuple_count = (FVizSize)count;
            active_attributes = fviz_unstructured_grid_cell_data(grid);
        }
        else if (fviz_vtk_keyword(line, "SCALARS"))
        {
            char name[256];
            char type_name[64];
            unsigned long components = 1ul;
            FVizDataType type;
            const int parsed = sscanf(line, "SCALARS %255s %63s %lu", name, type_name, &components);
            if (active_attributes == NULL || parsed < 2 || components == 0ul || components > UINT32_MAX)
            {
                result = fviz_vtk_format_error("invalid VTK SCALARS declaration");
                break;
            }
            if (!fviz_vtk_data_type(type_name, &type))
            {
                fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported VTK scalar data type");
                result = FVIZ_ERROR_NOT_SUPPORTED;
                break;
            }
            if (!fviz_vtk_read_line(file, line, sizeof(line)) || !fviz_vtk_keyword(line, "LOOKUP_TABLE"))
            {
                result = fviz_vtk_format_error("VTK SCALARS is missing LOOKUP_TABLE");
                break;
            }
            result = fviz_vtk_read_array(
                file, encoding, active_attributes, name, type, (uint32_t)components, active_tuple_count);
        }
        else if (fviz_vtk_keyword(line, "VECTORS") || fviz_vtk_keyword(line, "NORMALS") ||
                 fviz_vtk_keyword(line, "TENSORS"))
        {
            char keyword[32];
            char name[256];
            char type_name[64];
            FVizDataType type;
            uint32_t components;
            if (active_attributes == NULL || sscanf(line, "%31s %255s %63s", keyword, name, type_name) != 3)
            {
                result = fviz_vtk_format_error("invalid VTK attribute declaration");
                break;
            }
            if (!fviz_vtk_data_type(type_name, &type))
            {
                fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported VTK attribute data type");
                result = FVIZ_ERROR_NOT_SUPPORTED;
                break;
            }
            components = strcmp(keyword, "TENSORS") == 0 ? 9u : 3u;
            result = fviz_vtk_read_array(file, encoding, active_attributes, name, type, components, active_tuple_count);
        }
        else if (fviz_vtk_keyword(line, "TEXTURE_COORDINATES"))
        {
            char name[256];
            char type_name[64];
            unsigned long components;
            FVizDataType type;
            if (active_attributes == NULL ||
                sscanf(line, "TEXTURE_COORDINATES %255s %lu %63s", name, &components, type_name) != 3 ||
                components == 0ul || components > 3ul)
            {
                result = fviz_vtk_format_error("invalid VTK TEXTURE_COORDINATES declaration");
                break;
            }
            if (!fviz_vtk_data_type(type_name, &type))
            {
                fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported VTK texture-coordinate data type");
                result = FVIZ_ERROR_NOT_SUPPORTED;
                break;
            }
            result = fviz_vtk_read_array(
                file, encoding, active_attributes, name, type, (uint32_t)components, active_tuple_count);
        }
        else if (fviz_vtk_keyword(line, "COLOR_SCALARS"))
        {
            char name[256];
            unsigned long components;
            const FVizDataType type = encoding == FVIZ_VTK_BINARY ? FVIZ_DATA_UINT8 : FVIZ_DATA_FLOAT32;
            if (active_attributes == NULL ||
                sscanf(line, "COLOR_SCALARS %255s %lu", name, &components) != 2 ||
                components == 0ul || components > UINT32_MAX)
            {
                result = fviz_vtk_format_error("invalid VTK COLOR_SCALARS declaration");
                break;
            }
            result = fviz_vtk_read_array(
                file, encoding, active_attributes, name, type, (uint32_t)components, active_tuple_count);
        }
        else if (fviz_vtk_keyword(line, "FIELD"))
        {
            char field_name[256];
            unsigned long long count;
            FVizAttributeSet* destination = active_attributes != NULL
                ? active_attributes : fviz_unstructured_grid_field_data(grid);
            if (sscanf(line, "FIELD %255s %llu", field_name, &count) != 2)
            {
                result = fviz_vtk_format_error("invalid VTK FIELD declaration");
                break;
            }
            result = fviz_vtk_parse_field(file, encoding, destination, (FVizSize)count);
        }
        else if (fviz_vtk_keyword(line, "METADATA"))
        {
            /* VTK metadata is optional and has no effect on the numeric dataset. */
        }
        else if (fviz_vtk_keyword(line, "INFORMATION"))
        {
            unsigned long long count;
            if (sscanf(line, "INFORMATION %llu", &count) != 1 || count != 0u)
            {
                fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "non-empty VTK metadata is not supported");
                result = FVIZ_ERROR_NOT_SUPPORTED;
            }
        }
        else
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported section in VTK legacy file");
            result = FVIZ_ERROR_NOT_SUPPORTED;
        }
    }

    if (result == FVIZ_OK)
    {
        FVizSize i;
        if (cells == NULL || cell_types == NULL || cell_type_count != cell_count)
            result = fviz_vtk_format_error("VTK file must contain matching CELLS and CELL_TYPES sections");
        for (i = 0u; result == FVIZ_OK && i < cell_count; ++i)
            if (fviz_unstructured_grid_add_cell(grid, cell_types[i], cells[i].point_count, cells[i].ids) != FVIZ_OK)
                result = fviz_last_error_code();
        if (result == FVIZ_OK && fviz_unstructured_grid_validate(grid) != FVIZ_OK)
            result = fviz_last_error_code();
    }

cleanup:
    if (file != NULL) (void)fclose(file);
    fviz_vtk_free_cells(cells, cell_count);
    fviz_free(cell_types);
    if (result != FVIZ_OK)
    {
        fviz_release(grid);
        return result;
    }
    *out_grid = grid;
    return FVIZ_OK;
}
