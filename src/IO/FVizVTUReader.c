#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataType.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>
#include <FViz/IO/FVizVTUReader.h>

#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_VTU_MAX_NAME 128u

typedef struct FVizDataArrayBlock
{
    char name[FVIZ_VTU_MAX_NAME];
    char format[32];
    uint32_t components;
    const char* content_begin;
    const char* content_end;
} FVizDataArrayBlock;

static FVizCellType fviz_vtu_cell_type(int type)
{
    switch (type)
    {
        case 5: return FVIZ_CELL_TRIANGLE;
        case 9: return FVIZ_CELL_QUAD;
        case 10: return FVIZ_CELL_TETRA;
        case 12: return FVIZ_CELL_HEXAHEDRON;
        case 13: return FVIZ_CELL_WEDGE;
        case 14: return FVIZ_CELL_PYRAMID;
        default: return (FVizCellType)0;
    }
}

static FVizBool fviz_attr_string(const char* tag, const char* attr, char* out, FVizSize out_size)
{
    const char* start = tag;
    char pattern[64];
    const char* found;
    FVizSize length;
    (void)snprintf(pattern, sizeof(pattern), "%s=\"", attr);
    found = strstr(start, pattern);
    if (found == NULL) return FVIZ_FALSE;
    found += strlen(pattern);
    {
        const char* end = strchr(found, '"');
        if (end == NULL) return FVIZ_FALSE;
        length = (FVizSize)(end - found);
        if (length >= out_size) length = out_size - 1u;
        (void)memcpy(out, found, length);
        out[length] = '\0';
    }
    return FVIZ_TRUE;
}

