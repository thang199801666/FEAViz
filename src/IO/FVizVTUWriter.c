#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/IO/FVizVTUWriter.h>

#include <FViz/Core/FVizErrorInternal.h>

static const char* fviz_vtu_writer_type_name(FVizDataType type)
{
    switch (type)
    {
        case FVIZ_DATA_INT8: return "Int8";
        case FVIZ_DATA_UINT8: return "UInt8";
        case FVIZ_DATA_INT16: return "Int16";
        case FVIZ_DATA_UINT16: return "UInt16";
        case FVIZ_DATA_INT32: return "Int32";
        case FVIZ_DATA_UINT32: return "UInt32";
        case FVIZ_DATA_INT64: return "Int64";
        case FVIZ_DATA_UINT64: return "UInt64";
        case FVIZ_DATA_FLOAT32: return "Float32";
        case FVIZ_DATA_FLOAT64: return "Float64";
        default: return NULL;
    }
}

static FVizBool fviz_vtu_write_xml_escaped(FILE* file, const char* value)
{
    const unsigned char* cursor = (const unsigned char*)value;
    while (*cursor != 0u)
    {
        const char* replacement = NULL;
        if (*cursor == '&') replacement = "&amp;";
        else if (*cursor == '<') replacement = "&lt;";
        else if (*cursor == '>') replacement = "&gt;";
        else if (*cursor == '"') replacement = "&quot;";
        else if (*cursor == '\'') replacement = "&apos;";
        if (replacement != NULL)
        {
            if (fputs(replacement, file) == EOF) return FVIZ_FALSE;
        }
        else if (fputc(*cursor, file) == EOF)
            return FVIZ_FALSE;
        ++cursor;
    }
    return FVIZ_TRUE;
}

static void fviz_vtu_write_ascii_scalar(FILE* file, const void* tuple, FVizDataType type, uint32_t c)
{
    switch (type)
    {
        case FVIZ_DATA_INT8: (void)fprintf(file, "%" PRId8, ((const int8_t*)tuple)[c]); break;
        case FVIZ_DATA_UINT8: (void)fprintf(file, "%" PRIu8, ((const uint8_t*)tuple)[c]); break;
        case FVIZ_DATA_INT16: (void)fprintf(file, "%" PRId16, ((const int16_t*)tuple)[c]); break;
        case FVIZ_DATA_UINT16: (void)fprintf(file, "%" PRIu16, ((const uint16_t*)tuple)[c]); break;
        case FVIZ_DATA_INT32: (void)fprintf(file, "%" PRId32, ((const int32_t*)tuple)[c]); break;
        case FVIZ_DATA_UINT32: (void)fprintf(file, "%" PRIu32, ((const uint32_t*)tuple)[c]); break;
        case FVIZ_DATA_INT64: (void)fprintf(file, "%" PRId64, ((const int64_t*)tuple)[c]); break;
        case FVIZ_DATA_UINT64: (void)fprintf(file, "%" PRIu64, ((const uint64_t*)tuple)[c]); break;
        case FVIZ_DATA_FLOAT32: (void)fprintf(file, "%.9g", (double)((const float*)tuple)[c]); break;
        case FVIZ_DATA_FLOAT64: (void)fprintf(file, "%.17g", ((const double*)tuple)[c]); break;
        default: break;
    }
}

