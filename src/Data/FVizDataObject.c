#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataObject.h>
#include <FViz/Data/FVizImageData.h>
#include <FViz/Data/FVizMultiBlockDataSet.h>
#include <FViz/Data/FVizPartitionedDataSet.h>
#include <FViz/Data/FVizRectilinearGrid.h>
#include <FViz/Data/FVizStructuredGrid.h>
#include <FViz/Data/FVizTemporalDataSet.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Mesh/FVizPolyData.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizAttributeSetPrivate.h>
#include <FViz/Data/FVizDataArrayPrivate.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/Data/FVizDataSetPrivate.h>
#include <FViz/Data/FVizImageDataPrivate.h>
#include <FViz/Data/FVizMultiBlockDataSetPrivate.h>
#include <FViz/Data/FVizPartitionedDataSetPrivate.h>
#include <FViz/Data/FVizRectilinearGridPrivate.h>
#include <FViz/Data/FVizStructuredGridPrivate.h>
#include <FViz/Data/FVizTemporalDataSetPrivate.h>
#include <FViz/Data/FVizUnstructuredGridPrivate.h>
#include <FViz/Mesh/FVizCellArrayPrivate.h>
#include <FViz/Mesh/FVizPointsPrivate.h>
#include <FViz/Mesh/FVizPolyDataPrivate.h>

const FVizObjectClass g_fviz_data_object_class = {
    FVIZ_TYPE_DATA_OBJECT,
    "FVizDataObject",
    &g_fviz_object_class,
    NULL,
    NULL
};

FVizBool fviz_data_object_is_data_object(const FVizDataObject* data_object)
{
    return fviz_object_is_type((const FVizObject*)data_object, FVIZ_TYPE_DATA_OBJECT);
}

typedef struct FVizMemoryWalk
{
    FVizHashMap* visited;
    FVizDataObjectMemoryInfo info;
} FVizMemoryWalk;

static FVizResult fviz_memory_add(FVizSize* value, FVizSize add)
{
    if (value == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (add > (FVizSize)-1 - *value)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "data-object memory estimate overflowed FVizSize");
        return FVIZ_ERROR_OVERFLOW;
    }
    *value += add;
    return FVIZ_OK;
}

