#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Parallel/FVizParallel.h>
#include <FViz/Spatial/FVizBVH.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Spatial/FVizBVHPrivate.h>

static void fviz_bvh_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_bvh_class = {
    FVIZ_TYPE_BVH, "FVizBVH", &g_fviz_object_class, fviz_bvh_destroy, NULL
};

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
            fviz_vec3_add(
                fviz_vec3_add(range->points[a], range->points[b]), range->points[c]),
            1.0f / 3.0f);
    }
}

static void fviz_bvh_destroy(FVizObject* object)
{
    FVizBVH* bvh = (FVizBVH*)object;
    fviz_free(bvh->nodes);
    fviz_free(bvh->triangle_indices);
    fviz_free(bvh->triangle_centroids);
    fviz_release(bvh->poly_data);
    bvh->nodes = NULL;
    bvh->triangle_indices = NULL;
    bvh->triangle_centroids = NULL;
    bvh->poly_data = NULL;
}

static FVizResult fviz_bvh_reserve_nodes(FVizBVH* bvh, FVizSize capacity)
{
    FVizBVHNode* new_nodes;
    if (capacity <= bvh->node_capacity) return FVIZ_OK;
    new_nodes = (FVizBVHNode*)fviz_realloc(bvh->nodes, capacity * sizeof(FVizBVHNode));
    if (new_nodes == NULL) return fviz_last_error_code();
    bvh->nodes = new_nodes;
    bvh->node_capacity = capacity;
    return FVIZ_OK;
}

static int32_t fviz_bvh_add_node(FVizBVH* bvh, const FVizBVHNode* node)
{
    if (bvh->node_count == bvh->node_capacity)
    {
        FVizSize new_capacity = bvh->node_capacity == 0u ? 32u : bvh->node_capacity * 2u;
        if (fviz_bvh_reserve_nodes(bvh, new_capacity) != FVIZ_OK) return -1;
    }
    bvh->nodes[bvh->node_count] = *node;
    return (int32_t)bvh->node_count++;
}

static void fviz_bvh_partition(
    uint32_t* indices,
    int32_t begin,
    int32_t end,
    int axis,
    const FVizVec3* centroids)
{
    const int32_t pivot = begin;
    const float pivot_value = (axis == 0) ? centroids[indices[pivot]].x :
                             (axis == 1) ? centroids[indices[pivot]].y :
                                           centroids[indices[pivot]].z;
    int32_t i = begin + 1;
    int32_t j = end - 1;
    while (i <= j)
    {
        while (i <= j)
        {
            const float value = (axis == 0) ? centroids[indices[i]].x :
                               (axis == 1) ? centroids[indices[i]].y :
                                             centroids[indices[i]].z;
            if (value > pivot_value) break;
            ++i;
        }
        while (j >= i)
        {
            const float value = (axis == 0) ? centroids[indices[j]].x :
                               (axis == 1) ? centroids[indices[j]].y :
                                             centroids[indices[j]].z;
            if (value <= pivot_value) break;
            --j;
        }
        if (i < j)
        {
            uint32_t swap = indices[i];
            indices[i] = indices[j];
            indices[j] = swap;
        }
    }
    {
        uint32_t swap = indices[pivot];
        indices[pivot] = indices[j];
        indices[j] = swap;
    }
}

