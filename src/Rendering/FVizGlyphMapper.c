#include <math.h>
#include <stddef.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizQuat.h>
#include <FViz/Rendering/FVizGlyphMapper.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizGlyphMapperPrivate.h>

static void fviz_glyph_mapper_destroy(FVizObject* object);
static FVizMTime fviz_glyph_mapper_mtime(const FVizObject* object);

static void fviz_glyph_mapper_record_dirty(
    FVizGlyphMapper* mapper, FVizSize first, FVizSize count, FVizBool full)
{
    uint32_t slot;
    FVizGlyphDirtyRecord* record;
    fviz_object_modified((FVizObject*)mapper->instances);
    if (mapper->dirty_history_count < FVIZ_GLYPH_DIRTY_HISTORY_CAPACITY)
    {
        slot = (mapper->dirty_history_begin + mapper->dirty_history_count) %
            FVIZ_GLYPH_DIRTY_HISTORY_CAPACITY;
        ++mapper->dirty_history_count;
    }
    else
    {
        slot = mapper->dirty_history_begin;
        mapper->dirty_history_begin = (mapper->dirty_history_begin + 1u) %
            FVIZ_GLYPH_DIRTY_HISTORY_CAPACITY;
    }
    record = &mapper->dirty_history[slot];
    record->mtime = fviz_object_mtime((const FVizObject*)mapper->instances);
    record->first = first;
    record->count = count;
    record->full = full != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_object_modified((FVizObject*)mapper);
}

static const FVizObjectClass g_fviz_glyph_mapper_class = {
    FVIZ_TYPE_GLYPH_MAPPER,
    "FVizGlyphMapper",
    &g_fviz_object_class,
    fviz_glyph_mapper_destroy,
    fviz_glyph_mapper_mtime
};

static FVizMTime fviz_glyph_mapper_mtime(const FVizObject* object)
{
    const FVizGlyphMapper* mapper = (const FVizGlyphMapper*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    FVizMTime child = fviz_object_mtime((const FVizObject*)mapper->source);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)mapper->instances);
    if (child > mtime) mtime = child;
    return mtime;
}

FVizMTime fviz_internal_glyph_mapper_instances_mtime(const FVizGlyphMapper* mapper)
{
    return mapper != NULL ? fviz_object_mtime((const FVizObject*)mapper->instances) : 0u;
}

FVizResult fviz_internal_glyph_mapper_dirty_range_since(
    const FVizGlyphMapper* mapper, FVizMTime since_mtime, FVizDirtyRange* out_range)
{
    const FVizMTime current_mtime =
        fviz_internal_glyph_mapper_instances_mtime(mapper);
    uint32_t offset;
    FVizBool found = FVIZ_FALSE;
    if (mapper == NULL || out_range == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    out_range->first = 0u;
    out_range->count = 0u;
    out_range->full = FVIZ_FALSE;
    if (since_mtime >= current_mtime) return FVIZ_OK;
    if (since_mtime == 0u || mapper->dirty_history_count == 0u) goto full;
    {
        const FVizGlyphDirtyRecord* oldest =
            &mapper->dirty_history[mapper->dirty_history_begin];
        const uint32_t newest_slot = (mapper->dirty_history_begin +
            mapper->dirty_history_count - 1u) % FVIZ_GLYPH_DIRTY_HISTORY_CAPACITY;
        const FVizGlyphDirtyRecord* newest = &mapper->dirty_history[newest_slot];
        if (newest->mtime != current_mtime ||
            (mapper->dirty_history_count == FVIZ_GLYPH_DIRTY_HISTORY_CAPACITY &&
             since_mtime < oldest->mtime)) goto full;
    }
    for (offset = 0u; offset < mapper->dirty_history_count; ++offset)
    {
        const uint32_t slot = (mapper->dirty_history_begin + offset) %
            FVIZ_GLYPH_DIRTY_HISTORY_CAPACITY;
        const FVizGlyphDirtyRecord* record = &mapper->dirty_history[slot];
        FVizSize end;
        FVizSize current_end;
        if (record->mtime <= since_mtime) continue;
        if (record->full != FVIZ_FALSE) goto full;
        end = record->first + record->count;
        if (found == FVIZ_FALSE)
        {
            out_range->first = record->first;
            out_range->count = record->count;
            found = FVIZ_TRUE;
        }
        else
        {
            current_end = out_range->first + out_range->count;
            if (record->first < out_range->first) out_range->first = record->first;
            if (end > current_end) current_end = end;
            out_range->count = current_end - out_range->first;
        }
    }
    if (found != FVIZ_FALSE) return FVIZ_OK;
full:
    out_range->first = 0u;
    out_range->count = fviz_glyph_mapper_instance_count(mapper);
    out_range->full = FVIZ_TRUE;
    return FVIZ_OK;
}

static void fviz_glyph_mapper_destroy(FVizObject* object)
{
    FVizGlyphMapper* mapper = (FVizGlyphMapper*)object;
    fviz_release(mapper->source);
    fviz_release(mapper->instances);
    mapper->source = NULL;
    mapper->instances = NULL;
}

void fviz_vector_glyph_options_initialize(FVizVectorGlyphOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->scale_factor = 1.0f;
    options->scale_by_magnitude = FVIZ_TRUE;
    options->color_by_magnitude = FVIZ_TRUE;
    options->low_color[0] = 0.10f;
    options->low_color[1] = 0.35f;
    options->low_color[2] = 1.00f;
    options->high_color[0] = 1.00f;
    options->high_color[1] = 0.20f;
    options->high_color[2] = 0.05f;
    options->opacity = 1.0f;
}

void fviz_glyph_instance_initialize(FVizGlyphInstance* instance)
{
    if (instance == NULL) return;
    (void)memset(instance, 0, sizeof(*instance));
    instance->orientation = fviz_quat_identity();
    instance->scale = fviz_vec3(1.0f, 1.0f, 1.0f);
    instance->color[0] = 1.0f;
    instance->color[1] = 1.0f;
    instance->color[2] = 1.0f;
    instance->color[3] = 1.0f;
}

FVizResult fviz_glyph_mapper_create(FVizGlyphMapper** out_mapper)
{
    FVizGlyphMapper* mapper;
    if (out_mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_mapper = NULL;
    mapper = (FVizGlyphMapper*)fviz_internal_object_allocate(
        sizeof(*mapper), &g_fviz_glyph_mapper_class, NULL);
    if (mapper == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizGlyphInstance), &mapper->instances) != FVIZ_OK)
    {
        fviz_release(mapper);
        return fviz_last_error_code();
    }
    *out_mapper = mapper;
    return FVIZ_OK;
}

