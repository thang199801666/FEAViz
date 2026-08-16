#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataType.h>
#include <FViz/IO/FVizVTPReader.h>

#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_VTP_MAX_NAME 128u

typedef struct FVizVTPArrayBlock
{
    char name[FVIZ_VTP_MAX_NAME];
    char format[32];
    char type[32];
    uint32_t components;
    const char* content_begin;
    const char* content_end;
} FVizVTPArrayBlock;

static FVizBool fviz_vtp_attr_string(const char* tag, const char* attr, char* out, FVizSize out_size)
{
    char pattern[64];
    const char* found;
    const char* end;
    FVizSize written = 0u;
    if (tag == NULL || attr == NULL || out == NULL || out_size == 0u) return FVIZ_FALSE;
    (void)snprintf(pattern, sizeof(pattern), "%s=\"", attr);
    {
        const char* tag_end = strchr(tag, '>');
        found = strstr(tag, pattern);
        if (found == NULL || tag_end == NULL || found >= tag_end) return FVIZ_FALSE;
    }
    found += strlen(pattern);
    end = strchr(found, '"');
    if (end == NULL) return FVIZ_FALSE;
    while (found < end && written + 1u < out_size)
    {
        char decoded = *found;
        FVizSize consumed = 1u;
        if (*found == '&')
        {
            if ((FVizSize)(end - found) >= 5u && strncmp(found, "&amp;", 5u) == 0) { decoded='&'; consumed=5u; }
            else if ((FVizSize)(end - found) >= 4u && strncmp(found, "&lt;", 4u) == 0) { decoded='<'; consumed=4u; }
            else if ((FVizSize)(end - found) >= 4u && strncmp(found, "&gt;", 4u) == 0) { decoded='>'; consumed=4u; }
            else if ((FVizSize)(end - found) >= 6u && strncmp(found, "&quot;", 6u) == 0) { decoded='"'; consumed=6u; }
            else if ((FVizSize)(end - found) >= 6u && strncmp(found, "&apos;", 6u) == 0) { decoded='\''; consumed=6u; }
        }
        out[written++]=decoded;
        found += consumed;
    }
    out[written]='\0';
    return FVIZ_TRUE;
}

static FVizBool fviz_vtp_attr_size(const char* tag, const char* attr, FVizSize* out_value)
{
    char buffer[64];
    char* end = NULL;
    unsigned long long value;
    if (!fviz_vtp_attr_string(tag, attr, buffer, sizeof(buffer))) return FVIZ_FALSE;
    value = strtoull(buffer, &end, 10);
    if (end == buffer || *end != '\0' || value > (unsigned long long)((FVizSize)-1)) return FVIZ_FALSE;
    *out_value = (FVizSize)value;
    return FVIZ_TRUE;
}

static FVizDataType fviz_vtp_data_type(const char* name)
{
    if (strcmp(name, "Int8") == 0) return FVIZ_DATA_INT8;
    if (strcmp(name, "UInt8") == 0) return FVIZ_DATA_UINT8;
    if (strcmp(name, "Int16") == 0) return FVIZ_DATA_INT16;
    if (strcmp(name, "UInt16") == 0) return FVIZ_DATA_UINT16;
    if (strcmp(name, "Int32") == 0) return FVIZ_DATA_INT32;
    if (strcmp(name, "UInt32") == 0) return FVIZ_DATA_UINT32;
    if (strcmp(name, "Int64") == 0) return FVIZ_DATA_INT64;
    if (strcmp(name, "UInt64") == 0) return FVIZ_DATA_UINT64;
    if (strcmp(name, "Float32") == 0) return FVIZ_DATA_FLOAT32;
    if (strcmp(name, "Float64") == 0) return FVIZ_DATA_FLOAT64;
    return (FVizDataType)0;
}

