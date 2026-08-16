#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Algorithms/FVizMeshProcessingFilters.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizClipPolyDataFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizPlane plane;
    FVizBool inside_out;
    FVizBool generate_cap;
};

typedef struct FVizClipSegment
{
    uint32_t a;
    uint32_t b;
    FVizBool visited;
} FVizClipSegment;

typedef enum FVizClipVertexKind
{
    FVIZ_CLIP_VERTEX_ORIGINAL = 0,
    FVIZ_CLIP_VERTEX_EDGE = 1
} FVizClipVertexKind;

typedef struct FVizClipVertex
{
    FVizClipVertexKind kind;
    uint32_t a;
    uint32_t b;
    double t;
} FVizClipVertex;

typedef struct FVizClipPointData
{
    FVizSize count;
    const FVizDataArray** sources;
    FVizDataArray** destinations;
    const FVizDataArray* source_scalars;
    FVizDataArray* destination_scalars;
    unsigned char* scratch;
    FVizSize scratch_size;
} FVizClipPointData;

static FVizMTime fviz_clip_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static void fviz_clip_poly_data_filter_destroy(FVizObject* object)
{
    FVizClipPolyDataFilter* filter = (FVizClipPolyDataFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_clip_poly_data_filter_class = {
    FVIZ_TYPE_CLIP_POLY_DATA_FILTER,
    "FVizClipPolyDataFilter",
    &g_fviz_object_class,
    fviz_clip_poly_data_filter_destroy,
    NULL
};

static FVizResult fviz_clip_copy_attribute_set(
    const FVizAttributeSet* source,
    FVizAttributeSet* destination)
{
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        const FVizDataArray* source_array = fviz_attribute_set_const_array_at(source, i);
        FVizDataArray* copy = NULL;
        FVizAttributeRole role;
        if (fviz_data_array_deep_copy(source_array, &copy) != FVIZ_OK ||
            fviz_attribute_set_add(destination, name, copy) != FVIZ_OK)
        {
            fviz_release(copy);
            return fviz_last_error_code();
        }
        fviz_release(copy);
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(destination, role, name);
        }
    }
    return FVIZ_OK;
}

static void fviz_clip_point_data_release(FVizClipPointData* data)
{
    FVizSize i;
    if (data == NULL) return;
    for (i = 0u; i < data->count; ++i) fviz_release(data->destinations[i]);
    fviz_release(data->destination_scalars);
    fviz_free(data->scratch);
    fviz_free(data->destinations);
    fviz_free((void*)data->sources);
    memset(data, 0, sizeof(*data));
}

