#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Math/FVizMat3.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>
#include <FViz/Spatial/FVizPointLocator.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Spatial/FVizPointLocatorPrivate.h>

static FVizBool fviz_locator_component_value(const FVizDataArray* array, FVizSize index, uint32_t component,
                                             double* out_value)
{
    const void* tuple;
    if (array == NULL || out_value == NULL || component >= fviz_data_array_components(array)) return FVIZ_FALSE;
    tuple = fviz_data_array_const_tuple(array, index);
    if (tuple == NULL) return FVIZ_FALSE;
    switch (fviz_data_array_type(array))
    {
        case FVIZ_DATA_INT8:
            *out_value = ((const int8_t*)tuple)[component];
            return FVIZ_TRUE;
        case FVIZ_DATA_UINT8:
            *out_value = ((const uint8_t*)tuple)[component];
            return FVIZ_TRUE;
        case FVIZ_DATA_INT16:
            *out_value = ((const int16_t*)tuple)[component];
            return FVIZ_TRUE;
        case FVIZ_DATA_UINT16:
            *out_value = ((const uint16_t*)tuple)[component];
            return FVIZ_TRUE;
        case FVIZ_DATA_INT32:
            *out_value = ((const int32_t*)tuple)[component];
            return FVIZ_TRUE;
        case FVIZ_DATA_UINT32:
            *out_value = ((const uint32_t*)tuple)[component];
            return FVIZ_TRUE;
        case FVIZ_DATA_INT64:
            *out_value = (double)((const int64_t*)tuple)[component];
            return FVIZ_TRUE;
        case FVIZ_DATA_UINT64:
            *out_value = (double)((const uint64_t*)tuple)[component];
            return FVIZ_TRUE;
        case FVIZ_DATA_FLOAT32:
            *out_value = ((const float*)tuple)[component];
            return FVIZ_TRUE;
        case FVIZ_DATA_FLOAT64:
            *out_value = ((const double*)tuple)[component];
            return FVIZ_TRUE;
        default:
            return FVIZ_FALSE;
    }
}

static FVizBool fviz_locator_scalar_value(const FVizDataArray* array, FVizSize index, double* out_value)
{
    return fviz_locator_component_value(array, index, 0u, out_value);
}

static const float g_fviz_hex8_signs[8][3] = {{-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f},
                                              {-1.0f, 1.0f, -1.0f},  {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f},
                                              {1.0f, 1.0f, 1.0f},    {-1.0f, 1.0f, 1.0f}};

static FVizMat3 fviz_mat3_from_columns(FVizVec3 c0, FVizVec3 c1, FVizVec3 c2)
{
    FVizMat3 m;
    m.m[0] = c0.x;
    m.m[3] = c1.x;
    m.m[6] = c2.x;
    m.m[1] = c0.y;
    m.m[4] = c1.y;
    m.m[7] = c2.y;
    m.m[2] = c0.z;
    m.m[5] = c1.z;
    m.m[8] = c2.z;
    return m;
}

static void fviz_point_locator_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_point_locator_class = {FVIZ_TYPE_POINT_LOCATOR, "FVizPointLocator",
                                                           &g_fviz_object_class, fviz_point_locator_destroy, NULL};

static void fviz_point_locator_clear_acceleration(FVizPointLocator* locator)
{
    if (locator == NULL) return;
    fviz_free(locator->nodes);
    fviz_free(locator->cell_ids);
    fviz_free(locator->cell_bounds);
    fviz_free(locator->cell_centroids);
    locator->nodes = NULL;
    locator->cell_ids = NULL;
    locator->cell_bounds = NULL;
    locator->cell_centroids = NULL;
    locator->node_count = 0u;
    locator->node_capacity = 0u;
    locator->cell_count = 0u;
    locator->build_mtime = 0u;
}

static void fviz_point_locator_destroy(FVizObject* object)
{
    FVizPointLocator* locator = (FVizPointLocator*)object;
    fviz_point_locator_clear_acceleration(locator);
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
    locator =
        (FVizPointLocator*)fviz_internal_object_allocate(sizeof(FVizPointLocator), &g_fviz_point_locator_class, NULL);
    if (locator == NULL) return fviz_last_error_code();
    locator->grid = NULL;
    locator->nodes = NULL;
    locator->node_count = 0u;
    locator->node_capacity = 0u;
    locator->cell_ids = NULL;
    locator->cell_bounds = NULL;
    locator->cell_centroids = NULL;
    locator->cell_count = 0u;
    locator->build_mtime = 0u;
    locator->build_points_mtime = 0u;
    locator->build_cells_mtime = 0u;
    *out_locator = locator;
    return FVIZ_OK;
}