static FVizResult fviz_memory_add_total(FVizMemoryWalk* walk, FVizSize* category, FVizSize add)
{
    if (fviz_memory_add(category, add) != FVIZ_OK ||
        fviz_memory_add(&walk->info.total_bytes, add) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    return FVIZ_OK;
}

static FVizResult fviz_memory_mark(
    FVizMemoryWalk* walk, const void* pointer, FVizBool* out_is_new)
{
    FVizResult result;
    const FVizId key = (FVizId)(uintptr_t)pointer;
    if (out_is_new != NULL) *out_is_new = FVIZ_FALSE;
    if (pointer == NULL || walk == NULL || walk->visited == NULL || out_is_new == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid data-object memory-walk state");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_hash_map_contains(walk->visited, key) != FVIZ_FALSE) return FVIZ_OK;
    result = fviz_hash_map_set(walk->visited, key, (void*)pointer);
    if (result != FVIZ_OK) return result;
    *out_is_new = FVIZ_TRUE;
    return FVIZ_OK;
}

#define FVIZ_MEMORY_MARK_OR_RETURN(Walk, Pointer) \
    do { \
        FVizBool fviz_memory_is_new_ = FVIZ_FALSE; \
        FVizResult fviz_memory_mark_result_ = fviz_memory_mark((Walk), (Pointer), &fviz_memory_is_new_); \
        if (fviz_memory_mark_result_ != FVIZ_OK) return fviz_memory_mark_result_; \
        if (fviz_memory_is_new_ == FVIZ_FALSE) return FVIZ_OK; \
    } while (0)

static FVizResult fviz_memory_data_array(FVizMemoryWalk* walk, const FVizDataArray* array)
{
    FVizSize bytes;
    if (array == NULL) return FVIZ_OK;
    FVIZ_MEMORY_MARK_OR_RETURN(walk, array);
    if (fviz_size_multiply(fviz_data_array_tuple_count(array), fviz_data_array_tuple_stride(array), &bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*array)) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.attribute_bytes, bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    return FVIZ_OK;
}

static FVizResult fviz_memory_attribute_set(FVizMemoryWalk* walk, const FVizAttributeSet* set)
{
    FVizSize i;
    if (set == NULL) return FVIZ_OK;
    FVIZ_MEMORY_MARK_OR_RETURN(walk, set);
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*set)) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    for (i = 0u; i < fviz_attribute_set_count(set); ++i)
    {
        const char* name = fviz_attribute_set_name_at(set, i);
        const FVizDataArray* array = fviz_attribute_set_const_array_at(set, i);
        if (name != NULL)
        {
            const FVizSize length = (FVizSize)strlen(name) + 1u;
            if (fviz_memory_add_total(walk, &walk->info.attribute_bytes, length) != FVIZ_OK)
                return FVIZ_ERROR_OVERFLOW;
        }
        if (fviz_memory_data_array(walk, array) != FVIZ_OK) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_memory_data_set(FVizMemoryWalk* walk, const FVizDataSet* data_set)
{
    if (data_set == NULL) return FVIZ_OK;
    FVIZ_MEMORY_MARK_OR_RETURN(walk, data_set);
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*data_set)) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    if (fviz_memory_attribute_set(walk, data_set->point_data) != FVIZ_OK ||
        fviz_memory_attribute_set(walk, data_set->cell_data) != FVIZ_OK ||
        fviz_memory_attribute_set(walk, data_set->field_data) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizResult fviz_memory_points(FVizMemoryWalk* walk, const FVizPoints* points)
{
    FVizSize bytes;
    if (points == NULL) return FVIZ_OK;
    FVIZ_MEMORY_MARK_OR_RETURN(walk, points);
    if (fviz_size_multiply(fviz_points_count(points), sizeof(FVizVec3), &bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*points)) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.geometry_bytes, bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    return FVIZ_OK;
}

static FVizResult fviz_memory_cells(FVizMemoryWalk* walk, const FVizCellArray* cells)
{
    FVizSize connectivity_bytes;
    FVizSize offsets_bytes;
    FVizSize types_bytes;
    FVizSize id_size;
    FVizSize count;
    if (cells == NULL) return FVIZ_OK;
    FVIZ_MEMORY_MARK_OR_RETURN(walk, cells);
    count = fviz_cell_array_count(cells);
    id_size = fviz_cell_array_id_storage(cells) == FVIZ_ID_STORAGE_UINT64 ? sizeof(uint64_t) : sizeof(uint32_t);
    if (fviz_size_multiply(fviz_cell_array_connectivity_size(cells), id_size, &connectivity_bytes) != FVIZ_OK ||
        fviz_size_multiply(count + 1u, sizeof(FVizSize), &offsets_bytes) != FVIZ_OK ||
        fviz_size_multiply(count, sizeof(FVizCellType), &types_bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*cells)) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.topology_bytes, connectivity_bytes) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.topology_bytes, offsets_bytes) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.topology_bytes, types_bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    return FVIZ_OK;
}

static FVizResult fviz_memory_object(FVizMemoryWalk* walk, const FVizDataObject* data_object);

static FVizResult fviz_memory_poly_data(FVizMemoryWalk* walk, const FVizPolyData* poly)
{
    FVizSize point_bytes;
    FVizSize normal_bytes;
    FVizSize legacy_index_bytes;
    FVizSize legacy_line_bytes;
    FVIZ_MEMORY_MARK_OR_RETURN(walk, poly);
    if (fviz_size_multiply(fviz_poly_data_point_count(poly), sizeof(FVizVec3), &point_bytes) != FVIZ_OK ||
        fviz_size_multiply(fviz_poly_data_has_normals(poly) != FVIZ_FALSE ? fviz_poly_data_point_count(poly) : 0u,
            sizeof(FVizVec3), &normal_bytes) != FVIZ_OK ||
        fviz_size_multiply(fviz_poly_data_triangle_count(poly) * 3u, sizeof(uint32_t), &legacy_index_bytes) != FVIZ_OK ||
        fviz_size_multiply(fviz_poly_data_line_count(poly) * 2u, sizeof(uint32_t), &legacy_line_bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*poly)) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.geometry_bytes, point_bytes) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.geometry_bytes, normal_bytes) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.topology_bytes, legacy_index_bytes) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.topology_bytes, legacy_line_bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    if (fviz_memory_cells(walk, poly->verts) != FVIZ_OK ||
        fviz_memory_cells(walk, poly->lines) != FVIZ_OK ||
        fviz_memory_cells(walk, poly->polys) != FVIZ_OK ||
        fviz_memory_cells(walk, poly->strips) != FVIZ_OK ||
        fviz_memory_data_array(walk, poly->scalars) != FVIZ_OK ||
        fviz_memory_attribute_set(walk, poly->point_data) != FVIZ_OK ||
        fviz_memory_attribute_set(walk, poly->cell_data) != FVIZ_OK ||
        fviz_memory_attribute_set(walk, poly->field_data) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizResult fviz_memory_unstructured_grid(FVizMemoryWalk* walk, const FVizUnstructuredGrid* grid)
{
    FVIZ_MEMORY_MARK_OR_RETURN(walk, grid);
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*grid)) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    if (fviz_memory_points(walk, grid->points) != FVIZ_OK ||
        fviz_memory_cells(walk, grid->cells) != FVIZ_OK ||
        fviz_memory_data_set(walk, grid->data_set) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizResult fviz_memory_image_data(FVizMemoryWalk* walk, const FVizImageData* image)
{
    FVIZ_MEMORY_MARK_OR_RETURN(walk, image);
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*image)) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    return fviz_memory_data_set(walk, image->data_set);
}

