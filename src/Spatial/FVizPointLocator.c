#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Math/FVizMat3.h>
#include <FViz/Spatial/FVizPointLocator.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Spatial/FVizPointLocatorPrivate.h>

static FVizBool fviz_locator_component_value(const FVizDataArray* array, FVizSize index, uint32_t component, double* out_value)
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

static FVizBool fviz_locator_scalar_value(const FVizDataArray* array, FVizSize index, double* out_value)
{
    return fviz_locator_component_value(array, index, 0u, out_value);
}

static FVizMat3 fviz_mat3_from_columns(FVizVec3 c0, FVizVec3 c1, FVizVec3 c2)
{
    FVizMat3 m;
    m.m[0] = c0.x; m.m[3] = c1.x; m.m[6] = c2.x;
    m.m[1] = c0.y; m.m[4] = c1.y; m.m[7] = c2.y;
    m.m[2] = c0.z; m.m[5] = c1.z; m.m[8] = c2.z;
    return m;
}

static void fviz_point_locator_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_point_locator_class = {
    FVIZ_TYPE_POINT_LOCATOR, "FVizPointLocator", &g_fviz_object_class,
    fviz_point_locator_destroy, NULL
};

static void fviz_point_locator_destroy(FVizObject* object)
{
    FVizPointLocator* locator = (FVizPointLocator*)object;
    fviz_release(locator->grid);
    locator->grid = NULL;
}