static FVizMTime fviz_point_locator_grid_geometry_mtime(const FVizPointLocator* locator)
{
    FVizMTime points_mtime;
    FVizMTime cells_mtime;
    if (locator == NULL || locator->grid == NULL) return 0u;
    /* Attribute arrays intentionally do not invalidate spatial acceleration. */
    points_mtime = fviz_object_mtime((const FVizObject*)fviz_unstructured_grid_points(locator->grid));
    cells_mtime = fviz_object_mtime((const FVizObject*)fviz_unstructured_grid_cells(locator->grid));
    return points_mtime > cells_mtime ? points_mtime : cells_mtime;
}

static FVizMTime fviz_point_locator_points_mtime(const FVizPointLocator* locator)
{
    return locator != NULL && locator->grid != NULL
               ? fviz_object_mtime((const FVizObject*)fviz_unstructured_grid_points(locator->grid))
               : 0u;
}

static FVizMTime fviz_point_locator_cells_mtime(const FVizPointLocator* locator)
{
    return locator != NULL && locator->grid != NULL
               ? fviz_object_mtime((const FVizObject*)fviz_unstructured_grid_cells(locator->grid))
               : 0u;
}

static void fviz_point_locator_capture_source_mtimes(FVizPointLocator* locator)
{
    if (locator == NULL) return;
    locator->build_points_mtime = fviz_point_locator_points_mtime(locator);
    locator->build_cells_mtime = fviz_point_locator_cells_mtime(locator);
    locator->build_mtime = locator->build_points_mtime > locator->build_cells_mtime ? locator->build_points_mtime
                                                                                    : locator->build_cells_mtime;
}