static FVizResult fviz_bvh_build_recursive(
    FVizBVH* bvh,
    uint32_t* primitive_ids,
    int32_t begin,
    int32_t end,
    int depth,
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

    if ((uint32_t)(end - begin) <= FVIZ_BVH_LEAF_SIZE ||
        (uint32_t)depth >= FVIZ_BVH_MAX_DEPTH)
    {
        node.triangle_begin = (int32_t)bvh->triangle_count;
        node.triangle_end = node.triangle_begin + (end - begin);
        for (k = begin; k < end; ++k)
        {
            if (bvh->triangle_count >= bvh->triangle_capacity)
            {
                FVizSize new_capacity = bvh->triangle_capacity == 0u ? 64u : bvh->triangle_capacity * 2u;
                uint32_t* new_ids = (uint32_t*)fviz_realloc(bvh->triangle_indices, new_capacity * sizeof(uint32_t));
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
    else if (extent.y >= extent.z) axis = 1;
    else axis = 2;

    fviz_bvh_partition(primitive_ids, begin, end, axis, bvh->triangle_centroids);
    {
        int32_t mid = begin + (end - begin) / 2;
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

    if (bvh == NULL || poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "bvh and poly_data must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    triangle_count = fviz_poly_data_triangle_count(poly_data);
    if (triangle_count == 0u)
    {
        bvh->valid = FVIZ_FALSE;
        return FVIZ_OK;
    }
    points = fviz_poly_data_points(poly_data);
    indices = fviz_poly_data_triangle_indices(poly_data);
    if (points == NULL || indices == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "poly_data has no point or index data");
        return FVIZ_ERROR_INVALID_STATE;
    }

    primitive_ids = (uint32_t*)fviz_alloc(triangle_count * sizeof(uint32_t));
    bounds_cache = (FVizBounds*)fviz_alloc(triangle_count * sizeof(FVizBounds));
    if (primitive_ids == NULL || bounds_cache == NULL)
    {
        fviz_free(primitive_ids);
        fviz_free(bounds_cache);
        return fviz_last_error_code();
    }

    fviz_free(bvh->nodes);
    fviz_free(bvh->triangle_indices);
    fviz_free(bvh->triangle_centroids);
    fviz_release(bvh->poly_data);
    bvh->nodes = NULL;
    bvh->node_count = 0u;
    bvh->node_capacity = 0u;
    bvh->triangle_indices = NULL;
    bvh->triangle_count = 0u;
    bvh->triangle_capacity = 0u;
    bvh->root = -1;

    bvh->triangle_centroids = (FVizVec3*)fviz_alloc(triangle_count * sizeof(FVizVec3));
    if (bvh->triangle_centroids == NULL)
    {
        fviz_free(primitive_ids);
        fviz_free(bounds_cache);
        return fviz_last_error_code();
    }
    primitive_range.points = points;
    primitive_range.indices = indices;
    primitive_range.primitive_ids = primitive_ids;
    primitive_range.bounds = bounds_cache;
    primitive_range.centroids = bvh->triangle_centroids;
    if (fviz_parallel_for(
            0u, triangle_count, 512u,
            fviz_bvh_initialize_primitive_range, &primitive_range) != FVIZ_OK)
    {
        fviz_free(primitive_ids);
        fviz_free(bounds_cache);
        return fviz_last_error_code();
    }

    {
        int32_t root;
        bvh->poly_data = (FVizPolyData*)fviz_retain((FVizPolyData*)poly_data);
        if (fviz_bvh_reserve_nodes(bvh, 64u) != FVIZ_OK)
        {
            fviz_free(primitive_ids);
            fviz_free(bounds_cache);
            return fviz_last_error_code();
        }
        bvh->triangle_bounds_cache = bounds_cache;
        if (fviz_bvh_build_recursive(bvh, primitive_ids, 0, (int32_t)triangle_count, 0, &root) != FVIZ_OK)
        {
            fviz_free(primitive_ids);
            fviz_free(bounds_cache);
            return fviz_last_error_code();
        }
        bvh->root = root;
        bvh->valid = FVIZ_TRUE;
    }
    fviz_free(primitive_ids);
    fviz_free(bounds_cache);
    return FVIZ_OK;
}

FVizBool fviz_bvh_valid(const FVizBVH* bvh) { return bvh != NULL ? bvh->valid : FVIZ_FALSE; }
FVizSize fviz_bvh_triangle_count(const FVizBVH* bvh) { return bvh != NULL ? fviz_poly_data_triangle_count(bvh->poly_data) : 0u; }

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
            if (t0 > t1) { temp = t0; t0 = t1; t1 = temp; }
            if (t0 > t_min) t_min = t0;
            if (t1 < t_max) t_max = t1;
            if (t_min > t_max) return FVIZ_FALSE;
        }
    }
    if (t_min > t_max) return FVIZ_FALSE;
    if (out_t != NULL) *out_t = t_min > 0.0f ? t_min : t_max;
    return FVIZ_TRUE;
}

static FVizBool fviz_ray_triangle_intersect(
    FVizRay ray,
    FVizVec3 a,
    FVizVec3 b,
    FVizVec3 c,
    float* out_t,
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

static FVizBool fviz_bvh_ray_cast_node(const FVizBVH* bvh, FVizRay ray, FVizRayHit* out_hit, float* best_distance, FVizBool any)
{
    if (bvh->root < 0) return FVIZ_FALSE;
    {
        int32_t stack[64];
        int32_t stack_size = 1;
        stack[0] = bvh->root;
        while (stack_size > 0)
        {
            const FVizBVHNode* node;
            float box_t;
            --stack_size;
            node = &bvh->nodes[stack[stack_size]];
            if (!fviz_ray_box_intersect(ray, &node->bounds, &box_t)) continue;
            if (node->triangle_begin >= 0)
            {
                int32_t i;
                for (i = node->triangle_begin; i < node->triangle_end; ++i)
                {
                    const FVizPolyData* data = bvh->poly_data;
                    const uint32_t* indices = fviz_poly_data_triangle_indices(data);
                    const FVizVec3* points = fviz_poly_data_points(data);
                    const uint32_t tri = bvh->triangle_indices[i];
                    float t;
                    FVizVec3 normal;
                    if (!fviz_ray_triangle_intersect(ray,
                            points[indices[tri * 3u + 0u]],
                            points[indices[tri * 3u + 1u]],
                            points[indices[tri * 3u + 2u]],
                            &t, &normal)) continue;
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
                if (stack_size + 2 > 64) break;
                stack[stack_size++] = node->right;
                stack[stack_size++] = node->left;
            }
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

FVizBool fviz_bvh_intersects_bounds(const FVizBVH* bvh, const FVizBounds* bounds)
{
    FVizRay ray;
    FVizVec3 center;
    if (bvh == NULL || bounds == NULL || bvh->valid == FVIZ_FALSE) return FVIZ_FALSE;
    if (bvh->nodes[bvh->root].bounds.valid == FVIZ_FALSE) return FVIZ_FALSE;
    if (bounds->valid == FVIZ_FALSE) return FVIZ_FALSE;
    if (bounds->max.x < bvh->nodes[bvh->root].bounds.min.x ||
        bounds->min.x > bvh->nodes[bvh->root].bounds.max.x ||
        bounds->max.y < bvh->nodes[bvh->root].bounds.min.y ||
        bounds->min.y > bvh->nodes[bvh->root].bounds.max.y ||
        bounds->max.z < bvh->nodes[bvh->root].bounds.min.z ||
        bounds->min.z > bvh->nodes[bvh->root].bounds.max.z)
    {
        return FVIZ_FALSE;
    }
    center = fviz_bounds_center(bounds);
    ray.origin = center;
    ray.direction = fviz_vec3(1.0f, 0.0f, 0.0f);
    return fviz_bvh_ray_cast_any(bvh, ray);
}
