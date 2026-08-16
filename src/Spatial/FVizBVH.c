#include <limits.h>
#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Parallel/FVizParallel.h>
#include <FViz/Spatial/FVizBVH.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Spatial/FVizBVHPrivate.h>

static void fviz_bvh_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_bvh_class = {FVIZ_TYPE_BVH, "FVizBVH", &g_fviz_object_class, fviz_bvh_destroy,
                                                 NULL};

static FVizBounds fviz_triangle_bounds(const FVizVec3* points, const uint32_t* ids)
{
    FVizBounds bounds = fviz_bounds_empty();
    fviz_bounds_include_point(&bounds, points[ids[0]]);
    fviz_bounds_include_point(&bounds, points[ids[1]]);
    fviz_bounds_include_point(&bounds, points[ids[2]]);
    return bounds;
}

typedef struct FVizBVHPrimitiveRange
{
    const FVizVec3* points;
    const uint32_t* indices;
    uint32_t* primitive_ids;
    FVizBounds* bounds;
    FVizVec3* centroids;
} FVizBVHPrimitiveRange;

static void fviz_bvh_initialize_primitive_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizBVHPrimitiveRange* range = (FVizBVHPrimitiveRange*)user_data;
    FVizSize i;
    for (i = begin; i < end; ++i)
    {
        const uint32_t a = range->indices[i * 3u + 0u];
        const uint32_t b = range->indices[i * 3u + 1u];
        const uint32_t c = range->indices[i * 3u + 2u];
        range->primitive_ids[i] = (uint32_t)i;
        range->bounds[i] = fviz_triangle_bounds(range->points, &range->indices[i * 3u]);
        range->centroids[i] = fviz_vec3_scale(
            fviz_vec3_add(fviz_vec3_add(range->points[a], range->points[b]), range->points[c]), 1.0f / 3.0f);
    }
}

static void fviz_bvh_reset_storage(FVizBVH* bvh)
{
    if (bvh == NULL) return;
    fviz_free(bvh->nodes);
    fviz_free(bvh->triangle_indices);
    fviz_free(bvh->triangle_centroids);
    fviz_release(bvh->poly_data);
    bvh->nodes = NULL;
    bvh->node_count = 0u;
    bvh->node_capacity = 0u;
    bvh->root = -1;
    bvh->triangle_indices = NULL;
    bvh->triangle_count = 0u;
    bvh->triangle_capacity = 0u;
    bvh->triangle_bounds_cache = NULL;
    bvh->triangle_centroids = NULL;
    bvh->poly_data = NULL;
    bvh->source_geometry_mtime = 0u;
    bvh->source_topology_mtime = 0u;
    bvh->valid = FVIZ_FALSE;
}

static void fviz_bvh_destroy(FVizObject* object)
{
    fviz_bvh_reset_storage((FVizBVH*)object);
}