FVizResult fviz_point_locator_create(FVizPointLocator** out_locator)
{
    FVizPointLocator* locator;
    if (out_locator == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_locator must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_locator = NULL;
    locator = (FVizPointLocator*)fviz_internal_object_allocate(sizeof(FVizPointLocator), &g_fviz_point_locator_class, NULL);
    if (locator == NULL) return fviz_last_error_code();
    locator->grid = NULL;
    *out_locator = locator;
    return FVIZ_OK;
}

FVizResult fviz_point_locator_set_grid(FVizPointLocator* locator, const FVizUnstructuredGrid* grid)
{
    if (locator == NULL || grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "locator and grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_retain((FVizUnstructuredGrid*)grid) == NULL) return fviz_last_error_code();
    fviz_release(locator->grid);
    locator->grid = (FVizUnstructuredGrid*)grid;
    return FVIZ_OK;
}

const FVizUnstructuredGrid* fviz_point_locator_const_grid(const FVizPointLocator* locator)
{
    return locator != NULL ? locator->grid : NULL;
}

static FVizBool fviz_cell_barycentric_tetra(
    const FVizVec3* p,
    FVizVec3 query,
    FVizVec3* out_bary)
{
    const FVizVec3 v0 = p[1];
    const FVizVec3 v1 = p[2];
    const FVizVec3 v2 = p[3];
    const FVizVec3 v3 = p[0];
    const FVizMat3 m = fviz_mat3_from_columns(
        fviz_vec3_sub(v0, v3),
        fviz_vec3_sub(v1, v3),
        fviz_vec3_sub(v2, v3));
    const FVizMat3 inv = fviz_mat3_inverse(m);
    const FVizVec3 r = fviz_vec3_sub(query, v3);
    const FVizVec3 bary = fviz_mat3_transform_vec3(inv, r);
    if (bary.x < -1.0e-5f || bary.y < -1.0e-5f || bary.z < -1.0e-5f ||
        bary.x + bary.y + bary.z > 1.0f + 1.0e-5f)
    {
        return FVIZ_FALSE;
    }
    out_bary->x = bary.x;
    out_bary->y = bary.y;
    out_bary->z = bary.z;
    return FVIZ_TRUE;
}

static FVizBool fviz_cell_contains_hex(
    const FVizVec3* p,
    FVizVec3 query,
    FVizVec3* out_parametric)
{
    float r = 0.0f;
    float s = 0.0f;
    float t = 0.0f;
    int iteration;
    for (iteration = 0; iteration < 12; ++iteration)
    {
        FVizVec3 f = fviz_vec3(0.0f, 0.0f, 0.0f);
        FVizMat3 jacobian = fviz_mat3_identity();
        float dR[8];
        float dS[8];
        float dT[8];
        int k;
        for (k = 0; k < 8; ++k)
        {
            const float kr = (k & 1) ? 1.0f : -1.0f;
            const float ks = (k & 2) ? 1.0f : -1.0f;
            const float kt = (k & 4) ? 1.0f : -1.0f;
            const float weight = (1.0f + kr * r) * (1.0f + ks * s) * (1.0f + kt * t) * 0.125f;
            const float dRw = kr * (1.0f + ks * s) * (1.0f + kt * t) * 0.125f;
            const float dSw = (1.0f + kr * r) * ks * (1.0f + kt * t) * 0.125f;
            const float dTw = (1.0f + kr * r) * (1.0f + ks * s) * kt * 0.125f;
            dR[k] = dRw;
            dS[k] = dSw;
            dT[k] = dTw;
            f = fviz_vec3_add(f, fviz_vec3_scale(p[k], weight));
        }
        f = fviz_vec3_sub(f, query);
        jacobian.m[0] = 0; jacobian.m[1] = 0; jacobian.m[2] = 0;
        jacobian.m[3] = 0; jacobian.m[4] = 0; jacobian.m[5] = 0;
        jacobian.m[6] = 0; jacobian.m[7] = 0; jacobian.m[8] = 0;
        for (k = 0; k < 8; ++k)
        {
            jacobian.m[0] += dR[k] * p[k].x;
            jacobian.m[1] += dR[k] * p[k].y;
            jacobian.m[2] += dR[k] * p[k].z;
            jacobian.m[3] += dS[k] * p[k].x;
            jacobian.m[4] += dS[k] * p[k].y;
            jacobian.m[5] += dS[k] * p[k].z;
            jacobian.m[6] += dT[k] * p[k].x;
            jacobian.m[7] += dT[k] * p[k].y;
            jacobian.m[8] += dT[k] * p[k].z;
        }
        {
            const FVizMat3 jacobian_t = fviz_mat3_transpose(jacobian);
            const FVizMat3 inv = fviz_mat3_inverse(jacobian_t);
            const FVizVec3 delta = fviz_mat3_transform_vec3(inv, f);
            r -= delta.x;
            s -= delta.y;
            t -= delta.z;
        }
        if (fabsf(f.x) < 1.0e-6f && fabsf(f.y) < 1.0e-6f && fabsf(f.z) < 1.0e-6f) break;
    }
    if (r < -1.01f || r > 1.01f || s < -1.01f || s > 1.01f || t < -1.01f || t > 1.01f)
    {
        return FVIZ_FALSE;
    }
    out_parametric->x = r;
    out_parametric->y = s;
    out_parametric->z = t;
    return FVIZ_TRUE;
}

static FVizBool fviz_cell_point_contains(
    const FVizCellType type,
    const FVizVec3* p,
    FVizVec3 query,
    FVizVec3* out_weights,
    FVizSize* out_point_count)
{
    if (type == FVIZ_CELL_TETRA)
    {
        FVizVec3 bary;
        if (!fviz_cell_barycentric_tetra(p, query, &bary)) return FVIZ_FALSE;
        out_weights->x = bary.x;
        out_weights->y = bary.y;
        out_weights->z = bary.z;
        *out_point_count = 4u;
        return FVIZ_TRUE;
    }
    if (type == FVIZ_CELL_HEXAHEDRON)
    {
        FVizVec3 parametric;
        if (!fviz_cell_contains_hex(p, query, &parametric)) return FVIZ_FALSE;
        out_weights->x = parametric.x;
        out_weights->y = parametric.y;
        out_weights->z = parametric.z;
        *out_point_count = 8u;
        return FVIZ_TRUE;
    }
    (void)p;
    (void)query;
    return FVIZ_FALSE;
}

FVizBool fviz_point_locator_locate_point(
    const FVizPointLocator* locator,
    FVizVec3 point,
    FVizLocatedCell* out_result)
{
    FVizSize cell_id;
    if (locator == NULL || locator->grid == NULL || out_result == NULL) return FVIZ_FALSE;
    for (cell_id = 0u; cell_id < fviz_unstructured_grid_cell_count(locator->grid); ++cell_id)
    {
        const FVizCellType type = fviz_cell_array_type(fviz_unstructured_grid_cells(locator->grid), cell_id);
        const uint32_t* ids = fviz_cell_array_point_ids(fviz_unstructured_grid_cells(locator->grid), cell_id);
        const FVizSize count = fviz_cell_array_point_count(fviz_unstructured_grid_cells(locator->grid), cell_id);
        const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(locator->grid));
        FVizVec3 p[8];
        FVizVec3 weights;
        FVizSize point_count;
        FVizSize i;
        if (count > 8u) continue;
        for (i = 0u; i < count; ++i) p[i] = points[ids[i]];
        if (fviz_cell_point_contains(type, p, point, &weights, &point_count))
        {
            out_result->point = point;
            out_result->barycentric = weights;
            out_result->cell_index = cell_id;
            out_result->point_count = point_count;
            return FVIZ_TRUE;
        }
    }
    return FVIZ_FALSE;
}

static FVizBool fviz_interpolate_point_scalar(
    const FVizUnstructuredGrid* grid,
    const FVizDataArray* scalar,
    const FVizLocatedCell* location,
    float* out_value)
{
    const FVizCellType type = fviz_cell_array_type(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), location->cell_index);
    const uint32_t* ids = fviz_cell_array_point_ids(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), location->cell_index);
    double values[8];
    FVizSize i;
    if (fviz_data_array_tuple_count(scalar) != fviz_unstructured_grid_point_count(grid)) return FVIZ_FALSE;
    for (i = 0u; i < location->point_count; ++i)
    {
        double value = 0.0;
        if (!fviz_locator_scalar_value(scalar, ids[i], &value)) return FVIZ_FALSE;
        values[i] = value;
    }    if (type == FVIZ_CELL_TETRA)
    {
        const double w0 = 1.0 - location->barycentric.x - location->barycentric.y - location->barycentric.z;
        *out_value = (float)(w0 * values[0] +
            location->barycentric.x * values[1] +
            location->barycentric.y * values[2] +
            location->barycentric.z * values[3]);
        return FVIZ_TRUE;
    }
    if (type == FVIZ_CELL_HEXAHEDRON)
    {
        const float r = location->barycentric.x;
        const float s = location->barycentric.y;
        const float t = location->barycentric.z;
        double sum = 0.0;
        for (i = 0u; i < 8u; ++i)
        {
            const float kr = (i & 1) ? 1.0f : -1.0f;
            const float ks = (i & 2) ? 1.0f : -1.0f;
            const float kt = (i & 4) ? 1.0f : -1.0f;
            const float weight = (1.0f + kr * r) * (1.0f + ks * s) * (1.0f + kt * t) * 0.125f;
            sum += weight * values[i];
        }
        *out_value = (float)sum;
        return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

FVizResult fviz_point_locator_interpolate_scalar(
    const FVizPointLocator* locator,
    const char* scalar_name,
    FVizVec3 point,
    float* out_value)
{
    FVizLocatedCell location;
    const FVizDataArray* scalar;
    if (locator == NULL || locator->grid == NULL || scalar_name == NULL || out_value == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "locator, scalar name and output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    scalar = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(locator->grid), scalar_name);
    if (scalar == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "scalar field not found on grid");
        return FVIZ_ERROR_NOT_FOUND;
    }
    if (fviz_point_locator_locate_point(locator, point, &location) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "point not contained in the grid");
        return FVIZ_ERROR_NOT_FOUND;
    }
    if (!fviz_interpolate_point_scalar(locator->grid, scalar, &location, out_value))
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "interpolation for this cell type is unsupported");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    return FVIZ_OK;
}