static FVizResult fviz_memory_structured_grid(FVizMemoryWalk* walk, const FVizStructuredGrid* grid)
{
    FVIZ_MEMORY_MARK_OR_RETURN(walk, grid);
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*grid)) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    if (fviz_memory_points(walk, grid->points) != FVIZ_OK ||
        fviz_memory_data_set(walk, grid->data_set) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizResult fviz_memory_rectilinear_grid(FVizMemoryWalk* walk, const FVizRectilinearGrid* grid)
{
    uint32_t axis;
    FVIZ_MEMORY_MARK_OR_RETURN(walk, grid);
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*grid)) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    for (axis = 0u; axis < 3u; ++axis)
    {
        const FVizDataArray* coordinates = grid->coordinates[axis];
        FVizSize bytes;
        FVizBool is_new = FVIZ_FALSE;
        FVizResult mark_result;
        if (coordinates == NULL) continue;
        mark_result = fviz_memory_mark(walk, coordinates, &is_new);
        if (mark_result != FVIZ_OK) return mark_result;
        if (is_new == FVIZ_FALSE) continue;
        if (fviz_size_multiply(fviz_data_array_tuple_count(coordinates), fviz_data_array_tuple_stride(coordinates), &bytes) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*coordinates)) != FVIZ_OK ||
            fviz_memory_add_total(walk, &walk->info.geometry_bytes, bytes) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        ++walk->info.unique_object_count;
    }
    return fviz_memory_data_set(walk, grid->data_set);
}