static FVizResult fviz_bvh_reserve_nodes(FVizBVH* bvh, FVizSize capacity)
{
    FVizBVHNode* new_nodes;
    FVizSize bytes;
    if (capacity <= bvh->node_capacity) return FVIZ_OK;
    if (fviz_size_multiply(capacity, sizeof(FVizBVHNode), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    new_nodes = (FVizBVHNode*)fviz_realloc(bvh->nodes, bytes);
    if (new_nodes == NULL) return fviz_last_error_code();
    bvh->nodes = new_nodes;
    bvh->node_capacity = capacity;
    return FVIZ_OK;
}

static int32_t fviz_bvh_add_node(FVizBVH* bvh, const FVizBVHNode* node)
{
    if (bvh->node_count == bvh->node_capacity)
    {
        FVizSize new_capacity;
        if (bvh->node_capacity == 0u) new_capacity = 32u;
        else
        {
            if (bvh->node_capacity > ((FVizSize)-1) / 2u)
            {
                fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "BVH node capacity overflow");
                return -1;
            }
            new_capacity = bvh->node_capacity * 2u;
        }
        if (fviz_bvh_reserve_nodes(bvh, new_capacity) != FVIZ_OK) return -1;
    }
    bvh->nodes[bvh->node_count] = *node;
    return (int32_t)bvh->node_count++;
}

static float fviz_bvh_centroid_axis(const FVizVec3* centroids, uint32_t primitive_id, int axis)
{
    if (axis == 0) return centroids[primitive_id].x;
    if (axis == 1) return centroids[primitive_id].y;
    return centroids[primitive_id].z;
}

static int32_t fviz_bvh_partition(uint32_t* indices, int32_t begin, int32_t end, int32_t pivot_index, int axis,
                                  const FVizVec3* centroids)
{
    const uint32_t pivot_id = indices[pivot_index];
    const float pivot_value = fviz_bvh_centroid_axis(centroids, pivot_id, axis);
    int32_t store = begin;
    int32_t i;
    uint32_t swap = indices[pivot_index];
    indices[pivot_index] = indices[end - 1];
    indices[end - 1] = swap;
    for (i = begin; i < end - 1; ++i)
    {
        const uint32_t id = indices[i];
        const float value = fviz_bvh_centroid_axis(centroids, id, axis);
        if (value < pivot_value || (value == pivot_value && id < pivot_id))
        {
            swap = indices[store];
            indices[store] = indices[i];
            indices[i] = swap;
            ++store;
        }
    }
    swap = indices[store];
    indices[store] = indices[end - 1];
    indices[end - 1] = swap;
    return store;
}

static void fviz_bvh_quickselect(uint32_t* indices, int32_t begin, int32_t end, int32_t target, int axis,
                                 const FVizVec3* centroids)
{
    while (end - begin > 1)
    {
        const int32_t pivot_hint = begin + (end - begin) / 2;
        const int32_t pivot = fviz_bvh_partition(indices, begin, end, pivot_hint, axis, centroids);
        if (pivot == target) return;
        if (target < pivot) end = pivot;
        else
            begin = pivot + 1;
    }
}

static FVizResult fviz_bvh_build_recursive(FVizBVH* bvh, uint32_t* primitive_ids, int32_t begin, int32_t end, int depth,
                                           int32_t* out_node)
{
    FVizBounds bounds = fviz_bounds_empty();
    FVizVec3 extent;
    FVizBVHNode node;
    int32_t k;
    int axis;

    for (k = begin; k < end; ++k)
    {
        fviz_bounds_include_bounds(&bounds, &bvh->triangle_bounds_cache[primitive_ids[k]]);
    }
    node.bounds = bounds;
    node.left = -1;
    node.right = -1;
    node.triangle_begin = -1;
    node.triangle_end = -1;

    if ((uint32_t)(end - begin) <= FVIZ_BVH_LEAF_SIZE || (uint32_t)depth >= FVIZ_BVH_MAX_DEPTH)
    {
        node.triangle_begin = (int32_t)bvh->triangle_count;
        node.triangle_end = node.triangle_begin + (end - begin);
        for (k = begin; k < end; ++k)
        {
            if (bvh->triangle_count >= bvh->triangle_capacity)
            {
                FVizSize new_capacity;
                FVizSize bytes;
                uint32_t* new_ids;
                if (bvh->triangle_capacity == 0u) new_capacity = 64u;
                else
                {
                    if (bvh->triangle_capacity > ((FVizSize)-1) / 2u)
                    {
                        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "BVH triangle capacity overflow");
                        return FVIZ_ERROR_OVERFLOW;
                    }
                    new_capacity = bvh->triangle_capacity * 2u;
                }
                if (fviz_size_multiply(new_capacity, sizeof(uint32_t), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
                new_ids = (uint32_t*)fviz_realloc(bvh->triangle_indices, bytes);
                if (new_ids == NULL) return fviz_last_error_code();
                bvh->triangle_indices = new_ids;
                bvh->triangle_capacity = new_capacity;
            }
            bvh->triangle_indices[bvh->triangle_count++] = primitive_ids[k];
        }
        *out_node = fviz_bvh_add_node(bvh, &node);
        return *out_node >= 0 ? FVIZ_OK : FVIZ_ERROR_INTERNAL;
    }

    extent = fviz_vec3_sub(bounds.max, bounds.min);
    if (extent.x >= extent.y && extent.x >= extent.z) axis = 0;
    else if (extent.y >= extent.z)
        axis = 1;
    else
        axis = 2;

    {
        int32_t mid = begin + (end - begin) / 2;
        fviz_bvh_quickselect(primitive_ids, begin, end, mid, axis, bvh->triangle_centroids);
        int32_t left_node;
        int32_t right_node;
        if (fviz_bvh_build_recursive(bvh, primitive_ids, begin, mid, depth + 1, &left_node) != FVIZ_OK)
        {
            return fviz_last_error_code();
        }
        if (fviz_bvh_build_recursive(bvh, primitive_ids, mid, end, depth + 1, &right_node) != FVIZ_OK)
        {
            return fviz_last_error_code();
        }
        node.left = left_node;
        node.right = right_node;
    }
    *out_node = fviz_bvh_add_node(bvh, &node);
    return *out_node >= 0 ? FVIZ_OK : FVIZ_ERROR_INTERNAL;
}

FVizResult fviz_bvh_create(FVizBVH** out_bvh)
{
    FVizBVH* bvh;
    if (out_bvh == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_bvh must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_bvh = NULL;
    bvh = (FVizBVH*)fviz_internal_object_allocate(sizeof(FVizBVH), &g_fviz_bvh_class, NULL);
    if (bvh == NULL) return fviz_last_error_code();
    bvh->root = -1;
    bvh->valid = FVIZ_FALSE;
    *out_bvh = bvh;
    return FVIZ_OK;
}

FVizResult fviz_bvh_build(FVizBVH* bvh, const FVizPolyData* poly_data)
{
    const FVizVec3* points;
    const uint32_t* indices;
    FVizSize triangle_count;
    uint32_t* primitive_ids;
    FVizBounds* bounds_cache;
    FVizBVHPrimitiveRange primitive_range;
    FVizSize primitive_bytes;
    FVizSize bounds_bytes;
    FVizSize centroid_bytes;

    if (bvh == NULL || poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "bvh and poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    triangle_count = fviz_poly_data_triangle_count(poly_data);
    bvh->valid = FVIZ_FALSE;
    bvh->triangle_bounds_cache = NULL;
    if (triangle_count == 0u)
    {
        fviz_bvh_reset_storage(bvh);
        fviz_object_modified((FVizObject*)bvh);
        return FVIZ_OK;
    }
    if (triangle_count > (FVizSize)INT32_MAX || triangle_count > (FVizSize)UINT32_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "BVH currently supports at most INT32_MAX triangles");
        return FVIZ_ERROR_OVERFLOW;
    }
    points = fviz_poly_data_points(poly_data);
    indices = fviz_poly_data_triangle_indices(poly_data);
    if (points == NULL || indices == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "poly_data has no point or index data");
        return FVIZ_ERROR_INVALID_STATE;
    }

    if (fviz_size_multiply(triangle_count, sizeof(uint32_t), &primitive_bytes) != FVIZ_OK ||
        fviz_size_multiply(triangle_count, sizeof(FVizBounds), &bounds_bytes) != FVIZ_OK ||
        fviz_size_multiply(triangle_count, sizeof(FVizVec3), &centroid_bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    primitive_ids = (uint32_t*)fviz_alloc(primitive_bytes);
    bounds_cache = (FVizBounds*)fviz_alloc(bounds_bytes);
    if (primitive_ids == NULL || bounds_cache == NULL)
    {
        fviz_free(primitive_ids);
        fviz_free(bounds_cache);
        return fviz_last_error_code();
    }

    fviz_bvh_reset_storage(bvh);

    bvh->triangle_centroids = (FVizVec3*)fviz_alloc(centroid_bytes);
    bvh->triangle_indices = (uint32_t*)fviz_alloc(primitive_bytes);
    if (bvh->triangle_centroids == NULL || bvh->triangle_indices == NULL)
    {
        fviz_free(bvh->triangle_centroids);
        fviz_free(bvh->triangle_indices);
        bvh->triangle_centroids = NULL;
        bvh->triangle_indices = NULL;
        fviz_free(primitive_ids);
        fviz_free(bounds_cache);
        return fviz_last_error_code();
    }
    bvh->triangle_capacity = triangle_count;
    primitive_range.points = points;
    primitive_range.indices = indices;
    primitive_range.primitive_ids = primitive_ids;
    primitive_range.bounds = bounds_cache;
    primitive_range.centroids = bvh->triangle_centroids;
    if (fviz_parallel_for(0u, triangle_count, 512u, fviz_bvh_initialize_primitive_range, &primitive_range) != FVIZ_OK)
    {
        fviz_free(primitive_ids);
        fviz_free(bounds_cache);
        return fviz_last_error_code();
    }

    {
        int32_t root;
        bvh->poly_data = (FVizPolyData*)fviz_retain((FVizPolyData*)poly_data);
        {
            FVizSize estimated_leaves = (triangle_count + FVIZ_BVH_LEAF_SIZE - 1u) / FVIZ_BVH_LEAF_SIZE;
            FVizSize estimated_nodes;
            if (estimated_leaves < 32u) estimated_leaves = 32u;
            if (estimated_leaves > ((FVizSize)-1) / 4u) estimated_nodes = triangle_count;
            else
                estimated_nodes = estimated_leaves * 4u;
            if (estimated_nodes < 64u) estimated_nodes = 64u;
            if (fviz_bvh_reserve_nodes(bvh, estimated_nodes) != FVIZ_OK)
            {
                fviz_free(primitive_ids);
                fviz_free(bounds_cache);
                return fviz_last_error_code();
            }
        }
        bvh->triangle_bounds_cache = bounds_cache;
        if (fviz_bvh_build_recursive(bvh, primitive_ids, 0, (int32_t)triangle_count, 0, &root) != FVIZ_OK)
        {
            bvh->triangle_bounds_cache = NULL;
            fviz_free(primitive_ids);
            fviz_free(bounds_cache);
            return fviz_last_error_code();
        }
        bvh->root = root;
        bvh->valid = FVIZ_TRUE;
        bvh->source_geometry_mtime = fviz_poly_data_geometry_mtime(poly_data);
        bvh->source_topology_mtime = fviz_poly_data_topology_mtime(poly_data);
        bvh->triangle_bounds_cache = NULL;
    }
    fviz_free(primitive_ids);
    fviz_free(bounds_cache);
    fviz_object_modified((FVizObject*)bvh);
    return FVIZ_OK;
}

typedef struct FVizBVHRefitContext
{
    FVizBVH* bvh;
    const FVizVec3* points;
    const uint32_t* indices;
} FVizBVHRefitContext;

static void fviz_bvh_refit_leaf_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizBVHRefitContext* context = (FVizBVHRefitContext*)user_data;
    FVizSize node_index;
    for (node_index = begin; node_index < end; ++node_index)
    {
        FVizBVHNode* node = &context->bvh->nodes[node_index];
        FVizBounds bounds = fviz_bounds_empty();
        int32_t i;
        if (node->triangle_begin < 0) continue;
        for (i = node->triangle_begin; i < node->triangle_end; ++i)
        {
            const uint32_t triangle = context->bvh->triangle_indices[i];
            const FVizBounds triangle_bounds =
                fviz_triangle_bounds(context->points, &context->indices[(FVizSize)triangle * 3u]);
            fviz_bounds_include_bounds(&bounds, &triangle_bounds);
        }
        node->bounds = bounds;
    }
}

FVizResult fviz_bvh_refit(FVizBVH* bvh)
{
    const FVizVec3* points;
    const uint32_t* indices;
    FVizSize triangle_count;
    FVizSize node_index;
    FVizBVHRefitContext context;
    if (bvh == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "bvh must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (bvh->valid == FVIZ_FALSE || bvh->poly_data == NULL || bvh->root < 0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "BVH must be built before refit");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_poly_data_topology_mtime(bvh->poly_data) != bvh->source_topology_mtime)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                "BVH refit requires unchanged topology; call fviz_bvh_update or rebuild");
        return FVIZ_ERROR_INVALID_STATE;
    }
    triangle_count = fviz_poly_data_triangle_count(bvh->poly_data);
    if (triangle_count != bvh->triangle_count)
    {
        fviz_internal_set_error(
            FVIZ_ERROR_INVALID_STATE,
            "BVH refit requires an unchanged triangle count; rebuild the BVH after topology changes");
        return FVIZ_ERROR_INVALID_STATE;
    }
    points = fviz_poly_data_points(bvh->poly_data);
    indices = fviz_poly_data_triangle_indices(bvh->poly_data);
    if (points == NULL || indices == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "BVH source geometry is unavailable");
        return FVIZ_ERROR_INVALID_STATE;
    }

    context.bvh = bvh;
    context.points = points;
    context.indices = indices;
    if (fviz_parallel_for(0u, bvh->node_count, 128u, fviz_bvh_refit_leaf_range, &context) != FVIZ_OK)
        return fviz_last_error_code();
    /* Build recursion appends children before their parent. Leaf work is
       independent and parallel above; internal nodes reduce in dependency order. */
    for (node_index = 0u; node_index < bvh->node_count; ++node_index)
    {
        FVizBVHNode* node = &bvh->nodes[node_index];
        FVizBounds bounds = fviz_bounds_empty();
        if (node->triangle_begin < 0)
        {
            if (node->left >= 0) fviz_bounds_include_bounds(&bounds, &bvh->nodes[node->left].bounds);
            if (node->right >= 0) fviz_bounds_include_bounds(&bounds, &bvh->nodes[node->right].bounds);
            node->bounds = bounds;
        }
    }
    bvh->source_geometry_mtime = fviz_poly_data_geometry_mtime(bvh->poly_data);
    fviz_object_modified((FVizObject*)bvh);
    return FVIZ_OK;
}

FVizResult fviz_bvh_update(FVizBVH* bvh)
{
    FVizPolyData* source;
    FVizResult result;
    if (bvh == NULL || bvh->valid == FVIZ_FALSE || bvh->poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "BVH must have a non-empty source before update");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_poly_data_topology_mtime(bvh->poly_data) == bvh->source_topology_mtime)
    {
        if (fviz_poly_data_geometry_mtime(bvh->poly_data) == bvh->source_geometry_mtime) return FVIZ_OK;
        return fviz_bvh_refit(bvh);
    }
    /* Guard the retained source because build() resets the previous BVH storage
     * before retaining its replacement. */
    source = (FVizPolyData*)fviz_retain(bvh->poly_data);
    if (source == NULL) return fviz_last_error_code();
    result = fviz_bvh_build(bvh, source);
    fviz_release(source);
    return result;
}

FVizBool fviz_bvh_valid(const FVizBVH* bvh)
{
    return bvh != NULL ? bvh->valid : FVIZ_FALSE;
}

FVizBool fviz_bvh_current(const FVizBVH* bvh)
{
    return bvh != NULL && bvh->valid != FVIZ_FALSE && bvh->poly_data != NULL &&
                   fviz_poly_data_geometry_mtime(bvh->poly_data) == bvh->source_geometry_mtime &&
                   fviz_poly_data_topology_mtime(bvh->poly_data) == bvh->source_topology_mtime
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizBool fviz_bvh_refit_required(const FVizBVH* bvh)
{
    return bvh != NULL && bvh->valid != FVIZ_FALSE && bvh->poly_data != NULL &&
                   fviz_poly_data_topology_mtime(bvh->poly_data) == bvh->source_topology_mtime &&
                   fviz_poly_data_geometry_mtime(bvh->poly_data) != bvh->source_geometry_mtime
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizSize fviz_bvh_triangle_count(const FVizBVH* bvh)
{
    return bvh != NULL ? bvh->triangle_count : 0u;
}

static FVizBool fviz_ray_box_intersect(FVizRay ray, const FVizBounds* bounds, float* out_t)
{
    float t_min = 0.0f;
    float t_max = 1.0e30f;
    int axis;
    for (axis = 0; axis < 3; ++axis)
    {
        const float origin = axis == 0 ? ray.origin.x : axis == 1 ? ray.origin.y : ray.origin.z;
        const float direction = axis == 0 ? ray.direction.x : axis == 1 ? ray.direction.y : ray.direction.z;
        const float min = axis == 0 ? bounds->min.x : axis == 1 ? bounds->min.y : bounds->min.z;
        const float max = axis == 0 ? bounds->max.x : axis == 1 ? bounds->max.y : bounds->max.z;
        if (fabsf(direction) < 1.0e-8f)
        {
            if (origin < min || origin > max) return FVIZ_FALSE;
            continue;
        }
        {
            const float inv = 1.0f / direction;
            float t0 = (min - origin) * inv;
            float t1 = (max - origin) * inv;
            float temp;
            if (t0 > t1)
            {
                temp = t0;
                t0 = t1;
                t1 = temp;
            }
            if (t0 > t_min) t_min = t0;
            if (t1 < t_max) t_max = t1;
            if (t_min > t_max) return FVIZ_FALSE;
        }
    }
    if (t_min > t_max) return FVIZ_FALSE;
    if (out_t != NULL) *out_t = t_min;
    return FVIZ_TRUE;
}

static FVizBool fviz_ray_triangle_intersect(FVizRay ray, FVizVec3 a, FVizVec3 b, FVizVec3 c, float* out_t,
                                            FVizVec3* out_normal)
{
    const FVizVec3 edge1 = fviz_vec3_sub(b, a);
    const FVizVec3 edge2 = fviz_vec3_sub(c, a);
    const FVizVec3 pvec = fviz_vec3_cross(ray.direction, edge2);
    const float determinant = fviz_vec3_dot(edge1, pvec);
    FVizVec3 tvec;
    FVizVec3 qvec;
    float u;
    float v;
    float t;
    if (fabsf(determinant) < 1.0e-8f) return FVIZ_FALSE;
    {
        const float inv_det = 1.0f / determinant;
        tvec = fviz_vec3_sub(ray.origin, a);
        u = fviz_vec3_dot(tvec, pvec) * inv_det;
        if (u < 0.0f || u > 1.0f) return FVIZ_FALSE;
        qvec = fviz_vec3_cross(tvec, edge1);
        v = fviz_vec3_dot(ray.direction, qvec) * inv_det;
        if (v < 0.0f || u + v > 1.0f) return FVIZ_FALSE;
        t = fviz_vec3_dot(edge2, qvec) * inv_det;
    }
    if (t < 0.0f) return FVIZ_FALSE;
    if (out_t != NULL) *out_t = t;
    if (out_normal != NULL) *out_normal = fviz_vec3_normalize(fviz_vec3_cross(edge1, edge2));
    return FVIZ_TRUE;
}

typedef struct FVizBVHTraversalEntry
{
    int32_t node_index;
    float distance;
} FVizBVHTraversalEntry;

static FVizBool fviz_bvh_ray_cast_node(const FVizBVH* bvh, FVizRay ray, FVizRayHit* out_hit, float* best_distance,
                                       FVizBool any)
{
    FVizBVHTraversalEntry stack[64];
    int32_t stack_size = 0;
    float root_distance;
    if (bvh->root < 0 || fviz_ray_box_intersect(ray, &bvh->nodes[bvh->root].bounds, &root_distance) == FVIZ_FALSE)
        return FVIZ_FALSE;
    stack[stack_size++] = (FVizBVHTraversalEntry){bvh->root, root_distance};
    while (stack_size > 0)
    {
        const FVizBVHTraversalEntry entry = stack[--stack_size];
        const FVizBVHNode* node = &bvh->nodes[entry.node_index];
        if (entry.distance > *best_distance) continue;
        if (node->triangle_begin >= 0)
        {
            int32_t i;
            const FVizPolyData* data = bvh->poly_data;
            const uint32_t* indices = fviz_poly_data_triangle_indices(data);
            const FVizVec3* points = fviz_poly_data_points(data);
            for (i = node->triangle_begin; i < node->triangle_end; ++i)
            {
                const uint32_t tri = bvh->triangle_indices[i];
                float t;
                FVizVec3 normal;
                if (!fviz_ray_triangle_intersect(ray, points[indices[tri * 3u + 0u]], points[indices[tri * 3u + 1u]],
                                                 points[indices[tri * 3u + 2u]], &t, &normal))
                    continue;
                if (t < *best_distance)
                {
                    if (any) return FVIZ_TRUE;
                    *best_distance = t;
                    out_hit->point = fviz_ray_point_at(ray, t);
                    out_hit->normal = normal;
                    out_hit->distance = t;
                    out_hit->triangle_index = tri;
                }
            }
        }
        else
        {
            FVizBVHTraversalEntry children[2];
            int child_count = 0;
            float distance;
            if (node->left >= 0 &&
                fviz_ray_box_intersect(ray, &bvh->nodes[node->left].bounds, &distance) != FVIZ_FALSE &&
                distance <= *best_distance)
                children[child_count++] = (FVizBVHTraversalEntry){node->left, distance};
            if (node->right >= 0 &&
                fviz_ray_box_intersect(ray, &bvh->nodes[node->right].bounds, &distance) != FVIZ_FALSE &&
                distance <= *best_distance)
                children[child_count++] = (FVizBVHTraversalEntry){node->right, distance};
            if (child_count == 2 && children[0].distance > children[1].distance)
            {
                const FVizBVHTraversalEntry temporary = children[0];
                children[0] = children[1];
                children[1] = temporary;
            }
            if (stack_size + child_count > (int32_t)(sizeof(stack) / sizeof(stack[0])))
            {
                fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "BVH traversal stack overflow");
                return FVIZ_FALSE;
            }
            /* LIFO stack: push far child first so the near child is visited next. */
            if (child_count == 2) stack[stack_size++] = children[1];
            if (child_count >= 1) stack[stack_size++] = children[0];
        }
    }
    return out_hit->distance < 1.0e30f;
}

FVizBool fviz_bvh_ray_cast(const FVizBVH* bvh, FVizRay ray, FVizRayHit* out_hit)
{
    float best_distance = 1.0e30f;
    if (bvh == NULL || out_hit == NULL || bvh->valid == FVIZ_FALSE) return FVIZ_FALSE;
    out_hit->distance = 1.0e30f;
    out_hit->triangle_index = 0u;
    return fviz_bvh_ray_cast_node(bvh, ray, out_hit, &best_distance, FVIZ_FALSE);
}

FVizBool fviz_bvh_ray_cast_any(const FVizBVH* bvh, FVizRay ray)
{
    float best_distance = 1.0e30f;
    FVizRayHit hit;
    if (bvh == NULL || bvh->valid == FVIZ_FALSE) return FVIZ_FALSE;
    hit.distance = 1.0e30f;
    return fviz_bvh_ray_cast_node(bvh, ray, &hit, &best_distance, FVIZ_TRUE);
}

static float fviz_bvh_point_bounds_distance_squared(FVizVec3 point, const FVizBounds* bounds)
{
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    if (point.x < bounds->min.x) dx = bounds->min.x - point.x;
    else if (point.x > bounds->max.x)
        dx = point.x - bounds->max.x;
    if (point.y < bounds->min.y) dy = bounds->min.y - point.y;
    else if (point.y > bounds->max.y)
        dy = point.y - bounds->max.y;
    if (point.z < bounds->min.z) dz = bounds->min.z - point.z;
    else if (point.z > bounds->max.z)
        dz = point.z - bounds->max.z;
    return dx * dx + dy * dy + dz * dz;
}

static FVizVec3 fviz_bvh_closest_segment(FVizVec3 point, FVizVec3 a, FVizVec3 b, float* out_t)
{
    const FVizVec3 edge = fviz_vec3_sub(b, a);
    const float length_squared = fviz_vec3_dot(edge, edge);
    float t = length_squared > 1.0e-20f ? fviz_vec3_dot(fviz_vec3_sub(point, a), edge) / length_squared : 0.0f;
    if (t < 0.0f) t = 0.0f;
    else if (t > 1.0f)
        t = 1.0f;
    if (out_t != NULL) *out_t = t;
    return fviz_vec3_add(a, fviz_vec3_scale(edge, t));
}

static FVizVec3 fviz_bvh_closest_triangle(FVizVec3 point, FVizVec3 a, FVizVec3 b, FVizVec3 c, FVizVec3* out_barycentric)
{
    const FVizVec3 ab = fviz_vec3_sub(b, a);
    const FVizVec3 ac = fviz_vec3_sub(c, a);
    const FVizVec3 ap = fviz_vec3_sub(point, a);
    const float d1 = fviz_vec3_dot(ab, ap);
    const float d2 = fviz_vec3_dot(ac, ap);
    const FVizVec3 normal = fviz_vec3_cross(ab, ac);
    if (fviz_vec3_dot(normal, normal) <= 1.0e-20f)
    {
        float tab, tbc, tca;
        const FVizVec3 pab = fviz_bvh_closest_segment(point, a, b, &tab);
        const FVizVec3 pbc = fviz_bvh_closest_segment(point, b, c, &tbc);
        const FVizVec3 pca = fviz_bvh_closest_segment(point, c, a, &tca);
        const float dab = fviz_vec3_dot(fviz_vec3_sub(point, pab), fviz_vec3_sub(point, pab));
        const float dbc = fviz_vec3_dot(fviz_vec3_sub(point, pbc), fviz_vec3_sub(point, pbc));
        const float dca = fviz_vec3_dot(fviz_vec3_sub(point, pca), fviz_vec3_sub(point, pca));
        if (dab <= dbc && dab <= dca)
        {
            *out_barycentric = fviz_vec3(1.0f - tab, tab, 0.0f);
            return pab;
        }
        if (dbc <= dca)
        {
            *out_barycentric = fviz_vec3(0.0f, 1.0f - tbc, tbc);
            return pbc;
        }
        *out_barycentric = fviz_vec3(tca, 0.0f, 1.0f - tca);
        return pca;
    }
    if (d1 <= 0.0f && d2 <= 0.0f)
    {
        *out_barycentric = fviz_vec3(1.0f, 0.0f, 0.0f);
        return a;
    }
    {
        const FVizVec3 bp = fviz_vec3_sub(point, b);
        const float d3 = fviz_vec3_dot(ab, bp);
        const float d4 = fviz_vec3_dot(ac, bp);
        const float vc = d1 * d4 - d3 * d2;
        float vb;
        if (d3 >= 0.0f && d4 <= d3)
        {
            *out_barycentric = fviz_vec3(0.0f, 1.0f, 0.0f);
            return b;
        }
        {
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
            {
                const float v = d1 / (d1 - d3);
                *out_barycentric = fviz_vec3(1.0f - v, v, 0.0f);
                return fviz_vec3_add(a, fviz_vec3_scale(ab, v));
            }
        }
        {
            const FVizVec3 cp = fviz_vec3_sub(point, c);
            const float d5 = fviz_vec3_dot(ab, cp);
            const float d6 = fviz_vec3_dot(ac, cp);
            if (d6 >= 0.0f && d5 <= d6)
            {
                *out_barycentric = fviz_vec3(0.0f, 0.0f, 1.0f);
                return c;
            }
            {
                vb = d5 * d2 - d1 * d6;
                if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
                {
                    const float w = d2 / (d2 - d6);
                    *out_barycentric = fviz_vec3(1.0f - w, 0.0f, w);
                    return fviz_vec3_add(a, fviz_vec3_scale(ac, w));
                }
            }
            {
                const float va = d3 * d6 - d5 * d4;
                if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f)
                {
                    const FVizVec3 bc = fviz_vec3_sub(c, b);
                    const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                    *out_barycentric = fviz_vec3(0.0f, 1.0f - w, w);
                    return fviz_vec3_add(b, fviz_vec3_scale(bc, w));
                }
                {
                    const float denominator = 1.0f / (va + vb + vc);
                    const float v = vb * denominator;
                    const float w = vc * denominator;
                    *out_barycentric = fviz_vec3(1.0f - v - w, v, w);
                    return fviz_vec3_add(a, fviz_vec3_add(fviz_vec3_scale(ab, v), fviz_vec3_scale(ac, w)));
                }
            }
        }
    }
}

FVizResult fviz_bvh_closest_point(const FVizBVH* bvh, FVizVec3 query, float max_distance, FVizClosestPoint* out_result)
{
    FVizBVHTraversalEntry stack[FVIZ_BVH_MAX_DEPTH * 2u + 2u];
    int32_t stack_size = 0;
    const FVizVec3* points;
    const uint32_t* indices;
    float best_distance_squared;
    if (bvh == NULL || out_result == NULL || bvh->valid == FVIZ_FALSE || bvh->root < 0 || !isfinite(max_distance))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    best_distance_squared = max_distance < 0.0f ? 1.0e30f : max_distance * max_distance;
    (void)memset(out_result, 0, sizeof(*out_result));
    out_result->distance_squared = best_distance_squared;
    out_result->triangle_index = SIZE_MAX;
    points = fviz_poly_data_points(bvh->poly_data);
    indices = fviz_poly_data_triangle_indices(bvh->poly_data);
    stack[stack_size++] = (FVizBVHTraversalEntry){
        bvh->root, fviz_bvh_point_bounds_distance_squared(query, &bvh->nodes[bvh->root].bounds)};
    while (stack_size > 0)
    {
        const FVizBVHTraversalEntry entry = stack[--stack_size];
        const FVizBVHNode* node = &bvh->nodes[entry.node_index];
        if (entry.distance > best_distance_squared) continue;
        if (node->triangle_begin >= 0)
        {
            int32_t item;
            for (item = node->triangle_begin; item < node->triangle_end; ++item)
            {
                const uint32_t triangle = bvh->triangle_indices[item];
                const FVizVec3 a = points[indices[(FVizSize)triangle * 3u + 0u]];
                const FVizVec3 b = points[indices[(FVizSize)triangle * 3u + 1u]];
                const FVizVec3 c = points[indices[(FVizSize)triangle * 3u + 2u]];
                FVizVec3 barycentric;
                const FVizVec3 closest = fviz_bvh_closest_triangle(query, a, b, c, &barycentric);
                const FVizVec3 delta = fviz_vec3_sub(query, closest);
                const float distance_squared = fviz_vec3_dot(delta, delta);
                if (distance_squared < best_distance_squared ||
                    (distance_squared == best_distance_squared && (FVizSize)triangle < out_result->triangle_index))
                {
                    best_distance_squared = distance_squared;
                    out_result->point = closest;
                    out_result->normal = fviz_vec3_normalize(fviz_vec3_cross(fviz_vec3_sub(b, a), fviz_vec3_sub(c, a)));
                    out_result->barycentric = barycentric;
                    out_result->distance_squared = distance_squared;
                    out_result->triangle_index = triangle;
                }
            }
        }
        else
        {
            FVizBVHTraversalEntry children[2];
            int count = 0;
            if (node->left >= 0)
            {
                const float distance = fviz_bvh_point_bounds_distance_squared(query, &bvh->nodes[node->left].bounds);
                if (distance <= best_distance_squared)
                    children[count++] = (FVizBVHTraversalEntry){node->left, distance};
            }
            if (node->right >= 0)
            {
                const float distance = fviz_bvh_point_bounds_distance_squared(query, &bvh->nodes[node->right].bounds);
                if (distance <= best_distance_squared)
                    children[count++] = (FVizBVHTraversalEntry){node->right, distance};
            }
            if (count == 2 && children[0].distance > children[1].distance)
            {
                const FVizBVHTraversalEntry temporary = children[0];
                children[0] = children[1];
                children[1] = temporary;
            }
            if (count == 2) stack[stack_size++] = children[1];
            if (count >= 1) stack[stack_size++] = children[0];
        }
    }
    return out_result->triangle_index != SIZE_MAX ? FVIZ_OK : FVIZ_ERROR_NOT_FOUND;
}

typedef struct FVizBVHRayBatchContext
{
    const FVizBVH* bvh;
    const FVizRay* rays;
    FVizRayHit* hits;
    FVizBool* flags;
} FVizBVHRayBatchContext;

static FVizResult fviz_bvh_ray_batch_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizBVHRayBatchContext* context = (FVizBVHRayBatchContext*)user_data;
    FVizSize index;
    for (index = begin; index < end; ++index)
        context->flags[index] = fviz_bvh_ray_cast(context->bvh, context->rays[index], &context->hits[index]);
    return FVIZ_OK;
}