FVizResult fviz_glyph_mapper_set_source_poly_data(
    FVizGlyphMapper* mapper, FVizPolyData* source)
{
    if (mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "glyph mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (source != NULL && fviz_retain(source) == NULL) return fviz_last_error_code();
    fviz_release(mapper->source);
    mapper->source = source;
    fviz_object_modified((FVizObject*)mapper);
    return FVIZ_OK;
}

FVizPolyData* fviz_glyph_mapper_source_poly_data(FVizGlyphMapper* mapper)
{
    return mapper != NULL ? mapper->source : NULL;
}

const FVizPolyData* fviz_glyph_mapper_const_source_poly_data(const FVizGlyphMapper* mapper)
{
    return mapper != NULL ? mapper->source : NULL;
}

FVizResult fviz_glyph_mapper_reserve_instances(FVizGlyphMapper* mapper, FVizSize capacity)
{
    if (mapper == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_array_reserve(mapper->instances, capacity);
}

void fviz_glyph_mapper_set_gpu_residency_pinned(
    FVizGlyphMapper* mapper, FVizBool pinned)
{
    const FVizBool normalized = pinned != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (mapper == NULL || mapper->gpu_residency_pinned == normalized) return;
    mapper->gpu_residency_pinned = normalized;
    fviz_object_modified((FVizObject*)mapper);
}

FVizBool fviz_glyph_mapper_gpu_residency_pinned(const FVizGlyphMapper* mapper)
{
    return mapper != NULL ? mapper->gpu_residency_pinned : FVIZ_FALSE;
}

static FVizGlyphInstance fviz_glyph_instance_sanitize(const FVizGlyphInstance* source)
{
    FVizGlyphInstance instance = *source;
    instance.orientation = fviz_quat_normalize(instance.orientation);
    if (instance.scale.x == 0.0f) instance.scale.x = 1.0f;
    if (instance.scale.y == 0.0f) instance.scale.y = 1.0f;
    if (instance.scale.z == 0.0f) instance.scale.z = 1.0f;
    if (instance.color[0] < 0.0f) instance.color[0] = 0.0f;
    if (instance.color[1] < 0.0f) instance.color[1] = 0.0f;
    if (instance.color[2] < 0.0f) instance.color[2] = 0.0f;
    if (instance.color[3] < 0.0f) instance.color[3] = 0.0f;
    if (instance.color[0] > 1.0f) instance.color[0] = 1.0f;
    if (instance.color[1] > 1.0f) instance.color[1] = 1.0f;
    if (instance.color[2] > 1.0f) instance.color[2] = 1.0f;
    if (instance.color[3] > 1.0f) instance.color[3] = 1.0f;
    return instance;
}

FVizResult fviz_glyph_mapper_add_instance(
    FVizGlyphMapper* mapper, const FVizGlyphInstance* instance)
{
    FVizGlyphInstance sanitized;
    if (mapper == NULL || instance == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    sanitized = fviz_glyph_instance_sanitize(instance);
    if (fviz_array_push(mapper->instances, &sanitized) != FVIZ_OK) return fviz_last_error_code();
    if (sanitized.color[3] < 0.999999f) mapper->has_translucent_instances = FVIZ_TRUE;
    fviz_glyph_mapper_record_dirty(
        mapper, fviz_array_count(mapper->instances) - 1u, 1u, FVIZ_FALSE);
    return FVIZ_OK;
}

FVizResult fviz_glyph_mapper_add_instances(
    FVizGlyphMapper* mapper, const FVizGlyphInstance* instances, FVizSize count)
{
    FVizGlyphInstance* destination;
    FVizSize i;
    if (mapper == NULL || (instances == NULL && count != 0u)) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (count == 0u) return FVIZ_OK;
    if (fviz_array_append_uninitialized(mapper->instances, count, (void**)&destination) != FVIZ_OK)
        return fviz_last_error_code();
    for (i = 0u; i < count; ++i)
    {
        destination[i] = fviz_glyph_instance_sanitize(&instances[i]);
        if (destination[i].color[3] < 0.999999f) mapper->has_translucent_instances = FVIZ_TRUE;
    }
    fviz_glyph_mapper_record_dirty(
        mapper, fviz_array_count(mapper->instances) - count, count, FVIZ_FALSE);
    return FVIZ_OK;
}

static void fviz_glyph_mapper_recompute_translucency(FVizGlyphMapper* mapper)
{
    const FVizGlyphInstance* instances = fviz_glyph_mapper_instances(mapper);
    FVizSize i;
    mapper->has_translucent_instances = FVIZ_FALSE;
    for (i = 0u; i < fviz_glyph_mapper_instance_count(mapper); ++i)
    {
        if (instances[i].color[3] < 0.999999f)
        {
            mapper->has_translucent_instances = FVIZ_TRUE;
            return;
        }
    }
}

FVizResult fviz_glyph_mapper_set_instances(
    FVizGlyphMapper* mapper,
    FVizSize first,
    const FVizGlyphInstance* instances,
    FVizSize count)
{
    FVizGlyphInstance* destination;
    FVizSize i;
    const FVizSize instance_count = fviz_glyph_mapper_instance_count(mapper);
    if (mapper == NULL || (instances == NULL && count != 0u) ||
        first > instance_count || count > instance_count - first)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (count == 0u) return FVIZ_OK;
    destination = (FVizGlyphInstance*)fviz_array_data(mapper->instances) + first;
    for (i = 0u; i < count; ++i)
        destination[i] = fviz_glyph_instance_sanitize(&instances[i]);
    fviz_glyph_mapper_recompute_translucency(mapper);
    fviz_glyph_mapper_record_dirty(mapper, first, count, FVIZ_FALSE);
    return FVIZ_OK;
}

FVizResult fviz_glyph_mapper_set_instance(
    FVizGlyphMapper* mapper, FVizSize index, const FVizGlyphInstance* instance)
{
    return fviz_glyph_mapper_set_instances(mapper, index, instance, 1u);
}

void fviz_glyph_mapper_clear_instances(FVizGlyphMapper* mapper)
{
    if (mapper == NULL) return;
    fviz_array_clear(mapper->instances);
    mapper->has_translucent_instances = FVIZ_FALSE;
    fviz_glyph_mapper_record_dirty(mapper, 0u, 0u, FVIZ_TRUE);
}

FVizSize fviz_glyph_mapper_instance_count(const FVizGlyphMapper* mapper)
{
    return mapper != NULL ? fviz_array_count(mapper->instances) : 0u;
}

FVizBool fviz_glyph_mapper_has_translucent_instances(const FVizGlyphMapper* mapper)
{
    return mapper != NULL ? mapper->has_translucent_instances : FVIZ_FALSE;
}

const FVizGlyphInstance* fviz_glyph_mapper_instances(const FVizGlyphMapper* mapper)
{
    return mapper != NULL
        ? (const FVizGlyphInstance*)fviz_array_const_data(mapper->instances)
        : NULL;
}

FVizResult fviz_glyph_mapper_get_instance(
    const FVizGlyphMapper* mapper, FVizSize index, FVizGlyphInstance* out_instance)
{
    const FVizGlyphInstance* instance;
    if (mapper == NULL || out_instance == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    instance = (const FVizGlyphInstance*)fviz_array_const_at(mapper->instances, index);
    if (instance == NULL) return FVIZ_ERROR_NOT_FOUND;
    *out_instance = *instance;
    return FVIZ_OK;
}

static FVizQuat fviz_glyph_orientation_from_x(FVizVec3 direction)
{
    const FVizVec3 x_axis = {1.0f, 0.0f, 0.0f};
    const float length = fviz_vec3_length(direction);
    FVizVec3 unit;
    float dot;
    FVizVec3 axis;
    if (!(length > 1.0e-20f) || !isfinite(length)) return fviz_quat_identity();
    unit = fviz_vec3_scale(direction, 1.0f / length);
    dot = fviz_vec3_dot(x_axis, unit);
    if (dot >= 0.999999f) return fviz_quat_identity();
    if (dot <= -0.999999f)
        return fviz_quat_from_axis_angle(fviz_vec3(0.0f, 1.0f, 0.0f), 3.14159265358979323846f);
    axis = fviz_vec3_normalize(fviz_vec3_cross(x_axis, unit));
    return fviz_quat_from_axis_angle(axis, acosf(fmaxf(-1.0f, fminf(1.0f, dot))));
}

FVizResult fviz_glyph_mapper_build_from_point_vectors(
    FVizGlyphMapper* mapper,
    const FVizPolyData* input,
    const char* vector_array_name,
    const FVizVectorGlyphOptions* options)
{
    FVizVectorGlyphOptions defaults;
    const FVizVectorGlyphOptions* resolved = options;
    const FVizAttributeSet* point_data;
    const FVizDataArray* vectors;
    const FVizVec3* points;
    FVizSize point_count;
    FVizSize vector_count;
    FVizArray* generated = NULL;
    double minimum = 0.0;
    double maximum = 1.0;
    FVizSize i;
    FVizBool translucent = FVIZ_FALSE;

    if (mapper == NULL || input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "glyph mapper and vector input must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (resolved == NULL)
    {
        fviz_vector_glyph_options_initialize(&defaults);
        resolved = &defaults;
    }
    else if (resolved->struct_size < offsetof(FVizVectorGlyphOptions, opacity) + sizeof(resolved->opacity) ||
             !isfinite(resolved->scale_factor) || resolved->scale_factor < 0.0f ||
             !isfinite(resolved->opacity))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "vector glyph options are invalid or truncated");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    point_data = fviz_poly_data_const_point_data(input);
    vectors = vector_array_name != NULL
        ? fviz_attribute_set_const_get(point_data, vector_array_name)
        : fviz_attribute_set_const_active(point_data, FVIZ_ATTRIBUTE_VECTORS);
    if (vectors == NULL || fviz_data_array_components(vectors) < 3u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "a three-component point vector array is required");
        return FVIZ_ERROR_NOT_FOUND;
    }
    point_count = fviz_poly_data_point_count(input);
    vector_count = fviz_data_array_tuple_count(vectors);
    if (vector_count != point_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "point vector tuple count must match point count");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    points = fviz_poly_data_points(input);
    if (resolved->color_by_magnitude != FVIZ_FALSE && point_count > 0u)
    {
        FVizResult range_result = fviz_data_array_get_range(vectors, -1, FVIZ_TRUE, &minimum, &maximum);
        if (range_result != FVIZ_OK) return range_result;
    }
    if (fviz_array_create(sizeof(FVizGlyphInstance), &generated) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_array_reserve(generated, point_count) != FVIZ_OK)
    {
        fviz_release(generated);
        return fviz_last_error_code();
    }
    for (i = 0u; i < point_count; ++i)
    {
        double x, y, z;
        FVizVec3 vector;
        float magnitude;
        FVizGlyphInstance instance;
        float scale;
        if (fviz_data_array_get_component(vectors, i, 0u, &x) != FVIZ_OK ||
            fviz_data_array_get_component(vectors, i, 1u, &y) != FVIZ_OK ||
            fviz_data_array_get_component(vectors, i, 2u, &z) != FVIZ_OK)
        {
            fviz_release(generated);
            return fviz_last_error_code();
        }
        if (!isfinite(x) || !isfinite(y) || !isfinite(z)) continue;
        vector = fviz_vec3((float)x, (float)y, (float)z);
        magnitude = fviz_vec3_length(vector);
        if (!(magnitude > 1.0e-20f) || !isfinite(magnitude)) continue;
        fviz_glyph_instance_initialize(&instance);
        instance.position = points[i];
        instance.orientation = fviz_glyph_orientation_from_x(vector);
        scale = resolved->scale_factor * (resolved->scale_by_magnitude != FVIZ_FALSE ? magnitude : 1.0f);
        if (!(scale > 0.0f) || !isfinite(scale)) continue;
        instance.scale = fviz_vec3(scale, scale, scale);
        if (resolved->color_by_magnitude != FVIZ_FALSE)
        {
            const double denominator = maximum - minimum;
            float t = denominator > 1.0e-30 ? (float)(((double)magnitude - minimum) / denominator) : 0.5f;
            unsigned int c;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            for (c = 0u; c < 3u; ++c)
                instance.color[c] = resolved->low_color[c] +
                    (resolved->high_color[c] - resolved->low_color[c]) * t;
        }
        instance.color[3] = resolved->opacity < 0.0f ? 0.0f :
            (resolved->opacity > 1.0f ? 1.0f : resolved->opacity);
        instance = fviz_glyph_instance_sanitize(&instance);
        if (fviz_array_push(generated, &instance) != FVIZ_OK)
        {
            fviz_release(generated);
            return fviz_last_error_code();
        }
        if (instance.color[3] < 0.999999f) translucent = FVIZ_TRUE;
    }
    fviz_release(mapper->instances);
    mapper->instances = generated;
    mapper->has_translucent_instances = translucent;
    mapper->dirty_history_begin = 0u;
    mapper->dirty_history_count = 0u;
    fviz_glyph_mapper_record_dirty(
        mapper, 0u, fviz_array_count(generated), FVIZ_TRUE);
    return FVIZ_OK;
}

FVizBounds fviz_glyph_mapper_bounds(const FVizGlyphMapper* mapper)
{
    FVizBounds result = fviz_bounds_empty();
    FVizBounds source_bounds;
    FVizVec3 source_center;
    FVizVec3 source_half_extent;
    const FVizGlyphInstance* instances;
    FVizSize instance_count;
    FVizSize i;
    if (mapper == NULL || mapper->source == NULL) return result;
    source_bounds = fviz_poly_data_bounds(mapper->source);
    if (source_bounds.valid == FVIZ_FALSE) return result;
    source_center = fviz_vec3_scale(fviz_vec3_add(source_bounds.min, source_bounds.max), 0.5f);
    source_half_extent = fviz_vec3_scale(fviz_vec3_sub(source_bounds.max, source_bounds.min), 0.5f);
    instances = fviz_glyph_mapper_instances(mapper);
    instance_count = fviz_glyph_mapper_instance_count(mapper);
    for (i = 0u; i < instance_count; ++i)
    {
        const FVizGlyphInstance* instance = &instances[i];
        FVizVec3 center = fviz_vec3(
            source_center.x * instance->scale.x,
            source_center.y * instance->scale.y,
            source_center.z * instance->scale.z);
        FVizVec3 ex = fviz_quat_rotate_vec3(instance->orientation,
            fviz_vec3(fabsf(source_half_extent.x * instance->scale.x), 0.0f, 0.0f));
        FVizVec3 ey = fviz_quat_rotate_vec3(instance->orientation,
            fviz_vec3(0.0f, fabsf(source_half_extent.y * instance->scale.y), 0.0f));
        FVizVec3 ez = fviz_quat_rotate_vec3(instance->orientation,
            fviz_vec3(0.0f, 0.0f, fabsf(source_half_extent.z * instance->scale.z)));
        FVizVec3 extent;
        FVizVec3 minimum;
        FVizVec3 maximum;
        center = fviz_vec3_add(fviz_quat_rotate_vec3(instance->orientation, center), instance->position);
        extent = fviz_vec3(
            fabsf(ex.x) + fabsf(ey.x) + fabsf(ez.x),
            fabsf(ex.y) + fabsf(ey.y) + fabsf(ez.y),
            fabsf(ex.z) + fabsf(ey.z) + fabsf(ez.z));
        minimum = fviz_vec3_sub(center, extent);
        maximum = fviz_vec3_add(center, extent);
        fviz_bounds_include_point(&result, minimum);
        fviz_bounds_include_point(&result, maximum);
    }
    return result;
}