static FVizResult fviz_clip_point_data_prepare(
    const FVizPolyData* input,
    FVizPolyData* output,
    FVizClipPointData* data)
{
    const FVizAttributeSet* source_set = fviz_poly_data_const_point_data(input);
    FVizAttributeSet* destination_set = fviz_poly_data_point_data(output);
    const FVizSize input_points = fviz_poly_data_point_count(input);
    const FVizSize source_count = fviz_attribute_set_count(source_set);
    FVizSize valid_count = 0u;
    FVizSize i;
    memset(data, 0, sizeof(*data));
    if (source_count != 0u)
    {
        FVizSize bytes;
        if (fviz_size_multiply(source_count, sizeof(*data->sources), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
        data->sources = (const FVizDataArray**)fviz_alloc(bytes);
        if (data->sources == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
        if (fviz_size_multiply(source_count, sizeof(*data->destinations), &bytes) != FVIZ_OK)
        {
            fviz_clip_point_data_release(data);
            return FVIZ_ERROR_OVERFLOW;
        }
        data->destinations = (FVizDataArray**)fviz_alloc(bytes);
        if (data->destinations == NULL)
        {
            fviz_clip_point_data_release(data);
            return FVIZ_ERROR_OUT_OF_MEMORY;
        }
        memset(data->destinations, 0, bytes);
    }
    for (i = 0u; i < source_count; ++i)
    {
        const char* name = fviz_attribute_set_name_at(source_set, i);
        const FVizDataArray* source_array = fviz_attribute_set_const_array_at(source_set, i);
        FVizDataArray* destination_array = NULL;
        FVizAttributeRole role;
        if (fviz_data_array_tuple_count(source_array) != input_points) continue;
        if (fviz_data_array_create(
                fviz_data_array_type(source_array), fviz_data_array_components(source_array), &destination_array) != FVIZ_OK ||
            fviz_data_array_reserve(destination_array, input_points) != FVIZ_OK ||
            fviz_attribute_set_add(destination_set, name, destination_array) != FVIZ_OK)
        {
            fviz_release(destination_array);
            fviz_clip_point_data_release(data);
            return fviz_last_error_code();
        }
        data->sources[valid_count] = source_array;
        data->destinations[valid_count] = destination_array;
        ++valid_count;
        if (fviz_data_array_tuple_stride(source_array) > data->scratch_size)
            data->scratch_size = fviz_data_array_tuple_stride(source_array);
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source_set, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(destination_set, role, name);
        }
    }
    data->count = valid_count;
    data->source_scalars = fviz_poly_data_const_scalars(input);
    if (data->source_scalars != NULL && fviz_data_array_tuple_count(data->source_scalars) == input_points)
    {
        if (fviz_data_array_create(
                fviz_data_array_type(data->source_scalars), fviz_data_array_components(data->source_scalars),
                &data->destination_scalars) != FVIZ_OK ||
            fviz_data_array_reserve(data->destination_scalars, input_points) != FVIZ_OK)
        {
            fviz_clip_point_data_release(data);
            return fviz_last_error_code();
        }
        if (fviz_data_array_tuple_stride(data->source_scalars) > data->scratch_size)
            data->scratch_size = fviz_data_array_tuple_stride(data->source_scalars);
    }
    else
    {
        data->source_scalars = NULL;
    }
    if (data->scratch_size != 0u)
    {
        data->scratch = (unsigned char*)fviz_alloc(data->scratch_size);
        if (data->scratch == NULL)
        {
            fviz_clip_point_data_release(data);
            return FVIZ_ERROR_OUT_OF_MEMORY;
        }
    }
    return FVIZ_OK;
}

static void fviz_clip_write_value(unsigned char* destination, FVizDataType type, double value)
{
    switch (type)
    {
        case FVIZ_DATA_INT8: *(int8_t*)destination = (int8_t)value; break;
        case FVIZ_DATA_UINT8: *(uint8_t*)destination = (uint8_t)value; break;
        case FVIZ_DATA_INT16: { int16_t v = (int16_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_UINT16: { uint16_t v = (uint16_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_INT32: { int32_t v = (int32_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_UINT32: { uint32_t v = (uint32_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_INT64: { int64_t v = (int64_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_UINT64: { uint64_t v = (uint64_t)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_FLOAT32: { float v = (float)value; memcpy(destination, &v, sizeof(v)); break; }
        case FVIZ_DATA_FLOAT64: memcpy(destination, &value, sizeof(value)); break;
        default: break;
    }
}

static FVizResult fviz_clip_append_interpolated_tuple(
    const FVizDataArray* source,
    FVizDataArray* destination,
    uint32_t a,
    uint32_t b,
    double t,
    unsigned char* scratch)
{
    const uint32_t components = fviz_data_array_components(source);
    const FVizDataType type = fviz_data_array_type(source);
    const FVizSize type_size = fviz_data_type_size(type);
    uint32_t component;
    for (component = 0u; component < components; ++component)
    {
        double va = 0.0;
        double vb = 0.0;
        if (fviz_data_array_get_component(source, a, component, &va) != FVIZ_OK ||
            fviz_data_array_get_component(source, b, component, &vb) != FVIZ_OK)
            return fviz_last_error_code();
        fviz_clip_write_value(scratch + (FVizSize)component * type_size, type, va + (vb - va) * t);
    }
    return fviz_data_array_append_tuple(destination, scratch);
}

static FVizResult fviz_clip_append_original_data(FVizClipPointData* data, uint32_t source_id)
{
    FVizSize i;
    for (i = 0u; i < data->count; ++i)
        if (fviz_data_array_append_tuple(
                data->destinations[i], fviz_data_array_const_tuple(data->sources[i], source_id)) != FVIZ_OK)
            return fviz_last_error_code();
    if (data->source_scalars != NULL && fviz_data_array_append_tuple(
            data->destination_scalars, fviz_data_array_const_tuple(data->source_scalars, source_id)) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizResult fviz_clip_append_edge_data(FVizClipPointData* data, uint32_t a, uint32_t b, double t)
{
    FVizSize i;
    for (i = 0u; i < data->count; ++i)
        if (fviz_clip_append_interpolated_tuple(
                data->sources[i], data->destinations[i], a, b, t, data->scratch) != FVIZ_OK)
            return fviz_last_error_code();
    if (data->source_scalars != NULL && fviz_clip_append_interpolated_tuple(
            data->source_scalars, data->destination_scalars, a, b, t, data->scratch) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

static uint64_t fviz_clip_edge_key(uint32_t a, uint32_t b)
{
    const uint32_t lo = a < b ? a : b;
    const uint32_t hi = a < b ? b : a;
    return ((uint64_t)lo << 32u) | (uint64_t)hi;
}

static FVizResult fviz_clip_output_vertex(
    const FVizClipVertex* vertex,
    const FVizPolyData* input,
    FVizPolyData* output,
    uint32_t* original_map,
    FVizHashMap* edge_map,
    FVizClipPointData* point_data,
    uint32_t* out_id)
{
    const FVizVec3* points = fviz_poly_data_points(input);
    if (vertex->kind == FVIZ_CLIP_VERTEX_ORIGINAL)
    {
        uint32_t mapped = original_map[vertex->a];
        if (mapped == UINT32_MAX)
        {
            if (fviz_poly_data_add_point(output, points[vertex->a], &mapped) != FVIZ_OK ||
                fviz_clip_append_original_data(point_data, vertex->a) != FVIZ_OK)
                return fviz_last_error_code();
            original_map[vertex->a] = mapped;
        }
        *out_id = mapped;
        return FVIZ_OK;
    }
    else
    {
        const uint64_t key = fviz_clip_edge_key(vertex->a, vertex->b);
        void* value = NULL;
        if (fviz_hash_map_get(edge_map, key, &value) != FVIZ_FALSE)
        {
            *out_id = (uint32_t)(uintptr_t)value;
            return FVIZ_OK;
        }
        {
            const FVizVec3 pa = points[vertex->a];
            const FVizVec3 pb = points[vertex->b];
            const FVizVec3 point = fviz_vec3_add(pa, fviz_vec3_scale(fviz_vec3_sub(pb, pa), (float)vertex->t));
            uint32_t mapped;
            if (fviz_poly_data_add_point(output, point, &mapped) != FVIZ_OK ||
                fviz_clip_append_edge_data(point_data, vertex->a, vertex->b, vertex->t) != FVIZ_OK ||
                fviz_hash_map_set(edge_map, key, (void*)(uintptr_t)mapped) != FVIZ_OK)
                return fviz_last_error_code();
            *out_id = mapped;
            return FVIZ_OK;
        }
    }
}

static FVizBool fviz_clip_keep(double distance, FVizBool inside_out)
{
    return inside_out != FVIZ_FALSE ? (distance <= 0.0 ? FVIZ_TRUE : FVIZ_FALSE)
                                    : (distance >= 0.0 ? FVIZ_TRUE : FVIZ_FALSE);
}

static FVizClipVertex fviz_clip_intersection(uint32_t a, uint32_t b, double da, double db)
{
    FVizClipVertex result;
    const double denominator = da - db;
    result.kind = FVIZ_CLIP_VERTEX_EDGE;
    result.a = a;
    result.b = b;
    result.t = denominator != 0.0 ? da / denominator : 0.5;
    if (result.t < 0.0) result.t = 0.0;
    if (result.t > 1.0) result.t = 1.0;
    return result;
}

static double fviz_clip_cross_2d(double ax, double ay, double bx, double by, double cx, double cy)
{
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static FVizBool fviz_clip_point_in_triangle_2d(
    double px, double py,
    double ax, double ay,
    double bx, double by,
    double cx, double cy)
{
    const double e0 = fviz_clip_cross_2d(ax, ay, bx, by, px, py);
    const double e1 = fviz_clip_cross_2d(bx, by, cx, cy, px, py);
    const double e2 = fviz_clip_cross_2d(cx, cy, ax, ay, px, py);
    const double epsilon = 1.0e-12;
    return e0 >= -epsilon && e1 >= -epsilon && e2 >= -epsilon ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_clip_triangulate_cap_loop(
    FVizPolyData* output,
    const uint32_t* loop,
    FVizSize loop_count,
    FVizPlane plane,
    FVizBool inside_out,
    FVizSize* source_ids,
    FVizSize source_capacity,
    FVizSize* source_count)
{
    FVizVec3 normal = fviz_vec3_normalize(plane.normal);
    FVizVec3 reference;
    FVizVec3 axis_u;
    FVizVec3 axis_v;
    uint32_t* remaining = NULL;
    double* coordinates = NULL;
    FVizSize bytes;
    FVizSize remaining_count = loop_count;
    FVizSize i;
    double area = 0.0;
    const FVizVec3* points = fviz_poly_data_points(output);
    if (loop_count < 3u) return FVIZ_OK;
    if (inside_out == FVIZ_FALSE) normal = fviz_vec3_scale(normal, -1.0f);
    reference = fabsf(normal.x) < 0.8f ? fviz_vec3(1.0f, 0.0f, 0.0f) : fviz_vec3(0.0f, 1.0f, 0.0f);
    axis_u = fviz_vec3_normalize(fviz_vec3_cross(reference, normal));
    axis_v = fviz_vec3_cross(normal, axis_u);
    if (fviz_size_multiply(loop_count, sizeof(*remaining), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    remaining = (uint32_t*)fviz_alloc(bytes);
    if (remaining == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
    if (fviz_size_multiply(loop_count * 2u, sizeof(*coordinates), &bytes) != FVIZ_OK) goto fail;
    coordinates = (double*)fviz_alloc(bytes);
    if (coordinates == NULL) goto fail;
    for (i = 0u; i < loop_count; ++i)
    {
        remaining[i] = (uint32_t)i;
        coordinates[i * 2u] = (double)fviz_vec3_dot(points[loop[i]], axis_u);
        coordinates[i * 2u + 1u] = (double)fviz_vec3_dot(points[loop[i]], axis_v);
    }
    for (i = 0u; i < loop_count; ++i)
    {
        const FVizSize next = (i + 1u) % loop_count;
        area += coordinates[i * 2u] * coordinates[next * 2u + 1u] -
                coordinates[next * 2u] * coordinates[i * 2u + 1u];
    }
    if (fabs(area) <= 1.0e-14)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "clip cap loop has zero projected area");
        goto fail;
    }
    if (area < 0.0)
        for (i = 0u; i < loop_count / 2u; ++i)
        {
            const uint32_t temporary = remaining[i];
            remaining[i] = remaining[loop_count - 1u - i];
            remaining[loop_count - 1u - i] = temporary;
        }
    while (remaining_count > 3u)
    {
        FVizBool clipped_ear = FVIZ_FALSE;
        FVizSize ear;
        for (ear = 0u; ear < remaining_count; ++ear)
        {
            const uint32_t previous = remaining[(ear + remaining_count - 1u) % remaining_count];
            const uint32_t current = remaining[ear];
            const uint32_t next = remaining[(ear + 1u) % remaining_count];
            FVizBool contains_point = FVIZ_FALSE;
            FVizSize candidate;
            if (fviz_clip_cross_2d(
                    coordinates[previous * 2u], coordinates[previous * 2u + 1u],
                    coordinates[current * 2u], coordinates[current * 2u + 1u],
                    coordinates[next * 2u], coordinates[next * 2u + 1u]) <= 1.0e-14)
                continue;
            for (candidate = 0u; candidate < remaining_count; ++candidate)
            {
                const uint32_t point = remaining[candidate];
                if (point == previous || point == current || point == next) continue;
                if (fviz_clip_point_in_triangle_2d(
                        coordinates[point * 2u], coordinates[point * 2u + 1u],
                        coordinates[previous * 2u], coordinates[previous * 2u + 1u],
                        coordinates[current * 2u], coordinates[current * 2u + 1u],
                        coordinates[next * 2u], coordinates[next * 2u + 1u]) != FVIZ_FALSE)
                {
                    contains_point = FVIZ_TRUE;
                    break;
                }
            }
            if (contains_point != FVIZ_FALSE) continue;
            if (*source_count >= source_capacity ||
                fviz_poly_data_add_triangle(output, loop[previous], loop[current], loop[next]) != FVIZ_OK)
                goto fail;
            source_ids[(*source_count)++] = (FVizSize)-1;
            memmove(&remaining[ear], &remaining[ear + 1u], (remaining_count - ear - 1u) * sizeof(*remaining));
            --remaining_count;
            clipped_ear = FVIZ_TRUE;
            break;
        }
        if (clipped_ear == FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "clip cap loop is non-simple or numerically degenerate");
            goto fail;
        }
    }
    if (*source_count >= source_capacity ||
        fviz_poly_data_add_triangle(
            output, loop[remaining[0]], loop[remaining[1]], loop[remaining[2]]) != FVIZ_OK)
        goto fail;
    source_ids[(*source_count)++] = (FVizSize)-1;
    fviz_free(coordinates);
    fviz_free(remaining);
    return FVIZ_OK;
fail:
    fviz_free(coordinates);
    fviz_free(remaining);
    return fviz_last_error_code();
}

static FVizResult fviz_clip_generate_caps(
    FVizPolyData* output,
    FVizClipSegment* segments,
    FVizSize segment_count,
    FVizPlane plane,
    FVizBool inside_out,
    FVizSize* source_ids,
    FVizSize source_capacity,
    FVizSize* source_count)
{
    uint32_t* loop = NULL;
    FVizSize bytes;
    FVizSize seed;
    if (segment_count == 0u) return FVIZ_OK;
    if (fviz_size_multiply(segment_count, sizeof(*loop), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    loop = (uint32_t*)fviz_alloc(bytes);
    if (loop == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
    for (seed = 0u; seed < segment_count; ++seed)
    {
        FVizSize loop_count = 0u;
        uint32_t start;
        uint32_t current;
        if (segments[seed].visited != FVIZ_FALSE) continue;
        start = segments[seed].a;
        current = segments[seed].b;
        segments[seed].visited = FVIZ_TRUE;
        loop[loop_count++] = start;
        while (current != start)
        {
            FVizSize next_segment;
            FVizBool found = FVIZ_FALSE;
            if (loop_count >= segment_count)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "clip cap cut graph does not form simple closed loops");
                goto fail;
            }
            loop[loop_count++] = current;
            for (next_segment = 0u; next_segment < segment_count; ++next_segment)
            {
                if (segments[next_segment].visited != FVIZ_FALSE) continue;
                if (segments[next_segment].a == current || segments[next_segment].b == current)
                {
                    current = segments[next_segment].a == current ? segments[next_segment].b : segments[next_segment].a;
                    segments[next_segment].visited = FVIZ_TRUE;
                    found = FVIZ_TRUE;
                    break;
                }
            }
            if (found == FVIZ_FALSE)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "clip cap requires closed manifold cut loops");
                goto fail;
            }
        }
        if (fviz_clip_triangulate_cap_loop(
                output, loop, loop_count, plane, inside_out,
                source_ids, source_capacity, source_count) != FVIZ_OK)
            goto fail;
    }
    fviz_free(loop);
    return FVIZ_OK;
fail:
    fviz_free(loop);
    return fviz_last_error_code();
}

static FVizResult fviz_clip_copy_cell_data(
    const FVizPolyData* input,
    FVizPolyData* output,
    const FVizSize* source_triangles,
    FVizSize output_triangle_count)
{
    const FVizAttributeSet* source_set = fviz_poly_data_const_cell_data(input);
    FVizAttributeSet* destination_set = fviz_poly_data_cell_data(output);
    const FVizSize source_cell_count = fviz_poly_data_cell_count(input);
    const FVizSize triangle_offset = fviz_poly_data_vert_cell_count(input) + fviz_poly_data_line_cell_count(input);
    FVizSize array_index;
    FVizBool has_original_ids = FVIZ_FALSE;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source_set); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source_set, array_index);
        const FVizDataArray* source = fviz_attribute_set_const_array_at(source_set, array_index);
        FVizDataArray* destination = NULL;
        FVizSize i;
        FVizAttributeRole role;
        if (fviz_data_array_tuple_count(source) != source_cell_count) continue;
        if (strcmp(name, "FVizOriginalCellIds") == 0) has_original_ids = FVIZ_TRUE;
        if (fviz_data_array_create(
                fviz_data_array_type(source), fviz_data_array_components(source), &destination) != FVIZ_OK ||
            fviz_data_array_reserve(destination, output_triangle_count) != FVIZ_OK)
            goto fail;
        for (i = 0u; i < output_triangle_count; ++i)
        {
            if (source_triangles[i] == (FVizSize)-1)
            {
                const FVizSize stride = fviz_data_array_tuple_stride(source);
                unsigned char* zero = (unsigned char*)fviz_alloc(stride);
                if (zero == NULL && stride != 0u) goto fail;
                memset(zero, 0, stride);
                if (strcmp(name, "FVizOriginalCellIds") == 0 &&
                    fviz_data_array_type(source) == FVIZ_DATA_UINT64 &&
                    fviz_data_array_components(source) == 1u)
                    *(uint64_t*)zero = UINT64_MAX;
                if (fviz_data_array_append_tuple(destination, zero) != FVIZ_OK)
                {
                    fviz_free(zero);
                    goto fail;
                }
                fviz_free(zero);
            }
            else
            {
                const FVizSize source_id = triangle_offset + source_triangles[i];
                if (source_id >= source_cell_count ||
                    fviz_data_array_append_tuple(destination, fviz_data_array_const_tuple(source, source_id)) != FVIZ_OK)
                    goto fail;
            }
        }
        if (fviz_attribute_set_add(destination_set, name, destination) != FVIZ_OK) goto fail;
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source_set, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(destination_set, role, name);
        }
        fviz_release(destination);
        continue;
fail:
        fviz_release(destination);
        return fviz_last_error_code();
    }
    if (has_original_ids == FVIZ_FALSE)
    {
        FVizDataArray* ids = NULL;
        FVizSize i;
        if (fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &ids) != FVIZ_OK ||
            fviz_data_array_reserve(ids, output_triangle_count) != FVIZ_OK)
        {
            fviz_release(ids);
            return fviz_last_error_code();
        }
        for (i = 0u; i < output_triangle_count; ++i)
        {
            const uint64_t source_id = source_triangles[i] == (FVizSize)-1
                ? UINT64_MAX : (uint64_t)(triangle_offset + source_triangles[i]);
            if (fviz_data_array_append_tuple(ids, &source_id) != FVIZ_OK)
            {
                fviz_release(ids);
                return fviz_last_error_code();
            }
        }
        if (fviz_attribute_set_add(destination_set, "FVizOriginalCellIds", ids) != FVIZ_OK)
        {
            fviz_release(ids);
            return fviz_last_error_code();
        }
        fviz_release(ids);
    }
    {
        FVizDataArray* cap = NULL;
        FVizSize i;
        if (fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &cap) != FVIZ_OK ||
            fviz_data_array_reserve(cap, output_triangle_count) != FVIZ_OK)
        {
            fviz_release(cap);
            return fviz_last_error_code();
        }
        for (i = 0u; i < output_triangle_count; ++i)
        {
            const uint8_t value = source_triangles[i] == (FVizSize)-1 ? 1u : 0u;
            if (fviz_data_array_append_tuple(cap, &value) != FVIZ_OK)
            {
                fviz_release(cap);
                return fviz_last_error_code();
            }
        }
        if (fviz_attribute_set_add(destination_set, "FVizClipCap", cap) != FVIZ_OK)
        {
            fviz_release(cap);
            return fviz_last_error_code();
        }
        fviz_release(cap);
    }
    return FVIZ_OK;
}

static FVizResult fviz_clip_poly_data_filter_process_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* state)
{
    FVizClipPolyDataFilter* filter = (FVizClipPolyDataFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    const FVizVec3* points;
    const uint32_t* triangles;
    FVizSize point_count;
    FVizSize triangle_count;
    uint32_t* original_map = NULL;
    FVizHashMap* edge_map = NULL;
    FVizSize* source_ids = NULL;
    FVizSize source_id_capacity = 0u;
    FVizSize source_id_count = 0u;
    FVizClipSegment* segments = NULL;
    FVizSize segment_count = 0u;
    FVizClipPointData point_data;
    FVizSize i;
    FVizSize bytes;
    memset(&point_data, 0, sizeof(point_data));
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL || fviz_poly_data_validate(input) != FVIZ_OK)
    {
        if (input == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "clip poly data filter has no input");
        return input == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
    }
    point_count = fviz_poly_data_point_count(input);
    triangle_count = fviz_poly_data_triangle_count(input);
    points = fviz_poly_data_points(input);
    triangles = fviz_poly_data_triangle_indices(input);
    if (triangle_count == 0u || triangles == NULL ||
        fviz_poly_data_strip_cell_count(input) != 0u ||
        fviz_poly_data_poly_cell_count(input) != triangle_count)
    {
        fviz_internal_set_error(
            FVIZ_ERROR_INVALID_STATE,
            "clip poly data requires render-ready triangle polys; run TriangleFilter first for general PolyData");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK) goto fail;
    if (triangle_count > (FVizSize)-1 / 3u)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "clip triangle count overflow");
        goto fail;
    }
    if (fviz_poly_data_reserve(output, point_count, triangle_count) != FVIZ_OK ||
        fviz_clip_point_data_prepare(input, output, &point_data) != FVIZ_OK ||
        fviz_clip_copy_attribute_set(fviz_poly_data_const_field_data(input), fviz_poly_data_field_data(output)) != FVIZ_OK)
        goto fail;
    if (fviz_size_multiply(point_count, sizeof(*original_map), &bytes) != FVIZ_OK) goto fail;
    original_map = (uint32_t*)fviz_alloc(bytes);
    if (original_map == NULL && point_count != 0u) goto fail;
    memset(original_map, 0xFF, bytes);
    source_id_capacity = triangle_count * 3u;
    if (fviz_size_multiply(source_id_capacity, sizeof(*source_ids), &bytes) != FVIZ_OK) goto fail;
    source_ids = (FVizSize*)fviz_alloc(bytes);
    if (source_ids == NULL && triangle_count != 0u) goto fail;
    if (fviz_size_multiply(triangle_count, sizeof(*segments), &bytes) != FVIZ_OK) goto fail;
    segments = (FVizClipSegment*)fviz_alloc(bytes);
    if (segments == NULL && triangle_count != 0u) goto fail;
    if (fviz_hash_map_create_reserve(triangle_count < 16u ? 16u : triangle_count, &edge_map) != FVIZ_OK) goto fail;

    for (i = 0u; i < triangle_count; ++i)
    {
        const uint32_t ids[3] = {triangles[i * 3u], triangles[i * 3u + 1u], triangles[i * 3u + 2u]};
        const double distances[3] = {
            (double)fviz_plane_distance_to_point(filter->plane, points[ids[0]]),
            (double)fviz_plane_distance_to_point(filter->plane, points[ids[1]]),
            (double)fviz_plane_distance_to_point(filter->plane, points[ids[2]])};
        FVizClipVertex polygon[4];
        FVizSize polygon_count = 0u;
        uint32_t edge;
        for (edge = 0u; edge < 3u; ++edge)
        {
            const uint32_t s_index = edge;
            const uint32_t e_index = (edge + 1u) % 3u;
            const FVizBool s_in = fviz_clip_keep(distances[s_index], filter->inside_out);
            const FVizBool e_in = fviz_clip_keep(distances[e_index], filter->inside_out);
            if (s_in != FVIZ_FALSE && e_in != FVIZ_FALSE)
            {
                polygon[polygon_count++] = (FVizClipVertex){FVIZ_CLIP_VERTEX_ORIGINAL, ids[e_index], ids[e_index], 0.0};
            }
            else if (s_in != FVIZ_FALSE && e_in == FVIZ_FALSE)
            {
                polygon[polygon_count++] = fviz_clip_intersection(
                    ids[s_index], ids[e_index], distances[s_index], distances[e_index]);
            }
            else if (s_in == FVIZ_FALSE && e_in != FVIZ_FALSE)
            {
                polygon[polygon_count++] = fviz_clip_intersection(
                    ids[s_index], ids[e_index], distances[s_index], distances[e_index]);
                polygon[polygon_count++] = (FVizClipVertex){FVIZ_CLIP_VERTEX_ORIGINAL, ids[e_index], ids[e_index], 0.0};
            }
        }
        if (polygon_count >= 3u)
        {
            uint32_t output_ids[4];
            FVizSize p;
            for (p = 0u; p < polygon_count; ++p)
                if (fviz_clip_output_vertex(
                        &polygon[p], input, output, original_map, edge_map, &point_data, &output_ids[p]) != FVIZ_OK)
                    goto fail;
            if (filter->generate_cap != FVIZ_FALSE)
            {
                uint32_t cut_ids[2];
                FVizSize cut_count = 0u;
                for (p = 0u; p < polygon_count; ++p)
                    if (polygon[p].kind == FVIZ_CLIP_VERTEX_EDGE &&
                        (cut_count == 0u || cut_ids[0] != output_ids[p]))
                    {
                        if (cut_count < 2u) cut_ids[cut_count] = output_ids[p];
                        ++cut_count;
                    }
                if (cut_count == 2u && cut_ids[0] != cut_ids[1])
                {
                    segments[segment_count].a = cut_ids[0];
                    segments[segment_count].b = cut_ids[1];
                    segments[segment_count].visited = FVIZ_FALSE;
                    ++segment_count;
                }
            }
            for (p = 1u; p + 1u < polygon_count; ++p)
            {
                if (source_id_count >= source_id_capacity ||
                    fviz_poly_data_add_triangle(output, output_ids[0], output_ids[p], output_ids[p + 1u]) != FVIZ_OK)
                    goto fail;
                source_ids[source_id_count++] = i;
            }
        }
        if ((i & 4095u) == 4095u && fviz_algorithm_abort_requested(algorithm) != FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_CANCELLED, "clip poly data filter was aborted");
            goto fail;
        }
    }
    if (filter->generate_cap != FVIZ_FALSE &&
        fviz_clip_generate_caps(
            output, segments, segment_count, filter->plane, filter->inside_out,
            source_ids, source_id_capacity, &source_id_count) != FVIZ_OK)
        goto fail;
    if (point_data.destination_scalars != NULL &&
        fviz_poly_data_set_scalars(output, point_data.destination_scalars) != FVIZ_OK)
        goto fail;
    if (fviz_clip_copy_cell_data(input, output, source_ids, source_id_count) != FVIZ_OK ||
        (source_id_count != 0u && fviz_poly_data_compute_normals(output) != FVIZ_OK) ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_algorithm_report_progress(algorithm, 1.0);
    fviz_clip_point_data_release(&point_data);
    fviz_release(edge_map);
    fviz_free(segments);
    fviz_free(source_ids);
    fviz_free(original_map);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_clip_point_data_release(&point_data);
    fviz_release(edge_map);
    fviz_free(segments);
    fviz_free(source_ids);
    fviz_free(original_map);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_clip_poly_data_filter_create(FVizClipPolyDataFilter** out_filter)
{
    FVizClipPolyDataFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizClipPolyDataFilter*)fviz_internal_object_allocate(
        sizeof(*filter), &g_fviz_clip_poly_data_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->plane = fviz_plane_from_point_normal(fviz_vec3(0.0f, 0.0f, 0.0f), fviz_vec3(1.0f, 0.0f, 0.0f));
    filter->inside_out = FVIZ_FALSE;
    filter->generate_cap = FVIZ_FALSE;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_clip_poly_data_filter_process_request;
    callbacks.get_state_mtime = fviz_clip_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

void fviz_clip_poly_data_filter_set_plane(FVizClipPolyDataFilter* filter, FVizPlane plane)
{
    if (filter == NULL) return;
    filter->plane = plane;
    fviz_object_modified((FVizObject*)filter);
}

FVizPlane fviz_clip_poly_data_filter_plane(const FVizClipPolyDataFilter* filter)
{
    return filter != NULL ? filter->plane : fviz_plane_from_point_normal(
        fviz_vec3(0.0f, 0.0f, 0.0f), fviz_vec3(1.0f, 0.0f, 0.0f));
}

void fviz_clip_poly_data_filter_set_inside_out(FVizClipPolyDataFilter* filter, FVizBool inside_out)
{
    if (filter != NULL && filter->inside_out != inside_out)
    {
        filter->inside_out = inside_out;
        fviz_object_modified((FVizObject*)filter);
    }
}

FVizBool fviz_clip_poly_data_filter_inside_out(const FVizClipPolyDataFilter* filter)
{
    return filter != NULL ? filter->inside_out : FVIZ_FALSE;
}

void fviz_clip_poly_data_filter_set_generate_cap(FVizClipPolyDataFilter* filter, FVizBool enabled)
{
    enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (filter != NULL && filter->generate_cap != enabled)
    {
        filter->generate_cap = enabled;
        fviz_object_modified((FVizObject*)filter);
    }
}

FVizBool fviz_clip_poly_data_filter_generate_cap(const FVizClipPolyDataFilter* filter)
{
    return filter != NULL ? filter->generate_cap : FVIZ_FALSE;
}

FVizResult fviz_clip_poly_data_filter_set_input_data(FVizClipPolyDataFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input) : FVIZ_ERROR_INVALID_ARGUMENT;
}
FVizResult fviz_clip_poly_data_filter_set_input_connection(FVizClipPolyDataFilter* filter, FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input) : FVIZ_ERROR_INVALID_ARGUMENT;
}
FVizAlgorithm* fviz_clip_poly_data_filter_algorithm(FVizClipPolyDataFilter* filter) { return filter != NULL ? filter->algorithm : NULL; }
FVizAlgorithmOutput* fviz_clip_poly_data_filter_output_port(FVizClipPolyDataFilter* filter) { return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL; }
FVizPolyData* fviz_clip_poly_data_filter_output(FVizClipPolyDataFilter* filter) { return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL; }
FVizResult fviz_clip_poly_data_filter_update(FVizClipPolyDataFilter* filter) { return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT; }