FVizResult fviz_bvh_ray_cast_batch(const FVizBVH* bvh, const FVizRay* rays, FVizSize query_count, FVizRayHit* out_hits,
                                   FVizBool* out_hit_flags, FVizCancellationToken* cancellation)
{
    FVizBVHRayBatchContext context;
    if (bvh == NULL || (query_count != 0u && (rays == NULL || out_hits == NULL || out_hit_flags == NULL)))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    context.bvh = bvh;
    context.rays = rays;
    context.hits = out_hits;
    context.flags = out_hit_flags;
    return fviz_parallel_context_for(fviz_parallel_default_context(), 0u, query_count, 64u, fviz_bvh_ray_batch_range,
                                     &context, cancellation);
}

typedef struct FVizBVHClosestBatchContext
{
    const FVizBVH* bvh;
    const FVizVec3* queries;
    float max_distance;
    FVizClosestPoint* results;
    FVizBool* flags;
} FVizBVHClosestBatchContext;

static FVizResult fviz_bvh_closest_batch_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizBVHClosestBatchContext* context = (FVizBVHClosestBatchContext*)user_data;
    FVizSize index;
    for (index = begin; index < end; ++index)
    {
        const FVizResult result = fviz_bvh_closest_point(context->bvh, context->queries[index], context->max_distance,
                                                         &context->results[index]);
        if (result != FVIZ_OK && result != FVIZ_ERROR_NOT_FOUND) return result;
        context->flags[index] = result == FVIZ_OK ? FVIZ_TRUE : FVIZ_FALSE;
    }
    return FVIZ_OK;
}

