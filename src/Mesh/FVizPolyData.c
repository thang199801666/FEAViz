#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>

#include <FViz/Core/FVizArrayPrivate.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/Mesh/FVizPolyDataPrivate.h>

static void fviz_poly_data_destroy(FVizObject* object);
static FVizMTime fviz_poly_data_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_poly_data_class = {FVIZ_TYPE_POLY_DATA, "FVizPolyData", &g_fviz_data_object_class,
                                                       fviz_poly_data_destroy, fviz_poly_data_mtime};

static void fviz_poly_data_mark_modified(FVizPolyData* poly_data, FVizBool geometry, FVizBool topology,
                                         FVizBool attributes)
{
    FVizMTime mtime;
    if (poly_data == NULL) return;
    fviz_object_modified((FVizObject*)poly_data);
    mtime = fviz_internal_object_local_mtime((const FVizObject*)poly_data);
    if (geometry != FVIZ_FALSE) poly_data->geometry_mtime = mtime;
    if (topology != FVIZ_FALSE) poly_data->topology_mtime = mtime;
    if (attributes != FVIZ_FALSE) poly_data->attribute_mtime = mtime;
}

static void fviz_poly_data_geometry_modified(FVizPolyData* poly_data)
{
    fviz_poly_data_mark_modified(poly_data, FVIZ_TRUE, FVIZ_FALSE, FVIZ_FALSE);
}

