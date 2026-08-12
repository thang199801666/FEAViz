#include <limits.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Mesh/FVizPolyData.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/Mesh/FVizPolyDataPrivate.h>

static void fviz_poly_data_destroy(FVizObject* object);
static FVizMTime fviz_poly_data_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_poly_data_class = {
    FVIZ_TYPE_POLY_DATA,
    "FVizPolyData",
    &g_fviz_data_object_class,
    fviz_poly_data_destroy,
    fviz_poly_data_mtime
};

static FVizMTime fviz_poly_data_mtime(const FVizObject* object)
{
    const FVizPolyData* poly_data = (const FVizPolyData*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    FVizMTime child = fviz_object_mtime((const FVizObject*)poly_data->points);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)poly_data->normals);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)poly_data->indices);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)poly_data->line_indices);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)poly_data->scalars);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)poly_data->point_data);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)poly_data->cell_data);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)poly_data->field_data);
    if (child > mtime) mtime = child;
    return mtime;
}

static void fviz_poly_data_destroy(FVizObject* object)
{
    FVizPolyData* poly_data = (FVizPolyData*)object;
    fviz_release(poly_data->points);
    fviz_release(poly_data->normals);
    fviz_release(poly_data->indices);
    fviz_release(poly_data->line_indices);
    fviz_release(poly_data->scalars);
    fviz_release(poly_data->point_data);
    fviz_release(poly_data->cell_data);
    fviz_release(poly_data->field_data);
    poly_data->points = NULL;
    poly_data->normals = NULL;
    poly_data->indices = NULL;
    poly_data->line_indices = NULL;
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
        fviz_attribute_set_create(&poly_data->point_data) != FVIZ_OK ||
        fviz_attribute_set_create(&poly_data->cell_data) != FVIZ_OK ||
        fviz_attribute_set_create(&poly_data->field_data) != FVIZ_OK)
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
    fviz_array_clear(poly_data->line_indices);
    fviz_release(poly_data->scalars);
    poly_data->scalars = NULL;
    fviz_attribute_set_clear(poly_data->point_data);
    fviz_attribute_set_clear(poly_data->cell_data);
    fviz_attribute_set_clear(poly_data->field_data);
    fviz_bounds_reset(&poly_data->bounds);
    poly_data->bounds_dirty = FVIZ_FALSE;
    poly_data->normals_dirty = FVIZ_TRUE;
    fviz_object_modified((FVizObject*)poly_data);
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
    fviz_object_modified((FVizObject*)poly_data);
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
    fviz_object_modified((FVizObject*)poly_data);
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

FVizResult fviz_poly_data_add_line(FVizPolyData* poly_data, uint32_t a, uint32_t b)
{
    const FVizSize count = poly_data != NULL ? fviz_array_count(poly_data->points) : 0u;
    if (poly_data == NULL || a >= count || b >= count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "line indices must reference existing points");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_array_push(poly_data->line_indices, &a) != FVIZ_OK ||
        fviz_array_push(poly_data->line_indices, &b) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)poly_data);
    return FVIZ_OK;
}

