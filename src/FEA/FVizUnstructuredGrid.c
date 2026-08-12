#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>
#include <FViz/Parallel/FVizParallel.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizArray.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/FEA/FVizUnstructuredGridPrivate.h>
#include <FViz/FEA/FVizSurfaceFacePrivate.h>
#include <FViz/Mesh/FVizPointsPrivate.h>

typedef struct FVizSurfaceDefinition
{
    FVizCellType type;
    uint32_t face_count;
    uint32_t faces[6][4];
    uint32_t sizes[6];
} FVizSurfaceDefinition;

static void fviz_unstructured_grid_destroy(FVizObject* object);
static FVizMTime fviz_unstructured_grid_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_unstructured_grid_class = {
    FVIZ_TYPE_UNSTRUCTURED_GRID, "FVizUnstructuredGrid", &g_fviz_data_object_class,
    fviz_unstructured_grid_destroy, fviz_unstructured_grid_mtime
};

static FVizMTime fviz_unstructured_grid_mtime(const FVizObject* object)
{
    const FVizUnstructuredGrid* grid = (const FVizUnstructuredGrid*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    const FVizMTime points = fviz_object_mtime((const FVizObject*)grid->points);
    const FVizMTime cells = fviz_object_mtime((const FVizObject*)grid->cells);
    const FVizMTime data_set = fviz_object_mtime((const FVizObject*)grid->data_set);
    if (points > mtime) mtime = points;
    if (cells > mtime) mtime = cells;
    if (data_set > mtime) mtime = data_set;
    return mtime;
}

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
    fviz_object_modified((FVizObject*)grid);
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
    if (result == FVIZ_OK)
    {
        fviz_object_modified((FVizObject*)grid);
    }
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
    if (result == FVIZ_OK)
    {
        fviz_object_modified((FVizObject*)grid);
    }
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

static FVizBool fviz_component_value(const FVizDataArray* array, FVizSize index, uint32_t component, double* out_value)
{
    const void* tuple;
    if (array == NULL || out_value == NULL || component >= fviz_data_array_components(array)) return FVIZ_FALSE;
    tuple = fviz_data_array_const_tuple(array, index);
    if (tuple == NULL) return FVIZ_FALSE;
    switch (fviz_data_array_type(array))
    {
        case FVIZ_DATA_INT8: *out_value = ((const int8_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_UINT8: *out_value = ((const uint8_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_INT16: *out_value = ((const int16_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_UINT16: *out_value = ((const uint16_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_INT32: *out_value = ((const int32_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_UINT32: *out_value = ((const uint32_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_INT64: *out_value = (double)((const int64_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_UINT64: *out_value = (double)((const uint64_t*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_FLOAT32: *out_value = ((const float*)tuple)[component]; return FVIZ_TRUE;
        case FVIZ_DATA_FLOAT64: *out_value = ((const double*)tuple)[component]; return FVIZ_TRUE;
        default: return FVIZ_FALSE;
    }
}

static FVizBool fviz_scalar_value(const FVizDataArray* array, FVizSize index, double* out_value)
{
    return fviz_component_value(array, index, 0u, out_value);
}

static FVizResult fviz_copy_attribute_set(FVizAttributeSet* source, FVizAttributeSet* destination)
{
    FVizSize i;
    if (source == NULL) return FVIZ_OK;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        FVizDataArray* array = fviz_attribute_set_array_at(source, i);
        if (name == NULL || array == NULL) continue;
        if (fviz_attribute_set_add(destination, name, array) != FVIZ_OK) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_clone_topology(const FVizUnstructuredGrid* grid, FVizUnstructuredGrid** out_result)
{
    FVizUnstructuredGrid* result = NULL;
    const FVizVec3* points;
    FVizSize i;
    if (fviz_unstructured_grid_create(&result) != FVIZ_OK) return fviz_last_error_code();
    points = fviz_points_data(grid->points);
    for (i = 0u; i < fviz_points_count(grid->points); ++i)
    {
        if (fviz_unstructured_grid_add_point(result, points[i], NULL) != FVIZ_OK) goto fail;
    }
    for (i = 0u; i < fviz_cell_array_count(grid->cells); ++i)
    {
        if (fviz_unstructured_grid_add_cell(result, fviz_cell_array_type(grid->cells, i),
                fviz_cell_array_point_count(grid->cells, i), fviz_cell_array_point_ids(grid->cells, i)) != FVIZ_OK)
        {
            goto fail;
        }
    }
    if (fviz_copy_attribute_set(fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_point_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_cell_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_field_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_field_data(result)) != FVIZ_OK)
    {
        goto fail;
    }
    *out_result = result;
    return FVIZ_OK;
fail:
    fviz_release(result);
    return fviz_last_error_code();
}

FVizResult fviz_unstructured_grid_transform(
    const FVizUnstructuredGrid* grid,
    const FVizTransform* transform,
    FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* result = NULL;
    FVizSize i;
    if (grid == NULL || transform == NULL || out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid, transform and out_grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    if (fviz_clone_topology(grid, &result) != FVIZ_OK) return fviz_last_error_code();
    result->points->bounds = fviz_bounds_empty();
    for (i = 0u; i < fviz_points_count(result->points); ++i)
    {
        FVizVec3* point = (FVizVec3*)fviz_array_data(result->points->data);
        point[i] = fviz_transform_point(transform, point[i]);
        fviz_bounds_include_point(&result->points->bounds, point[i]);
    }
    fviz_object_modified((FVizObject*)result->points->data);
    *out_grid = result;
    return FVIZ_OK;
}

typedef struct FVizWarpRangeContext
{
    const FVizVec3* points;
    const FVizDataArray* vectors;
    FVizVec3* displaced;
    double scale;
} FVizWarpRangeContext;

static void fviz_warp_point_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizWarpRangeContext* context = (FVizWarpRangeContext*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i)
    {
        double vx = 0.0;
        double vy = 0.0;
        double vz = 0.0;
        (void)fviz_component_value(context->vectors, i, 0u, &vx);
        (void)fviz_component_value(context->vectors, i, 1u, &vy);
        (void)fviz_component_value(context->vectors, i, 2u, &vz);
        context->displaced[i] = fviz_vec3(
            context->points[i].x + (float)(vx * context->scale),
            context->points[i].y + (float)(vy * context->scale),
            context->points[i].z + (float)(vz * context->scale));
    }
}

FVizResult fviz_unstructured_grid_warp_by_vector(
    const FVizUnstructuredGrid* grid,
    const char* vector_name,
    double scale,
    FVizUnstructuredGrid** out_grid)
{
    const FVizDataArray* vectors;
    const FVizVec3* points;
    FVizVec3* displaced_points = NULL;
    FVizUnstructuredGrid* result = NULL;
    FVizWarpRangeContext context;
    const FVizSize point_count = grid != NULL ? fviz_points_count(grid->points) : 0u;
    FVizSize i;
    if (grid == NULL || vector_name == NULL || vector_name[0] == '\0' || out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "warp requires grid, vector name and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    vectors = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid), vector_name);
    if (vectors == NULL || fviz_data_array_components(vectors) != 3u ||
        fviz_data_array_tuple_count(vectors) != fviz_points_count(grid->points))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "warp vector must be a three-component point array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_unstructured_grid_create(&result) != FVIZ_OK) return fviz_last_error_code();
    points = fviz_points_data(grid->points);
    if (point_count > 0u)
    {
        displaced_points = (FVizVec3*)fviz_alloc(point_count * sizeof(FVizVec3));
        if (displaced_points == NULL)
        {
            fviz_release(result);
            return fviz_last_error_code();
        }
        context.points = points;
        context.vectors = vectors;
        context.displaced = displaced_points;
        context.scale = scale;
        if (fviz_parallel_for(0u, point_count, 256u, fviz_warp_point_range, &context) != FVIZ_OK)
        {
            fviz_free(displaced_points);
            fviz_release(result);
            return fviz_last_error_code();
        }
    }
    for (i = 0u; i < point_count; ++i)
    {
        if (fviz_unstructured_grid_add_point(result, displaced_points[i], NULL) != FVIZ_OK)
        {
            fviz_free(displaced_points);
            fviz_release(result);
            return fviz_last_error_code();
        }
    }
    fviz_free(displaced_points);
    for (i = 0u; i < fviz_cell_array_count(grid->cells); ++i)
    {
        if (fviz_unstructured_grid_add_cell(result, fviz_cell_array_type(grid->cells, i),
                fviz_cell_array_point_count(grid->cells, i), fviz_cell_array_point_ids(grid->cells, i)) != FVIZ_OK)
        {
            fviz_release(result);
            return fviz_last_error_code();
        }
    }
    if (fviz_copy_attribute_set(fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_point_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_cell_data(result)) != FVIZ_OK ||
        fviz_copy_attribute_set(fviz_unstructured_grid_field_data((FVizUnstructuredGrid*)grid),
            fviz_unstructured_grid_field_data(result)) != FVIZ_OK)
    {
        fviz_release(result);
        return fviz_last_error_code();
    }
    *out_grid = result;
    return FVIZ_OK;
}

FVizResult fviz_unstructured_grid_cell_data_to_point_data(
    const FVizUnstructuredGrid* grid,
    FVizUnstructuredGrid** out_grid)
{
    FVizUnstructuredGrid* result = NULL;
    FVizAttributeSet* cell_data;
    FVizSize i;
    if (grid == NULL || out_grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid and out_grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_grid = NULL;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_clone_topology(grid, &result) != FVIZ_OK) return fviz_last_error_code();
    cell_data = fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid);
    for (i = 0u; i < fviz_attribute_set_count(cell_data); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(cell_data, i);
        const char* name = fviz_attribute_set_name_at(cell_data, i);
        FVizDataArray* output;
        double* sums;
        uint32_t* counts;
        FVizSize point_count;
        FVizSize j;
        if (array == NULL || name == NULL || fviz_data_array_components(array) != 1u ||
            fviz_data_array_tuple_count(array) != fviz_cell_array_count(grid->cells))
        {
            continue;
        }
        point_count = fviz_points_count(grid->points);
        sums = (double*)fviz_alloc(point_count * sizeof(double));
        counts = (uint32_t*)fviz_alloc(point_count * sizeof(uint32_t));
        if (sums == NULL || counts == NULL)
        {
            fviz_free(sums);
            fviz_free(counts);
            fviz_release(result);
            return fviz_last_error_code();
        }
        for (j = 0u; j < point_count; ++j)
        {
            sums[j] = 0.0;
            counts[j] = 0u;
        }
        for (j = 0u; j < fviz_cell_array_count(grid->cells); ++j)
        {
            double value;
            FVizSize k;
            if (!fviz_scalar_value(array, j, &value))
            {
                fviz_free(sums);
                fviz_free(counts);
                fviz_release(result);
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell scalar type is unsupported");
                return FVIZ_ERROR_INVALID_ARGUMENT;
            }
            for (k = 0u; k < fviz_cell_array_point_count(grid->cells, j); ++k)
            {
                const uint32_t point_id = fviz_cell_array_point_ids(grid->cells, j)[k];
                sums[point_id] += value;
                counts[point_id] += 1u;
            }
        }
        if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &output) != FVIZ_OK)
        {
            fviz_free(sums);
            fviz_free(counts);
            fviz_release(result);
            return fviz_last_error_code();
        }
        for (j = 0u; j < point_count; ++j)
        {
            const float value = counts[j] != 0u ? (float)(sums[j] / (double)counts[j]) : 0.0f;
            if (fviz_data_array_append_tuple(output, &value) != FVIZ_OK)
            {
                fviz_release(output);
                fviz_free(sums);
                fviz_free(counts);
                fviz_release(result);
                return fviz_last_error_code();
            }
        }
        fviz_free(sums);
        fviz_free(counts);
        if (fviz_attribute_set_add(fviz_unstructured_grid_point_data(result), name, output) != FVIZ_OK)
        {
            fviz_release(output);
            fviz_release(result);
            return fviz_last_error_code();
        }
        fviz_release(output);
    }
    *out_grid = result;
    return FVIZ_OK;
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

static FVizResult fviz_transfer_point_scalars(const FVizUnstructuredGrid* grid, FVizPolyData* surface)
{
    FVizAttributeSet* source = fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid);
    FVizDataArray* first_active = NULL;
    FVizSize i;
    if (source == NULL) return FVIZ_OK;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(source, i);
        const char* name = fviz_attribute_set_name_at(source, i);
        FVizDataArray* output = NULL;
        FVizSize j;
        if (array == NULL || name == NULL || fviz_data_array_components(array) != 1u ||
            fviz_data_array_tuple_count(array) != fviz_points_count(grid->points))
        {
            continue;
        }
        if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &output) != FVIZ_OK) return fviz_last_error_code();
        for (j = 0u; j < fviz_data_array_tuple_count(array); ++j)
        {
            double value;
            float output_value;
            if (!fviz_scalar_value(array, j, &value))
            {
                fviz_release(output);
                fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "point scalar type is unsupported");
                return FVIZ_ERROR_INTERNAL;
            }
            output_value = (float)value;
            if (fviz_data_array_append_tuple(output, &output_value) != FVIZ_OK)
            {
                fviz_release(output);
                return fviz_last_error_code();
            }
        }
        if (fviz_attribute_set_add(fviz_poly_data_point_data(surface), name, output) != FVIZ_OK)
        {
            fviz_release(output);
            return fviz_last_error_code();
        }
        fviz_release(output);
        if (first_active == NULL) first_active = fviz_attribute_set_get(fviz_poly_data_point_data(surface), name);
    }
    if (first_active != NULL)
    {
        return fviz_poly_data_set_scalars(surface, first_active);
    }
    return FVIZ_OK;
}

static FVizResult fviz_extract_surface_internal(
    const FVizUnstructuredGrid* grid,
    FVizPolyData** out_surface,
    FVizBool with_scalars)
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
    if (with_scalars == FVIZ_TRUE && fviz_transfer_point_scalars(grid, surface) != FVIZ_OK) goto fail;
    fviz_release(faces);
    *out_surface = surface;
    return FVIZ_OK;
fail:
    fviz_release(faces);
    fviz_release(surface);
    return fviz_last_error_code();
}

FVizResult fviz_unstructured_grid_extract_surface(const FVizUnstructuredGrid* grid, FVizPolyData** out_surface)
{
    return fviz_extract_surface_internal(grid, out_surface, FVIZ_FALSE);
}

FVizResult fviz_unstructured_grid_extract_surface_scalars(const FVizUnstructuredGrid* grid, FVizPolyData** out_surface)
{
    return fviz_extract_surface_internal(grid, out_surface, FVIZ_TRUE);
}

#define FVIZ_SLICE_EPSILON 1.0e-6f
#define FVIZ_SLICE_MAX_VERTICES 16u

typedef struct FVizSliceField
{
    const FVizDataArray* array;
    char name[128];
} FVizSliceField;

static const uint32_t g_fviz_edges_tetra[6][2] = {
    {0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}
};
static const uint32_t g_fviz_edges_hex[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};
static const uint32_t g_fviz_edges_wedge[9][2] = {
    {0, 1}, {1, 2}, {2, 0},
    {3, 4}, {4, 5}, {5, 3},
    {0, 3}, {1, 4}, {2, 5}
};
static const uint32_t g_fviz_edges_pyramid[8][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {0, 4}, {1, 4}, {2, 4}, {3, 4}
};

static const uint32_t* fviz_cell_edge_table(FVizCellType type, uint32_t* out_edge_count)
{
    switch (type)
    {
        case FVIZ_CELL_TETRA: *out_edge_count = 6u; return &g_fviz_edges_tetra[0][0];
        case FVIZ_CELL_HEXAHEDRON: *out_edge_count = 12u; return &g_fviz_edges_hex[0][0];
        case FVIZ_CELL_WEDGE: *out_edge_count = 9u; return &g_fviz_edges_wedge[0][0];
        case FVIZ_CELL_PYRAMID: *out_edge_count = 8u; return &g_fviz_edges_pyramid[0][0];
        default: *out_edge_count = 0u; return NULL;
    }
}

static void fviz_slice_sort_angles(const float* angles, FVizSize* order, FVizSize count)
{
    FVizSize i;
    for (i = 1u; i < count; ++i)
    {
        const float angle = angles[order[i]];
        const FVizSize value = order[i];
        FVizSize j = i;
        while (j > 0u && angles[order[j - 1u]] > angle)
        {
            order[j] = order[j - 1u];
            --j;
        }
        order[j] = value;
    }
}

FVizResult fviz_unstructured_grid_slice(
    const FVizUnstructuredGrid* grid,
    FVizPlane plane,
    FVizPolyData** out_slice)
{
    FVizArray* fields = NULL;
    FVizDataArray** outputs = NULL;
    FVizPolyData* slice = NULL;
    const FVizVec3* points;
    FVizVec3 u_basis;
    FVizVec3 v_basis;
    FVizSize field_count = 0u;
    float* polygon_values = NULL;
    FVizSize cell_id;
    FVizSize field_id;

    if (grid == NULL || out_slice == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "grid and out_slice must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_slice = NULL;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_poly_data_create(&slice) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizSliceField), &fields) != FVIZ_OK) goto fail;
    points = fviz_points_data(grid->points);

    {
        FVizAttributeSet* point_data = fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid);
        for (field_id = 0u; field_id < fviz_attribute_set_count(point_data); ++field_id)
        {
            const FVizDataArray* array = fviz_attribute_set_const_array_at(point_data, field_id);
            const char* name = fviz_attribute_set_name_at(point_data, field_id);
            FVizSliceField field;
            if (array == NULL || name == NULL || fviz_data_array_components(array) != 1u ||
                fviz_data_array_tuple_count(array) != fviz_points_count(grid->points))
            {
                continue;
            }
            field.array = array;
            (void)strncpy(field.name, name, sizeof(field.name) - 1u);
            field.name[sizeof(field.name) - 1u] = '\0';
            if (fviz_array_push(fields, &field) != FVIZ_OK) goto fail;
        }
    }
    field_count = fviz_array_count(fields);
    if (field_count > 0u)
    {
        outputs = (FVizDataArray**)fviz_alloc(field_count * sizeof(FVizDataArray*));
        if (outputs == NULL) goto fail;
        for (field_id = 0u; field_id < field_count; ++field_id)
        {
            outputs[field_id] = NULL;
            if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &outputs[field_id]) != FVIZ_OK) goto fail;
        }
    }

    {
        FVizVec3 up = fviz_vec3(0.0f, 1.0f, 0.0f);
        if (fabsf(plane.normal.y) > 0.9f) up = fviz_vec3(1.0f, 0.0f, 0.0f);
        u_basis = fviz_vec3_normalize(fviz_vec3_cross(plane.normal, up));
        v_basis = fviz_vec3_cross(plane.normal, u_basis);
    }

    for (cell_id = 0u; cell_id < fviz_cell_array_count(grid->cells); ++cell_id)
    {
        const FVizCellType type = fviz_cell_array_type(grid->cells, cell_id);
        const uint32_t* cell_ids = fviz_cell_array_point_ids(grid->cells, cell_id);
        const FVizSize cell_point_count = fviz_cell_array_point_count(grid->cells, cell_id);
        uint32_t edge_count;
        const uint32_t* edges = fviz_cell_edge_table(type, &edge_count);
        float distances[8];
        FVizVec3 polygon_positions[FVIZ_SLICE_MAX_VERTICES];
        FVizSize polygon_count = 0u;
        FVizSize k;
        if (edges == NULL || cell_point_count > 8u) continue;

        for (k = 0u; k < cell_point_count; ++k)
        {
            distances[k] = fviz_plane_distance_to_point(plane, points[cell_ids[k]]);
        }
        {
            FVizBool has_nonnegative = FVIZ_FALSE;
            FVizBool has_nonpositive = FVIZ_FALSE;
            FVizBool all_on_plane = FVIZ_TRUE;
            for (k = 0u; k < cell_point_count; ++k)
            {
                if (distances[k] >= -FVIZ_SLICE_EPSILON) has_nonnegative = FVIZ_TRUE;
                if (distances[k] <= FVIZ_SLICE_EPSILON) has_nonpositive = FVIZ_TRUE;
                if (distances[k] > FVIZ_SLICE_EPSILON || distances[k] < -FVIZ_SLICE_EPSILON)
                {
                    all_on_plane = FVIZ_FALSE;
                }
            }
            if (all_on_plane == FVIZ_TRUE || has_nonnegative == FVIZ_FALSE || has_nonpositive == FVIZ_FALSE)
            {
                continue;
            }
        }

        if (field_count > 0u)
        {
            polygon_values = (float*)fviz_alloc(FVIZ_SLICE_MAX_VERTICES * field_count * sizeof(float));
            if (polygon_values == NULL) goto fail;
        }

        for (k = 0u; k < cell_point_count; ++k)
        {
            if (fabsf(distances[k]) <= FVIZ_SLICE_EPSILON)
            {
                FVizSize f;
                if (polygon_count >= FVIZ_SLICE_MAX_VERTICES) goto free_values_fail;
                polygon_positions[polygon_count] = points[cell_ids[k]];
                for (f = 0u; f < field_count; ++f)
                {
                    const FVizSliceField* field = (const FVizSliceField*)fviz_array_const_at(fields, f);
                    double value = 0.0;
                    (void)fviz_scalar_value(field->array, cell_ids[k], &value);
                    polygon_values[polygon_count * field_count + f] = (float)value;
                }
                ++polygon_count;
            }
        }
        for (k = 0u; k < edge_count; ++k)
        {
            const uint32_t a = cell_ids[edges[k * 2u + 0u]];
            const uint32_t b = cell_ids[edges[k * 2u + 1u]];
            const float da = distances[edges[k * 2u + 0u]];
            const float db = distances[edges[k * 2u + 1u]];
            const float t = da / (da - db);
            FVizSize f;
            if ((da >= 0.0f && db >= 0.0f) || (da <= 0.0f && db <= 0.0f)) continue;
            if (polygon_count >= FVIZ_SLICE_MAX_VERTICES) goto free_values_fail;
            polygon_positions[polygon_count] = fviz_vec3_add(points[a], fviz_vec3_scale(fviz_vec3_sub(points[b], points[a]), t));
            for (f = 0u; f < field_count; ++f)
            {
                const FVizSliceField* field = (const FVizSliceField*)fviz_array_const_at(fields, f);
                double va = 0.0;
                double vb = 0.0;
                (void)fviz_scalar_value(field->array, a, &va);
                (void)fviz_scalar_value(field->array, b, &vb);
                polygon_values[polygon_count * field_count + f] = (float)(va + (vb - va) * t);
            }
            ++polygon_count;
        }

        if (polygon_count >= 3u)
        {
            FVizVec3 centroid = fviz_vec3(0.0f, 0.0f, 0.0f);
            float angles[FVIZ_SLICE_MAX_VERTICES];
            FVizSize order[FVIZ_SLICE_MAX_VERTICES];
            uint32_t new_ids[FVIZ_SLICE_MAX_VERTICES];
            FVizSize i;
            for (i = 0u; i < polygon_count; ++i)
            {
                centroid = fviz_vec3_add(centroid, polygon_positions[i]);
            }
            centroid = fviz_vec3_scale(centroid, 1.0f / (float)polygon_count);
            for (i = 0u; i < polygon_count; ++i)
            {
                const FVizVec3 rel = fviz_vec3_sub(polygon_positions[i], centroid);
                angles[i] = atan2f(fviz_vec3_dot(v_basis, rel), fviz_vec3_dot(u_basis, rel));
                order[i] = i;
            }
            fviz_slice_sort_angles(angles, order, polygon_count);
            for (i = 0u; i < polygon_count; ++i)
            {
                if (fviz_poly_data_add_point(slice, polygon_positions[order[i]], &new_ids[i]) != FVIZ_OK)
                {
                    goto free_values_fail;
                }
            }
            for (i = 1u; i + 1u < polygon_count; ++i)
            {
                if (fviz_poly_data_add_triangle(slice, new_ids[0], new_ids[i], new_ids[i + 1u]) != FVIZ_OK)
                {
                    goto free_values_fail;
                }
            }
            for (field_id = 0u; field_id < field_count; ++field_id)
            {
                FVizSize v;
                for (v = 0u; v < polygon_count; ++v)
                {
                    const float value = polygon_values[order[v] * field_count + field_id];
                    if (fviz_data_array_append_tuple(outputs[field_id], &value) != FVIZ_OK) goto free_values_fail;
                }
            }
        }
        if (polygon_values != NULL)
        {
            fviz_free(polygon_values);
            polygon_values = NULL;
        }
    }

    for (field_id = 0u; field_id < field_count; ++field_id)
    {
        const FVizSliceField* field = (const FVizSliceField*)fviz_array_const_at(fields, field_id);
        if (fviz_attribute_set_add(fviz_poly_data_point_data(slice), field->name, outputs[field_id]) != FVIZ_OK)
        {
            goto fail;
        }
    }
    if (field_count > 0u)
    {
        const FVizSliceField* first = (const FVizSliceField*)fviz_array_const_at(fields, 0u);
        FVizDataArray* active = fviz_attribute_set_get(fviz_poly_data_point_data(slice), first->name);
        if (fviz_poly_data_set_scalars(slice, active) != FVIZ_OK) goto fail;
    }

    if (fviz_poly_data_compute_normals(slice) != FVIZ_OK) goto fail;
    fviz_release(fields);
    for (field_id = 0u; field_id < field_count; ++field_id)
    {
        fviz_release(outputs[field_id]);
    }
    fviz_free(outputs);
    *out_slice = slice;
    return FVIZ_OK;

free_values_fail:
    if (polygon_values != NULL)
    {
        fviz_free(polygon_values);
        polygon_values = NULL;
    }
fail:
    if (outputs != NULL)
    {
        for (field_id = 0u; field_id < field_count; ++field_id)
        {
            fviz_release(outputs[field_id]);
        }
        fviz_free(outputs);
    }
    fviz_release(fields);
    fviz_release(slice);
    return fviz_last_error_code();
}