FVizResult fviz_bvh_closest_point_batch(const FVizBVH* bvh, const FVizVec3* queries, FVizSize query_count,
                                        float max_distance, FVizClosestPoint* out_results, FVizBool* out_found_flags,
                                        FVizCancellationToken* cancellation)
{
    FVizBVHClosestBatchContext context;
    if (bvh == NULL || (query_count != 0u && (queries == NULL || out_results == NULL || out_found_flags == NULL)))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    context.bvh = bvh;
    context.queries = queries;
    context.max_distance = max_distance;
    context.results = out_results;
    context.flags = out_found_flags;
    return fviz_parallel_context_for(fviz_parallel_default_context(), 0u, query_count, 64u,
                                     fviz_bvh_closest_batch_range, &context, cancellation);
}

FVizSize fviz_bvh_memory_size(const FVizBVH* bvh)
{
    FVizSize bytes = sizeof(FVizBVH);
    FVizSize value;
    if (bvh == NULL) return 0u;
    if (fviz_size_multiply(bvh->node_capacity, sizeof(FVizBVHNode), &value) != FVIZ_OK) return SIZE_MAX;
    if (value > SIZE_MAX - bytes) return SIZE_MAX;
    bytes += value;
    if (fviz_size_multiply(bvh->triangle_capacity, sizeof(uint32_t) + sizeof(FVizBounds) + sizeof(FVizVec3), &value) !=
            FVIZ_OK ||
        value > SIZE_MAX - bytes)
        return SIZE_MAX;
    return bytes + value;
}

