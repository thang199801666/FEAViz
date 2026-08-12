#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizArray.h>
#include <FViz/FEA/FVizUnstructuredGridPrivate.h>
#include <FViz/FEA/FVizSurfaceFacePrivate.h>

typedef struct FVizSurfaceDefinition
{
    FVizCellType type;
    uint32_t face_count;
    uint32_t faces[6][4];
    uint32_t sizes[6];
} FVizSurfaceDefinition;

static void fviz_unstructured_grid_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_unstructured_grid_class = {
    FVIZ_TYPE_UNSTRUCTURED_GRID, "FVizUnstructuredGrid", &g_fviz_object_class, fviz_unstructured_grid_destroy
};

static void fviz_unstructured_grid_destroy(FVizObject* object)
{
    FVizUnstructuredGrid* grid = (FVizUnstructuredGrid*)object;
    fviz_release(grid->points);
    fviz_release(grid->cells);
    fviz_release(grid->data_set);
}

FVizResult fviz_unstructured_grid_create(FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* grid;
    if (out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    grid = (FVizUnstructuredGrid*)fviz_internal_object_allocate(
        sizeof(FVizUnstructuredGrid), &g_fviz_unstructured_grid_class, NULL);
    if (grid == NULL) return fviz_last_error_code();
    if (fviz_points_create(&grid->points) != FVIZ_OK ||
        fviz_cell_array_create(&grid->cells) != FVIZ_OK ||
        fviz_data_set_create(&grid->data_set) != FVIZ_OK)
    {
        fviz_release(grid);
        return fviz_last_error_code();
    }
    *out_grid = grid;
    return FVIZ_OK;
}

void fviz_unstructured_grid_clear(FVizUnstructuredGrid* grid)
{
    if (grid == NULL) return;
    fviz_points_clear(grid->points);
    fviz_cell_array_clear(grid->cells);
    fviz_attribute_set_clear(fviz_data_set_point_data(grid->data_set));
    fviz_attribute_set_clear(fviz_data_set_cell_data(grid->data_set));
    fviz_attribute_set_clear(fviz_data_set_field_data(grid->data_set));
    (void)fviz_data_set_set_point_count(grid->data_set, 0u);
    (void)fviz_data_set_set_cell_count(grid->data_set, 0u);
}

FVizPoints* fviz_unstructured_grid_points(FVizUnstructuredGrid* grid) { return grid != NULL ? grid->points : NULL; }
FVizCellArray* fviz_unstructured_grid_cells(FVizUnstructuredGrid* grid) { return grid != NULL ? grid->cells : NULL; }
FVizResult fviz_unstructured_grid_add_point(FVizUnstructuredGrid* grid, FVizVec3 point, uint32_t* out_id)
{
    FVizResult result;
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result = fviz_points_append(grid->points, point, out_id);
    if (result == FVIZ_OK) result = fviz_data_set_set_point_count(grid->data_set, fviz_points_count(grid->points));
    return result;
}

FVizResult fviz_unstructured_grid_add_cell(FVizUnstructuredGrid* grid, FVizCellType type, FVizSize point_count, const uint32_t* point_ids)
{
    FVizResult result;
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result = fviz_cell_array_append(grid->cells, type, point_count, point_ids);
    if (result == FVIZ_OK) result = fviz_data_set_set_cell_count(grid->data_set, fviz_cell_array_count(grid->cells));
    return result;
}
FVizAttributeSet* fviz_unstructured_grid_point_data(FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_data_set_point_data(grid->data_set) : NULL; }
FVizAttributeSet* fviz_unstructured_grid_cell_data(FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_data_set_cell_data(grid->data_set) : NULL; }
FVizAttributeSet* fviz_unstructured_grid_field_data(FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_data_set_field_data(grid->data_set) : NULL; }
FVizSize fviz_unstructured_grid_point_count(const FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_points_count(grid->points) : 0u; }
FVizSize fviz_unstructured_grid_cell_count(const FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_cell_array_count(grid->cells) : 0u; }
FVizBounds fviz_unstructured_grid_bounds(const FVizUnstructuredGrid* grid) { return grid != NULL ? fviz_points_bounds(grid->points) : fviz_bounds_empty(); }

FVizResult fviz_unstructured_grid_validate(const FVizUnstructuredGrid* grid)
{
    FVizResult result;
    if (grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result = fviz_cell_array_validate(grid->cells, fviz_points_count(grid->points));
    if (result != FVIZ_OK) return result;
    result = fviz_data_set_validate(grid->data_set);
    if (result != FVIZ_OK) return result;
    if (fviz_data_set_point_count(grid->data_set) != fviz_points_count(grid->points) ||
        fviz_data_set_cell_count(grid->data_set) != fviz_cell_array_count(grid->cells))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "grid topology and dataset counts differ");
        return FVIZ_ERROR_INVALID_STATE;
    }
    return FVIZ_OK;
}

