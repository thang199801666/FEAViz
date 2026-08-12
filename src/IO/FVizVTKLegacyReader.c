#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataType.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>
#include <FViz/IO/FVizVTKLegacyReader.h>

#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_VTK_MAX_LINE 4096u

static FVizBool fviz_vtk_read_token(FILE* file, char* out_token, FVizSize max_size)
{
    int c;
    FVizSize index = 0u;
    for (;;)
    {
        c = fgetc(file);
        if (c == EOF) break;
        if (isspace(c) || c == ',')
        {
            if (index > 0u) break;
            continue;
        }
        if (index + 1u < max_size)
        {
            out_token[index++] = (char)c;
        }
    }
    out_token[index] = '\0';
    return index > 0u ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_vtk_skip_line(FILE* file)
{
    int c;
    do
    {
        c = fgetc(file);
        if (c == EOF) return FVIZ_FALSE;
    } while (c != '\n');
    return FVIZ_TRUE;
}

static FVizCellType fviz_vtk_cell_type(int type)
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

static FVizBool fviz_vtk_starts_with(const char* line, const char* keyword)
{
    return strncmp(line, keyword, strlen(keyword)) == 0;
}

static FVizResult fviz_vtk_parse_points(FVizUnstructuredGrid* grid, FILE* file, FVizSize count)
{
    FVizSize i;
    for (i = 0u; i < count; ++i)
    {
        char token[64];
        float x;
        float y;
        float z;
        if (!fviz_vtk_read_token(file, token, sizeof(token))) return FVIZ_ERROR_IO;
        x = (float)strtod(token, NULL);
        if (!fviz_vtk_read_token(file, token, sizeof(token))) return FVIZ_ERROR_IO;
        y = (float)strtod(token, NULL);
        if (!fviz_vtk_read_token(file, token, sizeof(token))) return FVIZ_ERROR_IO;
        z = (float)strtod(token, NULL);
        if (fviz_unstructured_grid_add_point(grid, fviz_vec3(x, y, z), NULL) != FVIZ_OK)
        {
            return fviz_last_error_code();
        }
    }
    return FVIZ_OK;
}

typedef struct FVizVTKCellDraft
{
    uint32_t* ids;
    FVizSize point_count;
} FVizVTKCellDraft;

static FVizResult fviz_vtk_parse_cells(FILE* file, FVizSize count, FVizVTKCellDraft** out_cells)
{
    FVizVTKCellDraft* cells;
    FVizSize i;
    cells = (FVizVTKCellDraft*)fviz_alloc(count * sizeof(FVizVTKCellDraft));
    if (cells == NULL) return fviz_last_error_code();
    (void)memset(cells, 0, count * sizeof(FVizVTKCellDraft));
    for (i = 0u; i < count; ++i)
    {
        char token[64];
        FVizSize point_count;
        FVizSize k;
        if (!fviz_vtk_read_token(file, token, sizeof(token)))
        {
            fviz_free(cells);
            return FVIZ_ERROR_IO;
        }
        point_count = (FVizSize)strtoul(token, NULL, 10);
        cells[i].ids = (uint32_t*)fviz_alloc(point_count * sizeof(uint32_t));
        if (cells[i].ids == NULL)
        {
            fviz_free(cells);
            return fviz_last_error_code();
        }
        cells[i].point_count = point_count;
        for (k = 0u; k < point_count; ++k)
        {
            if (!fviz_vtk_read_token(file, token, sizeof(token)))
            {
                fviz_free(cells);
                return FVIZ_ERROR_IO;
            }
            cells[i].ids[k] = (uint32_t)strtoul(token, NULL, 10);
        }
    }
    *out_cells = cells;
    return FVIZ_OK;
}

static FVizResult fviz_vtk_parse_cell_types(FILE* file, FVizSize count, FVizCellType** out_types)
{
    FVizCellType* types;
    FVizSize i;
    types = (FVizCellType*)fviz_alloc(count * sizeof(FVizCellType));
    if (types == NULL) return fviz_last_error_code();
    for (i = 0u; i < count; ++i)
    {
        char token[64];
        if (!fviz_vtk_read_token(file, token, sizeof(token)))
        {
            fviz_free(types);
            return FVIZ_ERROR_IO;
        }
        types[i] = fviz_vtk_cell_type((int)strtol(token, NULL, 10));
    }
    *out_types = types;
    return FVIZ_OK;
}

static FVizResult fviz_vtk_parse_scalars(
    FVizUnstructuredGrid* grid,
    FVizAttributeSet* destination,
    FILE* file,
    FVizSize count,
    FVizBool cell_scalars)
{
    FVizDataArray* array = NULL;
    FVizSize i;
    (void)grid;
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &array) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < count; ++i)
    {
        char token[64];
        float value;
        if (!fviz_vtk_read_token(file, token, sizeof(token)))
        {
            fviz_release(array);
            return FVIZ_ERROR_IO;
        }
        value = (float)strtod(token, NULL);
        if (fviz_data_array_append_tuple(array, &value) != FVIZ_OK)
        {
            fviz_release(array);
            return fviz_last_error_code();
        }
    }
    {
        const char* name = cell_scalars ? "scalars_cell" : "scalars";
        if (fviz_attribute_set_add(destination, name, array) != FVIZ_OK)
        {
            fviz_release(array);
            return fviz_last_error_code();
        }
    }
    fviz_release(array);
    return FVIZ_OK;
}

