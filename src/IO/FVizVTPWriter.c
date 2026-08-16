#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Data/FVizDataType.h>
#include <FViz/IO/FVizVTPWriter.h>

#include <FViz/Core/FVizErrorInternal.h>

static const char* fviz_vtp_type_name(FVizDataType type)
{
    switch (type)
    {
        case FVIZ_DATA_INT8:
            return "Int8";
        case FVIZ_DATA_UINT8:
            return "UInt8";
        case FVIZ_DATA_INT16:
            return "Int16";
        case FVIZ_DATA_UINT16:
            return "UInt16";
        case FVIZ_DATA_INT32:
            return "Int32";
        case FVIZ_DATA_UINT32:
            return "UInt32";
        case FVIZ_DATA_INT64:
            return "Int64";
        case FVIZ_DATA_UINT64:
            return "UInt64";
        case FVIZ_DATA_FLOAT32:
            return "Float32";
        case FVIZ_DATA_FLOAT64:
            return "Float64";
        default:
            return NULL;
    }
}

static FVizBool fviz_vtp_write_xml_escaped(FILE* file, const char* text)
{
    const unsigned char* cursor = (const unsigned char*)(text != NULL ? text : "");
    while (*cursor != 0u)
    {
        const char* replacement = NULL;
        switch (*cursor)
        {
            case '&':
                replacement = "&amp;";
                break;
            case '<':
                replacement = "&lt;";
                break;
            case '>':
                replacement = "&gt;";
                break;
            case '"':
                replacement = "&quot;";
                break;
            case '\'':
                replacement = "&apos;";
                break;
            default:
                break;
        }
        if (replacement != NULL)
        {
            if (fputs(replacement, file) == EOF) return FVIZ_FALSE;
        }
        else if (fputc((int)*cursor, file) == EOF)
            return FVIZ_FALSE;
        ++cursor;
    }
    return FVIZ_TRUE;
}

static void fviz_vtp_write_scalar(FILE* file, const void* tuple, FVizDataType type, uint32_t component)
{
    switch (type)
    {
        case FVIZ_DATA_INT8:
            (void)fprintf(file, "%" PRId8, ((const int8_t*)tuple)[component]);
            break;
        case FVIZ_DATA_UINT8:
            (void)fprintf(file, "%" PRIu8, ((const uint8_t*)tuple)[component]);
            break;
        case FVIZ_DATA_INT16:
            (void)fprintf(file, "%" PRId16, ((const int16_t*)tuple)[component]);
            break;
        case FVIZ_DATA_UINT16:
            (void)fprintf(file, "%" PRIu16, ((const uint16_t*)tuple)[component]);
            break;
        case FVIZ_DATA_INT32:
            (void)fprintf(file, "%" PRId32, ((const int32_t*)tuple)[component]);
            break;
        case FVIZ_DATA_UINT32:
            (void)fprintf(file, "%" PRIu32, ((const uint32_t*)tuple)[component]);
            break;
        case FVIZ_DATA_INT64:
            (void)fprintf(file, "%" PRId64, ((const int64_t*)tuple)[component]);
            break;
        case FVIZ_DATA_UINT64:
            (void)fprintf(file, "%" PRIu64, ((const uint64_t*)tuple)[component]);
            break;
        case FVIZ_DATA_FLOAT32:
            (void)fprintf(file, "%.9g", (double)((const float*)tuple)[component]);
            break;
        case FVIZ_DATA_FLOAT64:
            (void)fprintf(file, "%.17g", ((const double*)tuple)[component]);
            break;
        default:
            break;
    }
}