static FVizBool fviz_locator_bounds_contains(const FVizBounds* bounds, FVizVec3 point)
{
    const float epsilon = 1.0e-5f;
    return bounds != NULL && bounds->valid != FVIZ_FALSE && point.x >= bounds->min.x - epsilon &&
                   point.x <= bounds->max.x + epsilon && point.y >= bounds->min.y - epsilon &&
                   point.y <= bounds->max.y + epsilon && point.z >= bounds->min.z - epsilon &&
                   point.z <= bounds->max.z + epsilon
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

static FVizResult fviz_locator_reserve_nodes(FVizPointLocator* locator, FVizSize capacity)
{
    FVizPointLocatorNode* nodes;
    FVizSize bytes;
    if (capacity <= locator->node_capacity) return FVIZ_OK;
    if (fviz_size_multiply(capacity, sizeof(*nodes), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    nodes = (FVizPointLocatorNode*)fviz_realloc(locator->nodes, bytes);
    if (nodes == NULL) return fviz_last_error_code();
    locator->nodes = nodes;
    locator->node_capacity = capacity;
    return FVIZ_OK;
}

static FVizResult fviz_locator_add_node(FVizPointLocator* locator, const FVizPointLocatorNode* node, int32_t* out_index)
{
    if (locator->node_count >= (FVizSize)INT32_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "point locator node index exceeds 32-bit hierarchy limit");
        return FVIZ_ERROR_OVERFLOW;
    }
    if (locator->node_count == locator->node_capacity)
    {
        FVizSize next = locator->node_capacity == 0u ? 64u : locator->node_capacity * 2u;
        if (next < locator->node_capacity || fviz_locator_reserve_nodes(locator, next) != FVIZ_OK)
            return fviz_last_error_code();
    }
    locator->nodes[locator->node_count] = *node;
    *out_index = (int32_t)locator->node_count++;
    return FVIZ_OK;
}

static float fviz_locator_centroid_axis(const FVizVec3* centroids, FVizSize cell_id, int axis)
{
    return axis == 0 ? centroids[cell_id].x : (axis == 1 ? centroids[cell_id].y : centroids[cell_id].z);
}

static FVizSize fviz_locator_partition(FVizSize* ids, FVizSize begin, FVizSize end, int axis, const FVizVec3* centroids)
{
    const FVizSize pivot_id = ids[begin];
    const float pivot_value = fviz_locator_centroid_axis(centroids, pivot_id, axis);
    FVizSize i = begin + 1u;
    FVizSize j = end - 1u;
    while (i <= j)
    {
        while (i <= j && fviz_locator_centroid_axis(centroids, ids[i], axis) <= pivot_value)
            ++i;
        while (j >= i && fviz_locator_centroid_axis(centroids, ids[j], axis) > pivot_value)
            --j;
        if (i < j)
        {
            const FVizSize temporary = ids[i];
            ids[i] = ids[j];
            ids[j] = temporary;
        }
    }
    ids[begin] = ids[j];
    ids[j] = pivot_id;
    return j;
}

static void fviz_locator_quickselect(FVizSize* ids, FVizSize begin, FVizSize end, FVizSize target, int axis,
                                     const FVizVec3* centroids)
{
    while (end > begin + 1u)
    {
        const FVizSize pivot = fviz_locator_partition(ids, begin, end, axis, centroids);
        if (pivot == target) return;
        if (target < pivot) end = pivot;
        else
            begin = pivot + 1u;
    }
}

static FVizResult fviz_locator_build_recursive(FVizPointLocator* locator, FVizSize begin, FVizSize end, uint32_t depth,
                                               int32_t* out_node)
{
    FVizPointLocatorNode node;
    FVizBounds bounds = fviz_bounds_empty();
    FVizSize i;
    FVizVec3 size;
    int axis;
    int32_t node_index;
    for (i = begin; i < end; ++i)
        fviz_bounds_include_bounds(&bounds, &locator->cell_bounds[locator->cell_ids[i]]);
    node.bounds = bounds;
    node.left = -1;
    node.right = -1;
    node.begin = begin;
    node.end = end;
    if (end - begin <= 12u || depth >= 32u) return fviz_locator_add_node(locator, &node, out_node);
    size = fviz_bounds_size(&bounds);
    axis = size.x >= size.y && size.x >= size.z ? 0 : (size.y >= size.z ? 1 : 2);
    {
        const FVizSize middle = begin + (end - begin) / 2u;
        fviz_locator_quickselect(locator->cell_ids, begin, end, middle, axis, locator->cell_centroids);
        if (middle == begin || middle == end) return fviz_locator_add_node(locator, &node, out_node);
        int32_t left = -1;
        int32_t right = -1;
        node.begin = node.end = 0u;
        if (fviz_locator_add_node(locator, &node, &node_index) != FVIZ_OK) return fviz_last_error_code();
        if (fviz_locator_build_recursive(locator, begin, middle, depth + 1u, &left) != FVIZ_OK ||
            fviz_locator_build_recursive(locator, middle, end, depth + 1u, &right) != FVIZ_OK)
            return fviz_last_error_code();
        locator->nodes[node_index].left = left;
        locator->nodes[node_index].right = right;
        *out_node = node_index;
    }
    return FVIZ_OK;
}

static FVizResult fviz_point_locator_update_cell_bounds(FVizPointLocator* locator, FVizBool initialize_ids)
{
    const FVizCellArray* cells = fviz_unstructured_grid_cells(locator->grid);
    const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(locator->grid));
    const FVizSize* offsets = fviz_cell_array_offsets(cells);
    const uint32_t* ids32 = fviz_cell_array_connectivity(cells);
    const uint64_t* ids64 = fviz_cell_array_connectivity64(cells);
    const FVizIdStorage storage = fviz_cell_array_id_storage(cells);
    const FVizSize point_count = fviz_unstructured_grid_point_count(locator->grid);
    FVizSize cell_id;
    if (offsets == NULL || points == NULL || (storage == FVIZ_ID_STORAGE_UINT32 && ids32 == NULL) ||
        (storage == FVIZ_ID_STORAGE_UINT64 && ids64 == NULL))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "point locator connectivity storage is unavailable");
        return FVIZ_ERROR_INVALID_STATE;
    }
    for (cell_id = 0u; cell_id < locator->cell_count; ++cell_id)
    {
        FVizBounds bounds = fviz_bounds_empty();
        FVizSize cursor;
        if (initialize_ids != FVIZ_FALSE) locator->cell_ids[cell_id] = cell_id;
        for (cursor = offsets[cell_id]; cursor < offsets[cell_id + 1u]; ++cursor)
        {
            const FVizId point_id = storage == FVIZ_ID_STORAGE_UINT64 ? (FVizId)ids64[cursor] : (FVizId)ids32[cursor];
            if (point_id >= point_count)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "point locator found invalid cell connectivity");
                return FVIZ_ERROR_INVALID_STATE;
            }
            fviz_bounds_include_point(&bounds, points[(FVizSize)point_id]);
        }
        locator->cell_bounds[cell_id] = bounds;
        locator->cell_centroids[cell_id] = fviz_bounds_center(&bounds);
    }
    return FVIZ_OK;
}

