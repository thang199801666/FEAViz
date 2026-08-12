#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Mesh/FVizPoints.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Mesh/FVizPointsPrivate.h>

static void fviz_points_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_points_class = {
    FVIZ_TYPE_POINTS, "FVizPoints", &g_fviz_object_class, fviz_points_destroy
};

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
    if (out_id != NULL) *out_id = (uint32_t)id;
    return FVIZ_OK;
}

FVizSize fviz_points_count(const FVizPoints* points) { return points != NULL ? fviz_array_count(points->data) : 0u; }
const FVizVec3* fviz_points_data(const FVizPoints* points) { return points != NULL ? (const FVizVec3*)fviz_array_const_data(points->data) : NULL; }
FVizBounds fviz_points_bounds(const FVizPoints* points) { return points != NULL ? points->bounds : fviz_bounds_empty(); }