static FVizBool fviz_vtp_write_array(FILE* file, const char* indentation, const char* name, const FVizDataArray* array,
                                     FVizBool include_tuple_count)
{
    const char* type_name = fviz_vtp_type_name(fviz_data_array_type(array));
    const uint32_t components = fviz_data_array_components(array);
    FVizSize tuple_index;
    if (type_name == NULL || components == 0u) return FVIZ_FALSE;
    if (fprintf(file, "%s<DataArray type=\"%s\" Name=\"", indentation, type_name) < 0 ||
        fviz_vtp_write_xml_escaped(file, name) == FVIZ_FALSE ||
        fprintf(file, "\" NumberOfComponents=\"%u\"", components) < 0 ||
        (include_tuple_count &&
         fprintf(file, " NumberOfTuples=\"%zu\"", (size_t)fviz_data_array_tuple_count(array)) < 0) ||
        fprintf(file, " format=\"ascii\">\n%s  ", indentation) < 0)
        return FVIZ_FALSE;
    for (tuple_index = 0u; tuple_index < fviz_data_array_tuple_count(array); ++tuple_index)
    {
        const void* tuple = fviz_data_array_const_tuple(array, tuple_index);
        uint32_t component;
        for (component = 0u; component < components; ++component)
        {
            fviz_vtp_write_scalar(file, tuple, fviz_data_array_type(array), component);
            if (fputc(' ', file) == EOF) return FVIZ_FALSE;
        }
    }
    return fprintf(file, "\n%s</DataArray>\n", indentation) >= 0 ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_vtp_write_attributes(FILE* file, const char* indentation, const char* section_name,
                                          const FVizAttributeSet* set)
{
    static const char* role_names[FVIZ_ATTRIBUTE_ROLE_COUNT] = {"Scalars", "Vectors", "Normals", "Tensors",
                                                                "GlobalIds"};
    FVizSize i;
    if (fprintf(file, "%s<%s", indentation, section_name) < 0) return FVIZ_FALSE;
    for (i = 0u; i < FVIZ_ATTRIBUTE_ROLE_COUNT; ++i)
    {
        const char* active = fviz_attribute_set_active_name(set, (FVizAttributeRole)i);
        if (active != NULL)
        {
            if (fprintf(file, " %s=\"", role_names[i]) < 0 || fviz_vtp_write_xml_escaped(file, active) == FVIZ_FALSE ||
                fputc('"', file) == EOF)
                return FVIZ_FALSE;
        }
    }
    if (fputs(">\n", file) == EOF) return FVIZ_FALSE;
    for (i = 0u; i < fviz_attribute_set_count(set); ++i)
    {
        char array_indentation[32];
        if (snprintf(array_indentation, sizeof(array_indentation), "%s  ", indentation) < 0) return FVIZ_FALSE;
        if (fviz_vtp_write_array(file, array_indentation, fviz_attribute_set_name_at(set, i),
                                 fviz_attribute_set_const_array_at(set, i),
                                 strcmp(section_name, "FieldData") == 0 ? FVIZ_TRUE : FVIZ_FALSE) == FVIZ_FALSE)
            return FVIZ_FALSE;
    }
    return fprintf(file, "%s</%s>\n", indentation, section_name) >= 0 ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_vtp_write_cell_section(FILE* file, const char* name, const FVizCellArray* cells)
{
    FVizSize i;
    FVizSize offset = 0u;
    if (fprintf(file, "      <%s>\n", name) < 0 ||
        fputs("        <DataArray type=\"UInt64\" Name=\"connectivity\" format=\"ascii\">\n          ", file) == EOF)
        return FVIZ_FALSE;
    for (i = 0u; i < fviz_cell_array_count(cells); ++i)
    {
        FVizCellView view;
        FVizSize j;
        if (fviz_cell_array_cell_view(cells, i, &view) != FVIZ_OK) return FVIZ_FALSE;
        for (j = 0u; j < view.point_count; ++j)
            if (fprintf(file, "%" PRIu64 " ", (uint64_t)fviz_cell_view_point_id(&view, j)) < 0) return FVIZ_FALSE;
    }
    if (fputs(
            "\n        </DataArray>\n        <DataArray type=\"UInt64\" Name=\"offsets\" format=\"ascii\">\n          ",
            file) == EOF)
        return FVIZ_FALSE;
    for (i = 0u; i < fviz_cell_array_count(cells); ++i)
    {
        const FVizSize count = fviz_cell_array_point_count(cells, i);
        if (offset > (FVizSize)-1 - count) return FVIZ_FALSE;
        offset += count;
        if (fprintf(file, "%" PRIu64 " ", (uint64_t)offset) < 0) return FVIZ_FALSE;
    }
    return fprintf(file, "\n        </DataArray>\n      </%s>\n", name) >= 0 ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_vtp_writer_options_initialize(FVizVTPWriterOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->output_mode = FVIZ_VTP_OUTPUT_ASCII;
}

FVizResult fviz_vtp_write(const char* file_path, const FVizPolyData* poly_data, const FVizVTPWriterOptions* options)
{
    FVizVTPWriterOptions defaults;
    FILE* file;
    FVizSize i;
    if (file_path == NULL || poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "VTP writer requires a file path and PolyData");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (options == NULL)
    {
        fviz_vtp_writer_options_initialize(&defaults);
        options = &defaults;
    }
    if (options->struct_size < sizeof(*options) || options->output_mode != FVIZ_VTP_OUTPUT_ASCII)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "unsupported VTP writer options");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_poly_data_validate(poly_data) != FVIZ_OK) return fviz_last_error_code();
    file = fopen(file_path, "wb");
    if (file == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to open VTP output file");
        return FVIZ_ERROR_IO;
    }
    if (fprintf(file, "<?xml version=\"1.0\"?>\n"
                      "<VTKFile type=\"PolyData\" version=\"1.0\" byte_order=\"LittleEndian\">\n"
                      "  <PolyData>\n") < 0 ||
        fviz_vtp_write_attributes(file, "    ", "FieldData", fviz_poly_data_const_field_data(poly_data)) ==
            FVIZ_FALSE ||
        fprintf(file,
                "    <Piece NumberOfPoints=\"%zu\" NumberOfVerts=\"%zu\" NumberOfLines=\"%zu\" NumberOfStrips=\"%zu\" "
                "NumberOfPolys=\"%zu\">\n",
                (size_t)fviz_poly_data_point_count(poly_data), (size_t)fviz_poly_data_vert_cell_count(poly_data),
                (size_t)fviz_poly_data_line_cell_count(poly_data), (size_t)fviz_poly_data_strip_cell_count(poly_data),
                (size_t)fviz_poly_data_poly_cell_count(poly_data)) < 0 ||
        fviz_vtp_write_attributes(file, "      ", "PointData", fviz_poly_data_const_point_data(poly_data)) ==
            FVIZ_FALSE ||
        fviz_vtp_write_attributes(file, "      ", "CellData", fviz_poly_data_const_cell_data(poly_data)) ==
            FVIZ_FALSE ||
        fputs("      <Points>\n        <DataArray type=\"Float32\" NumberOfComponents=\"3\" format=\"ascii\">\n        "
              "  ",
              file) == EOF)
        goto io_fail;
    for (i = 0u; i < fviz_poly_data_point_count(poly_data); ++i)
    {
        const FVizVec3 p = fviz_poly_data_points(poly_data)[i];
        if (fprintf(file, "%.9g %.9g %.9g ", (double)p.x, (double)p.y, (double)p.z) < 0) goto io_fail;
    }
    if (fputs("\n        </DataArray>\n      </Points>\n", file) == EOF ||
        fviz_vtp_write_cell_section(file, "Verts", fviz_poly_data_verts(poly_data)) == FVIZ_FALSE ||
        fviz_vtp_write_cell_section(file, "Lines", fviz_poly_data_lines(poly_data)) == FVIZ_FALSE ||
        fviz_vtp_write_cell_section(file, "Strips", fviz_poly_data_strips(poly_data)) == FVIZ_FALSE ||
        fviz_vtp_write_cell_section(file, "Polys", fviz_poly_data_polys(poly_data)) == FVIZ_FALSE ||
        fputs("    </Piece>\n  </PolyData>\n</VTKFile>\n", file) == EOF)
        goto io_fail;
    if (fclose(file) != 0)
    {
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed while closing VTP output file");
        return FVIZ_ERROR_IO;
    }
    return FVIZ_OK;
io_fail:
    (void)fclose(file);
    fviz_internal_set_error(FVIZ_ERROR_IO, "failed while writing VTP file");
    return FVIZ_ERROR_IO;
}