FVizResult fviz_point_locator_build(FVizPointLocator* locator)
{
    FVizSize cell_count;
    FVizSize bytes;
    int32_t root = -1;
    if (locator == NULL || locator->grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "point locator has no grid to build");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_unstructured_grid_validate(locator->grid) != FVIZ_OK) return fviz_last_error_code();
    fviz_point_locator_clear_acceleration(locator);
    cell_count = fviz_unstructured_grid_cell_count(locator->grid);
    if (cell_count == 0u)
    {
        fviz_point_locator_capture_source_mtimes(locator);
        return FVIZ_OK;
    }
    if (fviz_size_multiply(cell_count, sizeof(*locator->cell_ids), &bytes) != FVIZ_OK) goto fail;
    locator->cell_ids = (FVizSize*)fviz_alloc(bytes);
    if (fviz_size_multiply(cell_count, sizeof(*locator->cell_bounds), &bytes) != FVIZ_OK) goto fail;
    locator->cell_bounds = (FVizBounds*)fviz_alloc(bytes);
    if (fviz_size_multiply(cell_count, sizeof(*locator->cell_centroids), &bytes) != FVIZ_OK) goto fail;
    locator->cell_centroids = (FVizVec3*)fviz_alloc(bytes);
    if (locator->cell_ids == NULL || locator->cell_bounds == NULL || locator->cell_centroids == NULL) goto fail;
    locator->cell_count = cell_count;
    if (fviz_point_locator_update_cell_bounds(locator, FVIZ_TRUE) != FVIZ_OK) goto fail;
    {
        FVizSize estimated_nodes = cell_count / 4u + 64u;
        if (estimated_nodes < cell_count / 8u) estimated_nodes = cell_count;
        if (fviz_locator_reserve_nodes(locator, estimated_nodes) != FVIZ_OK) goto fail;
    }
    if (fviz_locator_build_recursive(locator, 0u, cell_count, 0u, &root) != FVIZ_OK) goto fail;
    if (root != 0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "point locator hierarchy root is invalid");
        goto fail;
    }
    fviz_point_locator_capture_source_mtimes(locator);
    fviz_object_modified((FVizObject*)locator);
    return FVIZ_OK;
fail:
    fviz_point_locator_clear_acceleration(locator);
    return fviz_last_error_code();
}

FVizResult fviz_point_locator_refit(FVizPointLocator* locator)
{
    FVizSize node_index;
    if (locator == NULL || locator->grid == NULL || locator->nodes == NULL || locator->node_count == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "point locator must be built before refit");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_unstructured_grid_validate(locator->grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_point_locator_cells_mtime(locator) != locator->build_cells_mtime)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                "point locator refit requires unchanged cell connectivity; call update or rebuild");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_unstructured_grid_cell_count(locator->grid) != locator->cell_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                "point locator refit requires an unchanged cell count; rebuild after topology growth");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_point_locator_update_cell_bounds(locator, FVIZ_FALSE) != FVIZ_OK) return fviz_last_error_code();
    /* Parent nodes are allocated before children, so reverse traversal updates
       children before recomputing their parent bounds. */
    for (node_index = locator->node_count; node_index > 0u; --node_index)
    {
        FVizPointLocatorNode* node = &locator->nodes[node_index - 1u];
        FVizBounds bounds = fviz_bounds_empty();
        if (node->left < 0 && node->right < 0)
        {
            FVizSize i;
            for (i = node->begin; i < node->end; ++i)
                fviz_bounds_include_bounds(&bounds, &locator->cell_bounds[locator->cell_ids[i]]);
        }
        else
        {
            if (node->left >= 0) fviz_bounds_include_bounds(&bounds, &locator->nodes[node->left].bounds);
            if (node->right >= 0) fviz_bounds_include_bounds(&bounds, &locator->nodes[node->right].bounds);
        }
        node->bounds = bounds;
    }
    fviz_point_locator_capture_source_mtimes(locator);
    fviz_object_modified((FVizObject*)locator);
    return FVIZ_OK;
}

FVizResult fviz_point_locator_update(FVizPointLocator* locator)
{
    if (locator == NULL || locator->grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "point locator has no grid to update");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (locator->build_cells_mtime != fviz_point_locator_cells_mtime(locator)) return fviz_point_locator_build(locator);
    if (locator->build_points_mtime != fviz_point_locator_points_mtime(locator))
    {
        if (locator->cell_count == 0u)
        {
            fviz_point_locator_capture_source_mtimes(locator);
            return FVIZ_OK;
        }
        return fviz_point_locator_refit(locator);
    }
    return FVIZ_OK;
}