static FVizBool fviz_vtp_find_data_array(
    const char* cursor,
    const char* end,
    const char** out_tag,
    const char** out_open_end,
    const char** out_close,
    const char** out_next)
{
    const char* tag = strstr(cursor, "<DataArray");
    const char* tag_end;
    const char* close;
    if (tag == NULL || tag >= end) return FVIZ_FALSE;
    tag_end = strchr(tag, '>');
    if (tag_end == NULL || tag_end >= end) return FVIZ_FALSE;
    if (tag_end > tag && tag_end[-1] == '/')
    {
        *out_tag = tag;
        *out_open_end = tag_end + 1;
        *out_close = tag_end + 1;
        *out_next = tag_end + 1;
        return FVIZ_TRUE;
    }
    close = strstr(tag_end + 1, "</DataArray>");
    if (close == NULL || close > end) return FVIZ_FALSE;
    *out_tag = tag;
    *out_open_end = tag_end + 1;
    *out_close = close;
    *out_next = close + strlen("</DataArray>");
    return FVIZ_TRUE;
}

static FVizResult fviz_vtp_collect_arrays(
    const char* section_begin,
    const char* section_end,
    FVizVTPArrayBlock* blocks,
    FVizSize capacity,
    FVizSize* out_count)
{
    const char* cursor = section_begin;
    FVizSize count = 0u;
    *out_count = 0u;
    while (cursor < section_end)
    {
        const char* tag;
        const char* content_begin;
        const char* content_end;
        const char* next;
        const char* tag_end;
        FVizVTPArrayBlock block;
        char component_buffer[32];
        if (!fviz_vtp_find_data_array(cursor, section_end, &tag, &content_begin, &content_end, &next)) break;
        if (count >= capacity)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "VTP section contains too many arrays");
            return FVIZ_ERROR_OVERFLOW;
        }
        (void)memset(&block, 0, sizeof(block));
        (void)strcpy(block.format, "ascii");
        (void)strcpy(block.type, "Float32");
        block.components = 1u;
        tag_end = strchr(tag, '>');
        if (tag_end == NULL || tag_end > section_end) return FVIZ_ERROR_PARSE;
        (void)fviz_vtp_attr_string(tag, "Name", block.name, sizeof(block.name));
        (void)fviz_vtp_attr_string(tag, "format", block.format, sizeof(block.format));
        (void)fviz_vtp_attr_string(tag, "type", block.type, sizeof(block.type));
        if (fviz_vtp_attr_string(tag, "NumberOfComponents", component_buffer, sizeof(component_buffer)))
        {
            const unsigned long value = strtoul(component_buffer, NULL, 10);
            if (value == 0u || value > UINT32_MAX) return FVIZ_ERROR_PARSE;
            block.components = (uint32_t)value;
        }
        if (strcmp(block.format, "ascii") != 0 && block.format[0] != '\0')
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "VTP reader currently supports ASCII DataArray only");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        block.content_begin = content_begin;
        block.content_end = content_end;
        blocks[count++] = block;
        cursor = next;
    }
    *out_count = count;
    return FVIZ_OK;
}

static FVizBool fviz_vtp_find_section(
    const char* begin,
    const char* end,
    const char* name,
    const char** out_tag,
    const char** out_content_begin,
    const char** out_content_end)
{
    char open_pattern[64];
    char close_pattern[64];
    const char* tag;
    const char* tag_end;
    const char* close;
    (void)snprintf(open_pattern, sizeof(open_pattern), "<%s", name);
    (void)snprintf(close_pattern, sizeof(close_pattern), "</%s>", name);
    tag = strstr(begin, open_pattern);
    if (tag == NULL || tag >= end) return FVIZ_FALSE;
    tag_end = strchr(tag, '>');
    if (tag_end == NULL || tag_end >= end) return FVIZ_FALSE;
    close = strstr(tag_end + 1, close_pattern);
    if (close == NULL || close > end) return FVIZ_FALSE;
    *out_tag = tag;
    *out_content_begin = tag_end + 1;
    *out_content_end = close;
    return FVIZ_TRUE;
}

