#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizGhost.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/IO/FVizPVTUWriter.h>

#include <FViz/Core/FVizErrorInternal.h>

static FVizBool fviz_pvtu_extension_equals(const char* extension, const char* expected)
{
    if (extension == NULL || expected == NULL) return FVIZ_FALSE;
    while (*extension != '\0' && *expected != '\0')
    {
        if (tolower((unsigned char)*extension) != tolower((unsigned char)*expected)) return FVIZ_FALSE;
        ++extension;
        ++expected;
    }
    return *extension == '\0' && *expected == '\0' ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_pvtu_writer_names(const char* manifest_path, FVizSize piece_index, char** out_source_name,
                                         char** out_full_path)
{
    const char* slash_a;
    const char* slash_b;
    const char* slash;
    const char* base;
    const char* extension;
    FVizSize prefix_length;
    FVizSize stem_length;
    FVizSize source_capacity;
    FVizSize full_capacity;
    char* source;
    char* full;
    int written;
    if (manifest_path == NULL || out_source_name == NULL || out_full_path == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_source_name = NULL;
    *out_full_path = NULL;
    slash_a = strrchr(manifest_path, '/');
    slash_b = strrchr(manifest_path, '\\');
    slash = slash_a != NULL && (slash_b == NULL || slash_a > slash_b) ? slash_a : slash_b;
    prefix_length = slash != NULL ? (FVizSize)(slash - manifest_path) + 1u : 0u;
    base = manifest_path + prefix_length;
    extension = strrchr(base, '.');
    stem_length = extension != NULL && fviz_pvtu_extension_equals(extension, ".pvtu") != FVIZ_FALSE
                      ? (FVizSize)(extension - base)
                      : strlen(base);
    if (stem_length == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PVTU writer manifest has no basename");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_size_add(stem_length, 40u, &source_capacity) != FVIZ_OK ||
        fviz_size_add(prefix_length, source_capacity, &full_capacity) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    source = (char*)fviz_alloc(source_capacity);
    full = (char*)fviz_alloc(full_capacity);
    if (source == NULL || full == NULL)
    {
        fviz_free(full);
        fviz_free(source);
        return fviz_last_error_code();
    }
    (void)memcpy(source, base, stem_length);
    source[stem_length] = '\0';
    written = snprintf(source + stem_length, source_capacity - stem_length, "_piece%05llu.vtu",
                       (unsigned long long)piece_index);
    if (written < 0 || (FVizSize)written >= source_capacity - stem_length)
    {
        fviz_free(full);
        fviz_free(source);
        return FVIZ_ERROR_OVERFLOW;
    }
    if (prefix_length != 0u) (void)memcpy(full, manifest_path, prefix_length);
    (void)memcpy(full + prefix_length, source, strlen(source) + 1u);
    *out_source_name = source;
    *out_full_path = full;
    return FVIZ_OK;
}

static const char* fviz_pvtu_writer_type_name(FVizDataType type)
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

static FVizBool fviz_pvtu_write_xml_escaped(FILE* file, const char* value)
{
    const unsigned char* cursor = (const unsigned char*)value;
    while (cursor != NULL && *cursor != 0u)
    {
        const char* replacement = NULL;
        if (*cursor == '&') replacement = "&amp;";
        else if (*cursor == '<')
            replacement = "&lt;";
        else if (*cursor == '>')
            replacement = "&gt;";
        else if (*cursor == '"')
            replacement = "&quot;";
        else if (*cursor == '\'')
            replacement = "&apos;";
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

static FVizBool fviz_pvtu_attribute_schema_equal(const FVizAttributeSet* left, const FVizAttributeSet* right)
{
    FVizSize i;
    FVizAttributeRole role;
    if (left == NULL || right == NULL || fviz_attribute_set_count(left) != fviz_attribute_set_count(right))
        return FVIZ_FALSE;
    for (i = 0u; i < fviz_attribute_set_count(left); ++i)
    {
        const char* left_name = fviz_attribute_set_name_at(left, i);
        const char* right_name = fviz_attribute_set_name_at(right, i);
        const FVizDataArray* left_array = fviz_attribute_set_const_array_at(left, i);
        const FVizDataArray* right_array = fviz_attribute_set_const_array_at(right, i);
        if (left_name == NULL || right_name == NULL || strcmp(left_name, right_name) != 0 || left_array == NULL ||
            right_array == NULL || fviz_data_array_type(left_array) != fviz_data_array_type(right_array) ||
            fviz_data_array_components(left_array) != fviz_data_array_components(right_array))
            return FVIZ_FALSE;
    }
    for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
    {
        const char* left_active = fviz_attribute_set_active_name(left, role);
        const char* right_active = fviz_attribute_set_active_name(right, role);
        if (left_active == NULL || right_active == NULL)
        {
            if (left_active != right_active) return FVIZ_FALSE;
        }
        else if (strcmp(left_active, right_active) != 0)
            return FVIZ_FALSE;
    }
    return FVIZ_TRUE;
}

static FVizBool fviz_pvtu_has_native_ghost(const FVizAttributeSet* attributes)
{
    const FVizDataArray* array =
        attributes != NULL ? fviz_attribute_set_const_get(attributes, FVIZ_GHOST_ARRAY_NAME) : NULL;
    return array != NULL && fviz_data_array_type(array) == FVIZ_DATA_UINT8 && fviz_data_array_components(array) == 1u
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

static const char* fviz_pvtu_export_name(const FVizAttributeSet* attributes, const char* name,
                                         const FVizDataArray* array, FVizBool associated_data)
{
    if (associated_data != FVIZ_FALSE && name != NULL && array != NULL && strcmp(name, FVIZ_GHOST_ARRAY_NAME) == 0 &&
        fviz_data_array_type(array) == FVIZ_DATA_UINT8 && fviz_data_array_components(array) == 1u)
        return FVIZ_VTK_GHOST_ARRAY_NAME;
    (void)attributes;
    return name;
}

static FVizBool fviz_pvtu_skip_export_name(const FVizAttributeSet* attributes, const char* name)
{
    return name != NULL && strcmp(name, FVIZ_VTK_GHOST_ARRAY_NAME) == 0 &&
                   fviz_pvtu_has_native_ghost(attributes) != FVIZ_FALSE
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

static FVizBool fviz_pvtu_write_parallel_attributes(FILE* file, const char* section_name,
                                                    const FVizAttributeSet* attributes)
{
    static const char* role_attributes[FVIZ_ATTRIBUTE_ROLE_COUNT] = {"Scalars", "Vectors", "Normals", "Tensors",
                                                                     "GlobalIds"};
    FVizSize i;
    FVizAttributeRole role;
    const FVizBool associated_data =
        (strcmp(section_name, "PointData") == 0 || strcmp(section_name, "CellData") == 0) ? FVIZ_TRUE : FVIZ_FALSE;
    if (fprintf(file, "    <P%s", section_name) < 0) return FVIZ_FALSE;
    for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
    {
        const char* active = fviz_attribute_set_active_name(attributes, role);
        const FVizDataArray* active_array = active != NULL ? fviz_attribute_set_const_get(attributes, active) : NULL;
        const char* export_active =
            active != NULL ? fviz_pvtu_export_name(attributes, active, active_array, associated_data) : NULL;
        if (export_active != NULL)
        {
            if (fprintf(file, " %s=\"", role_attributes[role]) < 0 ||
                fviz_pvtu_write_xml_escaped(file, export_active) == FVIZ_FALSE || fputc('"', file) == EOF)
                return FVIZ_FALSE;
        }
    }
    if (fputs(">\n", file) == EOF) return FVIZ_FALSE;
    for (i = 0u; i < fviz_attribute_set_count(attributes); ++i)
    {
        const char* name = fviz_attribute_set_name_at(attributes, i);
        const FVizDataArray* array = fviz_attribute_set_const_array_at(attributes, i);
        const char* export_name;
        const char* type_name = array != NULL ? fviz_pvtu_writer_type_name(fviz_data_array_type(array)) : NULL;
        if (fviz_pvtu_skip_export_name(attributes, name) != FVIZ_FALSE) continue;
        export_name = fviz_pvtu_export_name(attributes, name, array, associated_data);
        if (export_name == NULL || array == NULL || type_name == NULL ||
            fprintf(file, "      <PDataArray type=\"%s\" Name=\"", type_name) < 0 ||
            fviz_pvtu_write_xml_escaped(file, export_name) == FVIZ_FALSE ||
            fprintf(file, "\" NumberOfComponents=\"%u\"/>\n", fviz_data_array_components(array)) < 0)
            return FVIZ_FALSE;
    }
    return fprintf(file, "    </P%s>\n", section_name) >= 0 ? FVIZ_TRUE : FVIZ_FALSE;
}

static uint32_t fviz_pvtu_grid_ghost_level(const FVizUnstructuredGrid* grid)
{
    const FVizAttributeSet* cells = fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid);
    const FVizDataArray* levels = fviz_attribute_set_const_get(cells, FVIZ_GHOST_LEVEL_ARRAY_NAME);
    const FVizDataArray* ghosts = fviz_attribute_set_const_get(cells, FVIZ_GHOST_ARRAY_NAME);
    const FVizSize cell_count = fviz_unstructured_grid_cell_count(grid);
    uint32_t maximum = 0u;
    FVizSize i;
    if (levels != NULL && fviz_data_array_type(levels) == FVIZ_DATA_UINT16 &&
        fviz_data_array_components(levels) == 1u && fviz_data_array_tuple_count(levels) == cell_count)
    {
        const uint16_t* values = (const uint16_t*)fviz_data_array_const_data(levels);
        for (i = 0u; i < cell_count; ++i)
            if ((uint32_t)values[i] > maximum) maximum = (uint32_t)values[i];
    }
    if (maximum == 0u && ghosts != NULL && fviz_data_array_type(ghosts) == FVIZ_DATA_UINT8 &&
        fviz_data_array_components(ghosts) == 1u && fviz_data_array_tuple_count(ghosts) == cell_count)
    {
        const uint8_t* values = (const uint8_t*)fviz_data_array_const_data(ghosts);
        for (i = 0u; i < cell_count; ++i)
            if ((values[i] & (uint8_t)(FVIZ_GHOST_DUPLICATE | FVIZ_GHOST_HIDDEN)) != 0u)
            {
                maximum = 1u;
                break;
            }
    }
    return maximum;
}

void fviz_pvtu_writer_options_initialize(FVizPVTUWriterOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    fviz_vtu_writer_options_initialize(&options->piece_options);
}

FVizResult fviz_pvtu_write(const char* file_path, const FVizPartitionedDataSet* data_set,
                           const FVizPVTUWriterOptions* options)
{
    FVizPVTUWriterOptions defaults;
    FVizSize count;
    FVizSize piece_index;
    const FVizUnstructuredGrid* first_grid = NULL;
    uint32_t ghost_level = 0u;
    char** generated_paths = NULL;
    char** source_names = NULL;
    FILE* manifest = NULL;
    FVizResult result = FVIZ_OK;
    if (file_path == NULL || file_path[0] == '\0' || data_set == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (options == NULL)
    {
        fviz_pvtu_writer_options_initialize(&defaults);
        options = &defaults;
    }
    if (options->struct_size < sizeof(*options)) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_partitioned_data_set_count(data_set);
    if (count == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PVTU writer requires at least one partition");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    generated_paths = (char**)fviz_alloc(count * sizeof(*generated_paths));
    source_names = (char**)fviz_alloc(count * sizeof(*source_names));
    if (generated_paths == NULL || source_names == NULL)
    {
        fviz_free(source_names);
        fviz_free(generated_paths);
        return fviz_last_error_code();
    }
    (void)memset(generated_paths, 0, count * sizeof(*generated_paths));
    (void)memset(source_names, 0, count * sizeof(*source_names));

    for (piece_index = 0u; piece_index < count; ++piece_index)
    {
        const FVizDataObject* piece = fviz_partitioned_data_set_const_partition(data_set, piece_index);
        if (piece == NULL || fviz_object_is_type((const FVizObject*)piece, FVIZ_TYPE_UNSTRUCTURED_GRID) == FVIZ_FALSE)
        {
            result = FVIZ_ERROR_INVALID_ARGUMENT;
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "PVTU writer requires every partition to be an UnstructuredGrid");
            goto cleanup;
        }
        if (first_grid == NULL) first_grid = (const FVizUnstructuredGrid*)piece;
        else if (fviz_pvtu_attribute_schema_equal(fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)first_grid),
                                                  fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)piece)) ==
                     FVIZ_FALSE ||
                 fviz_pvtu_attribute_schema_equal(fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)first_grid),
                                                  fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)piece)) ==
                     FVIZ_FALSE)
        {
            result = FVIZ_ERROR_INVALID_ARGUMENT;
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "PVTU partitions must have matching point/cell attribute schemas");
            goto cleanup;
        }
        {
            const uint32_t piece_ghost_level = fviz_pvtu_grid_ghost_level((const FVizUnstructuredGrid*)piece);
            if (piece_ghost_level > ghost_level) ghost_level = piece_ghost_level;
        }
        result =
            fviz_pvtu_writer_names(file_path, piece_index, &source_names[piece_index], &generated_paths[piece_index]);
        if (result != FVIZ_OK) goto cleanup;
        result =
            fviz_vtu_write(generated_paths[piece_index], (const FVizUnstructuredGrid*)piece, &options->piece_options);
        if (result != FVIZ_OK) goto cleanup;
    }

    manifest = fopen(file_path, "wb");
    if (manifest == NULL)
    {
        result = FVIZ_ERROR_IO;
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to create PVTU manifest");
        goto cleanup;
    }
    if (fprintf(manifest,
                "<?xml version=\"1.0\"?>\n"
                "<VTKFile type=\"PUnstructuredGrid\" version=\"1.0\" byte_order=\"LittleEndian\">\n"
                "  <PUnstructuredGrid GhostLevel=\"%u\">\n",
                ghost_level) < 0 ||
        fviz_pvtu_write_parallel_attributes(manifest, "PointData",
                                            fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)first_grid)) ==
            FVIZ_FALSE ||
        fviz_pvtu_write_parallel_attributes(
            manifest, "CellData", fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)first_grid)) == FVIZ_FALSE ||
        fputs("    <PPoints><PDataArray type=\"Float32\" NumberOfComponents=\"3\"/></PPoints>\n", manifest) == EOF)
    {
        result = FVIZ_ERROR_IO;
        goto cleanup;
    }
    for (piece_index = 0u; piece_index < count; ++piece_index)
    {
        if (fputs("    <Piece Source=\"", manifest) == EOF ||
            fviz_pvtu_write_xml_escaped(manifest, source_names[piece_index]) == FVIZ_FALSE ||
            fputs("\"/>\n", manifest) == EOF)
        {
            result = FVIZ_ERROR_IO;
            goto cleanup;
        }
    }
    if (fputs("  </PUnstructuredGrid>\n</VTKFile>\n", manifest) == EOF)
    {
        result = FVIZ_ERROR_IO;
        goto cleanup;
    }
    if (fclose(manifest) != 0)
    {
        manifest = NULL;
        result = FVIZ_ERROR_IO;
        goto cleanup;
    }
    manifest = NULL;

cleanup:
    if (manifest != NULL) (void)fclose(manifest);
    if (result != FVIZ_OK)
    {
        (void)remove(file_path);
        for (piece_index = 0u; piece_index < count; ++piece_index)
            if (generated_paths[piece_index] != NULL) (void)remove(generated_paths[piece_index]);
    }
    for (piece_index = 0u; piece_index < count; ++piece_index)
    {
        fviz_free(source_names[piece_index]);
        fviz_free(generated_paths[piece_index]);
    }
    fviz_free(source_names);
    fviz_free(generated_paths);
    return result;
}