FVizSize fviz_poly_data_line_count(const FVizPolyData* poly_data)
{
    return poly_data != NULL ? fviz_array_count(poly_data->line_indices) / 2u : 0u;
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

FVizResult fviz_poly_data_set_scalars(FVizPolyData* poly_data, FVizDataArray* scalars)
{
    if (poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (scalars != NULL)
    {
        if (fviz_data_array_type(scalars) != FVIZ_DATA_FLOAT32 || fviz_data_array_components(scalars) != 1u)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data scalars must be a float32 single-component array");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        if (fviz_data_array_tuple_count(scalars) != fviz_poly_data_point_count(poly_data))
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "poly_data scalars count must match point count");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        if (fviz_retain(scalars) == NULL)
        {
            return fviz_last_error_code();
        }
    }
    fviz_release(poly_data->scalars);
    poly_data->scalars = scalars;
    fviz_object_modified((FVizObject*)poly_data);
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

static FVizResult fviz_poly_data_copy_attributes(
    const FVizAttributeSet* source,
    FVizAttributeSet* destination,
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
            if (fviz_data_array_create(
                    fviz_data_array_type(array), fviz_data_array_components(array), &copied) != FVIZ_OK ||
                fviz_data_array_resize(copied, fviz_data_array_tuple_count(array)) != FVIZ_OK)
            {
                fviz_release(copied);
                return fviz_last_error_code();
            }
            (void)memcpy(
                fviz_data_array_data(copied),
                fviz_data_array_const_data(array),
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

FVizResult fviz_poly_data_shallow_copy(
    const FVizPolyData* source,
    FVizPolyData** out_copy)
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
#define FVIZ_SHARE_CHILD(field) \
    do { fviz_release(copy->field); copy->field = fviz_retain(source->field); } while (0)
    FVIZ_SHARE_CHILD(points);
    FVIZ_SHARE_CHILD(normals);
    FVIZ_SHARE_CHILD(indices);
    FVIZ_SHARE_CHILD(line_indices);
    FVIZ_SHARE_CHILD(point_data);
    FVIZ_SHARE_CHILD(cell_data);
    FVIZ_SHARE_CHILD(field_data);
#undef FVIZ_SHARE_CHILD
    copy->scalars = (FVizDataArray*)fviz_retain(source->scalars);
    copy->bounds = source->bounds;
    copy->bounds_dirty = source->bounds_dirty;
    copy->normals_dirty = source->normals_dirty;
    *out_copy = copy;
    return FVIZ_OK;
}

FVizResult fviz_poly_data_copy_structure(
    const FVizPolyData* source,
    FVizPolyData** out_copy)
{
    FVizPolyData* copy = NULL;
    FVizSize i;
    const FVizVec3* points;
    const uint32_t* triangles;
    const uint32_t* lines;
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
    lines = fviz_poly_data_line_indices(source);
    for (i = 0u; i < fviz_poly_data_point_count(source); ++i)
        if (fviz_poly_data_add_point(copy, points[i], NULL) != FVIZ_OK) goto fail;
    for (i = 0u; i < fviz_poly_data_triangle_count(source); ++i)
        if (fviz_poly_data_add_triangle(
                copy, triangles[i * 3u], triangles[i * 3u + 1u], triangles[i * 3u + 2u]) != FVIZ_OK) goto fail;
    for (i = 0u; i < fviz_poly_data_line_count(source); ++i)
        if (fviz_poly_data_add_line(copy, lines[i * 2u], lines[i * 2u + 1u]) != FVIZ_OK) goto fail;
    *out_copy = copy;
    return FVIZ_OK;
fail:
    fviz_release(copy);
    return fviz_last_error_code();
}

FVizResult fviz_poly_data_deep_copy(
    const FVizPolyData* source,
    FVizPolyData** out_copy)
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
        if (fviz_data_array_create(
                fviz_data_array_type(source->scalars),
                fviz_data_array_components(source->scalars), &scalars) != FVIZ_OK ||
            fviz_data_array_resize(scalars, fviz_data_array_tuple_count(source->scalars)) != FVIZ_OK)
        {
            fviz_release(scalars);
            fviz_release(copy);
            return fviz_last_error_code();
        }
        (void)memcpy(
            fviz_data_array_data(scalars),
            fviz_data_array_const_data(source->scalars),
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
    total = sizeof(*poly_data) +
        fviz_poly_data_point_count(poly_data) * sizeof(FVizVec3) +
        fviz_poly_data_triangle_count(poly_data) * 3u * sizeof(uint32_t) +
        fviz_poly_data_line_count(poly_data) * 2u * sizeof(uint32_t);
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
    fviz_object_modified((FVizObject*)poly_data);
    return FVIZ_OK;
}