static FVizBool fviz_attr_long(const char* tag, const char* attr, long* out_value)
{
    char buffer[64];
    char* end = NULL;
    if (!fviz_attr_string(tag, attr, buffer, sizeof(buffer))) return FVIZ_FALSE;
    *out_value = strtol(buffer, &end, 10);
    return end != buffer ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_find_data_array(const char* text, const char* end, const char** out_open_end, const char** out_close)
{
    const char* open = strstr(text, "<DataArray");
    const char* close;
    const char* tag_end;
    if (open == NULL || open >= end) return FVIZ_FALSE;
    tag_end = strchr(open, '>');
    if (tag_end == NULL || tag_end >= end) return FVIZ_FALSE;
    if (tag_end > open && tag_end[-1] == '/')
    {
        *out_open_end = tag_end + 1;
        *out_close = tag_end + 1;
        return FVIZ_TRUE;
    }
    close = strstr(tag_end, "</DataArray>");
    if (close == NULL || close >= end) return FVIZ_FALSE;
    *out_open_end = tag_end + 1;
    *out_close = close;
    return FVIZ_TRUE;
}

static FVizSize fviz_parse_floats(const char* begin, const char* end, float* values, FVizSize max_count)
{
    const char* cursor = begin;
    FVizSize count = 0u;
    while (cursor < end && count < max_count)
    {
        char* next = NULL;
        double value;
        while (cursor < end && (isspace((unsigned char)*cursor) || *cursor == ',')) ++cursor;
        if (cursor >= end) break;
        value = strtod(cursor, &next);
        if (next == cursor) break;
        values[count++] = (float)value;
        cursor = next;
    }
    return count;
}

static FVizSize fviz_parse_ints(const char* begin, const char* end, int32_t* values, FVizSize max_count)
{
    const char* cursor = begin;
    FVizSize count = 0u;
    while (cursor < end && count < max_count)
    {
        char* next = NULL;
        long value;
        while (cursor < end && (isspace((unsigned char)*cursor) || *cursor == ',')) ++cursor;
        if (cursor >= end) break;
        value = strtol(cursor, &next, 10);
        if (next == cursor) break;
        values[count++] = (int32_t)value;
        cursor = next;
    }
    return count;
}

static FVizResult fviz_vtu_parse_points(
    FVizUnstructuredGrid* grid,
    const FVizDataArrayBlock* block,
    const char* text_end)
{
    float* values;
    FVizSize count;
    FVizSize i;
    const char* begin = block->content_begin;
    const char* end = block->content_end < text_end ? block->content_end : text_end;
    (void)grid;
    if (block->components != 3u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "VTU points must have three components");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    values = (float*)fviz_alloc(3u * 4096u * sizeof(float));
    if (values == NULL) return fviz_last_error_code();
    count = fviz_parse_floats(begin, end, values, 3u * 4096u);
    for (i = 0u; i + 2u < count; i += 3u)
    {
        if (fviz_unstructured_grid_add_point(grid,
                fviz_vec3(values[i], values[i + 1u], values[i + 2u]), NULL) != FVIZ_OK)
        {
            fviz_free(values);
            return fviz_last_error_code();
        }
    }
    fviz_free(values);
    return FVIZ_OK;
}

static FVizResult fviz_vtu_parse_cells(
    FVizUnstructuredGrid* grid,
    const FVizDataArrayBlock* blocks,
    FVizSize block_count,
    const char* text_end)
{
    const FVizDataArrayBlock* connectivity = NULL;
    const FVizDataArrayBlock* offsets = NULL;
    const FVizDataArrayBlock* types = NULL;
    int32_t* conn = NULL;
    int32_t* off = NULL;
    int32_t* typ = NULL;
    FVizSize conn_count = 0u;
    FVizSize off_count = 0u;
    FVizSize typ_count = 0u;
    FVizSize i;
    FVizResult result = FVIZ_OK;
    FVizSize j;

    for (i = 0u; i < block_count; ++i)
    {
        if (strcmp(blocks[i].name, "connectivity") == 0) connectivity = &blocks[i];
        else if (strcmp(blocks[i].name, "offsets") == 0) offsets = &blocks[i];
        else if (strcmp(blocks[i].name, "types") == 0) types = &blocks[i];
    }
    if (connectivity == NULL || offsets == NULL || types == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "VTU Cells requires connectivity, offsets and types arrays");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }

    conn_count = 0u;
    off_count = 0u;
    typ_count = 0u;
    for (i = 0u; i < 3u; ++i)
    {
        const FVizDataArrayBlock* block = i == 0u ? connectivity : i == 1u ? offsets : types;
        FVizSize estimated = (FVizSize)(block->content_end - block->content_begin) / 2u + 8u;
        int32_t* buffer;
        if (i == 0u) buffer = (int32_t*)fviz_alloc(estimated * sizeof(int32_t));
        else if (i == 1u) buffer = (int32_t*)fviz_alloc(estimated * sizeof(int32_t));
        else buffer = (int32_t*)fviz_alloc(estimated * sizeof(int32_t));
        if (buffer == NULL)
        {
            result = fviz_last_error_code();
            goto cleanup;
        }
        if (i == 0u)
        {
            conn = buffer;
            conn_count = fviz_parse_ints(block->content_begin, block->content_end < text_end ? block->content_end : text_end, conn, estimated);
        }
        else if (i == 1u)
        {
            off = buffer;
            off_count = fviz_parse_ints(block->content_begin, block->content_end < text_end ? block->content_end : text_end, off, estimated);
        }
        else
        {
            typ = buffer;
            typ_count = fviz_parse_ints(block->content_begin, block->content_end < text_end ? block->content_end : text_end, typ, estimated);
        }
    }
    if (off_count != typ_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "VTU cell offsets and types count mismatch");
        result = FVIZ_ERROR_INVALID_STATE;
        goto cleanup;
    }

    j = 0u;
    for (i = 0u; i < off_count; ++i)
    {
        const int32_t end_offset = off[i];
        const int32_t begin_offset = i == 0u ? 0 : off[i - 1u];
        const FVizSize count = (FVizSize)(end_offset - begin_offset);
        FVizCellType type = fviz_vtu_cell_type(typ[i]);
        uint32_t* ids;
        FVizSize k;
        if (type == (FVizCellType)0)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported VTK cell type in VTU");
            result = FVIZ_ERROR_NOT_SUPPORTED;
            goto cleanup;
        }
        if (begin_offset < 0 || end_offset < begin_offset || (FVizSize)end_offset > conn_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "VTU cell offsets are out of range");
            result = FVIZ_ERROR_INVALID_STATE;
            goto cleanup;
        }
        ids = (uint32_t*)fviz_alloc(count * sizeof(uint32_t));
        if (ids == NULL)
        {
            result = fviz_last_error_code();
            goto cleanup;
        }
        for (k = 0u; k < count; ++k) ids[k] = (uint32_t)conn[j + k];
        if (fviz_unstructured_grid_add_cell(grid, type, count, ids) != FVIZ_OK)
        {
            fviz_free(ids);
            result = fviz_last_error_code();
            goto cleanup;
        }
        fviz_free(ids);
        j += count;
    }