static FVizResult fviz_vtp_store_numeric(
    unsigned char* destination,
    FVizDataType type,
    double signed_or_float,
    uint64_t unsigned_value,
    FVizBool unsigned_kind)
{
    switch (type)
    {
        case FVIZ_DATA_INT8: { const int8_t v=(int8_t)signed_or_float; (void)memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_UINT8: { const uint8_t v=(uint8_t)unsigned_value; (void)memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_INT16: { const int16_t v=(int16_t)signed_or_float; (void)memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_UINT16: { const uint16_t v=(uint16_t)unsigned_value; (void)memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_INT32: { const int32_t v=(int32_t)signed_or_float; (void)memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_UINT32: { const uint32_t v=(uint32_t)unsigned_value; (void)memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_INT64: { const int64_t v=(int64_t)signed_or_float; (void)memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_UINT64: { const uint64_t v=unsigned_value; (void)memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_FLOAT32: { const float v=(float)signed_or_float; (void)memcpy(destination,&v,sizeof(v)); break; }
        case FVIZ_DATA_FLOAT64: { const double v=signed_or_float; (void)memcpy(destination,&v,sizeof(v)); break; }
        default: (void)unsigned_kind; return FVIZ_ERROR_NOT_SUPPORTED;
    }
    return FVIZ_OK;
}

static FVizResult fviz_vtp_parse_attribute_arrays(
    FVizAttributeSet* destination,
    const char* section_tag,
    const char* begin,
    const char* end,
    FVizSize expected_tuples,
    FVizSize maximum_values)
{
    FVizVTPArrayBlock blocks[128];
    FVizSize block_count = 0u;
    FVizSize i;
    static const char* role_names[FVIZ_ATTRIBUTE_ROLE_COUNT] = {
        "Scalars", "Vectors", "Normals", "Tensors", "GlobalIds"};
    if (fviz_vtp_collect_arrays(begin, end, blocks, 128u, &block_count) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < block_count; ++i)
    {
        const FVizVTPArrayBlock* block = &blocks[i];
        FVizDataType type = fviz_vtp_data_type(block->type);
        FVizDataArray* array = NULL;
        const FVizSize component_size = fviz_data_type_size(type);
        const FVizSize components = block->components;
        FVizSize value_count;
        const char* cursor = block->content_begin;
        unsigned char* raw;
        FVizSize value_index = 0u;
        if (block->name[0] == '\0' || component_size == 0u ||
            components == 0u || expected_tuples > maximum_values ||
            expected_tuples > maximum_values / components ||
            expected_tuples > (FVizSize)-1 / components)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "VTP attribute dimensions exceed configured limits");
            return FVIZ_ERROR_OVERFLOW;
        }
        value_count = expected_tuples * components;
        if (fviz_data_array_create(type, block->components, &array) != FVIZ_OK ||
            fviz_data_array_resize(array, expected_tuples) != FVIZ_OK)
        {
            fviz_release(array);
            return fviz_last_error_code();
        }
        raw = (unsigned char*)fviz_data_array_data(array);
        while (cursor < block->content_end && value_index < value_count)
        {
            char* next = NULL;
            while (cursor < block->content_end && (isspace((unsigned char)*cursor) || *cursor == ',')) ++cursor;
            if (cursor >= block->content_end || *cursor == '<') break;
            if (type == FVIZ_DATA_UINT8 || type == FVIZ_DATA_UINT16 || type == FVIZ_DATA_UINT32 || type == FVIZ_DATA_UINT64)
            {
                const uint64_t value = (uint64_t)strtoull(cursor, &next, 10);
                if (next == cursor || next > block->content_end ||
                    fviz_vtp_store_numeric(raw + value_index * component_size, type, (double)value, value, FVIZ_TRUE) != FVIZ_OK)
                    break;
            }
            else
            {
                const double value = strtod(cursor, &next);
                if (next == cursor || next > block->content_end ||
                    fviz_vtp_store_numeric(raw + value_index * component_size, type, value, (uint64_t)value, FVIZ_FALSE) != FVIZ_OK)
                    break;
            }
            cursor = next;
            ++value_index;
        }
        if (value_index != value_count)
        {
            fviz_release(array);
            fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP attribute value count does not match tuple metadata");
            return FVIZ_ERROR_PARSE;
        }
        if (fviz_attribute_set_add(destination, block->name, array) != FVIZ_OK)
        {
            fviz_release(array);
            return fviz_last_error_code();
        }
        fviz_release(array);
    }
    for (i = 0u; i < FVIZ_ATTRIBUTE_ROLE_COUNT; ++i)
    {
        char active[FVIZ_VTP_MAX_NAME];
        if (fviz_vtp_attr_string(section_tag, role_names[i], active, sizeof(active)))
        {
            if (fviz_attribute_set_set_active(destination, (FVizAttributeRole)i, active) != FVIZ_OK)
                return fviz_last_error_code();
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_vtp_parse_field_arrays(
    FVizAttributeSet* destination,
    const char* begin,
    const char* end,
    FVizSize maximum_values)
{
    FVizVTPArrayBlock blocks[128];
    FVizSize block_count = 0u;
    FVizSize i;
    if (fviz_vtp_collect_arrays(begin, end, blocks, 128u, &block_count) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < block_count; ++i)
    {
        const FVizVTPArrayBlock* block = &blocks[i];
        const FVizDataType type = fviz_vtp_data_type(block->type);
        const FVizSize component_size = fviz_data_type_size(type);
        const FVizSize components = block->components;
        const char* cursor = block->content_begin;
        FVizSize value_count = 0u;
        FVizSize tuple_count;
        FVizDataArray* array = NULL;
        unsigned char* raw;
        FVizSize value_index = 0u;
        if (block->name[0] == '\0' || component_size == 0u || components == 0u)
        {
            fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP FieldData contains invalid array metadata");
            return FVIZ_ERROR_PARSE;
        }
        while (cursor < block->content_end)
        {
            char* next = NULL;
            while (cursor < block->content_end && (isspace((unsigned char)*cursor) || *cursor == ',')) ++cursor;
            if (cursor >= block->content_end || *cursor == '<') break;
            if (type == FVIZ_DATA_UINT8 || type == FVIZ_DATA_UINT16 || type == FVIZ_DATA_UINT32 || type == FVIZ_DATA_UINT64)
                (void)strtoull(cursor, &next, 10);
            else
                (void)strtod(cursor, &next);
            if (next == cursor || next > block->content_end)
            {
                fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP FieldData contains an invalid numeric token");
                return FVIZ_ERROR_PARSE;
            }
            if (value_count == maximum_values)
            {
                fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "VTP FieldData exceeds configured value limit");
                return FVIZ_ERROR_OVERFLOW;
            }
            ++value_count;
            cursor = next;
        }
        if (value_count % components != 0u)
        {
            fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP FieldData value count is not divisible by component count");
            return FVIZ_ERROR_PARSE;
        }
        tuple_count = value_count / components;
        if (fviz_data_array_create(type, block->components, &array) != FVIZ_OK ||
            fviz_data_array_resize(array, tuple_count) != FVIZ_OK)
        {
            fviz_release(array);
            return fviz_last_error_code();
        }
        raw = (unsigned char*)fviz_data_array_data(array);
        cursor = block->content_begin;
        while (cursor < block->content_end && value_index < value_count)
        {
            char* next = NULL;
            while (cursor < block->content_end && (isspace((unsigned char)*cursor) || *cursor == ',')) ++cursor;
            if (cursor >= block->content_end || *cursor == '<') break;
            if (type == FVIZ_DATA_UINT8 || type == FVIZ_DATA_UINT16 || type == FVIZ_DATA_UINT32 || type == FVIZ_DATA_UINT64)
            {
                const uint64_t value = (uint64_t)strtoull(cursor, &next, 10);
                if (next == cursor || next > block->content_end ||
                    fviz_vtp_store_numeric(raw + value_index * component_size, type, (double)value, value, FVIZ_TRUE) != FVIZ_OK)
                    break;
            }
            else
            {
                const double value = strtod(cursor, &next);
                if (next == cursor || next > block->content_end ||
                    fviz_vtp_store_numeric(raw + value_index * component_size, type, value, (uint64_t)value, FVIZ_FALSE) != FVIZ_OK)
                    break;
            }
            cursor = next;
            ++value_index;
        }
        if (value_index != value_count || fviz_attribute_set_add(destination, block->name, array) != FVIZ_OK)
        {
            fviz_release(array);
            if (value_index != value_count)
                fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP FieldData value count changed while parsing");
            return fviz_last_error_code();
        }
        fviz_release(array);
    }
    return FVIZ_OK;
}

static FVizResult fviz_vtp_parse_points(
    FVizPolyData* output,
    const char* begin,
    const char* end,
    FVizSize expected_points)
{
    FVizVTPArrayBlock blocks[4];
    FVizSize block_count = 0u;
    FVizVec3* points = NULL;
    const char* cursor;
    FVizSize value_index = 0u;
    FVizSize values;
    if (fviz_vtp_collect_arrays(begin, end, blocks, 4u, &block_count) != FVIZ_OK) return fviz_last_error_code();
    if (block_count == 0u || blocks[0].components != 3u)
    {
        fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP Points requires one 3-component DataArray");
        return FVIZ_ERROR_PARSE;
    }
    if (expected_points > (FVizSize)-1 / sizeof(*points)) return FVIZ_ERROR_OVERFLOW;
    points = (FVizVec3*)fviz_alloc(expected_points * sizeof(*points));
    if (points == NULL && expected_points != 0u) return fviz_last_error_code();
    values = expected_points * 3u;
    cursor = blocks[0].content_begin;
    while (cursor < blocks[0].content_end && value_index < values)
    {
        char* next = NULL;
        double value;
        while (cursor < blocks[0].content_end && (isspace((unsigned char)*cursor) || *cursor == ',')) ++cursor;
        if (cursor >= blocks[0].content_end || *cursor == '<') break;
        value = strtod(cursor, &next);
        if (next == cursor || next > blocks[0].content_end) break;
        ((float*)points)[value_index++] = (float)value;
        cursor = next;
    }
    if (value_index != values || fviz_poly_data_add_points_ids(output, points, expected_points, NULL) != FVIZ_OK)
    {
        fviz_free(points);
        if (value_index != values) fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP point value count mismatch");
        return fviz_last_error_code() == FVIZ_OK ? FVIZ_ERROR_PARSE : fviz_last_error_code();
    }
    fviz_free(points);
    return FVIZ_OK;
}

static FVizResult fviz_vtp_parse_cell_section(
    FVizPolyData* output,
    const char* begin,
    const char* end,
    FVizCellType single_type,
    FVizCellType multiple_type,
    FVizSize expected_cells,
    FVizSize max_connectivity)
{
    FVizVTPArrayBlock blocks[8];
    FVizSize block_count = 0u;
    const FVizVTPArrayBlock* connectivity = NULL;
    const FVizVTPArrayBlock* offsets = NULL;
    FVizId* ids = NULL;
    FVizSize* offs = NULL;
    FVizSize conn_count = 0u;
    FVizSize offset_count = 0u;
    FVizSize i;
    FVizResult result = FVIZ_OK;
    if (expected_cells == 0u) return FVIZ_OK;
    if (fviz_vtp_collect_arrays(begin, end, blocks, 8u, &block_count) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < block_count; ++i)
    {
        if (strcmp(blocks[i].name, "connectivity") == 0) connectivity = &blocks[i];
        else if (strcmp(blocks[i].name, "offsets") == 0) offsets = &blocks[i];
    }
    if (connectivity == NULL || offsets == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP cell section requires connectivity and offsets arrays");
        return FVIZ_ERROR_PARSE;
    }
    {
        const char* cursor = connectivity->content_begin;
        FVizSize estimate = (FVizSize)(connectivity->content_end - connectivity->content_begin) / 2u + 1u;
        if (estimate > max_connectivity) estimate = max_connectivity;
        ids = (FVizId*)fviz_alloc(estimate * sizeof(*ids));
        if (ids == NULL && estimate != 0u) return fviz_last_error_code();
        while (cursor < connectivity->content_end && conn_count < estimate)
        {
            char* next = NULL;
            unsigned long long value;
            while (cursor < connectivity->content_end && (isspace((unsigned char)*cursor) || *cursor == ',')) ++cursor;
            if (cursor >= connectivity->content_end || *cursor == '<') break;
            value = strtoull(cursor, &next, 10);
            if (next == cursor || next > connectivity->content_end) break;
            ids[conn_count++] = (FVizId)value;
            cursor = next;
        }
        while (cursor < connectivity->content_end && isspace((unsigned char)*cursor)) ++cursor;
        if (cursor < connectivity->content_end)
        {
            result = FVIZ_ERROR_OVERFLOW;
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "VTP connectivity exceeds configured limit");
            goto cleanup;
        }
    }
    offs = (FVizSize*)fviz_alloc(expected_cells * sizeof(*offs));
    if (offs == NULL && expected_cells != 0u) { result = fviz_last_error_code(); goto cleanup; }
    {
        const char* cursor = offsets->content_begin;
        while (cursor < offsets->content_end && offset_count < expected_cells)
        {
            char* next = NULL;
            unsigned long long value;
            while (cursor < offsets->content_end && (isspace((unsigned char)*cursor) || *cursor == ',')) ++cursor;
            if (cursor >= offsets->content_end || *cursor == '<') break;
            value = strtoull(cursor, &next, 10);
            if (next == cursor || next > offsets->content_end || value > (unsigned long long)((FVizSize)-1)) break;
            offs[offset_count++] = (FVizSize)value;
            cursor = next;
        }
    }
    if (offset_count != expected_cells)
    {
        result = FVIZ_ERROR_PARSE;
        fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP offsets count does not match Piece metadata");
        goto cleanup;
    }
    {
        FVizSize start = 0u;
        for (i = 0u; i < expected_cells; ++i)
        {
            const FVizSize stop = offs[i];
            FVizSize n;
            FVizCellType type;
            if (stop < start || stop > conn_count)
            {
                result = FVIZ_ERROR_PARSE;
                fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP cell offset is outside connectivity");
                goto cleanup;
            }
            n = stop - start;
            type = n == 1u ? single_type : multiple_type;
            if (single_type == FVIZ_CELL_LINE)
                type = n == 2u ? FVIZ_CELL_LINE : FVIZ_CELL_POLY_LINE;
            else if (single_type == FVIZ_CELL_TRIANGLE)
                type = n == 3u ? FVIZ_CELL_TRIANGLE : n == 4u ? FVIZ_CELL_QUAD : FVIZ_CELL_POLYGON;
            else if (single_type == FVIZ_CELL_TRIANGLE_STRIP)
                type = FVIZ_CELL_TRIANGLE_STRIP;
            if (n == 0u || fviz_poly_data_add_cell_ids(output, type, n, ids + start) != FVIZ_OK)
            {
                if (n == 0u) fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP cell contains no points");
                result = fviz_last_error_code() == FVIZ_OK ? FVIZ_ERROR_PARSE : fviz_last_error_code();
                goto cleanup;
            }
            start = stop;
        }
        if (start != conn_count)
        {
            result = FVIZ_ERROR_PARSE;
            fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP connectivity has values not referenced by offsets");
            goto cleanup;
        }
    }
cleanup:
    fviz_free(ids);
    fviz_free(offs);
    return result;
}

void fviz_vtp_reader_options_initialize(FVizVTPReaderOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->maximum_file_bytes = 1024u * 1024u * 1024u;
    options->maximum_points = 100000000u;
    options->maximum_cells = 100000000u;
    options->maximum_connectivity_values = 1000000000u;
    options->maximum_array_values = 1000000000u;
}

FVizResult fviz_vtp_read(const char* file_path, FVizPolyData** out_poly_data)
{
    FVizVTPReaderOptions options;
    fviz_vtp_reader_options_initialize(&options);
    return fviz_vtp_read_with_options(file_path, &options, out_poly_data);
}

FVizResult fviz_vtp_read_with_options(
    const char* file_path,
    const FVizVTPReaderOptions* options,
    FVizPolyData** out_poly_data)
{
    FILE* file = NULL;
    long file_size_long;
    FVizSize file_size;
    char* text = NULL;
    const char* end;
    const char* poly_data;
    const char* poly_data_tag_end;
    const char* poly_data_end;
    const char* piece;
    const char* piece_tag_end;
    const char* piece_end;
    FVizSize point_count = 0u, vert_count = 0u, line_count = 0u, strip_count = 0u, poly_count = 0u;
    FVizPolyData* output = NULL;
    FVizResult result = FVIZ_OK;
    struct Section { const char* tag; const char* begin; const char* end; } points={0}, point_data={0}, cell_data={0}, field_data={0}, verts={0}, lines={0}, strips={0}, polys={0};
    if (out_poly_data != NULL) *out_poly_data = NULL;
    if (file_path == NULL || options == NULL || out_poly_data == NULL || options->struct_size < sizeof(*options))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "VTP reader arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    file = fopen(file_path, "rb");
    if (file == NULL) { fviz_internal_set_error(FVIZ_ERROR_IO, "failed to open VTP file"); return FVIZ_ERROR_IO; }
    if (fseek(file, 0, SEEK_END) != 0 || (file_size_long = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0)
    { result = FVIZ_ERROR_IO; goto cleanup; }
    file_size = (FVizSize)file_size_long;
    if (file_size > options->maximum_file_bytes || file_size > (FVizSize)-1 - 1u)
    { result = FVIZ_ERROR_OVERFLOW; fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "VTP file exceeds configured size limit"); goto cleanup; }
    text = (char*)fviz_alloc(file_size + 1u);
    if (text == NULL) { result = fviz_last_error_code(); goto cleanup; }
    if (file_size != 0u && fread(text, 1u, file_size, file) != file_size)
    { result = FVIZ_ERROR_IO; goto cleanup; }
    text[file_size] = '\0';
    end = text + file_size;
    if (strstr(text, "<VTKFile") == NULL || strstr(text, "type=\"PolyData\"") == NULL)
    { result = FVIZ_ERROR_PARSE; fviz_internal_set_error(FVIZ_ERROR_PARSE, "file is not VTK XML PolyData"); goto cleanup; }
    poly_data = strstr(text, "<PolyData");
    if (poly_data == NULL || poly_data >= end || (poly_data_tag_end = strchr(poly_data, '>')) == NULL ||
        (poly_data_end = strstr(poly_data_tag_end, "</PolyData>")) == NULL)
    { result = FVIZ_ERROR_PARSE; goto cleanup; }
    piece = strstr(poly_data_tag_end, "<Piece");
    if (piece == NULL || piece >= poly_data_end || (piece_tag_end = strchr(piece, '>')) == NULL ||
        piece_tag_end >= poly_data_end || (piece_end = strstr(piece_tag_end, "</Piece>")) == NULL || piece_end >= poly_data_end)
    { result = FVIZ_ERROR_PARSE; goto cleanup; }
    {
        const char* next_piece = strstr(piece_end + sizeof("</Piece>") - 1u, "<Piece");
        if (next_piece != NULL && next_piece < poly_data_end)
        {
            result = FVIZ_ERROR_NOT_SUPPORTED;
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "VTP reader currently supports exactly one Piece");
            goto cleanup;
        }
    }
    if (!fviz_vtp_attr_size(piece, "NumberOfPoints", &point_count)) point_count = 0u;
    (void)fviz_vtp_attr_size(piece, "NumberOfVerts", &vert_count);
    (void)fviz_vtp_attr_size(piece, "NumberOfLines", &line_count);
    (void)fviz_vtp_attr_size(piece, "NumberOfStrips", &strip_count);
    (void)fviz_vtp_attr_size(piece, "NumberOfPolys", &poly_count);
    if (point_count > options->maximum_points || vert_count > options->maximum_cells ||
        line_count > options->maximum_cells || strip_count > options->maximum_cells || poly_count > options->maximum_cells ||
        vert_count > options->maximum_cells - (line_count <= options->maximum_cells ? line_count : options->maximum_cells) ||
        vert_count + line_count > options->maximum_cells - (strip_count <= options->maximum_cells ? strip_count : options->maximum_cells) ||
        vert_count + line_count + strip_count > options->maximum_cells - (poly_count <= options->maximum_cells ? poly_count : options->maximum_cells))
    { result = FVIZ_ERROR_OVERFLOW; fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "VTP Piece exceeds configured topology limits"); goto cleanup; }
#define FIND_SECTION(var,name) (void)fviz_vtp_find_section(piece_tag_end, piece_end, name, &var.tag, &var.begin, &var.end)
    FIND_SECTION(points,"Points"); FIND_SECTION(point_data,"PointData"); FIND_SECTION(cell_data,"CellData");
    (void)fviz_vtp_find_section(poly_data_tag_end, piece, "FieldData", &field_data.tag, &field_data.begin, &field_data.end);
    FIND_SECTION(verts,"Verts"); FIND_SECTION(lines,"Lines"); FIND_SECTION(strips,"Strips"); FIND_SECTION(polys,"Polys");
#undef FIND_SECTION
    if (points.begin == NULL) { result = FVIZ_ERROR_PARSE; fviz_internal_set_error(FVIZ_ERROR_PARSE, "VTP Piece has no Points section"); goto cleanup; }
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_reserve(output, point_count, poly_count + strip_count) != FVIZ_OK ||
        fviz_vtp_parse_points(output, points.begin, points.end, point_count) != FVIZ_OK)
    { result = fviz_last_error_code(); goto cleanup; }
    if (vert_count > 0u && (verts.begin == NULL || fviz_vtp_parse_cell_section(output, verts.begin, verts.end, FVIZ_CELL_VERTEX, FVIZ_CELL_POLY_VERTEX, vert_count, options->maximum_connectivity_values) != FVIZ_OK)) { result=fviz_last_error_code(); goto cleanup; }
    if (line_count > 0u && (lines.begin == NULL || fviz_vtp_parse_cell_section(output, lines.begin, lines.end, FVIZ_CELL_LINE, FVIZ_CELL_POLY_LINE, line_count, options->maximum_connectivity_values) != FVIZ_OK)) { result=fviz_last_error_code(); goto cleanup; }
    /* Preserve FEAViz logical cell ordering: verts, lines, polys, strips. */
    if (poly_count > 0u && (polys.begin == NULL || fviz_vtp_parse_cell_section(output, polys.begin, polys.end, FVIZ_CELL_TRIANGLE, FVIZ_CELL_POLYGON, poly_count, options->maximum_connectivity_values) != FVIZ_OK)) { result=fviz_last_error_code(); goto cleanup; }
    if (strip_count > 0u && (strips.begin == NULL || fviz_vtp_parse_cell_section(output, strips.begin, strips.end, FVIZ_CELL_TRIANGLE_STRIP, FVIZ_CELL_TRIANGLE_STRIP, strip_count, options->maximum_connectivity_values) != FVIZ_OK)) { result=fviz_last_error_code(); goto cleanup; }
    if (point_data.begin != NULL && fviz_vtp_parse_attribute_arrays(fviz_poly_data_point_data(output), point_data.tag, point_data.begin, point_data.end, point_count, options->maximum_array_values) != FVIZ_OK) { result=fviz_last_error_code(); goto cleanup; }
    if (cell_data.begin != NULL && fviz_vtp_parse_attribute_arrays(fviz_poly_data_cell_data(output), cell_data.tag, cell_data.begin, cell_data.end, fviz_poly_data_cell_count(output), options->maximum_array_values) != FVIZ_OK) { result=fviz_last_error_code(); goto cleanup; }
    if (field_data.begin != NULL &&
        fviz_vtp_parse_field_arrays(fviz_poly_data_field_data(output), field_data.begin, field_data.end, options->maximum_array_values) != FVIZ_OK)
    { result=fviz_last_error_code(); goto cleanup; }
    if (fviz_poly_data_validate(output) != FVIZ_OK) { result=fviz_last_error_code(); goto cleanup; }
    *out_poly_data = output;
    output = NULL;
cleanup:
    if (file != NULL) (void)fclose(file);
    fviz_free(text);
    fviz_release(output);
    if (result == FVIZ_ERROR_IO && fviz_last_error_code() == FVIZ_OK)
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed while reading VTP file");
    return result;
}