static FVizBool fviz_surface_definition(FVizCellType type, FVizSurfaceDefinition* definition)
{
    (void)memset(definition, 0, sizeof(*definition));
    definition->type = type;
    switch (type)
    {
        case FVIZ_CELL_TETRA:
            definition->face_count = 4u;
            definition->sizes[0] = 3u; definition->faces[0][0] = 0u; definition->faces[0][1] = 2u; definition->faces[0][2] = 1u;
            definition->sizes[1] = 3u; definition->faces[1][0] = 0u; definition->faces[1][1] = 1u; definition->faces[1][2] = 3u;
            definition->sizes[2] = 3u; definition->faces[2][0] = 1u; definition->faces[2][1] = 2u; definition->faces[2][2] = 3u;
            definition->sizes[3] = 3u; definition->faces[3][0] = 2u; definition->faces[3][1] = 0u; definition->faces[3][2] = 3u;
            return FVIZ_TRUE;
        case FVIZ_CELL_HEXAHEDRON:
            definition->face_count = 6u;
            definition->sizes[0] = 4u; definition->faces[0][0] = 0u; definition->faces[0][1] = 3u; definition->faces[0][2] = 2u; definition->faces[0][3] = 1u;
            definition->sizes[1] = 4u; definition->faces[1][0] = 4u; definition->faces[1][1] = 5u; definition->faces[1][2] = 6u; definition->faces[1][3] = 7u;
            definition->sizes[2] = 4u; definition->faces[2][0] = 0u; definition->faces[2][1] = 1u; definition->faces[2][2] = 5u; definition->faces[2][3] = 4u;
            definition->sizes[3] = 4u; definition->faces[3][0] = 1u; definition->faces[3][1] = 2u; definition->faces[3][2] = 6u; definition->faces[3][3] = 5u;
            definition->sizes[4] = 4u; definition->faces[4][0] = 2u; definition->faces[4][1] = 3u; definition->faces[4][2] = 7u; definition->faces[4][3] = 6u;
            definition->sizes[5] = 4u; definition->faces[5][0] = 3u; definition->faces[5][1] = 0u; definition->faces[5][2] = 4u; definition->faces[5][3] = 7u;
            return FVIZ_TRUE;
        case FVIZ_CELL_WEDGE:
            definition->face_count = 5u;
            definition->sizes[0] = 3u; definition->faces[0][0] = 0u; definition->faces[0][1] = 2u; definition->faces[0][2] = 1u;
            definition->sizes[1] = 3u; definition->faces[1][0] = 3u; definition->faces[1][1] = 4u; definition->faces[1][2] = 5u;
            definition->sizes[2] = 4u; definition->faces[2][0] = 0u; definition->faces[2][1] = 1u; definition->faces[2][2] = 4u; definition->faces[2][3] = 3u;
            definition->sizes[3] = 4u; definition->faces[3][0] = 1u; definition->faces[3][1] = 2u; definition->faces[3][2] = 5u; definition->faces[3][3] = 4u;
            definition->sizes[4] = 4u; definition->faces[4][0] = 2u; definition->faces[4][1] = 0u; definition->faces[4][2] = 3u; definition->faces[4][3] = 5u;
            return FVIZ_TRUE;
        case FVIZ_CELL_PYRAMID:
            definition->face_count = 5u;
            definition->sizes[0] = 4u; definition->faces[0][0] = 0u; definition->faces[0][1] = 3u; definition->faces[0][2] = 2u; definition->faces[0][3] = 1u;
            definition->sizes[1] = 3u; definition->faces[1][0] = 0u; definition->faces[1][1] = 1u; definition->faces[1][2] = 4u;
            definition->sizes[2] = 3u; definition->faces[2][0] = 1u; definition->faces[2][1] = 2u; definition->faces[2][2] = 4u;
            definition->sizes[3] = 3u; definition->faces[3][0] = 2u; definition->faces[3][1] = 3u; definition->faces[3][2] = 4u;
            definition->sizes[4] = 3u; definition->faces[4][0] = 3u; definition->faces[4][1] = 0u; definition->faces[4][2] = 4u;
            return FVIZ_TRUE;
        default:
            return FVIZ_FALSE;
    }
}