static FVizResult fviz_memory_partitioned(FVizMemoryWalk* walk, const FVizPartitionedDataSet* data_set)
{
    FVizSize i;
    const FVizSize count = fviz_partitioned_data_set_count(data_set);
    FVIZ_MEMORY_MARK_OR_RETURN(walk, data_set);
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*data_set)) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.composite_bytes, count * sizeof(FVizPartitionEntry)) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    for (i = 0u; i < count; ++i)
    {
        const char* name = fviz_partitioned_data_set_partition_name(data_set, i);
        const FVizDataObject* child = fviz_partitioned_data_set_const_partition(data_set, i);
        if (name != NULL && fviz_memory_add_total(walk, &walk->info.composite_bytes, (FVizSize)strlen(name) + 1u) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        if (child != NULL && fviz_memory_object(walk, child) != FVIZ_OK) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_memory_multi_block(FVizMemoryWalk* walk, const FVizMultiBlockDataSet* data_set)
{
    FVizSize i;
    const FVizSize count = fviz_multi_block_data_set_count(data_set);
    FVIZ_MEMORY_MARK_OR_RETURN(walk, data_set);
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*data_set)) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.composite_bytes, count * sizeof(FVizMultiBlockEntry)) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    for (i = 0u; i < count; ++i)
    {
        const char* name = fviz_multi_block_data_set_block_name(data_set, i);
        const FVizDataObject* child = fviz_multi_block_data_set_const_block(data_set, i);
        if (name != NULL && fviz_memory_add_total(walk, &walk->info.composite_bytes, (FVizSize)strlen(name) + 1u) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        if (child != NULL && fviz_memory_object(walk, child) != FVIZ_OK) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_memory_temporal(FVizMemoryWalk* walk, const FVizTemporalDataSet* data_set)
{
    FVizSize i;
    const FVizSize count = fviz_temporal_data_set_step_count(data_set);
    FVIZ_MEMORY_MARK_OR_RETURN(walk, data_set);
    if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(*data_set)) != FVIZ_OK ||
        fviz_memory_add_total(walk, &walk->info.composite_bytes, count * sizeof(FVizTemporalEntry)) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    ++walk->info.unique_object_count;
    for (i = 0u; i < count; ++i)
    {
        const FVizDataObject* child = fviz_temporal_data_set_const_data(data_set, i);
        if (child != NULL && fviz_memory_object(walk, child) != FVIZ_OK) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_memory_object(FVizMemoryWalk* walk, const FVizDataObject* data_object)
{
    const FVizObject* object = (const FVizObject*)data_object;
    if (data_object == NULL) return FVIZ_OK;
    if (fviz_object_is_type(object, FVIZ_TYPE_POLY_DATA) != FVIZ_FALSE)
        return fviz_memory_poly_data(walk, (const FVizPolyData*)data_object);
    if (fviz_object_is_type(object, FVIZ_TYPE_UNSTRUCTURED_GRID) != FVIZ_FALSE)
        return fviz_memory_unstructured_grid(walk, (const FVizUnstructuredGrid*)data_object);
    if (fviz_object_is_type(object, FVIZ_TYPE_STRUCTURED_GRID) != FVIZ_FALSE)
        return fviz_memory_structured_grid(walk, (const FVizStructuredGrid*)data_object);
    if (fviz_object_is_type(object, FVIZ_TYPE_RECTILINEAR_GRID) != FVIZ_FALSE)
        return fviz_memory_rectilinear_grid(walk, (const FVizRectilinearGrid*)data_object);
    if (fviz_object_is_type(object, FVIZ_TYPE_IMAGE_DATA) != FVIZ_FALSE)
        return fviz_memory_image_data(walk, (const FVizImageData*)data_object);
    if (fviz_object_is_type(object, FVIZ_TYPE_PARTITIONED_DATA_SET) != FVIZ_FALSE)
        return fviz_memory_partitioned(walk, (const FVizPartitionedDataSet*)data_object);
    if (fviz_object_is_type(object, FVIZ_TYPE_MULTI_BLOCK_DATA_SET) != FVIZ_FALSE)
        return fviz_memory_multi_block(walk, (const FVizMultiBlockDataSet*)data_object);
    if (fviz_object_is_type(object, FVIZ_TYPE_TEMPORAL_DATA_SET) != FVIZ_FALSE)
        return fviz_memory_temporal(walk, (const FVizTemporalDataSet*)data_object);
    {
        FVizBool is_new = FVIZ_FALSE;
        FVizResult mark_result = fviz_memory_mark(walk, data_object, &is_new);
        if (mark_result != FVIZ_OK) return mark_result;
        if (is_new != FVIZ_FALSE)
        {
            if (fviz_memory_add_total(walk, &walk->info.object_bytes, sizeof(FVizObject)) != FVIZ_OK)
                return FVIZ_ERROR_OVERFLOW;
            ++walk->info.unique_object_count;
        }
    }
    return FVIZ_OK;
}

FVizResult fviz_data_object_memory_info(
    const FVizDataObject* data_object,
    FVizDataObjectMemoryInfo* out_info)
{
    FVizMemoryWalk walk;
    FVizResult result;
    if (out_info != NULL) (void)memset(out_info, 0, sizeof(*out_info));
    if (data_object == NULL || out_info == NULL ||
        fviz_data_object_is_data_object(data_object) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "memory info requires a data object and output pointer");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(&walk, 0, sizeof(walk));
    if (fviz_hash_map_create(&walk.visited) != FVIZ_OK) return fviz_last_error_code();
    result = fviz_memory_object(&walk, data_object);
    if (result == FVIZ_OK) *out_info = walk.info;
    fviz_release(walk.visited);
    return result;
}

FVizSize fviz_data_object_memory_size(const FVizDataObject* data_object)
{
    FVizDataObjectMemoryInfo info;
    if (fviz_data_object_memory_info(data_object, &info) != FVIZ_OK) return 0u;
    return info.total_bytes;
}