FVizBool fviz_point_locator_refit_required(const FVizPointLocator* locator)
{
    return locator != NULL && locator->grid != NULL &&
                   locator->build_cells_mtime == fviz_point_locator_cells_mtime(locator) &&
                   locator->build_points_mtime != fviz_point_locator_points_mtime(locator)
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizBool fviz_point_locator_acceleration_valid(const FVizPointLocator* locator)
{
    if (locator == NULL || locator->grid == NULL) return FVIZ_FALSE;
    return locator->build_mtime == fviz_point_locator_grid_geometry_mtime(locator) &&
                   (locator->cell_count == 0u || (locator->nodes != NULL && locator->node_count != 0u))
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizSize fviz_point_locator_indexed_cell_count(const FVizPointLocator* locator)
{
    return locator != NULL && fviz_point_locator_acceleration_valid(locator) != FVIZ_FALSE ? locator->cell_count : 0u;
}

FVizResult fviz_point_locator_set_grid(FVizPointLocator* locator, const FVizUnstructuredGrid* grid)
{
    FVizResult result;
    if (locator == NULL || grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "locator and grid must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_retain((FVizUnstructuredGrid*)grid) == NULL) return fviz_last_error_code();
    fviz_point_locator_clear_acceleration(locator);
    fviz_release(locator->grid);
    locator->grid = (FVizUnstructuredGrid*)grid;
    result = fviz_point_locator_build(locator);
    return result;
}

const FVizUnstructuredGrid* fviz_point_locator_const_grid(const FVizPointLocator* locator)
{
    return locator != NULL ? locator->grid : NULL;
}

static FVizBool fviz_cell_barycentric_tetra(const FVizVec3* p, FVizVec3 query, FVizVec3* out_bary)
{
    const FVizVec3 v0 = p[1];
    const FVizVec3 v1 = p[2];
    const FVizVec3 v2 = p[3];
    const FVizVec3 v3 = p[0];
    const FVizMat3 m = fviz_mat3_from_columns(fviz_vec3_sub(v0, v3), fviz_vec3_sub(v1, v3), fviz_vec3_sub(v2, v3));
    const FVizMat3 inv = fviz_mat3_inverse(m);
    const FVizVec3 r = fviz_vec3_sub(query, v3);
    const FVizVec3 bary = fviz_mat3_transform_vec3(inv, r);
    if (bary.x < -1.0e-5f || bary.y < -1.0e-5f || bary.z < -1.0e-5f || bary.x + bary.y + bary.z > 1.0f + 1.0e-5f)
    {
        return FVIZ_FALSE;
    }
    out_bary->x = bary.x;
    out_bary->y = bary.y;
    out_bary->z = bary.z;
    return FVIZ_TRUE;
}

static FVizBool fviz_cell_contains_hex(const FVizVec3* p, FVizVec3 query, FVizVec3* out_parametric)
{
    float r = 0.0f;
    float s = 0.0f;
    float t = 0.0f;
    int iteration;
    for (iteration = 0; iteration < 12; ++iteration)
    {
        FVizVec3 f = fviz_vec3(0.0f, 0.0f, 0.0f);
        FVizMat3 jacobian;
        int k;
        (void)memset(&jacobian, 0, sizeof(jacobian));
        for (k = 0; k < 8; ++k)
        {
            const float kr = g_fviz_hex8_signs[k][0];
            const float ks = g_fviz_hex8_signs[k][1];
            const float kt = g_fviz_hex8_signs[k][2];
            const float one_r = 1.0f + kr * r;
            const float one_s = 1.0f + ks * s;
            const float one_t = 1.0f + kt * t;
            const float weight = one_r * one_s * one_t * 0.125f;
            const float dRw = kr * one_s * one_t * 0.125f;
            const float dSw = one_r * ks * one_t * 0.125f;
            const float dTw = one_r * one_s * kt * 0.125f;
            f = fviz_vec3_add(f, fviz_vec3_scale(p[k], weight));
            jacobian.m[0] += dRw * p[k].x;
            jacobian.m[1] += dRw * p[k].y;
            jacobian.m[2] += dRw * p[k].z;
            jacobian.m[3] += dSw * p[k].x;
            jacobian.m[4] += dSw * p[k].y;
            jacobian.m[5] += dSw * p[k].z;
            jacobian.m[6] += dTw * p[k].x;
            jacobian.m[7] += dTw * p[k].y;
            jacobian.m[8] += dTw * p[k].z;
        }
        f = fviz_vec3_sub(f, query);
        if (fabsf(f.x) < 1.0e-6f && fabsf(f.y) < 1.0e-6f && fabsf(f.z) < 1.0e-6f) break;
        {
            const FVizMat3 jacobian_t = fviz_mat3_transpose(jacobian);
            const FVizMat3 inv = fviz_mat3_inverse(jacobian_t);
            const FVizVec3 delta = fviz_mat3_transform_vec3(inv, f);
            r -= delta.x;
            s -= delta.y;
            t -= delta.z;
        }
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

static FVizVec3 fviz_cell_map_shape(FVizCellType type, const FVizVec3* points, FVizSize point_count,
                                    FVizVec3 parametric, FVizBool* out_ok)
{
    double weights[20];
    FVizSize count = 0u, i;
    double x = 0.0, y = 0.0, z = 0.0;
    if (out_ok != NULL) *out_ok = FVIZ_FALSE;
    if (point_count > 20u || fviz_cell_type_shape_weights(type, parametric, weights, 20u, &count) != FVIZ_OK ||
        count != point_count)
        return fviz_vec3(0, 0, 0);
    for (i = 0u; i < count; ++i)
    {
        x += weights[i] * (double)points[i].x;
        y += weights[i] * (double)points[i].y;
        z += weights[i] * (double)points[i].z;
    }
    if (out_ok != NULL) *out_ok = FVIZ_TRUE;
    return fviz_vec3((float)x, (float)y, (float)z);
}

static FVizBool fviz_cell_contains_high_order(FVizCellType type, const FVizVec3* points, FVizSize point_count,
                                              FVizVec3 query, FVizVec3* out_parametric)
{
    FVizVec3 q = type == FVIZ_CELL_QUADRATIC_TETRA ? fviz_vec3(0.25f, 0.25f, 0.25f) : fviz_vec3(0, 0, 0);
    const float h = 1.0e-4f;
    int iteration;
    FVizBool ok = FVIZ_FALSE;
    for (iteration = 0; iteration < 20; ++iteration)
    {
        FVizVec3 mapped = fviz_cell_map_shape(type, points, point_count, q, &ok);
        FVizVec3 residual;
        FVizVec3 dr, ds, dt;
        FVizVec3 qp, qm;
        FVizMat3 jacobian, inv;
        FVizVec3 delta;
        float norm;
        if (ok == FVIZ_FALSE) return FVIZ_FALSE;
        residual = fviz_vec3_sub(mapped, query);
        norm = sqrtf(residual.x * residual.x + residual.y * residual.y + residual.z * residual.z);
        if (norm < 2.0e-6f) break;
        qp = q;
        qm = q;
        qp.x += h;
        qm.x -= h;
        dr = fviz_vec3_scale(fviz_vec3_sub(fviz_cell_map_shape(type, points, point_count, qp, &ok),
                                           fviz_cell_map_shape(type, points, point_count, qm, &ok)),
                             0.5f / h);
        if (ok == FVIZ_FALSE) return FVIZ_FALSE;
        qp = q;
        qm = q;
        qp.y += h;
        qm.y -= h;
        ds = fviz_vec3_scale(fviz_vec3_sub(fviz_cell_map_shape(type, points, point_count, qp, &ok),
                                           fviz_cell_map_shape(type, points, point_count, qm, &ok)),
                             0.5f / h);
        if (ok == FVIZ_FALSE) return FVIZ_FALSE;
        qp = q;
        qm = q;
        qp.z += h;
        qm.z -= h;
        dt = fviz_vec3_scale(fviz_vec3_sub(fviz_cell_map_shape(type, points, point_count, qp, &ok),
                                           fviz_cell_map_shape(type, points, point_count, qm, &ok)),
                             0.5f / h);
        if (ok == FVIZ_FALSE) return FVIZ_FALSE;
        jacobian = fviz_mat3_from_columns(dr, ds, dt);
        inv = fviz_mat3_inverse(jacobian);
        delta = fviz_mat3_transform_vec3(inv, residual);
        if (!isfinite(delta.x) || !isfinite(delta.y) || !isfinite(delta.z) || fabsf(delta.x) > 4.0f ||
            fabsf(delta.y) > 4.0f || fabsf(delta.z) > 4.0f)
            return FVIZ_FALSE;
        q.x -= delta.x;
        q.y -= delta.y;
        q.z -= delta.z;
    }
    {
        const FVizVec3 mapped = fviz_cell_map_shape(type, points, point_count, q, &ok);
        const FVizVec3 r = fviz_vec3_sub(mapped, query);
        const float norm = sqrtf(r.x * r.x + r.y * r.y + r.z * r.z);
        if (ok == FVIZ_FALSE || norm > 2.0e-4f) return FVIZ_FALSE;
    }
    if (type == FVIZ_CELL_QUADRATIC_TETRA)
    {
        if (q.x < -1.0e-4f || q.y < -1.0e-4f || q.z < -1.0e-4f || q.x + q.y + q.z > 1.0001f) return FVIZ_FALSE;
    }
    else
    {
        if (q.x < -1.0001f || q.x > 1.0001f || q.y < -1.0001f || q.y > 1.0001f || q.z < -1.0001f || q.z > 1.0001f)
            return FVIZ_FALSE;
    }
    *out_parametric = q;
    return FVIZ_TRUE;
}

static FVizBool fviz_cell_point_contains(const FVizCellType type, const FVizVec3* p, FVizVec3 query,
                                         FVizVec3* out_weights, FVizSize* out_point_count)
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
    if (type == FVIZ_CELL_QUADRATIC_TETRA || type == FVIZ_CELL_QUADRATIC_HEXAHEDRON)
    {
        FVizVec3 parametric;
        const FVizSize point_count = type == FVIZ_CELL_QUADRATIC_TETRA ? 10u : 20u;
        if (fviz_cell_contains_high_order(type, p, point_count, query, &parametric) == FVIZ_FALSE) return FVIZ_FALSE;
        *out_weights = parametric;
        *out_point_count = point_count;
        return FVIZ_TRUE;
    }
    (void)p;
    (void)query;
    return FVIZ_FALSE;
}

static FVizBool fviz_point_locator_test_cell(const FVizPointLocator* locator, FVizSize cell_id, FVizVec3 point,
                                             FVizLocatedCell* out_result)
{
    const FVizCellArray* cells = fviz_unstructured_grid_cells(locator->grid);
    const FVizCellType type = fviz_cell_array_type(cells, cell_id);
    FVizCellView view;
    const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points(locator->grid));
    FVizVec3 p[20];
    FVizVec3 weights;
    FVizSize contained_point_count;
    FVizSize i;
    if (fviz_cell_array_cell_view(cells, cell_id, &view) != FVIZ_OK || view.point_count == 0u || view.point_count > 20u)
        return FVIZ_FALSE;
    for (i = 0u; i < view.point_count; ++i)
    {
        const FVizId point_id = fviz_cell_view_point_id(&view, i);
        if (point_id >= fviz_unstructured_grid_point_count(locator->grid)) return FVIZ_FALSE;
        p[i] = points[(FVizSize)point_id];
    }
    if (fviz_cell_point_contains(type, p, point, &weights, &contained_point_count) == FVIZ_FALSE) return FVIZ_FALSE;
    out_result->point = point;
    out_result->barycentric = weights;
    out_result->cell_index = cell_id;
    out_result->point_count = contained_point_count;
    return FVIZ_TRUE;
}

FVizBool fviz_point_locator_locate_point(const FVizPointLocator* locator, FVizVec3 point, FVizLocatedCell* out_result)
{
    FVizLocatedCell best = {0};
    FVizBool found = FVIZ_FALSE;
    if (locator == NULL || locator->grid == NULL || out_result == NULL) return FVIZ_FALSE;
    if (fviz_point_locator_acceleration_valid(locator) != FVIZ_FALSE && locator->cell_count != 0u)
    {
        int32_t stack[80];
        FVizSize stack_size = 0u;
        stack[stack_size++] = 0;
        while (stack_size != 0u)
        {
            const int32_t node_index = stack[--stack_size];
            const FVizPointLocatorNode* node;
            if (node_index < 0 || (FVizSize)node_index >= locator->node_count) continue;
            node = &locator->nodes[node_index];
            if (fviz_locator_bounds_contains(&node->bounds, point) == FVIZ_FALSE) continue;
            if (node->left < 0 && node->right < 0)
            {
                FVizSize i;
                for (i = node->begin; i < node->end; ++i)
                {
                    const FVizSize cell_id = locator->cell_ids[i];
                    FVizLocatedCell candidate;
                    if (fviz_locator_bounds_contains(&locator->cell_bounds[cell_id], point) == FVIZ_FALSE) continue;
                    if (fviz_point_locator_test_cell(locator, cell_id, point, &candidate) != FVIZ_FALSE &&
                        (found == FVIZ_FALSE || cell_id < best.cell_index))
                    {
                        best = candidate;
                        found = FVIZ_TRUE;
                    }
                }
            }
            else
            {
                if (node->right >= 0 && stack_size < sizeof(stack) / sizeof(stack[0]))
                    stack[stack_size++] = node->right;
                if (node->left >= 0 && stack_size < sizeof(stack) / sizeof(stack[0])) stack[stack_size++] = node->left;
            }
        }
        if (found != FVIZ_FALSE)
        {
            *out_result = best;
            return FVIZ_TRUE;
        }
        return FVIZ_FALSE;
    }
    else
    {
        FVizSize cell_id;
        for (cell_id = 0u; cell_id < fviz_unstructured_grid_cell_count(locator->grid); ++cell_id)
            if (fviz_point_locator_test_cell(locator, cell_id, point, out_result) != FVIZ_FALSE) return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

static FVizBool fviz_interpolate_point_scalar(const FVizUnstructuredGrid* grid, const FVizDataArray* scalar,
                                              const FVizLocatedCell* location, float* out_value)
{
    const FVizCellArray* cells = fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid);
    const FVizCellType type = fviz_cell_array_type(cells, location->cell_index);
    FVizCellView view;
    double values[20];
    FVizSize i;
    if (fviz_data_array_tuple_count(scalar) != fviz_unstructured_grid_point_count(grid) ||
        fviz_cell_array_cell_view(cells, location->cell_index, &view) != FVIZ_OK ||
        view.point_count < location->point_count)
        return FVIZ_FALSE;
    for (i = 0u; i < location->point_count; ++i)
    {
        const FVizId point_id = fviz_cell_view_point_id(&view, i);
        double value = 0.0;
        if (point_id >= fviz_unstructured_grid_point_count(grid) ||
            !fviz_locator_scalar_value(scalar, (FVizSize)point_id, &value))
            return FVIZ_FALSE;
        values[i] = value;
    }
    {
        double weights[20] = {0.0};
        FVizSize weight_count = 0u;
        double sum = 0.0;
        if (fviz_cell_type_shape_weights(type, location->barycentric, weights, 20u, &weight_count) != FVIZ_OK ||
            weight_count != location->point_count)
            return FVIZ_FALSE;
        for (i = 0u; i < weight_count; ++i)
            sum += weights[i] * values[i];
        *out_value = (float)sum;
        return FVIZ_TRUE;
    }
}

FVizResult fviz_point_locator_interpolate_scalar(const FVizPointLocator* locator, const char* scalar_name,
                                                 FVizVec3 point, float* out_value)
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

FVizVec3 fviz_point_locator_interpolate_vector(const FVizPointLocator* locator, const char* vector_name, FVizVec3 point)
{
    FVizLocatedCell location;
    const FVizDataArray* vectors;
    const FVizCellArray* cells;
    FVizCellView view;
    double weights[20] = {0.0};
    FVizSize weight_count = 0u;
    FVizSize i;
    double vx = 0.0;
    double vy = 0.0;
    double vz = 0.0;
    if (locator == NULL || locator->grid == NULL || vector_name == NULL) return fviz_vec3(0.0f, 0.0f, 0.0f);
    vectors = fviz_attribute_set_const_get(fviz_unstructured_grid_point_data(locator->grid), vector_name);
    if (vectors == NULL || fviz_data_array_components(vectors) != 3u ||
        fviz_data_array_tuple_count(vectors) != fviz_unstructured_grid_point_count(locator->grid))
        return fviz_vec3(0.0f, 0.0f, 0.0f);
    if (fviz_point_locator_locate_point(locator, point, &location) == FVIZ_FALSE) return fviz_vec3(0.0f, 0.0f, 0.0f);
    cells = fviz_unstructured_grid_cells(locator->grid);
    if (fviz_cell_array_cell_view(cells, location.cell_index, &view) != FVIZ_OK ||
        view.point_count < location.point_count ||
        fviz_cell_type_shape_weights(fviz_cell_array_type(cells, location.cell_index), location.barycentric, weights,
                                     20u, &weight_count) != FVIZ_OK ||
        weight_count != location.point_count)
        return fviz_vec3(0.0f, 0.0f, 0.0f);
    for (i = 0u; i < weight_count; ++i)
    {
        const FVizId point_id = fviz_cell_view_point_id(&view, i);
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        if (point_id >= fviz_unstructured_grid_point_count(locator->grid) ||
            !fviz_locator_component_value(vectors, (FVizSize)point_id, 0u, &x) ||
            !fviz_locator_component_value(vectors, (FVizSize)point_id, 1u, &y) ||
            !fviz_locator_component_value(vectors, (FVizSize)point_id, 2u, &z))
            return fviz_vec3(0.0f, 0.0f, 0.0f);
        vx += weights[i] * x;
        vy += weights[i] * y;
        vz += weights[i] * z;
    }
    return fviz_vec3((float)vx, (float)vy, (float)vz);
}