static FVizBool fviz_bvh_bounds_overlap(const FVizBounds* a, const FVizBounds* b)
{
    if (a == NULL || b == NULL || a->valid == FVIZ_FALSE || b->valid == FVIZ_FALSE) return FVIZ_FALSE;
    return !(a->max.x < b->min.x || a->min.x > b->max.x || a->max.y < b->min.y || a->min.y > b->max.y ||
             a->max.z < b->min.z || a->min.z > b->max.z)
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizBool fviz_bvh_intersects_bounds(const FVizBVH* bvh, const FVizBounds* bounds)
{
    int32_t stack[FVIZ_BVH_MAX_DEPTH * 2u + 2u];
    int32_t stack_size = 0;
    const FVizVec3* points;
    const uint32_t* indices;
    if (bvh == NULL || bounds == NULL || bvh->valid == FVIZ_FALSE || bvh->root < 0) return FVIZ_FALSE;
    if (fviz_bvh_bounds_overlap(&bvh->nodes[bvh->root].bounds, bounds) == FVIZ_FALSE) return FVIZ_FALSE;
    points = fviz_poly_data_points(bvh->poly_data);
    indices = fviz_poly_data_triangle_indices(bvh->poly_data);
    if (points == NULL || indices == NULL) return FVIZ_FALSE;
    stack[stack_size++] = bvh->root;
    while (stack_size > 0)
    {
        const FVizBVHNode* node = &bvh->nodes[stack[--stack_size]];
        if (fviz_bvh_bounds_overlap(&node->bounds, bounds) == FVIZ_FALSE) continue;
        if (node->triangle_begin >= 0)
        {
            int32_t i;
            for (i = node->triangle_begin; i < node->triangle_end; ++i)
            {
                const uint32_t triangle = bvh->triangle_indices[i];
                const FVizBounds triangle_bounds = fviz_triangle_bounds(points, &indices[(FVizSize)triangle * 3u]);
                if (fviz_bvh_bounds_overlap(&triangle_bounds, bounds) != FVIZ_FALSE) return FVIZ_TRUE;
            }
        }
        else
        {
            if (stack_size + 2 > (int32_t)(sizeof(stack) / sizeof(stack[0])))
            {
                fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "BVH bounds traversal stack overflow");
                return FVIZ_FALSE;
            }
            if (node->left >= 0) stack[stack_size++] = node->left;
            if (node->right >= 0) stack[stack_size++] = node->right;
        }
    }
    return FVIZ_FALSE;
}