cleanup:
    fviz_free(conn);
    fviz_free(off);
    fviz_free(typ);
    return result;
}

static FVizResult fviz_vtu_parse_data_arrays(
    FVizAttributeSet* destination,
    const FVizDataArrayBlock* blocks,
    FVizSize block_count,
    const char* text_end)
{
    FVizSize i;
    for (i = 0u; i < block_count; ++i)
    {
        const FVizDataArrayBlock* block = &blocks[i];
        float* values;
        FVizSize count;
        FVizDataArray* array = NULL;
        FVizSize j;
        const char* begin = block->content_begin;
        const char* end = block->content_end < text_end ? block->content_end : text_end;
        FVizSize component_count = block->components == 0u ? 1u : block->components;
        values = (float*)fviz_alloc(component_count * 65536u * sizeof(float));
        if (values == NULL) return fviz_last_error_code();
        count = fviz_parse_floats(begin, end, values, component_count * 65536u);
        if (count == 0u || count % component_count != 0u)
        {
            fviz_free(values);
            continue;
        }
        if (fviz_data_array_create(FVIZ_DATA_FLOAT32, (uint32_t)component_count, &array) != FVIZ_OK)
        {
            fviz_free(values);
            return fviz_last_error_code();
        }
        for (j = 0u; j < count; j += component_count)
        {
            if (fviz_data_array_append_tuple(array, &values[j]) != FVIZ_OK)
            {
                fviz_release(array);
                fviz_free(values);
                return fviz_last_error_code();
            }
        }
        fviz_free(values);
        if (fviz_attribute_set_add(destination, block->name, array) != FVIZ_OK)
        {
            fviz_release(array);
            return fviz_last_error_code();
        }
        fviz_release(array);
    }
    return FVIZ_OK;
}

