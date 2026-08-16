#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Mesh/FVizPoints.h>

#include <FViz/Core/FVizArrayPrivate.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Mesh/FVizPointsPrivate.h>

static void fviz_points_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_points_class = {FVIZ_TYPE_POINTS, "FVizPoints", &g_fviz_object_class,
                                                    fviz_points_destroy, NULL};

static void fviz_points_destroy(FVizObject* object)
{
    FVizPoints* points = (FVizPoints*)object;
    fviz_release(points->data);
    points->data = NULL;
}

FVizResult fviz_points_create(FVizPoints** out_points)
{
    FVizPoints* points;
    if (out_points == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_points must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_points = NULL;
    points = (FVizPoints*)fviz_internal_object_allocate(sizeof(FVizPoints), &g_fviz_points_class, NULL);
    if (points == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizVec3), &points->data) != FVIZ_OK)
    {
        fviz_release(points);
        return fviz_last_error_code();
    }
    points->bounds = fviz_bounds_empty();
    points->bounds_dirty = FVIZ_FALSE;
    *out_points = points;
    return FVIZ_OK;
}

void fviz_points_clear(FVizPoints* points)
{
    if (points == NULL) return;
    if (fviz_array_count(points->data) == 0u) return;
    fviz_internal_array_clear(points->data);
    points->bounds = fviz_bounds_empty();
    points->bounds_dirty = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)points);
}

FVizResult fviz_points_reserve(FVizPoints* points, FVizSize capacity)
{
    if (points == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "points must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_array_reserve(points->data, capacity);
}

static FVizResult fviz_points_append_many_native(FVizPoints* points, const FVizVec3* values, FVizSize count,
                                                 FVizSize* out_first)
{
    const FVizSize first = points != NULL ? fviz_array_count(points->data) : 0u;
    FVizSize i;
    if (points == NULL || (values == NULL && count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "points append requires valid storage and values");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (count > (FVizSize)-1 - first)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "point count overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    if (fviz_internal_array_append(points->data, values, count) != FVIZ_OK) return fviz_last_error_code();
    if (points->bounds_dirty == FVIZ_FALSE)
        for (i = 0u; i < count; ++i)
            fviz_bounds_include_point(&points->bounds, values[i]);
    if (count != 0u) fviz_object_modified((FVizObject*)points);
    if (out_first != NULL) *out_first = first;
    return FVIZ_OK;
}

FVizResult fviz_points_append_many(FVizPoints* points, const FVizVec3* values, FVizSize count, uint32_t* out_first_id)
{
    const FVizSize first = points != NULL ? fviz_array_count(points->data) : 0u;
    FVizSize native_first = 0u;
    FVizResult result;
    if (points != NULL && count > (FVizSize)UINT32_MAX + 1u - first)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW,
                                "point IDs exceed compatibility UINT32 capacity; use fviz_points_append_many_ids");
        return FVIZ_ERROR_OVERFLOW;
    }
    result = fviz_points_append_many_native(points, values, count, &native_first);
    if (result == FVIZ_OK && out_first_id != NULL) *out_first_id = (uint32_t)native_first;
    return result;
}

FVizResult fviz_points_append(FVizPoints* points, FVizVec3 point, uint32_t* out_id)
{
    return fviz_points_append_many(points, &point, 1u, out_id);
}

FVizResult fviz_points_append_many_ids(FVizPoints* points, const FVizVec3* values, FVizSize count, FVizId* out_first_id)
{
    FVizSize first = 0u;
    FVizResult result = fviz_points_append_many_native(points, values, count, &first);
    if (out_first_id != NULL) *out_first_id = result == FVIZ_OK ? (FVizId)first : FVIZ_INVALID_ID;
    return result;
}

FVizResult fviz_points_append_id(FVizPoints* points, FVizVec3 point, FVizId* out_id)
{
    return fviz_points_append_many_ids(points, &point, 1u, out_id);
}

FVizResult fviz_points_set_many(FVizPoints* points, FVizSize first, const FVizVec3* values, FVizSize count)
{
    FVizVec3* destination;
    FVizSize point_count;
    FVizSize bytes;
    if (points == NULL || (values == NULL && count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "point update requires valid storage and values");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    point_count = fviz_array_count(points->data);
    if (first > point_count || count > point_count - first)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "point update range is out of bounds");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (count == 0u) return FVIZ_OK;
    if (fviz_size_multiply(count, sizeof(FVizVec3), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    destination = (FVizVec3*)fviz_array_data(points->data) + first;
    if (memcmp(destination, values, (size_t)bytes) == 0) return FVIZ_OK;
    (void)memcpy(destination, values, (size_t)bytes);
    points->bounds_dirty = FVIZ_TRUE;
    fviz_object_modified((FVizObject*)points);
    return FVIZ_OK;
}

FVizResult fviz_points_set(FVizPoints* points, FVizSize index, FVizVec3 value)
{
    return fviz_points_set_many(points, index, &value, 1u);
}

FVizSize fviz_points_count(const FVizPoints* points)
{
    return points != NULL ? fviz_array_count(points->data) : 0u;
}

const FVizVec3* fviz_points_data(const FVizPoints* points)
{
    return points != NULL ? (const FVizVec3*)fviz_array_const_data(points->data) : NULL;
}

FVizBounds fviz_points_bounds(const FVizPoints* points)
{
    FVizPoints* mutable_points;
    FVizSize i;
    if (points == NULL) return fviz_bounds_empty();
    if (points->bounds_dirty == FVIZ_FALSE) return points->bounds;
    mutable_points = (FVizPoints*)points;
    mutable_points->bounds = fviz_bounds_empty();
    for (i = 0u; i < fviz_array_count(points->data); ++i)
        fviz_bounds_include_point(&mutable_points->bounds, ((const FVizVec3*)fviz_array_const_data(points->data))[i]);
    mutable_points->bounds_dirty = FVIZ_FALSE;
    return mutable_points->bounds;
}