static FVizBool fviz_interpolate_point_component(
    const FVizUnstructuredGrid* grid,
    const FVizDataArray* array,
    uint32_t component,
    const FVizLocatedCell* location,
    double* out_value)
{
    const FVizCellType type = fviz_cell_array_type(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), location->cell_index);
    const uint32_t* ids = fviz_cell_array_point_ids(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), location->cell_index);
    double values[8];
    FVizSize i;
    for (i = 0u; i < location->point_count; ++i)
    {
        if (!fviz_locator_component_value(array, ids[i], component, &values[i])) return FVIZ_FALSE;
    }
    if (type == FVIZ_CELL_TETRA)
    {
        const double w0 = 1.0 - location->barycentric.x - location->barycentric.y - location->barycentric.z;
        *out_value = w0 * values[0] +
            location->barycentric.x * values[1] +
            location->barycentric.y * values[2] +
            location->barycentric.z * values[3];
        return FVIZ_TRUE;
    }
    if (type == FVIZ_CELL_HEXAHEDRON)
    {
        const float r = location->barycentric.x;
        const float s = location->barycentric.y;
        const float t = location->barycentric.z;
        double sum = 0.0;
        for (i = 0u; i < 8u; ++i)
        {
            const float kr = (i & 1) ? 1.0f : -1.0f;
            const float ks = (i & 2) ? 1.0f : -1.0f;
            const float kt = (i & 4) ? 1.0f : -1.0f;
            const float weight = (1.0f + kr * r) * (1.0f + ks * s) * (1.0f + kt * t) * 0.125f;
            sum += weight * values[i];
        }
        *out_value = sum;
        return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

FVizVec3 fviz_point_locator_interpolate_vector(
    const FVizPointLocator* locator,
    const char* vector_name,
    FVizVec3 point)
{
    FVizLocatedCell location;
    const FVizDataArray* vectors;
    FVizVec3 result = fviz_vec3(0.0f, 0.0f, 0.0f);
    double vx = 0.0;
    double vy = 0.0;
    double vz = 0.0;
    if (locator == NULL || locator->grid == NULL || vector_name == NULL) return result;
    vectors = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(locator->grid), vector_name);
    if (vectors == NULL || fviz_data_array_components(vectors) != 3u) return result;
    if (fviz_point_locator_locate_point(locator, point, &location) == FVIZ_FALSE) return result;
    if (!fviz_interpolate_point_component(locator->grid, vectors, 0u, &location, &vx) ||
        !fviz_interpolate_point_component(locator->grid, vectors, 1u, &location, &vy) ||
        !fviz_interpolate_point_component(locator->grid, vectors, 2u, &location, &vz))
    {
        return result;
    }
    return fviz_vec3((float)vx, (float)vy, (float)vz);
}