static FVizResult fviz_vtk_parse_vectors(
    FVizUnstructuredGrid* grid,
    FVizAttributeSet* destination,
    FILE* file,
    FVizSize count,
    FVizBool cell_vectors)
{
    FVizDataArray* array = NULL;
    FVizSize i;
    (void)grid;
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &array) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < count; ++i)
    {
        float tuple[3];
        FVizSize c;
        for (c = 0u; c < 3u; ++c)
        {
            char token[64];
            if (!fviz_vtk_read_token(file, token, sizeof(token)))
            {
                fviz_release(array);
                return FVIZ_ERROR_IO;
            }
            tuple[c] = (float)strtod(token, NULL);
        }
        if (fviz_data_array_append_tuple(array, tuple) != FVIZ_OK)
        {
            fviz_release(array);
            return fviz_last_error_code();
        }
    }
    {
        const char* name = cell_vectors ? "vectors_cell" : "vectors";
        if (fviz_attribute_set_add(destination, name, array) != FVIZ_OK)
        {
            fviz_release(array);
            return fviz_last_error_code();
        }
    }
    fviz_release(array);
    return FVIZ_OK;
}

FVizResult fviz_vtk_legacy_read(const char* file_path, FVizUnstructuredGrid** out_grid)
{
    FILE* file = NULL;
    FVizUnstructuredGrid* grid = NULL;
    char line[FVIZ_VTK_MAX_LINE];
    FVizSize point_count = 0u;
    FVizSize cell_count = 0u;
    FVizBool header_ok = FVIZ_FALSE;
    FVizResult result;
    FVizVTKCellDraft* draft_cells = NULL;
    FVizCellType* draft_types = NULL;

    if (out_grid == NULL || file_path == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "file path and output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    file = fopen(file_path, "r");
    if (file == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_IO, "failed to open VTK file");
        return FVIZ_ERROR_IO;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (fviz_vtk_starts_with(line, "DATASET UNSTRUCTURED_GRID"))
        {
            header_ok = FVIZ_TRUE;
            break;
        }
        if (fviz_vtk_starts_with(line, "DATASET"))
        {
            break;
        }
    }
    if (!header_ok)
    {
        fclose(file);
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "not a VTK legacy unstructured grid file");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    if (fviz_unstructured_grid_create(&grid) != FVIZ_OK)
    {
        fclose(file);
        return fviz_last_error_code();
    }

    result = FVIZ_OK;
    while (fgets(line, sizeof(line), file) != NULL && result == FVIZ_OK)
    {
        if (fviz_vtk_starts_with(line, "POINTS"))
        {
            char* rest = line + strlen("POINTS");
            point_count = (FVizSize)strtoul(rest, NULL, 10);
            result = fviz_vtk_parse_points(grid, file, point_count);
        }
        else if (fviz_vtk_starts_with(line, "CELLS"))
        {
            char* rest = line + strlen("CELLS");
            cell_count = (FVizSize)strtoul(rest, NULL, 10);
            result = fviz_vtk_parse_cells(file, cell_count, &draft_cells);
        }
        else if (fviz_vtk_starts_with(line, "CELL_TYPES"))
        {
            char* rest = line + strlen("CELL_TYPES");
            result = fviz_vtk_parse_cell_types(file, (FVizSize)strtoul(rest, NULL, 10), &draft_types);
        }
        else if (fviz_vtk_starts_with(line, "POINT_DATA"))
        {
            char* rest = line + strlen("POINT_DATA");
            FVizSize count = (FVizSize)strtoul(rest, NULL, 10);
            while (result == FVIZ_OK && fgets(line, sizeof(line), file) != NULL)
            {
                if (fviz_vtk_starts_with(line, "SCALARS"))
                {
                    if (fgets(line, sizeof(line), file) != NULL && fviz_vtk_starts_with(line, "LOOKUP_TABLE"))
                    {
                        result = FVIZ_OK;
                    }
                    result = result == FVIZ_OK
                        ? fviz_vtk_parse_scalars(grid, fviz_unstructured_grid_point_data(grid), file, count, FVIZ_FALSE)
                        : FVIZ_ERROR_IO;
                    break;
                }
                if (fviz_vtk_starts_with(line, "VECTORS"))
                {
                    result = fviz_vtk_parse_vectors(grid, fviz_unstructured_grid_point_data(grid), file, count, FVIZ_FALSE);
                    break;
                }
                if (fviz_vtk_starts_with(line, "METADATA"))
                {
                    result = FVIZ_OK;
                    break;
                }
            }
        }
        else if (fviz_vtk_starts_with(line, "CELL_DATA"))
        {
            char* rest = line + strlen("CELL_DATA");
            FVizSize count = (FVizSize)strtoul(rest, NULL, 10);
            while (result == FVIZ_OK && fgets(line, sizeof(line), file) != NULL)
            {
                if (fviz_vtk_starts_with(line, "SCALARS"))
                {
                    if (fgets(line, sizeof(line), file) != NULL && fviz_vtk_starts_with(line, "LOOKUP_TABLE"))
                    {
                        result = FVIZ_OK;
                    }
                    result = result == FVIZ_OK
                        ? fviz_vtk_parse_scalars(grid, fviz_unstructured_grid_cell_data(grid), file, count, FVIZ_TRUE)
                        : FVIZ_ERROR_IO;
                    break;
                }
                if (fviz_vtk_starts_with(line, "VECTORS"))
                {
                    result = fviz_vtk_parse_vectors(grid, fviz_unstructured_grid_cell_data(grid), file, count, FVIZ_TRUE);
                    break;
                }
                if (fviz_vtk_starts_with(line, "METADATA"))
                {
                    result = FVIZ_OK;
                    break;
                }
            }
        }
        else if (fviz_vtk_starts_with(line, "FIELD"))
        {
            result = fviz_vtk_skip_line(file) ? FVIZ_OK : FVIZ_ERROR_IO;
        }
    }

    fclose(file);
    if (result != FVIZ_OK)
    {
        goto cleanup;
    }
    if (draft_cells != NULL && draft_types != NULL)
    {
        FVizSize i;
        for (i = 0u; i < cell_count; ++i)
        {
            if (draft_types[i] == (FVizCellType)0)
            {
                fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported VTK cell type in legacy file");
                result = FVIZ_ERROR_NOT_SUPPORTED;
                goto cleanup;
            }
            if (fviz_unstructured_grid_add_cell(grid, draft_types[i], draft_cells[i].point_count, draft_cells[i].ids) != FVIZ_OK)
            {
                result = fviz_last_error_code();
                goto cleanup;
            }
        }
    }
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto cleanup;
    }
    {
        FVizSize i;
        if (draft_cells != NULL)
        {
            for (i = 0u; i < cell_count; ++i) fviz_free(draft_cells[i].ids);
            fviz_free(draft_cells);
        }
        fviz_free(draft_types);
    }
    *out_grid = grid;
    return FVIZ_OK;

cleanup:
    if (draft_cells != NULL)
    {
        FVizSize i;
        for (i = 0u; i < cell_count; ++i) fviz_free(draft_cells[i].ids);
        fviz_free(draft_cells);
    }
    fviz_free(draft_types);
    fviz_release(grid);
    return result;
}