static FVizBool fviz_vtu_write_ascii_array(
    FILE* file,
    const char* indentation,
    const char* name,
    const FVizDataArray* array)
{
    const char* type_name = fviz_vtu_writer_type_name(fviz_data_array_type(array));
    const uint32_t components = fviz_data_array_components(array);
    FVizSize tuple_index;
    if (type_name == NULL) return FVIZ_FALSE;
    if (fprintf(file, "%s<DataArray type=\"%s\" Name=\"", indentation, type_name) < 0 ||
        fviz_vtu_write_xml_escaped(file, name) == FVIZ_FALSE ||
        fprintf(file, "\" NumberOfComponents=\"%u\" format=\"ascii\">\n%s  ",
            components, indentation) < 0)
        return FVIZ_FALSE;
    for (tuple_index = 0u; tuple_index < fviz_data_array_tuple_count(array); ++tuple_index)
    {
        const void* tuple = fviz_data_array_const_tuple(array, tuple_index);
        uint32_t component;
        for (component = 0u; component < components; ++component)
        {
            fviz_vtu_write_ascii_scalar(file, tuple, fviz_data_array_type(array), component);
            if (fputc(' ', file) == EOF) return FVIZ_FALSE;
        }
    }
    return fprintf(file, "\n%s</DataArray>\n", indentation) >= 0 ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_vtu_write_appended_tag(
    FILE* file,
    const char* indentation,
    const char* type_name,
    const char* name,
    uint32_t components,
    uint64_t* offset,
    uint64_t bytes,
    FVizVTUHeaderWidth header_width)
{
    if (fprintf(file, "%s<DataArray type=\"%s\" Name=\"", indentation, type_name) < 0 ||
        fviz_vtu_write_xml_escaped(file, name) == FVIZ_FALSE ||
        fprintf(file, "\" NumberOfComponents=\"%u\" format=\"appended\" offset=\"%" PRIu64 "\"/>\n",
            components, *offset) < 0)
        return FVIZ_FALSE;
    *offset += (uint64_t)header_width + bytes;
    return FVIZ_TRUE;
}

static FVizBool fviz_vtu_write_attributes(
    FILE* file,
    const char* section_name,
    const FVizAttributeSet* attributes,
    const FVizVTUWriterOptions* options,
    uint64_t* offset)
{
    static const char* role_attributes[FVIZ_ATTRIBUTE_ROLE_COUNT] = {
        "Scalars", "Vectors", "Normals", "Tensors", "GlobalIds"};
    FVizSize i;
    if (fprintf(file, "      <%s", section_name) < 0) return FVIZ_FALSE;
    for (i = 0u; i < FVIZ_ATTRIBUTE_ROLE_COUNT; ++i)
    {
        const char* active_name = fviz_attribute_set_active_name(
            attributes, (FVizAttributeRole)i);
        if (active_name != NULL)
        {
            if (fprintf(file, " %s=\"", role_attributes[i]) < 0 ||
                fviz_vtu_write_xml_escaped(file, active_name) == FVIZ_FALSE ||
                fputc('"', file) == EOF)
                return FVIZ_FALSE;
        }
    }
    if (fputs(">\n", file) == EOF) return FVIZ_FALSE;
    for (i = 0u; i < fviz_attribute_set_count(attributes); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(attributes, i);
        const char* name = fviz_attribute_set_name_at(attributes, i);
        if (options->output_mode == FVIZ_VTU_OUTPUT_ASCII)
        {
            if (fviz_vtu_write_ascii_array(file, "        ", name, array) == FVIZ_FALSE)
                return FVIZ_FALSE;
        }
        else
        {
            uint64_t bytes = (uint64_t)fviz_data_array_tuple_count(array) *
                (uint64_t)fviz_data_array_tuple_stride(array);
            if (fviz_vtu_write_appended_tag(file, "        ",
                    fviz_vtu_writer_type_name(fviz_data_array_type(array)), name,
                    fviz_data_array_components(array), offset, bytes,
                    options->header_width) == FVIZ_FALSE)
                return FVIZ_FALSE;
        }
    }
    return fprintf(file, "      </%s>\n", section_name) >= 0 ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_vtu_write_header_value(
    FILE* file,
    FVizVTUHeaderWidth width,
    uint64_t value)
{
    if (width == FVIZ_VTU_HEADER_UINT32)
    {
        const uint32_t narrowed = (uint32_t)value;
        if (value > UINT32_MAX) return FVIZ_FALSE;
        return fwrite(&narrowed, sizeof(narrowed), 1u, file) == 1u ? FVIZ_TRUE : FVIZ_FALSE;
    }
    return fwrite(&value, sizeof(value), 1u, file) == 1u ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_vtu_write_raw_block(
    FILE* file,
    FVizVTUHeaderWidth width,
    const void* data,
    uint64_t bytes)
{
    if (fviz_vtu_write_header_value(file, width, bytes) == FVIZ_FALSE) return FVIZ_FALSE;
    return bytes == 0u || fwrite(data, 1u, (size_t)bytes, file) == (size_t)bytes
        ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_vtu_write_appended_attributes(
    FILE* file,
    const FVizAttributeSet* attributes,
    FVizVTUHeaderWidth width)
{
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(attributes); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(attributes, i);
        const uint64_t bytes = (uint64_t)fviz_data_array_tuple_count(array) *
            (uint64_t)fviz_data_array_tuple_stride(array);
        if (fviz_vtu_write_raw_block(file, width, fviz_data_array_const_data(array), bytes) == FVIZ_FALSE)
            return FVIZ_FALSE;
    }
    return FVIZ_TRUE;
}

void fviz_vtu_writer_options_initialize(FVizVTUWriterOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->output_mode = FVIZ_VTU_OUTPUT_APPENDED_RAW;
    options->header_width = FVIZ_VTU_HEADER_UINT64;
}

FVizResult fviz_vtu_write(
    const char* file_path,
    const FVizUnstructuredGrid* grid,
    const FVizVTUWriterOptions* options)
{
    FVizVTUWriterOptions defaults;
    FILE* file;
    FVizPoints* grid_points;
    FVizCellArray* cells;
    const FVizVec3* points;
    FVizSize point_count;
    FVizSize cell_count;
    FVizSize connectivity_count;
    uint64_t offset = 0u;
    FVizSize i;
    FVizBool ok = FVIZ_TRUE;
    if (file_path == NULL || grid == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (options == NULL)
    {
        fviz_vtu_writer_options_initialize(&defaults);
        options = &defaults;
    }
    if (options->struct_size < sizeof(*options) ||
        (options->output_mode != FVIZ_VTU_OUTPUT_ASCII &&
         options->output_mode != FVIZ_VTU_OUTPUT_APPENDED_RAW) ||
        (options->header_width != FVIZ_VTU_HEADER_UINT32 &&
         options->header_width != FVIZ_VTU_HEADER_UINT64))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (options->compress != FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
            "VTU compression support is not enabled in this build");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    grid_points = fviz_unstructured_grid_points((FVizUnstructuredGrid*)grid);
    cells = fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid);
    points = fviz_points_data(grid_points);
    point_count = fviz_points_count(grid_points);
    cell_count = fviz_cell_array_count(cells);
    connectivity_count = fviz_cell_array_connectivity_size(cells);
    file = fopen(file_path, "wb");
    if (file == NULL) return FVIZ_ERROR_IO;
    if (fprintf(file,
            "<?xml version=\"1.0\"?>\n"
            "<VTKFile type=\"UnstructuredGrid\" version=\"1.0\" byte_order=\"LittleEndian\" header_type=\"%s\">\n"
            "  <UnstructuredGrid>\n"
            "    <Piece NumberOfPoints=\"%zu\" NumberOfCells=\"%zu\">\n",
            options->header_width == FVIZ_VTU_HEADER_UINT32 ? "UInt32" : "UInt64",
            point_count, cell_count) < 0)
        ok = FVIZ_FALSE;
    if (ok != FVIZ_FALSE) ok = fviz_vtu_write_attributes(file, "PointData",
        fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid), options, &offset);
    if (ok != FVIZ_FALSE) ok = fviz_vtu_write_attributes(file, "CellData",
        fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid), options, &offset);
    if (ok != FVIZ_FALSE) ok = fviz_vtu_write_attributes(file, "FieldData",
        fviz_unstructured_grid_field_data((FVizUnstructuredGrid*)grid), options, &offset);
    if (ok != FVIZ_FALSE && fputs("      <Points>\n", file) == EOF) ok = FVIZ_FALSE;
    if (ok != FVIZ_FALSE)
    {
        if (options->output_mode == FVIZ_VTU_OUTPUT_ASCII)
        {
            if (fputs("        <DataArray type=\"Float32\" Name=\"Points\" NumberOfComponents=\"3\" format=\"ascii\">\n          ", file) == EOF)
                ok = FVIZ_FALSE;
            for (i = 0u; ok != FVIZ_FALSE && i < point_count; ++i)
                if (fprintf(file, "%.9g %.9g %.9g ", (double)points[i].x,
                        (double)points[i].y, (double)points[i].z) < 0)
                    ok = FVIZ_FALSE;
            if (ok != FVIZ_FALSE && fputs("\n        </DataArray>\n", file) == EOF) ok = FVIZ_FALSE;
        }
        else
            ok = fviz_vtu_write_appended_tag(file, "        ", "Float32", "Points", 3u,
                &offset, (uint64_t)point_count * 3u * sizeof(float), options->header_width);
    }
    if (ok != FVIZ_FALSE && fputs("      </Points>\n      <Cells>\n", file) == EOF) ok = FVIZ_FALSE;
    if (ok != FVIZ_FALSE && options->output_mode == FVIZ_VTU_OUTPUT_APPENDED_RAW)
    {
        ok = fviz_vtu_write_appended_tag(file, "        ", "UInt64", "connectivity", 1u,
            &offset, (uint64_t)connectivity_count * sizeof(uint64_t), options->header_width);
        if (ok != FVIZ_FALSE) ok = fviz_vtu_write_appended_tag(file, "        ", "UInt64", "offsets", 1u,
            &offset, (uint64_t)cell_count * sizeof(uint64_t), options->header_width);
        if (ok != FVIZ_FALSE) ok = fviz_vtu_write_appended_tag(file, "        ", "UInt8", "types", 1u,
            &offset, (uint64_t)cell_count, options->header_width);
    }
    else if (ok != FVIZ_FALSE)
    {
        uint64_t running = 0u;
        if (fputs("        <DataArray type=\"UInt64\" Name=\"connectivity\" format=\"ascii\">\n          ", file) == EOF) ok = FVIZ_FALSE;
        for (i = 0u; ok != FVIZ_FALSE && i < connectivity_count; ++i)
            if (fprintf(file, "%" PRIu32 " ", fviz_cell_array_connectivity(cells)[i]) < 0) ok = FVIZ_FALSE;
        if (ok != FVIZ_FALSE && fputs("\n        </DataArray>\n        <DataArray type=\"UInt64\" Name=\"offsets\" format=\"ascii\">\n          ", file) == EOF) ok = FVIZ_FALSE;
        for (i = 0u; ok != FVIZ_FALSE && i < cell_count; ++i)
        {
            running += fviz_cell_array_point_count(cells, i);
            if (fprintf(file, "%" PRIu64 " ", running) < 0) ok = FVIZ_FALSE;
        }
        if (ok != FVIZ_FALSE && fputs("\n        </DataArray>\n        <DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">\n          ", file) == EOF) ok = FVIZ_FALSE;
        for (i = 0u; ok != FVIZ_FALSE && i < cell_count; ++i)
            if (fprintf(file, "%u ", (unsigned)fviz_cell_array_type(cells, i)) < 0) ok = FVIZ_FALSE;
        if (ok != FVIZ_FALSE && fputs("\n        </DataArray>\n", file) == EOF) ok = FVIZ_FALSE;
    }
    if (ok != FVIZ_FALSE && fputs("      </Cells>\n    </Piece>\n  </UnstructuredGrid>\n", file) == EOF)
        ok = FVIZ_FALSE;
    if (ok != FVIZ_FALSE && options->output_mode == FVIZ_VTU_OUTPUT_APPENDED_RAW)
    {
        uint64_t running = 0u;
        if (fputs("  <AppendedData encoding=\"raw\">_", file) == EOF) ok = FVIZ_FALSE;
        if (ok != FVIZ_FALSE) ok = fviz_vtu_write_appended_attributes(file,
            fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid), options->header_width);
        if (ok != FVIZ_FALSE) ok = fviz_vtu_write_appended_attributes(file,
            fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid), options->header_width);
        if (ok != FVIZ_FALSE) ok = fviz_vtu_write_appended_attributes(file,
            fviz_unstructured_grid_field_data((FVizUnstructuredGrid*)grid), options->header_width);
        if (ok != FVIZ_FALSE) ok = fviz_vtu_write_raw_block(file, options->header_width,
            points, (uint64_t)point_count * 3u * sizeof(float));
        if (ok != FVIZ_FALSE && fviz_vtu_write_header_value(file, options->header_width,
                (uint64_t)connectivity_count * sizeof(uint64_t)) == FVIZ_FALSE) ok = FVIZ_FALSE;
        for (i = 0u; ok != FVIZ_FALSE && i < connectivity_count; ++i)
        {
            const uint64_t id = fviz_cell_array_connectivity(cells)[i];
            if (fwrite(&id, sizeof(id), 1u, file) != 1u) ok = FVIZ_FALSE;
        }
        if (ok != FVIZ_FALSE && fviz_vtu_write_header_value(file, options->header_width,
                (uint64_t)cell_count * sizeof(uint64_t)) == FVIZ_FALSE) ok = FVIZ_FALSE;
        for (i = 0u; ok != FVIZ_FALSE && i < cell_count; ++i)
        {
            running += fviz_cell_array_point_count(cells, i);
            if (fwrite(&running, sizeof(running), 1u, file) != 1u) ok = FVIZ_FALSE;
        }
        if (ok != FVIZ_FALSE && fviz_vtu_write_header_value(file, options->header_width,
                (uint64_t)cell_count) == FVIZ_FALSE) ok = FVIZ_FALSE;
        for (i = 0u; ok != FVIZ_FALSE && i < cell_count; ++i)
        {
            const uint8_t type = (uint8_t)fviz_cell_array_type(cells, i);
            if (fwrite(&type, sizeof(type), 1u, file) != 1u) ok = FVIZ_FALSE;
        }
        if (ok != FVIZ_FALSE && fputs("</AppendedData>\n", file) == EOF) ok = FVIZ_FALSE;
    }
    if (ok != FVIZ_FALSE && fputs("</VTKFile>\n", file) == EOF) ok = FVIZ_FALSE;
    if (fclose(file) != 0) ok = FVIZ_FALSE;
    if (ok == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed while writing VTU file");
        return FVIZ_ERROR_IO;
    }
    return FVIZ_OK;
}
