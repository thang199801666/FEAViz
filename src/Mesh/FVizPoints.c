#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Mesh/FVizPoints.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Mesh/FVizPointsPrivate.h>

static void fviz_points_destroy(FVizObject* object);
static FVizMTime fviz_points_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_points_class = {
    FVIZ_TYPE_POINTS, "FVizPoints", &g_fviz_object_class,
    fviz_points_destroy, fviz_points_mtime
};

static FVizMTime fviz_points_mtime(const FVizObject* object)
{
    const FVizPoints* points = (const FVizPoints*)object;
    const FVizMTime local = fviz_internal_object_local_mtime(object);
    const FVizMTime data = fviz_object_mtime((const FVizObject*)points->data);
    return data > local ? data : local;
}

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
    *out_points = points;
    return FVIZ_OK;
}

void fviz_points_clear(FVizPoints* points)
{
    if (points == NULL) return;
    fviz_array_clear(points->data);
    points->bounds = fviz_bounds_empty();
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

FVizResult fviz_points_append(FVizPoints* points, FVizVec3 point, uint32_t* out_id)
{
    FVizSize id;
    if (points == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "points must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    id = fviz_array_count(points->data);
    if (id > UINT32_MAX) return FVIZ_ERROR_OVERFLOW;
    if (fviz_array_push(points->data, &point) != FVIZ_OK) return fviz_last_error_code();
    fviz_bounds_include_point(&points->bounds, point);
    fviz_object_modified((FVizObject*)points);
    if (out_id != NULL) *out_id = (uint32_t)id;
    return FVIZ_OK;
}

FVizResult fviz_points_append_id(FVizPoints* points, FVizVec3 point, FVizId* out_id)
{
    uint32_t compatibility_id = 0u;
    FVizResult result = fviz_points_append(points, point, &compatibility_id);
    if (out_id != NULL) *out_id = result == FVIZ_OK ? (FVizId)compatibility_id : FVIZ_INVALID_ID;
    return result;
}

FVizSize fviz_points_count(const FVizPoints* points) { return points != NULL ? fviz_array_count(points->data) : 0u; }
const FVizVec3* fviz_points_data(const FVizPoints* points) { return points != NULL ? (const FVizVec3*)fviz_array_const_data(points->data) : NULL; }
FVizBounds fviz_points_bounds(const FVizPoints* points) { return points != NULL ? points->bounds : fviz_bounds_empty(); }