static void fviz_surface_sort(uint32_t* values, uint32_t count)
{
    uint32_t i;
    for (i = 1u; i < count; ++i)
    {
        uint32_t value = values[i];
        uint32_t j = i;
        while (j > 0u && values[j - 1u] > value)
        {
            values[j] = values[j - 1u];
            --j;
        }
        values[j] = value;
    }
}

static FVizResult fviz_surface_add_face(FVizArray* faces, const uint32_t* ids, uint32_t count)
{
    FVizSize i;
    FVizSurfaceFace face;
    if (count > 4u) return FVIZ_ERROR_NOT_SUPPORTED;
    (void)memset(&face, 0, sizeof(face));
    face.count = count;
    face.occurrences = 1u;
    for (i = 0u; i < count; ++i) face.ids[i] = ids[i], face.sorted[i] = ids[i];
    fviz_surface_sort(face.sorted, count);
    for (i = 0u; i < fviz_array_count(faces); ++i)
    {
        FVizSurfaceFace* existing = (FVizSurfaceFace*)fviz_array_at(faces, i);
        if (existing->count == face.count && memcmp(existing->sorted, face.sorted, count * sizeof(uint32_t)) == 0)
        {
            existing->occurrences += 1u;
            return FVIZ_OK;
        }
    }
    return fviz_array_push(faces, &face);
}

static FVizBool fviz_scalar_value(const FVizDataArray* array, FVizSize index, double* out_value)
{
    const void* tuple;
    if (array == NULL || out_value == NULL || fviz_data_array_components(array) != 1u) return FVIZ_FALSE;
    tuple = fviz_data_array_const_tuple(array, index);
    if (tuple == NULL) return FVIZ_FALSE;
    switch (fviz_data_array_type(array))
    {
        case FVIZ_DATA_INT8: *out_value = *(const int8_t*)tuple; return FVIZ_TRUE;
        case FVIZ_DATA_UINT8: *out_value = *(const uint8_t*)tuple; return FVIZ_TRUE;
        case FVIZ_DATA_INT16: *out_value = *(const int16_t*)tuple; return FVIZ_TRUE;
        case FVIZ_DATA_UINT16: *out_value = *(const uint16_t*)tuple; return FVIZ_TRUE;
        case FVIZ_DATA_INT32: *out_value = *(const int32_t*)tuple; return FVIZ_TRUE;
        case FVIZ_DATA_UINT32: *out_value = *(const uint32_t*)tuple; return FVIZ_TRUE;
        case FVIZ_DATA_INT64: *out_value = (double)*(const int64_t*)tuple; return FVIZ_TRUE;
        case FVIZ_DATA_UINT64: *out_value = (double)*(const uint64_t*)tuple; return FVIZ_TRUE;
        case FVIZ_DATA_FLOAT32: *out_value = *(const float*)tuple; return FVIZ_TRUE;
        case FVIZ_DATA_FLOAT64: *out_value = *(const double*)tuple; return FVIZ_TRUE;
        default: return FVIZ_FALSE;
    }
}