FVizResult fviz_vtu_read(const char* file_path, FVizUnstructuredGrid** out_grid)
{
    FILE* file = NULL;
    char* text = NULL;
    long file_size;
    FVizUnstructuredGrid* grid = NULL;
    FVizDataArrayBlock points_block;
    FVizDataArrayBlock cell_blocks[8];
    FVizDataArrayBlock point_data_blocks[32];
    FVizDataArrayBlock cell_data_blocks[32];
    FVizSize cell_block_count = 0u;
    FVizSize point_data_count = 0u;
    FVizSize cell_data_count = 0u;
    FVizBool found_points = FVIZ_FALSE;
    const char* cursor;
    FVizResult result;

    if (out_grid == NULL || file_path == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "file path and output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    file = fopen(file_path, "rb");
    if (file == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to open VTU file");
        return FVIZ_ERROR_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (file_size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to size VTU file");
        return FVIZ_ERROR_IO;
    }
    text = (char*)fviz_alloc((FVizSize)file_size + 1u);
    if (text == NULL)
    {
        fclose(file);
        return fviz_last_error_code();
    }
    if (fread(text, 1u, (FVizSize)file_size, file) != (FVizSize)file_size)
    {
        fclose(file);
        fviz_free(text);
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to read VTU file");
        return FVIZ_ERROR_IO;
    }
    fclose(file);
    text[file_size] = '\0';
    cursor = text;
    (void)memset(&points_block, 0, sizeof(points_block));

    while (cursor < text + file_size)
    {
        const char* open_end;
        const char* close;
        char tag[512];
        FVizSize tag_length;
        const char* open = strstr(cursor, "<DataArray");
        const char* section = cursor;
        int section_kind = 0;
        if (open == NULL || open >= text + file_size) break;
        {
            const char* point_data_tag = strstr(cursor, "<PointData");
            const char* cell_data_tag = strstr(cursor, "<CellData");
            const char* next_section = NULL;
            if (point_data_tag != NULL && point_data_tag < open && point_data_tag < text + file_size)
            {
                next_section = point_data_tag;
                section_kind = 1;
            }
            if (cell_data_tag != NULL && cell_data_tag < open && cell_data_tag < text + file_size &&
                (next_section == NULL || cell_data_tag < next_section))
            {
                next_section = cell_data_tag;
                section_kind = 2;
            }
            if (next_section != NULL) section = next_section;
        }
        if (!fviz_find_data_array(open, text + file_size, &open_end, &close)) break;
        tag_length = (FVizSize)(open_end - open);
        if (tag_length >= sizeof(tag)) tag_length = sizeof(tag) - 1u;
        (void)memcpy(tag, open, tag_length);
        tag[tag_length] = '\0';

        {
            FVizDataArrayBlock block;
            long components = 1;
            (void)memset(&block, 0, sizeof(block));
            if (!fviz_attr_string(tag, "Name", block.name, sizeof(block.name)))
            {
                cursor = close + strlen("</DataArray>");
                continue;
            }
            if (!fviz_attr_string(tag, "format", block.format, sizeof(block.format)))
            {
                (void)strcpy(block.format, "ascii");
            }
            (void)fviz_attr_long(tag, "NumberOfComponents", &components);
            block.components = components > 0 ? (uint32_t)components : 1u;
            block.content_begin = open_end;
            block.content_end = close;

            if (strcmp(block.format, "ascii") != 0 && strcmp(block.format, "") != 0)
            {
                cursor = close + strlen("</DataArray>");
                continue;
            }

            if (strcmp(block.name, "Points") == 0)
            {
                points_block = block;
                found_points = FVIZ_TRUE;
            }
            else if (strcmp(block.name, "connectivity") == 0 ||
                     strcmp(block.name, "offsets") == 0 ||
                     strcmp(block.name, "types") == 0)
            {
                if (cell_block_count < 8u) cell_blocks[cell_block_count++] = block;
            }
            else if (section_kind == 2 && cell_data_count < 32u)
            {
                cell_data_blocks[cell_data_count++] = block;
            }
            else if (point_data_count < 32u)
            {
                point_data_blocks[point_data_count++] = block;
            }
            else if (cell_data_count < 32u)
            {
                cell_data_blocks[cell_data_count++] = block;
            }
        }
        cursor = close + strlen("</DataArray>");
    }

    if (!found_points)
    {
        fviz_free(text);
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "VTU file has no Points array");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }

    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK)
    {
        fviz_free(text);
        return fviz_last_error_code();
    }
    result = fviz_vtu_parse_points(grid, &points_block, text + file_size);
    if (result == FVIZ_OK)
    {
        result = fviz_vtu_parse_cells(grid, cell_blocks, cell_block_count, text + file_size);
    }
    if (result == FVIZ_OK)
    {
        result = fviz_vtu_parse_data_arrays(
            fviz_unstructured_grid_point_data(grid),
            point_data_blocks, point_data_count, text + file_size);
    }
    if (result == FVIZ_OK)
    {
        result = fviz_vtu_parse_data_arrays(
            fviz_unstructured_grid_cell_data(grid),
            cell_data_blocks, cell_data_count, text + file_size);
    }
    fviz_free(text);
    if (result != FVIZ_OK)
    {
        fviz_release(grid);
        return result;
    }
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK)
    {
        fviz_release(grid);
        return fviz_last_error_code();
    }
    *out_grid = grid;
    return FVIZ_OK;
}