static void fviz_poly_data_geometry_modified_range(FVizPolyData* poly_data, FVizSize first, FVizSize count,
                                                   FVizBool full)
{
    uint32_t slot;
    FVizPolyDataDirtyRecord* record;
    fviz_poly_data_geometry_modified(poly_data);
    if (poly_data->geometry_dirty_count < FVIZ_POLY_DATA_DIRTY_HISTORY_CAPACITY)
    {
        slot =
            (poly_data->geometry_dirty_begin + poly_data->geometry_dirty_count) % FVIZ_POLY_DATA_DIRTY_HISTORY_CAPACITY;
        ++poly_data->geometry_dirty_count;
    }
    else
    {
        slot = poly_data->geometry_dirty_begin;
        poly_data->geometry_dirty_begin =
            (poly_data->geometry_dirty_begin + 1u) % FVIZ_POLY_DATA_DIRTY_HISTORY_CAPACITY;
    }
    record = &poly_data->geometry_dirty_history[slot];
    record->mtime = poly_data->geometry_mtime;
    record->first = first;
    record->count = count;
    record->full = full != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

static void fviz_poly_data_topology_modified(FVizPolyData* poly_data, FVizBool affects_normals)
{
    fviz_poly_data_mark_modified(poly_data, affects_normals, FVIZ_TRUE, FVIZ_FALSE);
}

static void fviz_poly_data_attributes_modified(FVizPolyData* poly_data)
{
    fviz_poly_data_mark_modified(poly_data, FVIZ_FALSE, FVIZ_FALSE, FVIZ_TRUE);
}

static FVizBool fviz_poly_data_dependency_modified(FVizObject* caller, FVizEventId event_id, void* call_data,
                                                   void* client_data)
{
    FVizPolyData* poly_data = (FVizPolyData*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (poly_data != NULL) fviz_poly_data_attributes_modified(poly_data);
    return FVIZ_FALSE;
}

static FVizResult fviz_poly_data_observe_dependency(FVizPolyData* poly_data, FVizObject* dependency,
                                                    FVizObserverTag* out_tag)
{
    if (out_tag == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (dependency == NULL) return FVIZ_OK;
    return fviz_object_add_observer(dependency, FVIZ_EVENT_MODIFIED, 0.0f, fviz_poly_data_dependency_modified,
                                    poly_data, out_tag);
}

static void fviz_poly_data_unobserve_dependency(FVizObject* dependency, FVizObserverTag* tag)
{
    if (tag == NULL) return;
    if (dependency != NULL && *tag != FVIZ_OBSERVER_TAG_INVALID) (void)fviz_object_remove_observer(dependency, *tag);
    *tag = FVIZ_OBSERVER_TAG_INVALID;
}

static void fviz_poly_data_unobserve_attributes(FVizPolyData* poly_data)
{
    if (poly_data == NULL) return;
    fviz_poly_data_unobserve_dependency((FVizObject*)poly_data->scalars, &poly_data->scalars_modified_tag);
    fviz_poly_data_unobserve_dependency((FVizObject*)poly_data->point_data, &poly_data->point_data_modified_tag);
    fviz_poly_data_unobserve_dependency((FVizObject*)poly_data->cell_data, &poly_data->cell_data_modified_tag);
    fviz_poly_data_unobserve_dependency((FVizObject*)poly_data->field_data, &poly_data->field_data_modified_tag);
}

static FVizResult fviz_poly_data_observe_attributes(FVizPolyData* poly_data)
{
    if (poly_data == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_poly_data_observe_dependency(poly_data, (FVizObject*)poly_data->scalars,
                                          &poly_data->scalars_modified_tag) != FVIZ_OK ||
        fviz_poly_data_observe_dependency(poly_data, (FVizObject*)poly_data->point_data,
                                          &poly_data->point_data_modified_tag) != FVIZ_OK ||
        fviz_poly_data_observe_dependency(poly_data, (FVizObject*)poly_data->cell_data,
                                          &poly_data->cell_data_modified_tag) != FVIZ_OK ||
        fviz_poly_data_observe_dependency(poly_data, (FVizObject*)poly_data->field_data,
                                          &poly_data->field_data_modified_tag) != FVIZ_OK)
    {
        fviz_poly_data_unobserve_attributes(poly_data);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizMTime fviz_poly_data_mtime(const FVizObject* object)
{
    /* Geometry/topology storage is private and every public mutation updates
       the PolyData revision. Attribute containers are observed explicitly. */
    return fviz_internal_object_local_mtime(object);
}

static void fviz_poly_data_destroy(FVizObject* object)
{
    FVizPolyData* poly_data = (FVizPolyData*)object;
    fviz_poly_data_unobserve_attributes(poly_data);
    fviz_release(poly_data->points);
    fviz_release(poly_data->normals);
    fviz_release(poly_data->indices);
    fviz_release(poly_data->line_indices);
    fviz_release(poly_data->verts);
    fviz_release(poly_data->lines);
    fviz_release(poly_data->polys);
    fviz_release(poly_data->strips);
    fviz_release(poly_data->scalars);
    fviz_release(poly_data->point_data);
    fviz_release(poly_data->cell_data);
    fviz_release(poly_data->field_data);
    poly_data->points = NULL;
    poly_data->normals = NULL;
    poly_data->indices = NULL;
    poly_data->line_indices = NULL;
    poly_data->verts = NULL;
    poly_data->lines = NULL;
    poly_data->polys = NULL;
    poly_data->strips = NULL;
    poly_data->scalars = NULL;
    poly_data->point_data = NULL;
    poly_data->cell_data = NULL;
    poly_data->field_data = NULL;
}

FVizResult fviz_poly_data_create(FVizPolyData** out_poly_data)
{
    FVizPolyData* poly_data;
    if (out_poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_poly_data = NULL;
    poly_data = (FVizPolyData*)fviz_internal_object_allocate(sizeof(FVizPolyData), &g_fviz_poly_data_class, NULL);
    if (poly_data == NULL)
    {
        return fviz_last_error_code();
    }
    if (fviz_array_create(sizeof(FVizVec3), &poly_data->points) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizVec3), &poly_data->normals) != FVIZ_OK ||
        fviz_array_create(sizeof(uint32_t), &poly_data->indices) != FVIZ_OK ||
        fviz_array_create(sizeof(uint32_t), &poly_data->line_indices) != FVIZ_OK ||
        fviz_cell_array_create(&poly_data->verts) != FVIZ_OK || fviz_cell_array_create(&poly_data->lines) != FVIZ_OK ||
        fviz_cell_array_create(&poly_data->polys) != FVIZ_OK || fviz_cell_array_create(&poly_data->strips) != FVIZ_OK ||
        fviz_attribute_set_create(&poly_data->point_data) != FVIZ_OK ||
        fviz_attribute_set_create(&poly_data->cell_data) != FVIZ_OK ||
        fviz_attribute_set_create(&poly_data->field_data) != FVIZ_OK)
    {
        fviz_release(poly_data);
        return fviz_last_error_code();
    }
    if (fviz_poly_data_observe_attributes(poly_data) != FVIZ_OK)
    {
        fviz_release(poly_data);
        return fviz_last_error_code();
    }
    poly_data->bounds = fviz_bounds_empty();
    poly_data->geometry_mtime = fviz_internal_object_local_mtime((const FVizObject*)poly_data);
    poly_data->topology_mtime = poly_data->geometry_mtime;
    poly_data->attribute_mtime = poly_data->geometry_mtime;
    poly_data->bounds_dirty = FVIZ_FALSE;
    poly_data->normals_dirty = FVIZ_TRUE;
    poly_data->geometry_dirty_begin = 0u;
    poly_data->geometry_dirty_count = 0u;
    *out_poly_data = poly_data;
    return FVIZ_OK;
}

void fviz_poly_data_clear(FVizPolyData* poly_data)
{
    if (poly_data == NULL) return;
    fviz_internal_array_clear(poly_data->points);
    fviz_internal_array_clear(poly_data->normals);
    fviz_internal_array_clear(poly_data->indices);
    fviz_internal_array_clear(poly_data->line_indices);
    fviz_cell_array_clear(poly_data->verts);
    fviz_cell_array_clear(poly_data->lines);
    fviz_cell_array_clear(poly_data->polys);
    fviz_cell_array_clear(poly_data->strips);
    fviz_poly_data_unobserve_attributes(poly_data);
    fviz_release(poly_data->scalars);
    poly_data->scalars = NULL;
    fviz_attribute_set_clear(poly_data->point_data);
    fviz_attribute_set_clear(poly_data->cell_data);
    fviz_attribute_set_clear(poly_data->field_data);
    (void)fviz_poly_data_observe_attributes(poly_data);
    fviz_bounds_reset(&poly_data->bounds);
    poly_data->bounds_dirty = FVIZ_FALSE;
    poly_data->normals_dirty = FVIZ_TRUE;
    fviz_poly_data_mark_modified(poly_data, FVIZ_TRUE, FVIZ_TRUE, FVIZ_TRUE);
}

static FVizSize fviz_poly_data_growth_capacity(FVizSize current, FVizSize required)
{
    FVizSize capacity = current == 0u ? 8u : current;
    while (capacity < required)
    {
        if (capacity > ((FVizSize)-1) / 2u) return required;
        capacity *= 2u;
    }
    return capacity;
}

static FVizResult fviz_poly_data_ensure_array_capacity(FVizArray* array, FVizSize required)
{
    const FVizSize capacity = fviz_array_capacity(array);
    if (required <= capacity) return FVIZ_OK;
    return fviz_array_reserve(array, fviz_poly_data_growth_capacity(capacity, required));
}

FVizResult fviz_poly_data_reserve(FVizPolyData* poly_data, FVizSize point_capacity, FVizSize triangle_capacity)
{
    FVizSize index_capacity;
    if (poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_size_multiply(triangle_capacity, 3u, &index_capacity) != FVIZ_OK)
    {
        return FVIZ_ERROR_OVERFLOW;
    }
    if (fviz_array_reserve(poly_data->points, point_capacity) != FVIZ_OK ||
        fviz_array_reserve(poly_data->normals, point_capacity) != FVIZ_OK ||
        fviz_array_reserve(poly_data->indices, index_capacity) != FVIZ_OK ||
        fviz_cell_array_reserve(poly_data->polys, triangle_capacity, index_capacity) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_points_ids(FVizPolyData* poly_data, const FVizVec3* points, FVizSize point_count,
                                         FVizId* out_first_id)
{
    const FVizSize first = poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
    FVizSize i;
    if (poly_data == NULL || (points == NULL && point_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data point batch is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if ((uintmax_t)first > UINT64_MAX || (uintmax_t)point_count > UINT64_MAX - (uintmax_t)first)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "PolyData point IDs exceed FVizId capacity");
        return FVIZ_ERROR_OVERFLOW;
    }
    if (point_count == 0u)
    {
        if (out_first_id != NULL) *out_first_id = (FVizId)first;
        return FVIZ_OK;
    }
    if (fviz_internal_array_append(poly_data->points, points, point_count) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < point_count; ++i)
        fviz_bounds_include_point(&poly_data->bounds, points[i]);
    poly_data->normals_dirty = FVIZ_TRUE;
    fviz_poly_data_geometry_modified_range(poly_data, 0u, fviz_array_count(poly_data->points), FVIZ_TRUE);
    if (out_first_id != NULL) *out_first_id = (FVizId)first;
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_points(FVizPolyData* poly_data, const FVizVec3* points, FVizSize point_count,
                                     uint32_t* out_first_index)
{
    const FVizSize first = poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
    FVizId native_first = 0u;
    if (poly_data == NULL || (points == NULL && point_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data point batch is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (first > (FVizSize)UINT32_MAX || point_count > (FVizSize)UINT32_MAX + 1u - first)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW,
                                "legacy PolyData point insertion uses uint32_t IDs; use fviz_poly_data_add_points_ids");
        return FVIZ_ERROR_OVERFLOW;
    }
    if (fviz_poly_data_add_points_ids(poly_data, points, point_count, &native_first) != FVIZ_OK)
        return fviz_last_error_code();
    if (out_first_index != NULL) *out_first_index = (uint32_t)native_first;
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_point(FVizPolyData* poly_data, FVizVec3 point, uint32_t* out_index)
{
    return fviz_poly_data_add_points(poly_data, &point, 1u, out_index);
}

FVizResult fviz_poly_data_set_points(FVizPolyData* poly_data, const FVizVec3* points, FVizSize point_count)
{
    const FVizSize old_count = poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
    const FVizBool has_topology =
        poly_data != NULL && fviz_poly_data_cell_count(poly_data) != 0u ? FVIZ_TRUE : FVIZ_FALSE;
    FVizSize bytes = 0u;
    FVizSize i;
    if (poly_data == NULL || (points == NULL && point_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data point replacement is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (has_topology != FVIZ_FALSE && point_count != old_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                "replacing PolyData coordinates cannot change point count while topology exists");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_size_multiply(point_count, sizeof(FVizVec3), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    if (fviz_internal_array_resize_untracked(poly_data->points, point_count) != FVIZ_OK) return fviz_last_error_code();
    if (bytes != 0u) (void)memcpy(fviz_array_data(poly_data->points), points, bytes);
    fviz_bounds_reset(&poly_data->bounds);
    for (i = 0u; i < point_count; ++i)
        fviz_bounds_include_point(&poly_data->bounds, points[i]);
    poly_data->bounds_dirty = FVIZ_FALSE;
    poly_data->normals_dirty = FVIZ_TRUE;
    fviz_poly_data_geometry_modified_range(poly_data, 0u, point_count, FVIZ_TRUE);
    return FVIZ_OK;
}

FVizResult fviz_poly_data_set_points_range(FVizPolyData* poly_data, FVizSize first, const FVizVec3* points,
                                           FVizSize point_count)
{
    FVizVec3* destination;
    FVizSize count;
    FVizSize bytes;
    if (poly_data == NULL || (points == NULL && point_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PolyData point range is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_array_count(poly_data->points);
    if (first > count || point_count > count - first)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PolyData point range is out of bounds");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (point_count == 0u) return FVIZ_OK;
    if (fviz_size_multiply(point_count, sizeof(FVizVec3), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    destination = (FVizVec3*)fviz_array_data(poly_data->points) + first;
    if (memcmp(destination, points, (size_t)bytes) == 0) return FVIZ_OK;
    (void)memcpy(destination, points, (size_t)bytes);
    poly_data->bounds_dirty = FVIZ_TRUE;
    poly_data->normals_dirty = FVIZ_TRUE;
    fviz_poly_data_geometry_modified_range(poly_data, first, point_count, FVIZ_FALSE);
    return FVIZ_OK;
}

FVizResult fviz_poly_data_set_point(FVizPolyData* poly_data, FVizSize index, FVizVec3 point)
{
    FVizVec3* destination;
    if (poly_data == NULL || index >= fviz_array_count(poly_data->points))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data point index is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    destination = (FVizVec3*)fviz_array_at(poly_data->points, index);
    if (destination->x == point.x && destination->y == point.y && destination->z == point.z) return FVIZ_OK;
    return fviz_poly_data_set_points_range(poly_data, index, &point, 1u);
}

FVizResult fviz_poly_data_get_point(const FVizPolyData* poly_data, FVizSize index, FVizVec3* out_point)
{
    const FVizVec3* source;
    if (poly_data == NULL || out_point == NULL || index >= fviz_array_count(poly_data->points))
    {
        if (out_point != NULL) *out_point = fviz_vec3(0.0f, 0.0f, 0.0f);
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data point access is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    source = (const FVizVec3*)fviz_array_const_at(poly_data->points, index);
    *out_point = *source;
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_triangles(FVizPolyData* poly_data, const uint32_t* triangle_indices,
                                        FVizSize triangle_count)
{
    const FVizSize point_count = poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
    const FVizSize old_index_count = poly_data != NULL ? fviz_array_count(poly_data->indices) : 0u;
    const FVizSize old_cell_count = poly_data != NULL ? fviz_cell_array_count(poly_data->polys) : 0u;
    const FVizSize old_connectivity = poly_data != NULL ? fviz_cell_array_connectivity_size(poly_data->polys) : 0u;
    FVizSize added_indices;
    FVizSize i;
    if (poly_data == NULL || (triangle_indices == NULL && triangle_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "triangle batch is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (triangle_count == 0u) return FVIZ_OK;
    if (fviz_size_multiply(triangle_count, 3u, &added_indices) != FVIZ_OK ||
        added_indices > (FVizSize)-1 - old_index_count || added_indices > (FVizSize)-1 - old_connectivity ||
        triangle_count > (FVizSize)-1 - old_cell_count)
        return FVIZ_ERROR_OVERFLOW;
    for (i = 0u; i < added_indices; ++i)
    {
        if ((FVizSize)triangle_indices[i] >= point_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "triangle indices must reference existing points");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    /* Preflight both storage paths so allocation failure cannot leave legacy and logical topology out of sync. */
    if (fviz_poly_data_ensure_array_capacity(poly_data->indices, old_index_count + added_indices) != FVIZ_OK ||
        fviz_cell_array_reserve(poly_data->polys, old_cell_count + triangle_count, old_connectivity + added_indices) !=
            FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_internal_array_append(poly_data->indices, triangle_indices, added_indices) != FVIZ_OK ||
        fviz_cell_array_append_fixed(poly_data->polys, FVIZ_CELL_TRIANGLE, 3u, triangle_count, triangle_indices) !=
            FVIZ_OK)
        return fviz_last_error_code();
    poly_data->normals_dirty = FVIZ_TRUE;
    fviz_poly_data_topology_modified(poly_data, FVIZ_TRUE);
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_triangle(FVizPolyData* poly_data, uint32_t a, uint32_t b, uint32_t c)
{
    const uint32_t ids[3] = {a, b, c};
    return fviz_poly_data_add_triangles(poly_data, ids, 1u);
}

FVizSize fviz_poly_data_point_count(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
}

FVizSize fviz_poly_data_triangle_count(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? fviz_array_count(poly_data->indices) / 3u : 0u;
}

static FVizResult fviz_poly_data_validate_point_ids(const FVizPolyData* poly_data, FVizSize point_count,
                                                    const uint32_t* point_ids)
{
    FVizSize i;
    const FVizSize available = poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
    if (poly_data == NULL || point_ids == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data topology requires point IDs");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < point_count; ++i)
    {
        if ((FVizSize)point_ids[i] >= available)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data topology references a missing point");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_lines(FVizPolyData* poly_data, const uint32_t* line_indices, FVizSize line_count)
{
    const FVizSize point_count = poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
    const FVizSize old_index_count = poly_data != NULL ? fviz_array_count(poly_data->line_indices) : 0u;
    const FVizSize old_cell_count = poly_data != NULL ? fviz_cell_array_count(poly_data->lines) : 0u;
    const FVizSize old_connectivity = poly_data != NULL ? fviz_cell_array_connectivity_size(poly_data->lines) : 0u;
    FVizSize added_indices;
    FVizSize i;
    if (poly_data == NULL || (line_indices == NULL && line_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "line batch is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (line_count == 0u) return FVIZ_OK;
    if (fviz_size_multiply(line_count, 2u, &added_indices) != FVIZ_OK ||
        added_indices > (FVizSize)-1 - old_index_count || added_indices > (FVizSize)-1 - old_connectivity ||
        line_count > (FVizSize)-1 - old_cell_count)
        return FVIZ_ERROR_OVERFLOW;
    for (i = 0u; i < added_indices; ++i)
    {
        if ((FVizSize)line_indices[i] >= point_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "line indices must reference existing points");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    if (fviz_poly_data_ensure_array_capacity(poly_data->line_indices, old_index_count + added_indices) != FVIZ_OK ||
        fviz_cell_array_reserve(poly_data->lines, old_cell_count + line_count, old_connectivity + added_indices) !=
            FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_internal_array_append(poly_data->line_indices, line_indices, added_indices) != FVIZ_OK ||
        fviz_cell_array_append_fixed(poly_data->lines, FVIZ_CELL_LINE, 2u, line_count, line_indices) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_poly_data_topology_modified(poly_data, FVIZ_FALSE);
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_line(FVizPolyData* poly_data, uint32_t a, uint32_t b)
{
    const uint32_t ids[2] = {a, b};
    return fviz_poly_data_add_lines(poly_data, ids, 1u);
}

FVizResult fviz_poly_data_add_vertex(FVizPolyData* poly_data, uint32_t point_id)
{
    return fviz_poly_data_add_poly_vertex(poly_data, 1u, &point_id);
}

FVizResult fviz_poly_data_add_poly_vertex(FVizPolyData* poly_data, FVizSize point_count, const uint32_t* point_ids)
{
    const FVizCellType type = point_count == 1u ? FVIZ_CELL_VERTEX : FVIZ_CELL_POLY_VERTEX;
    if (fviz_poly_data_validate_point_ids(poly_data, point_count, point_ids) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_cell_array_append(poly_data->verts, type, point_count, point_ids) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_poly_data_topology_modified(poly_data, FVIZ_FALSE);
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_poly_line(FVizPolyData* poly_data, FVizSize point_count, const uint32_t* point_ids)
{
    if (fviz_poly_data_validate_point_ids(poly_data, point_count, point_ids) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_cell_array_append(poly_data->lines, FVIZ_CELL_POLY_LINE, point_count, point_ids) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_poly_data_topology_modified(poly_data, FVIZ_FALSE);
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_polygon(FVizPolyData* poly_data, FVizSize point_count, const uint32_t* point_ids)
{
    FVizCellType type;
    if (fviz_poly_data_validate_point_ids(poly_data, point_count, point_ids) != FVIZ_OK) return fviz_last_error_code();
    type = point_count == 3u ? FVIZ_CELL_TRIANGLE : (point_count == 4u ? FVIZ_CELL_QUAD : FVIZ_CELL_POLYGON);
    if (fviz_cell_array_append(poly_data->polys, type, point_count, point_ids) != FVIZ_OK)
        return fviz_last_error_code();
    poly_data->normals_dirty = FVIZ_TRUE;
    fviz_poly_data_topology_modified(poly_data, FVIZ_TRUE);
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_quad(FVizPolyData* poly_data, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    const uint32_t ids[4] = {a, b, c, d};
    return fviz_poly_data_add_polygon(poly_data, 4u, ids);
}

FVizResult fviz_poly_data_add_triangle_strip(FVizPolyData* poly_data, FVizSize point_count, const uint32_t* point_ids)
{
    if (fviz_poly_data_validate_point_ids(poly_data, point_count, point_ids) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_cell_array_append(poly_data->strips, FVIZ_CELL_TRIANGLE_STRIP, point_count, point_ids) != FVIZ_OK)
        return fviz_last_error_code();
    poly_data->normals_dirty = FVIZ_TRUE;
    fviz_poly_data_topology_modified(poly_data, FVIZ_TRUE);
    return FVIZ_OK;
}

static FVizResult fviz_poly_data_validate_native_point_ids(const FVizPolyData* poly_data, FVizSize point_count,
                                                           const FVizId* point_ids)
{
    const FVizSize available = poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
    FVizSize i;
    if (poly_data == NULL || (point_ids == NULL && point_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data topology requires native point IDs");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < point_count; ++i)
    {
        if (point_ids[i] >= (FVizId)available)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data topology references a missing point");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_cell_ids(FVizPolyData* poly_data, FVizCellType type, FVizSize point_count,
                                       const FVizId* point_ids)
{
    FVizCellArray* target = NULL;
    FVizBool renderable = FVIZ_TRUE;
    uint32_t compatible[4];
    FVizSize i;
    if (fviz_poly_data_validate_native_point_ids(poly_data, point_count, point_ids) != FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_cell_type_accepts_point_count(type, point_count) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PolyData cell arity does not match its cell type");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    switch (type)
    {
        case FVIZ_CELL_VERTEX:
        case FVIZ_CELL_POLY_VERTEX:
            target = poly_data->verts;
            break;
        case FVIZ_CELL_LINE:
        case FVIZ_CELL_POLY_LINE:
            target = poly_data->lines;
            break;
        case FVIZ_CELL_TRIANGLE:
        case FVIZ_CELL_QUAD:
        case FVIZ_CELL_POLYGON:
            target = poly_data->polys;
            break;
        case FVIZ_CELL_TRIANGLE_STRIP:
            target = poly_data->strips;
            break;
        default:
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "cell type is not valid PolyData topology");
            return FVIZ_ERROR_NOT_SUPPORTED;
    }
    if ((type == FVIZ_CELL_LINE && point_count == 2u) || (type == FVIZ_CELL_TRIANGLE && point_count == 3u))
    {
        for (i = 0u; i < point_count; ++i)
        {
            if (point_ids[i] > UINT32_MAX) renderable = FVIZ_FALSE;
            else
                compatible[i] = (uint32_t)point_ids[i];
        }
        if (renderable != FVIZ_FALSE)
        {
            FVizArray* legacy = type == FVIZ_CELL_LINE ? poly_data->line_indices : poly_data->indices;
            const FVizSize old_indices = fviz_array_count(legacy);
            const FVizSize old_cells = fviz_cell_array_count(target);
            const FVizSize old_connectivity = fviz_cell_array_connectivity_size(target);
            if (old_indices > (FVizSize)-1 - point_count || old_cells == (FVizSize)-1 ||
                old_connectivity > (FVizSize)-1 - point_count)
                return FVIZ_ERROR_OVERFLOW;
            if (fviz_poly_data_ensure_array_capacity(legacy, old_indices + point_count) != FVIZ_OK ||
                fviz_cell_array_reserve(target, old_cells + 1u, old_connectivity + point_count) != FVIZ_OK)
                return fviz_last_error_code();
            if (fviz_internal_array_append(legacy, compatible, point_count) != FVIZ_OK ||
                fviz_cell_array_append_ids(target, type, point_count, point_ids) != FVIZ_OK)
                return fviz_last_error_code();
        }
        else if (fviz_cell_array_append_ids(target, type, point_count, point_ids) != FVIZ_OK)
            return fviz_last_error_code();
    }
    else if (fviz_cell_array_append_ids(target, type, point_count, point_ids) != FVIZ_OK)
        return fviz_last_error_code();
    if (type == FVIZ_CELL_TRIANGLE || type == FVIZ_CELL_QUAD || type == FVIZ_CELL_POLYGON ||
        type == FVIZ_CELL_TRIANGLE_STRIP)
        poly_data->normals_dirty = FVIZ_TRUE;
    fviz_poly_data_topology_modified(poly_data, (type == FVIZ_CELL_TRIANGLE || type == FVIZ_CELL_QUAD ||
                                                 type == FVIZ_CELL_POLYGON || type == FVIZ_CELL_TRIANGLE_STRIP)
                                                    ? FVIZ_TRUE
                                                    : FVIZ_FALSE);
    return FVIZ_OK;
}

FVizSize fviz_poly_data_line_count(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? fviz_array_count(poly_data->line_indices) / 2u : 0u;
}

FVizSize fviz_poly_data_vert_cell_count(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? fviz_cell_array_count(poly_data->verts) : 0u;
}

FVizSize fviz_poly_data_line_cell_count(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? fviz_cell_array_count(poly_data->lines) : 0u;
}

FVizSize fviz_poly_data_poly_cell_count(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? fviz_cell_array_count(poly_data->polys) : 0u;
}

FVizSize fviz_poly_data_strip_cell_count(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? fviz_cell_array_count(poly_data->strips) : 0u;
}

FVizSize fviz_poly_data_cell_count(const FVizPolyData* poly_data)
{
    return fviz_poly_data_vert_cell_count(poly_data) + fviz_poly_data_line_cell_count(poly_data) +
           fviz_poly_data_poly_cell_count(poly_data) + fviz_poly_data_strip_cell_count(poly_data);
}

const FVizCellArray* fviz_poly_data_verts(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->verts : NULL;
}

const FVizCellArray* fviz_poly_data_lines(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->lines : NULL;
}

const FVizCellArray* fviz_poly_data_polys(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->polys : NULL;
}

const FVizCellArray* fviz_poly_data_strips(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->strips : NULL;
}

const uint32_t* fviz_poly_data_line_indices(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? (const uint32_t*)fviz_array_const_data(poly_data->line_indices) : NULL;
}

const FVizVec3* fviz_poly_data_points(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? (const FVizVec3*)fviz_array_const_data(poly_data->points) : NULL;
}

const FVizVec3* fviz_poly_data_normals(const FVizPolyData* poly_data)
{
    if (poly_data == NULL || poly_data->normals_dirty == FVIZ_TRUE) return NULL;
    return (const FVizVec3*)fviz_array_const_data(poly_data->normals);
}

const uint32_t* fviz_poly_data_triangle_indices(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? (const uint32_t*)fviz_array_const_data(poly_data->indices) : NULL;
}

FVizBool fviz_poly_data_has_normals(const FVizPolyData* poly_data)
{
    return poly_data != NULL && poly_data->normals_dirty == FVIZ_FALSE &&
                   fviz_array_count(poly_data->normals) == fviz_array_count(poly_data->points)
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizBounds fviz_poly_data_bounds(const FVizPolyData* poly_data)
{
    if (poly_data != NULL && poly_data->bounds_dirty != FVIZ_FALSE)
    {
        FVizPolyData* mutable_data = (FVizPolyData*)poly_data;
        const FVizVec3* points = fviz_poly_data_points(poly_data);
        FVizSize i;
        fviz_bounds_reset(&mutable_data->bounds);
        for (i = 0u; i < fviz_poly_data_point_count(poly_data); ++i)
            fviz_bounds_include_point(&mutable_data->bounds, points[i]);
        mutable_data->bounds_dirty = FVIZ_FALSE;
    }
    return poly_data != NULL ? poly_data->bounds : fviz_bounds_empty();
}

FVizMTime fviz_poly_data_geometry_mtime(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->geometry_mtime : 0u;
}

FVizResult fviz_poly_data_geometry_dirty_range_since(const FVizPolyData* poly_data, FVizMTime since_mtime,
                                                     FVizDirtyRange* out_range)
{
    uint32_t offset;
    FVizBool found = FVIZ_FALSE;
    if (poly_data == NULL || out_range == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PolyData dirty range query is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    out_range->first = 0u;
    out_range->count = 0u;
    out_range->full = FVIZ_FALSE;
    if (since_mtime >= poly_data->geometry_mtime) return FVIZ_OK;
    if (since_mtime == 0u || poly_data->geometry_dirty_count == 0u) goto full;
    {
        const FVizPolyDataDirtyRecord* oldest = &poly_data->geometry_dirty_history[poly_data->geometry_dirty_begin];
        const uint32_t newest_slot = (poly_data->geometry_dirty_begin + poly_data->geometry_dirty_count - 1u) %
                                     FVIZ_POLY_DATA_DIRTY_HISTORY_CAPACITY;
        const FVizPolyDataDirtyRecord* newest = &poly_data->geometry_dirty_history[newest_slot];
        if (newest->mtime != poly_data->geometry_mtime ||
            (poly_data->geometry_dirty_count == FVIZ_POLY_DATA_DIRTY_HISTORY_CAPACITY && since_mtime < oldest->mtime))
            goto full;
    }
    for (offset = 0u; offset < poly_data->geometry_dirty_count; ++offset)
    {
        const uint32_t slot = (poly_data->geometry_dirty_begin + offset) % FVIZ_POLY_DATA_DIRTY_HISTORY_CAPACITY;
        const FVizPolyDataDirtyRecord* record = &poly_data->geometry_dirty_history[slot];
        FVizSize end;
        FVizSize current_end;
        if (record->mtime <= since_mtime) continue;
        if (record->full != FVIZ_FALSE) goto full;
        end = record->first + record->count;
        if (found == FVIZ_FALSE)
        {
            out_range->first = record->first;
            out_range->count = record->count;
            found = FVIZ_TRUE;
        }
        else
        {
            current_end = out_range->first + out_range->count;
            if (record->first < out_range->first) out_range->first = record->first;
            if (end > current_end) current_end = end;
            out_range->count = current_end - out_range->first;
        }
    }
    if (found == FVIZ_FALSE) goto full;
    return FVIZ_OK;
full:
    out_range->first = 0u;
    out_range->count = fviz_poly_data_point_count(poly_data);
    out_range->full = FVIZ_TRUE;
    return FVIZ_OK;
}

FVizMTime fviz_poly_data_topology_mtime(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->topology_mtime : 0u;
}

FVizMTime fviz_poly_data_attribute_mtime(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->attribute_mtime : 0u;
}

FVizResult fviz_poly_data_validate(const FVizPolyData* poly_data)
{
    FVizSize i;
    const uint32_t* indices;
    const uint32_t* line_indices;
    FVizSize point_count;
    FVizSize index_count;
    FVizSize line_index_count;
    if (poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    point_count = fviz_poly_data_point_count(poly_data);
    index_count = fviz_array_count(poly_data->indices);
    line_index_count = fviz_array_count(poly_data->line_indices);
    if (index_count % 3u != 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "triangle index count is not divisible by three");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (line_index_count % 2u != 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "line index count is not divisible by two");
        return FVIZ_ERROR_INVALID_STATE;
    }
    indices = fviz_poly_data_triangle_indices(poly_data);
    for (i = 0u; i < index_count; ++i)
    {
        if ((FVizSize)indices[i] >= point_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "poly_data contains an out-of-range triangle index");
            return FVIZ_ERROR_INVALID_STATE;
        }
    }
    line_indices = fviz_poly_data_line_indices(poly_data);
    for (i = 0u; i < line_index_count; ++i)
    {
        if ((FVizSize)line_indices[i] >= point_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "poly_data contains an out-of-range line index");
            return FVIZ_ERROR_INVALID_STATE;
        }
    }
    if (fviz_cell_array_validate(poly_data->verts, point_count) != FVIZ_OK ||
        fviz_cell_array_validate(poly_data->lines, point_count) != FVIZ_OK ||
        fviz_cell_array_validate(poly_data->polys, point_count) != FVIZ_OK ||
        fviz_cell_array_validate(poly_data->strips, point_count) != FVIZ_OK)
        return fviz_last_error_code();
    for (i = 0u; i < fviz_cell_array_count(poly_data->verts); ++i)
    {
        const FVizCellType type = fviz_cell_array_type(poly_data->verts, i);
        if (type != FVIZ_CELL_VERTEX && type != FVIZ_CELL_POLY_VERTEX) goto invalid_topology_category;
    }
    for (i = 0u; i < fviz_cell_array_count(poly_data->lines); ++i)
    {
        const FVizCellType type = fviz_cell_array_type(poly_data->lines, i);
        if (type != FVIZ_CELL_LINE && type != FVIZ_CELL_POLY_LINE) goto invalid_topology_category;
    }
    for (i = 0u; i < fviz_cell_array_count(poly_data->polys); ++i)
    {
        const FVizCellType type = fviz_cell_array_type(poly_data->polys, i);
        if (type != FVIZ_CELL_TRIANGLE && type != FVIZ_CELL_QUAD && type != FVIZ_CELL_POLYGON)
            goto invalid_topology_category;
    }
    for (i = 0u; i < fviz_cell_array_count(poly_data->strips); ++i)
        if (fviz_cell_array_type(poly_data->strips, i) != FVIZ_CELL_TRIANGLE_STRIP) goto invalid_topology_category;
    return FVIZ_OK;
invalid_topology_category:
    fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "poly_data cell is stored in the wrong topology category");
    return FVIZ_ERROR_INVALID_STATE;
}

FVizResult fviz_poly_data_set_scalars(FVizPolyData* poly_data, FVizDataArray* scalars)
{
    FVizObserverTag new_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (scalars == poly_data->scalars) return FVIZ_OK;
    if (scalars != NULL)
    {
        if (fviz_data_array_type(scalars) != FVIZ_DATA_FLOAT32 || fviz_data_array_components(scalars) != 1u)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "poly_data scalars must be a float32 single-component array");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        if (fviz_data_array_tuple_count(scalars) != fviz_poly_data_point_count(poly_data))
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data scalars count must match point count");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        if (fviz_retain(scalars) == NULL) return fviz_last_error_code();
        if (fviz_poly_data_observe_dependency(poly_data, (FVizObject*)scalars, &new_tag) != FVIZ_OK)
        {
            fviz_release(scalars);
            return fviz_last_error_code();
        }
    }
    fviz_poly_data_unobserve_dependency((FVizObject*)poly_data->scalars, &poly_data->scalars_modified_tag);
    fviz_release(poly_data->scalars);
    poly_data->scalars = scalars;
    poly_data->scalars_modified_tag = new_tag;
    fviz_poly_data_attributes_modified(poly_data);
    return FVIZ_OK;
}

const FVizDataArray* fviz_poly_data_const_scalars(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->scalars : NULL;
}

FVizAttributeSet* fviz_poly_data_point_data(FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->point_data : NULL;
}

const FVizAttributeSet* fviz_poly_data_const_point_data(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->point_data : NULL;
}

static FVizResult fviz_poly_data_copy_attributes(const FVizAttributeSet* source, FVizAttributeSet* destination,
                                                 FVizBool deep)
{
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        const FVizDataArray* array = fviz_attribute_set_const_array_at(source, i);
        FVizDataArray* copied = (FVizDataArray*)array;
        FVizAttributeRole role;
        if (deep == FVIZ_TRUE)
        {
            if (fviz_data_array_create(fviz_data_array_type(array), fviz_data_array_components(array), &copied) !=
                    FVIZ_OK ||
                fviz_data_array_resize(copied, fviz_data_array_tuple_count(array)) != FVIZ_OK)
            {
                fviz_release(copied);
                return fviz_last_error_code();
            }
            (void)memcpy(fviz_data_array_data(copied), fviz_data_array_const_data(array),
                         fviz_data_array_tuple_count(array) * fviz_data_array_tuple_stride(array));
        }
        if (fviz_attribute_set_add(destination, name, copied) != FVIZ_OK)
        {
            if (deep == FVIZ_TRUE) fviz_release(copied);
            return fviz_last_error_code();
        }
        if (deep == FVIZ_TRUE) fviz_release(copied);
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source, role);
            if (active != NULL && strcmp(active, name) == 0 &&
                fviz_attribute_set_set_active(destination, role, name) != FVIZ_OK)
                return fviz_last_error_code();
        }
    }
    return FVIZ_OK;
}

FVizResult fviz_poly_data_shallow_copy(const FVizPolyData* source, FVizPolyData** out_copy)
{
    FVizPolyData* copy = NULL;
    if (source == NULL || out_copy == NULL)
    {
        if (out_copy != NULL) *out_copy = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data shallow copy requires source and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_copy = NULL;
    if (fviz_poly_data_create(&copy) != FVIZ_OK) return fviz_last_error_code();
    fviz_poly_data_unobserve_attributes(copy);
#define FVIZ_SHARE_CHILD(field)                                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        fviz_release(copy->field);                                                                                     \
        copy->field = fviz_retain(source->field);                                                                      \
    } while (0)
    FVIZ_SHARE_CHILD(points);
    FVIZ_SHARE_CHILD(normals);
    FVIZ_SHARE_CHILD(indices);
    FVIZ_SHARE_CHILD(line_indices);
    FVIZ_SHARE_CHILD(verts);
    FVIZ_SHARE_CHILD(lines);
    FVIZ_SHARE_CHILD(polys);
    FVIZ_SHARE_CHILD(strips);
    FVIZ_SHARE_CHILD(point_data);
    FVIZ_SHARE_CHILD(cell_data);
    FVIZ_SHARE_CHILD(field_data);
#undef FVIZ_SHARE_CHILD
    copy->scalars = (FVizDataArray*)fviz_retain(source->scalars);
    if (fviz_poly_data_observe_attributes(copy) != FVIZ_OK)
    {
        fviz_release(copy);
        return fviz_last_error_code();
    }
    copy->bounds = source->bounds;
    copy->bounds_dirty = source->bounds_dirty;
    copy->normals_dirty = source->normals_dirty;
    *out_copy = copy;
    return FVIZ_OK;
}

FVizResult fviz_poly_data_copy_structure(const FVizPolyData* source, FVizPolyData** out_copy)
{
    FVizPolyData* copy = NULL;
    FVizCellArray* verts = NULL;
    FVizCellArray* lines = NULL;
    FVizCellArray* polys = NULL;
    FVizCellArray* strips = NULL;
    const FVizVec3* points;
    const uint32_t* triangles;
    const uint32_t* legacy_lines;
    if (source == NULL || out_copy == NULL)
    {
        if (out_copy != NULL) *out_copy = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly data structure copy requires source and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_copy = NULL;
    if (fviz_poly_data_create(&copy) != FVIZ_OK) return fviz_last_error_code();
    points = fviz_poly_data_points(source);
    triangles = fviz_poly_data_triangle_indices(source);
    legacy_lines = fviz_poly_data_line_indices(source);
    if (fviz_poly_data_add_points(copy, points, fviz_poly_data_point_count(source), NULL) != FVIZ_OK ||
        fviz_array_reserve(copy->indices, fviz_poly_data_triangle_count(source) * 3u) != FVIZ_OK ||
        fviz_array_reserve(copy->line_indices, fviz_poly_data_line_count(source) * 2u) != FVIZ_OK ||
        fviz_internal_array_append(copy->indices, triangles, fviz_poly_data_triangle_count(source) * 3u) != FVIZ_OK ||
        fviz_internal_array_append(copy->line_indices, legacy_lines, fviz_poly_data_line_count(source) * 2u) != FVIZ_OK)
        goto fail;
    if (fviz_cell_array_deep_copy(source->verts, &verts) != FVIZ_OK ||
        fviz_cell_array_deep_copy(source->lines, &lines) != FVIZ_OK ||
        fviz_cell_array_deep_copy(source->polys, &polys) != FVIZ_OK ||
        fviz_cell_array_deep_copy(source->strips, &strips) != FVIZ_OK)
        goto fail;
    fviz_release(copy->verts);
    copy->verts = verts;
    verts = NULL;
    fviz_release(copy->lines);
    copy->lines = lines;
    lines = NULL;
    fviz_release(copy->polys);
    copy->polys = polys;
    polys = NULL;
    fviz_release(copy->strips);
    copy->strips = strips;
    strips = NULL;
    copy->bounds = source->bounds;
    copy->bounds_dirty = source->bounds_dirty;
    copy->normals_dirty = FVIZ_TRUE;
    fviz_poly_data_mark_modified(copy, FVIZ_TRUE, FVIZ_TRUE, FVIZ_FALSE);
    *out_copy = copy;
    return FVIZ_OK;
fail:
    fviz_release(verts);
    fviz_release(lines);
    fviz_release(polys);
    fviz_release(strips);
    fviz_release(copy);
    return fviz_last_error_code();
}

FVizResult fviz_poly_data_deep_copy(const FVizPolyData* source, FVizPolyData** out_copy)
{
    FVizPolyData* copy = NULL;
    if (fviz_poly_data_copy_structure(source, &copy) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_poly_data_copy_attributes(source->point_data, copy->point_data, FVIZ_TRUE) != FVIZ_OK ||
        fviz_poly_data_copy_attributes(source->cell_data, copy->cell_data, FVIZ_TRUE) != FVIZ_OK ||
        fviz_poly_data_copy_attributes(source->field_data, copy->field_data, FVIZ_TRUE) != FVIZ_OK)
    {
        fviz_release(copy);
        return fviz_last_error_code();
    }
    if (source->scalars != NULL)
    {
        FVizDataArray* scalars = NULL;
        if (fviz_data_array_create(fviz_data_array_type(source->scalars), fviz_data_array_components(source->scalars),
                                   &scalars) != FVIZ_OK ||
            fviz_data_array_resize(scalars, fviz_data_array_tuple_count(source->scalars)) != FVIZ_OK)
        {
            fviz_release(scalars);
            fviz_release(copy);
            return fviz_last_error_code();
        }
        (void)memcpy(fviz_data_array_data(scalars), fviz_data_array_const_data(source->scalars),
                     fviz_data_array_tuple_count(source->scalars) * fviz_data_array_tuple_stride(source->scalars));
        if (fviz_poly_data_set_scalars(copy, scalars) != FVIZ_OK)
        {
            fviz_release(scalars);
            fviz_release(copy);
            return fviz_last_error_code();
        }
        fviz_release(scalars);
    }
    *out_copy = copy;
    return FVIZ_OK;
}

FVizSize fviz_poly_data_memory_size(const FVizPolyData* poly_data)
{
    FVizSize total;
    FVizSize i;
    if (poly_data == NULL) return 0u;
    total =
        sizeof(*poly_data) + fviz_poly_data_point_count(poly_data) * sizeof(FVizVec3) +
        fviz_poly_data_triangle_count(poly_data) * 3u * sizeof(uint32_t) +
        fviz_poly_data_line_count(poly_data) * 2u * sizeof(uint32_t) +
        (fviz_cell_array_connectivity_size(poly_data->verts) + fviz_cell_array_connectivity_size(poly_data->lines) +
         fviz_cell_array_connectivity_size(poly_data->polys) + fviz_cell_array_connectivity_size(poly_data->strips)) *
            sizeof(uint32_t) +
        (fviz_poly_data_cell_count(poly_data) + 4u) * sizeof(FVizSize) +
        fviz_poly_data_cell_count(poly_data) * sizeof(FVizCellType);
    for (i = 0u; i < fviz_attribute_set_count(poly_data->point_data); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(poly_data->point_data, i);
        total += fviz_data_array_tuple_count(array) * fviz_data_array_tuple_stride(array);
    }
    for (i = 0u; i < fviz_attribute_set_count(poly_data->cell_data); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(poly_data->cell_data, i);
        total += fviz_data_array_tuple_count(array) * fviz_data_array_tuple_stride(array);
    }
    return total;
}

FVizAttributeSet* fviz_poly_data_cell_data(FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->cell_data : NULL;
}

const FVizAttributeSet* fviz_poly_data_const_cell_data(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->cell_data : NULL;
}

FVizAttributeSet* fviz_poly_data_field_data(FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->field_data : NULL;
}

const FVizAttributeSet* fviz_poly_data_const_field_data(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->field_data : NULL;
}

FVizResult fviz_poly_data_compute_normals(FVizPolyData* poly_data)
{
    FVizSize i;
    FVizVec3* normals;
    const FVizVec3* points;
    const FVizSize point_count = fviz_poly_data_point_count(poly_data);

    if (poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_poly_data_validate(poly_data) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_internal_array_resize_untracked(poly_data->normals, point_count) != FVIZ_OK) return fviz_last_error_code();
    normals = (FVizVec3*)fviz_array_data(poly_data->normals);
    if (point_count != 0u) (void)memset(normals, 0, point_count * sizeof(FVizVec3));
    points = fviz_poly_data_points(poly_data);

    if (fviz_poly_data_poly_cell_count(poly_data) != 0u || fviz_poly_data_strip_cell_count(poly_data) != 0u)
    {
        const FVizCellArray* polys = fviz_poly_data_polys(poly_data);
        const FVizCellArray* strips = fviz_poly_data_strips(poly_data);
        for (i = 0u; i < fviz_cell_array_count(polys); ++i)
        {
            FVizCellView view;
            FVizVec3 face = fviz_vec3(0.0f, 0.0f, 0.0f);
            FVizSize j;
            if (fviz_cell_array_cell_view(polys, i, &view) != FVIZ_OK) return fviz_last_error_code();
            /* Newell's method handles triangles, quads and arbitrary planar polygons. */
            for (j = 0u; j < view.point_count; ++j)
            {
                const FVizSize ia = (FVizSize)fviz_cell_view_point_id(&view, j);
                const FVizSize ib = (FVizSize)fviz_cell_view_point_id(&view, (j + 1u) % view.point_count);
                const FVizVec3 a = points[ia];
                const FVizVec3 b = points[ib];
                face.x += (a.y - b.y) * (a.z + b.z);
                face.y += (a.z - b.z) * (a.x + b.x);
                face.z += (a.x - b.x) * (a.y + b.y);
            }
            for (j = 0u; j < view.point_count; ++j)
            {
                const FVizSize id = (FVizSize)fviz_cell_view_point_id(&view, j);
                normals[id] = fviz_vec3_add(normals[id], face);
            }
        }
        for (i = 0u; i < fviz_cell_array_count(strips); ++i)
        {
            FVizCellView view;
            FVizSize j;
            if (fviz_cell_array_cell_view(strips, i, &view) != FVIZ_OK) return fviz_last_error_code();
            for (j = 0u; j + 2u < view.point_count; ++j)
            {
                const FVizSize a = (FVizSize)fviz_cell_view_point_id(&view, j);
                const FVizSize b = (FVizSize)fviz_cell_view_point_id(&view, j + 1u);
                const FVizSize ia = (j & 1u) == 0u ? a : b;
                const FVizSize ib = (j & 1u) == 0u ? b : a;
                const FVizSize ic = (FVizSize)fviz_cell_view_point_id(&view, j + 2u);
                const FVizVec3 face =
                    fviz_vec3_cross(fviz_vec3_sub(points[ib], points[ia]), fviz_vec3_sub(points[ic], points[ia]));
                normals[ia] = fviz_vec3_add(normals[ia], face);
                normals[ib] = fviz_vec3_add(normals[ib], face);
                normals[ic] = fviz_vec3_add(normals[ic], face);
            }
        }
    }
    else
    {
        /* Compatibility fallback for legacy objects containing only render-ready triangle indices. */
        const uint32_t* indices = fviz_poly_data_triangle_indices(poly_data);
        for (i = 0u; i < fviz_poly_data_triangle_count(poly_data); ++i)
        {
            const uint32_t ia = indices[i * 3u + 0u];
            const uint32_t ib = indices[i * 3u + 1u];
            const uint32_t ic = indices[i * 3u + 2u];
            const FVizVec3 face =
                fviz_vec3_cross(fviz_vec3_sub(points[ib], points[ia]), fviz_vec3_sub(points[ic], points[ia]));
            normals[ia] = fviz_vec3_add(normals[ia], face);
            normals[ib] = fviz_vec3_add(normals[ib], face);
            normals[ic] = fviz_vec3_add(normals[ic], face);
        }
    }

    for (i = 0u; i < point_count; ++i)
        normals[i] = fviz_vec3_normalize(normals[i]);
    poly_data->normals_dirty = FVIZ_FALSE;
    fviz_poly_data_geometry_modified(poly_data);
    return FVIZ_OK;
}

typedef struct FVizEdgeKey
{
    uint32_t a;
    uint32_t b;
} FVizEdgeKey;

static int fviz_edge_key_compare(const void* left, const void* right)
{
    const FVizEdgeKey* a = (const FVizEdgeKey*)left;
    const FVizEdgeKey* b = (const FVizEdgeKey*)right;
    if (a->a < b->a) return -1;
    if (a->a > b->a) return 1;
    if (a->b < b->b) return -1;
    if (a->b > b->b) return 1;
    return 0;
}

/* Iterates a cell array and appends each edge (with normalized orientation)
 * to the key buffer. */
static FVizResult fviz_poly_data_collect_cell_edges(const FVizCellArray* cells, const uint32_t* triangle_indices,
                                                    FVizSize triangle_count, FVizSize poly_cell_count,
                                                    FVizEdgeKey* keys, FVizSize* out_key_count, FVizSize key_capacity)
{
    FVizSize key_count = 0u;
    FVizSize i;
    /* Triangle fast path. */
    if (cells == NULL)
    {
        for (i = 0u; i < triangle_count; ++i)
        {
            const uint32_t a = triangle_indices[i * 3u + 0u];
            const uint32_t b = triangle_indices[i * 3u + 1u];
            const uint32_t c = triangle_indices[i * 3u + 2u];
            uint32_t edges[3][2];
            edges[0][0] = a;
            edges[0][1] = b;
            edges[1][0] = b;
            edges[1][1] = c;
            edges[2][0] = c;
            edges[2][1] = a;
            {
                FVizSize e;
                for (e = 0u; e < 3u; ++e)
                {
                    uint32_t p = edges[e][0], q = edges[e][1];
                    if (p > q)
                    {
                        const uint32_t t = p;
                        p = q;
                        q = t;
                    }
                    if (key_count >= key_capacity) return FVIZ_ERROR_OVERFLOW;
                    keys[key_count].a = p;
                    keys[key_count].b = q;
                    ++key_count;
                }
            }
        }
        *out_key_count = key_count;
        return FVIZ_OK;
    }
    for (i = 0u; i < poly_cell_count; ++i)
    {
        FVizCellView view;
        FVizSize j;
        if (fviz_cell_array_cell_view(cells, i, &view) != FVIZ_OK) continue;
        for (j = 0u; j < view.point_count; ++j)
        {
            const FVizId pid_a = fviz_cell_view_point_id(&view, j);
            const FVizId pid_b = fviz_cell_view_point_id(&view, (j + 1u) % view.point_count);
            uint32_t p = (uint32_t)pid_a, q = (uint32_t)pid_b;
            if (p > q)
            {
                const uint32_t t = p;
                p = q;
                q = t;
            }
            if (key_count >= key_capacity) return FVIZ_ERROR_OVERFLOW;
            keys[key_count].a = p;
            keys[key_count].b = q;
            ++key_count;
        }
    }
    *out_key_count = key_count;
    return FVIZ_OK;
}

FVizResult fviz_poly_data_extract_edges(const FVizPolyData* input, FVizPolyData** out_edges)
{
    FVizSize triangle_count = 0u;
    FVizSize poly_cell_count = 0u;
    FVizSize vert_count = 0u;
    FVizSize line_cell_count = 0u;
    FVizSize strip_count = 0u;
    FVizSize key_count = 0u;
    FVizSize unique_count = 0u;
    FVizEdgeKey* keys = NULL;
    FVizPolyData* output = NULL;
    FVizSize i;
    if (out_edges != NULL) *out_edges = NULL;
    if (input == NULL || out_edges == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    triangle_count = fviz_poly_data_triangle_count(input);
    poly_cell_count = fviz_poly_data_poly_cell_count(input);
    vert_count = fviz_poly_data_vert_cell_count(input);
    line_cell_count = fviz_poly_data_line_cell_count(input);
    strip_count = fviz_poly_data_strip_cell_count(input);
    {
        FVizSize max_edges = 0u;
        if (fviz_size_add(triangle_count * 3u, poly_cell_count * 4u, &max_edges) != FVIZ_OK)
            return fviz_last_error_code();
        if (fviz_size_add(max_edges, strip_count * 4u, &max_edges) != FVIZ_OK) return fviz_last_error_code();
        if (fviz_size_add(max_edges, line_cell_count * 2u, &max_edges) != FVIZ_OK) return fviz_last_error_code();
        keys = (FVizEdgeKey*)fviz_alloc(max_edges * sizeof(*keys));
        if (keys == NULL) return fviz_last_error_code();
        if (fviz_poly_data_collect_cell_edges(NULL, fviz_poly_data_triangle_indices(input), triangle_count,
                                              poly_cell_count, keys, &key_count, max_edges) != FVIZ_OK)
            goto fail;
        /* Polys (the cells array covers polygon + other cells). */
        {
            FVizEdgeKey* after_triangles = keys + key_count;
            FVizSize extra = 0u;
            FVizSize capacity = max_edges > key_count ? max_edges - key_count : 0u;
            if (fviz_poly_data_collect_cell_edges(fviz_poly_data_polys(input), NULL, 0u, poly_cell_count,
                                                  after_triangles, &extra, capacity) != FVIZ_OK)
                goto fail;
            key_count += extra;
        }
        (void)vert_count;
    }
    /* Sort and deduplicate. */
    if (key_count > 1u) qsort(keys, (size_t)key_count, sizeof(*keys), fviz_edge_key_compare);
    {
        FVizSize out = 0u;
        for (i = 0u; i < key_count; ++i)
        {
            if (i == 0u || keys[i].a != keys[i - 1u].a || keys[i].b != keys[i - 1u].b)
            {
                keys[out] = keys[i];
                ++out;
            }
        }
        unique_count = out;
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_add_points(output, fviz_poly_data_points(input), fviz_poly_data_point_count(input), NULL) !=
            FVIZ_OK)
        goto fail;
    for (i = 0u; i < unique_count; ++i)
        if (fviz_poly_data_add_line(output, keys[i].a, keys[i].b) != FVIZ_OK) goto fail;
    if (fviz_poly_data_validate(output) != FVIZ_OK) goto fail;
    fviz_free(keys);
    *out_edges = output;
    return FVIZ_OK;
fail:
    fviz_free(keys);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_poly_data_delaunay_2d(const FVizPolyData* input, FVizPolyData** out_triangulation)
{
    const FVizVec3* points;
    FVizSize point_count;
    FVizSize i;
    FVizPolyData* output = NULL;
    double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;
    double diameter;
    FVizVec3 super[3];
    if (out_triangulation != NULL) *out_triangulation = NULL;
    if (input == NULL || out_triangulation == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    point_count = fviz_poly_data_point_count(input);
    if (point_count < 3u) return FVIZ_ERROR_INVALID_ARGUMENT;
    points = fviz_poly_data_points(input);
    min_x = max_x = (double)points[0].x;
    min_y = max_y = (double)points[0].y;
    for (i = 1u; i < point_count; ++i)
    {
        if ((double)points[i].x < min_x) min_x = (double)points[i].x;
        if ((double)points[i].x > max_x) max_x = (double)points[i].x;
        if ((double)points[i].y < min_y) min_y = (double)points[i].y;
        if ((double)points[i].y > max_y) max_y = (double)points[i].y;
    }
    diameter = (max_x - min_x) > (max_y - min_y) ? (max_x - min_x) : (max_y - min_y);
    if (diameter < 1.0e-12) return FVIZ_ERROR_INVALID_ARGUMENT;
    /* Super-triangle enclosing the whole point set. */
    super[0] = fviz_vec3((float)(min_x - 10.0 * diameter), (float)(min_y - 10.0 * diameter), 0.0f);
    super[1] = fviz_vec3((float)(min_x + 11.0 * diameter), (float)(min_y - 10.0 * diameter), 0.0f);
    super[2] = fviz_vec3((float)(min_x + 0.5 * diameter), (float)(min_y + 11.0 * diameter), 0.0f);
    {
        /* Bowyer-Watson: maintain a triangle list as triples of point indices.
         * Indices 0,1,2 are the super-triangle corners; real points follow. */
        FVizSize* triangles = NULL;
        FVizSize triangle_capacity = 64u;
        FVizSize triangle_count = 0u;
        triangles = (FVizSize*)fviz_alloc(triangle_capacity * 3u * sizeof(*triangles));
        if (triangles == NULL) return fviz_last_error_code();
        triangles[0] = 0u;
        triangles[1] = 1u;
        triangles[2] = 2u;
        triangle_count = 1u;
        for (i = 0u; i < point_count; ++i)
        {
            const double px = (double)points[i].x;
            const double py = (double)points[i].y;
            FVizSize* bad = NULL;
            FVizSize bad_count = 0u;
            FVizSize bad_capacity = 32u;
            FVizSize t;
            FVizSize* edge_seen = NULL;
            FVizSize edge_count = 0u;
            FVizSize edge_capacity = 64u;
            FVizSize e;
            bad = (FVizSize*)fviz_alloc(bad_capacity * 3u * sizeof(*bad));
            edge_seen = (FVizSize*)fviz_alloc(edge_capacity * 2u * sizeof(*edge_seen));
            if (bad == NULL || edge_seen == NULL)
            {
                fviz_free(bad);
                fviz_free(edge_seen);
                fviz_free(triangles);
                fviz_release(output);
                return fviz_last_error_code();
            }
            for (t = 0u; t < triangle_count; ++t)
            {
                const FVizSize a = triangles[t * 3u + 0u];
                const FVizSize b = triangles[t * 3u + 1u];
                const FVizSize c = triangles[t * 3u + 2u];
                double ax, ay, bx, by, cx, cy;
                double dax, day, dbx, dby, ddenom, du, dv;
                if (a < 3u)
                {
                    ax = (double)super[a].x;
                    ay = (double)super[a].y;
                }
                else
                {
                    ax = (double)points[a - 3u].x;
                    ay = (double)points[a - 3u].y;
                }
                if (b < 3u)
                {
                    bx = (double)super[b].x;
                    by = (double)super[b].y;
                }
                else
                {
                    bx = (double)points[b - 3u].x;
                    by = (double)points[b - 3u].y;
                }
                if (c < 3u)
                {
                    cx = (double)super[c].x;
                    cy = (double)super[c].y;
                }
                else
                {
                    cx = (double)points[c - 3u].x;
                    cy = (double)points[c - 3u].y;
                }
                /* Circumcenter of (a,b,c) relative to p. */
                dax = ax - px;
                day = ay - py;
                dbx = bx - px;
                dby = by - py;
                ddenom = 2.0 * (dax * dby - day * dbx);
                if (fabs(ddenom) < 1.0e-14) continue;
                du = (dax * dax + day * day) * dby - (dbx * dbx + dby * dby) * day;
                dv = (dbx * dbx + dby * dby) * dax - (dax * dax + day * day) * dbx;
                du /= ddenom;
                dv /= ddenom;
                /* Circumradius^2 from vertex a to the center (du,dv) relative to p. */
                {
                    double radius_squared = (dax - du) * (dax - du) + (day - dv) * (day - dv);
                    const double dist_squared = du * du + dv * dv;
                    if (dist_squared <= radius_squared + 1.0e-9) /* p inside or on circumcircle */
                    {
                        if (bad_count + 3u > bad_capacity)
                        {
                            bad_capacity *= 2u;
                            {
                                FVizSize* grown = (FVizSize*)fviz_realloc(bad, bad_capacity * 3u * sizeof(*grown));
                                if (grown == NULL)
                                {
                                    fviz_free(bad);
                                    fviz_free(edge_seen);
                                    fviz_free(triangles);
                                    fviz_release(output);
                                    return fviz_last_error_code();
                                }
                                bad = grown;
                            }
                        }
                        bad[bad_count * 3u + 0u] = a;
                        bad[bad_count * 3u + 1u] = b;
                        bad[bad_count * 3u + 2u] = c;
                        ++bad_count;
                    }
                }
            }
            /* Collect boundary edges of the union of bad triangles. */
            for (t = 0u; t < bad_count; ++t)
            {
                const FVizSize edges[3][2] = {{bad[t * 3u + 0u], bad[t * 3u + 1u]},
                                              {bad[t * 3u + 1u], bad[t * 3u + 2u]},
                                              {bad[t * 3u + 2u], bad[t * 3u + 0u]}};
                FVizSize ed;
                for (ed = 0u; ed < 3u; ++ed)
                {
                    FVizSize p = edges[ed][0], q = edges[ed][1];
                    FVizBool shared = FVIZ_FALSE;
                    FVizSize tt;
                    if (p > q)
                    {
                        const FVizSize tmp = p;
                        p = q;
                        q = tmp;
                    }
                    for (tt = 0u; tt < bad_count && shared == FVIZ_FALSE; ++tt)
                    {
                        if (tt == t) continue;
                        if ((bad[tt * 3u + 0u] == p && bad[tt * 3u + 1u] == q) ||
                            (bad[tt * 3u + 1u] == p && bad[tt * 3u + 2u] == q) ||
                            (bad[tt * 3u + 2u] == p && bad[tt * 3u + 0u] == q))
                            shared = FVIZ_TRUE;
                    }
                    if (shared == FVIZ_FALSE)
                    {
                        if (edge_count + 2u > edge_capacity)
                        {
                            edge_capacity *= 2u;
                            {
                                FVizSize* grown =
                                    (FVizSize*)fviz_realloc(edge_seen, edge_capacity * 2u * sizeof(*grown));
                                if (grown == NULL)
                                {
                                    fviz_free(bad);
                                    fviz_free(edge_seen);
                                    fviz_free(triangles);
                                    fviz_release(output);
                                    return fviz_last_error_code();
                                }
                                edge_seen = grown;
                            }
                        }
                        edge_seen[edge_count * 2u + 0u] = p;
                        edge_seen[edge_count * 2u + 1u] = q;
                        ++edge_count;
                    }
                }
            }
            /* Remove bad triangles and add new ones. */
            {
                FVizSize write = 0u;
                FVizSize read;
                for (read = 0u; read < triangle_count; ++read)
                {
                    FVizBool is_bad = FVIZ_FALSE;
                    FVizSize tt;
                    for (tt = 0u; tt < bad_count; ++tt)
                        if (triangles[read * 3u] == bad[tt * 3u] && triangles[read * 3u + 1u] == bad[tt * 3u + 1u] &&
                            triangles[read * 3u + 2u] == bad[tt * 3u + 2u])
                            is_bad = FVIZ_TRUE;
                    if (is_bad == FVIZ_FALSE)
                    {
                        triangles[write * 3u] = triangles[read * 3u];
                        triangles[write * 3u + 1u] = triangles[read * 3u + 1u];
                        triangles[write * 3u + 2u] = triangles[read * 3u + 2u];
                        ++write;
                    }
                }
                triangle_count = write;
            }
            for (e = 0u; e < edge_count; ++e)
            {
                if (triangle_count + 1u > triangle_capacity)
                {
                    triangle_capacity *= 2u;
                    {
                        FVizSize* grown = (FVizSize*)fviz_realloc(triangles, triangle_capacity * 3u * sizeof(*grown));
                        if (grown == NULL)
                        {
                            fviz_free(bad);
                            fviz_free(edge_seen);
                            fviz_free(triangles);
                            fviz_release(output);
                            return fviz_last_error_code();
                        }
                        triangles = grown;
                    }
                }
                triangles[triangle_count * 3u + 0u] = edge_seen[e * 2u + 0u];
                triangles[triangle_count * 3u + 1u] = edge_seen[e * 2u + 1u];
                triangles[triangle_count * 3u + 2u] = 3u + i;
                ++triangle_count;
            }
            fviz_free(bad);
            fviz_free(edge_seen);
        }
        /* Build output: copy input points (preserving z), emit triangles that
         * do not touch a super-triangle corner. */
        if (fviz_poly_data_create(&output) != FVIZ_OK ||
            fviz_poly_data_add_points(output, points, point_count, NULL) != FVIZ_OK)
            goto fail;
        {
            FVizSize t;
            for (t = 0u; t < triangle_count; ++t)
            {
                const FVizSize a = triangles[t * 3u + 0u];
                const FVizSize b = triangles[t * 3u + 1u];
                const FVizSize c = triangles[t * 3u + 2u];
                if (a < 3u || b < 3u || c < 3u) continue;
                if (a - 3u > UINT32_MAX || b - 3u > UINT32_MAX || c - 3u > UINT32_MAX) continue;
                if (fviz_poly_data_add_triangle(output, (uint32_t)(a - 3u), (uint32_t)(b - 3u), (uint32_t)(c - 3u)) !=
                    FVIZ_OK)
                    goto fail;
            }
        }
        if (fviz_poly_data_validate(output) != FVIZ_OK) goto fail;
        fviz_free(triangles);
    }
    *out_triangulation = output;
    return FVIZ_OK;
fail:
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_poly_data_glyph_3d(const FVizPolyData* input, const char* scale_array_name,
                                   const char* orientation_array_name, double scale_factor, FVizPolyData** out_glyphs)
{
    const FVizVec3* points;
    const FVizDataArray* scale_array = NULL;
    const FVizDataArray* orientation_array = NULL;
    FVizSize point_count;
    FVizSize i;
    FVizPolyData* output = NULL;
    if (out_glyphs != NULL) *out_glyphs = NULL;
    if (input == NULL || out_glyphs == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    point_count = fviz_poly_data_point_count(input);
    points = fviz_poly_data_points(input);
    if (scale_array_name != NULL && scale_array_name[0] != '\0')
        scale_array = fviz_attribute_set_const_get(fviz_poly_data_const_point_data(input), scale_array_name);
    if (orientation_array_name != NULL && orientation_array_name[0] != '\0')
        orientation_array =
            fviz_attribute_set_const_get(fviz_poly_data_const_point_data(input), orientation_array_name);
    if (fviz_poly_data_create(&output) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < point_count; ++i)
    {
        FVizVec3 direction = fviz_vec3(1.0f, 0.0f, 0.0f);
        double scale = 1.0;
        FVizVec3 tip;
        FVizVec3 base;
        uint32_t p0 = 0u, p1 = 0u;
        if (scale_array != NULL && i < fviz_data_array_tuple_count(scale_array))
        {
            double s = 0.0;
            (void)fviz_data_array_get_component(scale_array, i, 0u, &s);
            scale = s;
        }
        scale *= scale_factor;
        if (orientation_array != NULL && i < fviz_data_array_tuple_count(orientation_array))
        {
            double vx = 0.0, vy = 0.0, vz = 0.0;
            (void)fviz_data_array_get_component(orientation_array, i, 0u, &vx);
            (void)fviz_data_array_get_component(orientation_array, i, 1u, &vy);
            (void)fviz_data_array_get_component(orientation_array, i, 2u, &vz);
            direction = fviz_vec3_normalize(fviz_vec3((float)vx, (float)vy, (float)vz));
        }
        if (scale < 0.0) scale = 0.0;
        base = points[i];
        tip = fviz_vec3_add(base, fviz_vec3_scale(direction, (float)scale));
        if (fviz_poly_data_add_point(output, base, &p0) != FVIZ_OK ||
            fviz_poly_data_add_point(output, tip, &p1) != FVIZ_OK || fviz_poly_data_add_line(output, p0, p1) != FVIZ_OK)
            goto fail;
    }
    if (fviz_poly_data_validate(output) != FVIZ_OK) goto fail;
    *out_glyphs = output;
    return FVIZ_OK;
fail:
    fviz_release(output);
    return fviz_last_error_code();
}

/* Concatenates a point-data array from `other` onto `target` (same-name arrays
 * only), returning a retained copy or NULL when absent. */
static FVizResult fviz_poly_data_concat_point_array(FVizAttributeSet* target_set, const FVizAttributeSet* source_set,
                                                    const char* name)
{
    const FVizDataArray* source = fviz_attribute_set_const_get(source_set, name);
    FVizDataArray* target = fviz_attribute_set_get(target_set, name);
    FVizDataArray* copy = NULL;
    if (source == NULL) return FVIZ_OK;
    if (fviz_data_array_deep_copy(source, &copy) != FVIZ_OK) return fviz_last_error_code();
    if (target == NULL)
    {
        if (fviz_attribute_set_add(target_set, name, copy) != FVIZ_OK)
        {
            fviz_release(copy);
            return fviz_last_error_code();
        }
    }
    else
    {
        const FVizSize first = fviz_data_array_tuple_count(target);
        if (fviz_data_array_resize(target, first + fviz_data_array_tuple_count(copy)) != FVIZ_OK ||
            fviz_data_array_set_tuples(target, first, fviz_data_array_const_data(copy),
                                       fviz_data_array_tuple_count(copy)) != FVIZ_OK)
        {
            fviz_release(copy);
            return fviz_last_error_code();
        }
    }
    fviz_release(copy);
    return FVIZ_OK;
}

FVizResult fviz_poly_data_append(FVizPolyData* target, const FVizPolyData* other)
{
    const FVizVec3* points;
    FVizSize point_count;
    FVizSize base_point_count;
    FVizSize i;
    if (target == NULL || other == NULL || target == other) return FVIZ_ERROR_INVALID_ARGUMENT;
    base_point_count = fviz_poly_data_point_count(target);
    points = fviz_poly_data_points(other);
    point_count = fviz_poly_data_point_count(other);
    /* Points. */
    if (point_count > 0u)
    {
        uint32_t first = 0u;
        if (fviz_poly_data_add_points(target, points, point_count, &first) != FVIZ_OK) return fviz_last_error_code();
    }
    /* Triangles (legacy fast path) and full cells. */
    {
        const uint32_t* tris = fviz_poly_data_triangle_indices(other);
        const FVizSize tri_count = fviz_poly_data_triangle_count(other);
        FVizSize tri;
        for (tri = 0u; tri < tri_count; ++tri)
        {
            const uint32_t a = (uint32_t)((FVizSize)tris[tri * 3u + 0u] + base_point_count);
            const uint32_t b = (uint32_t)((FVizSize)tris[tri * 3u + 1u] + base_point_count);
            const uint32_t c = (uint32_t)((FVizSize)tris[tri * 3u + 2u] + base_point_count);
            if (fviz_poly_data_add_triangle(target, a, b, c) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    /* Lines. */
    {
        const uint32_t* lines = fviz_poly_data_line_indices(other);
        const FVizSize line_count = fviz_poly_data_line_cell_count(other);
        FVizSize li;
        for (li = 0u; li < line_count; ++li)
        {
            const uint32_t a = (uint32_t)((FVizSize)lines[li * 2u + 0u] + base_point_count);
            const uint32_t b = (uint32_t)((FVizSize)lines[li * 2u + 1u] + base_point_count);
            if (fviz_poly_data_add_line(target, a, b) != FVIZ_OK) return fviz_last_error_code();
        }
    }
    /* Generic cells from the logical cell arrays (polys/strips). */
    for (i = 0u; i < fviz_cell_array_count(fviz_poly_data_polys(other)); ++i)
    {
        FVizCellView view;
        uint32_t ids[64];
        FVizSize j;
        if (fviz_cell_array_cell_view(fviz_poly_data_polys(other), i, &view) != FVIZ_OK) continue;
        if (view.point_count > 64u) continue;
        for (j = 0u; j < view.point_count; ++j)
            ids[j] = (uint32_t)((FVizSize)fviz_cell_view_point_id(&view, j) + base_point_count);
        if (fviz_poly_data_add_polygon(target, view.point_count, ids) != FVIZ_OK) return fviz_last_error_code();
    }
    for (i = 0u; i < fviz_cell_array_count(fviz_poly_data_strips(other)); ++i)
    {
        FVizCellView view;
        uint32_t ids[64];
        FVizSize j;
        if (fviz_cell_array_cell_view(fviz_poly_data_strips(other), i, &view) != FVIZ_OK) continue;
        if (view.point_count > 64u) continue;
        for (j = 0u; j < view.point_count; ++j)
            ids[j] = (uint32_t)((FVizSize)fviz_cell_view_point_id(&view, j) + base_point_count);
        if (fviz_poly_data_add_triangle_strip(target, view.point_count, ids) != FVIZ_OK) return fviz_last_error_code();
    }
    /* Concatenate point attributes. */
    {
        FVizAttributeSet* target_point = fviz_poly_data_point_data(target);
        const FVizAttributeSet* source_point = fviz_poly_data_const_point_data(other);
        FVizSize array_count = fviz_attribute_set_count(source_point);
        for (i = 0u; i < array_count; ++i)
        {
            const char* name = fviz_attribute_set_name_at(source_point, i);
            if (name == NULL) continue;
            if (fviz_poly_data_concat_point_array(target_point, source_point, name) != FVIZ_OK)
                return fviz_last_error_code();
        }
    }
    if (fviz_poly_data_validate(target) != FVIZ_OK) return fviz_last_error_code();
    return FVIZ_OK;
}