FVizResult fviz_unstructured_grid_threshold_cells(
    const FVizUnstructuredGrid* grid,
    const char* scalar_name,
    double minimum,
    double maximum,
    FVizUnstructuredGrid** out_grid)
{
    const FVizDataArray* scalar;
    FVizUnstructuredGrid* result = NULL;
    FVizSize i;
    const FVizVec3* points;
    if (grid == NULL || scalar_name == NULL || scalar_name[0] == '\0' || out_grid == NULL || minimum > maximum)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "threshold requires grid, scalar name, range and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    scalar = fviz_attribute_set_const_get(fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid), scalar_name);
    if (scalar == NULL || fviz_data_array_components(scalar) != 1u ||
        fviz_data_array_tuple_count(scalar) != fviz_unstructured_grid_cell_count(grid))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "threshold scalar must be a one-component cell array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_unstructured_grid_create(&result) != FVIZ_OK) return fviz_last_error_code();
    points = fviz_points_data(grid->points);
    for (i = 0u; i < fviz_points_count(grid->points); ++i)
        if (fviz_unstructured_grid_add_point(result, points[i], NULL) != FVIZ_OK) goto fail;
    for (i = 0u; i < fviz_cell_array_count(grid->cells); ++i)
    {
        double value;
        if (!fviz_scalar_value(scalar, i, &value))
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "threshold scalar type is unsupported");
            goto fail;
        }
        if (value >= minimum && value <= maximum &&
            fviz_unstructured_grid_add_cell(result, fviz_cell_array_type(grid->cells, i),
                fviz_cell_array_point_count(grid->cells, i), fviz_cell_array_point_ids(grid->cells, i)) != FVIZ_OK)
            goto fail;
    }
    {
        FVizDataArray* output_scalar = NULL;
        if (fviz_data_array_create(fviz_data_array_type(scalar), 1u, &output_scalar) != FVIZ_OK) goto fail;
        for (i = 0u; i < fviz_data_array_tuple_count(scalar); ++i)
        {
            double value;
            if (!fviz_scalar_value(scalar, i, &value) || (value >= minimum && value <= maximum &&
                fviz_data_array_append_tuple(output_scalar, fviz_data_array_const_tuple(scalar, i)) != FVIZ_OK))
            {
                fviz_release(output_scalar);
                fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "failed to copy threshold scalar");
                goto fail;
            }
        }
        if (fviz_attribute_set_add(fviz_unstructured_grid_cell_data(result), scalar_name, output_scalar) != FVIZ_OK)
        {
            fviz_release(output_scalar);
            goto fail;
        }
        fviz_release(output_scalar);
    }
    *out_grid = result;
    return FVIZ_OK;
fail:
    fviz_release(result);
    return fviz_last_error_code();
}

FVizResult fviz_unstructured_grid_extract_surface(const FVizUnstructuredGrid* grid, FVizPolyData** out_surface)
{
    FVizArray* faces = NULL;
    FVizPolyData* surface = NULL;
    FVizSize cell_id;
    FVizSize i;
    const FVizVec3* points;
    if (out_surface == NULL || grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid and out_surface must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_surface = NULL;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizSurfaceFace), &faces) != FVIZ_OK || fviz_poly_data_create(&surface) != FVIZ_OK)
    {
        fviz_release(faces);
        fviz_release(surface);
        return fviz_last_error_code();
    }
    points = fviz_points_data(grid->points);
    if (fviz_poly_data_reserve(surface, fviz_points_count(grid->points), 0u) != FVIZ_OK)
        goto fail;
    for (i = 0u; i < fviz_points_count(grid->points); ++i)
        if (fviz_poly_data_add_point(surface, points[i], NULL) != FVIZ_OK) goto fail;
    for (cell_id = 0u; cell_id < fviz_cell_array_count(grid->cells); ++cell_id)
    {
        FVizSurfaceDefinition definition;
        const uint32_t* cell_ids = fviz_cell_array_point_ids(grid->cells, cell_id);
        uint32_t face_id;
        if (!fviz_surface_definition(fviz_cell_array_type(grid->cells, cell_id), &definition))
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "surface extraction requires a supported volume cell");
            goto fail;
        }
        for (face_id = 0u; face_id < definition.face_count; ++face_id)
        {
            uint32_t ids[4];
            uint32_t j;
            for (j = 0u; j < definition.sizes[face_id]; ++j) ids[j] = cell_ids[definition.faces[face_id][j]];
            if (fviz_surface_add_face(faces, ids, definition.sizes[face_id]) != FVIZ_OK) goto fail;
        }
    }
    for (i = 0u; i < fviz_array_count(faces); ++i)
    {
        const FVizSurfaceFace* face = (const FVizSurfaceFace*)fviz_array_const_at(faces, i);
        if (face->occurrences == 1u)
        {
            if (face->count == 3u)
            {
                if (fviz_poly_data_add_triangle(surface, face->ids[0], face->ids[1], face->ids[2]) != FVIZ_OK) goto fail;
            }
            else if (face->count == 4u)
            {
                if (fviz_poly_data_add_triangle(surface, face->ids[0], face->ids[1], face->ids[2]) != FVIZ_OK ||
                    fviz_poly_data_add_triangle(surface, face->ids[0], face->ids[2], face->ids[3]) != FVIZ_OK) goto fail;
            }
        }
    }
    fviz_release(faces);
    *out_surface = surface;
    return FVIZ_OK;
fail:
    fviz_release(faces);
    fviz_release(surface);
    return fviz_last_error_code();
}
