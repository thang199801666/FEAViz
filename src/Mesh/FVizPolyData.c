#include <limits.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Mesh/FVizPolyData.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Mesh/FVizPolyDataPrivate.h>

static void fviz_poly_data_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_poly_data_class = {
    FVIZ_TYPE_POLY_DATA,
    "FVizPolyData",
    &g_fviz_object_class,
    fviz_poly_data_destroy
};

static void fviz_poly_data_destroy(FVizObject* object)
{
    FVizPolyData* poly_data = (FVizPolyData*)object;
    fviz_release(poly_data->points);
    fviz_release(poly_data->normals);
    fviz_release(poly_data->indices);
    poly_data->points = NULL;
    poly_data->normals = NULL;
    poly_data->indices = NULL;
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
        fviz_array_create(sizeof(uint32_t), &poly_data->indices) != FVIZ_OK)
    {
        fviz_release(poly_data);
        return fviz_last_error_code();
    }
    poly_data->bounds = fviz_bounds_empty();
    poly_data->bounds_dirty = FVIZ_FALSE;
    poly_data->normals_dirty = FVIZ_TRUE;
    *out_poly_data = poly_data;
    return FVIZ_OK;
}

void fviz_poly_data_clear(FVizPolyData* poly_data)
{
    if (poly_data == NULL) return;
    fviz_array_clear(poly_data->points);
    fviz_array_clear(poly_data->normals);
    fviz_array_clear(poly_data->indices);
    fviz_bounds_reset(&poly_data->bounds);
    poly_data->bounds_dirty = FVIZ_FALSE;
    poly_data->normals_dirty = FVIZ_TRUE;
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
        fviz_array_reserve(poly_data->indices, index_capacity) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_point(FVizPolyData* poly_data, FVizVec3 point, uint32_t* out_index)
{
    FVizSize index;
    if (poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    index = fviz_array_count(poly_data->points);
    if (index > UINT32_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "Phase 8 PolyData uses 32-bit rendering indices");
        return FVIZ_ERROR_OVERFLOW;
    }
    if (fviz_array_push(poly_data->points, &point) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    fviz_bounds_include_point(&poly_data->bounds, point);
    poly_data->normals_dirty = FVIZ_TRUE;
    if (out_index != NULL) *out_index = (uint32_t)index;
    return FVIZ_OK;
}

FVizResult fviz_poly_data_add_triangle(FVizPolyData* poly_data, uint32_t a, uint32_t b, uint32_t c)
{
    const FVizSize count = poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
    if (poly_data == NULL || a >= count || b >= count || c >= count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "triangle indices must reference existing points");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_array_push(poly_data->indices, &a) != FVIZ_OK ||
        fviz_array_push(poly_data->indices, &b) != FVIZ_OK ||
        fviz_array_push(poly_data->indices, &c) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    poly_data->normals_dirty = FVIZ_TRUE;
    return FVIZ_OK;
}

FVizSize fviz_poly_data_point_count(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
}

FVizSize fviz_poly_data_triangle_count(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? fviz_array_count(poly_data->indices) / 3u : 0u;
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
        fviz_array_count(poly_data->normals) == fviz_array_count(poly_data->points) ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBounds fviz_poly_data_bounds(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? poly_data->bounds : fviz_bounds_empty();
}

FVizResult fviz_poly_data_validate(const FVizPolyData* poly_data)
{
    FVizSize i;
    const uint32_t* indices;
    const FVizSize point_count = fviz_poly_data_point_count(poly_data);
    const FVizSize index_count = poly_data != NULL ? fviz_array_count(poly_data->indices) : 0u;
    if (poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (index_count % 3u != 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "triangle index count is not divisible by three");
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
    return FVIZ_OK;
}

FVizResult fviz_poly_data_compute_normals(FVizPolyData* poly_data)
{
    FVizSize i;
    FVizVec3* normals;
    const FVizVec3* points;
    const uint32_t* indices;
    const FVizSize point_count = fviz_poly_data_point_count(poly_data);
    const FVizSize triangle_count = fviz_poly_data_triangle_count(poly_data);

    if (poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_poly_data_validate(poly_data) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    if (fviz_array_resize(poly_data->normals, point_count) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    normals = (FVizVec3*)fviz_array_data(poly_data->normals);
    if (point_count != 0u)
    {
        (void)memset(normals, 0, point_count * sizeof(FVizVec3));
    }
    points = fviz_poly_data_points(poly_data);
    indices = fviz_poly_data_triangle_indices(poly_data);

    for (i = 0u; i < triangle_count; ++i)
    {
        const uint32_t ia = indices[i * 3u + 0u];
        const uint32_t ib = indices[i * 3u + 1u];
        const uint32_t ic = indices[i * 3u + 2u];
        const FVizVec3 e1 = fviz_vec3_sub(points[ib], points[ia]);
        const FVizVec3 e2 = fviz_vec3_sub(points[ic], points[ia]);
        const FVizVec3 face = fviz_vec3_cross(e1, e2);
        normals[ia] = fviz_vec3_add(normals[ia], face);
        normals[ib] = fviz_vec3_add(normals[ib], face);
        normals[ic] = fviz_vec3_add(normals[ic], face);
    }
    for (i = 0u; i < point_count; ++i)
    {
        normals[i] = fviz_vec3_normalize(normals[i]);
    }
    poly_data->normals_dirty = FVIZ_FALSE;
    return FVIZ_OK;
}
