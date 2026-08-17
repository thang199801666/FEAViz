#include <FViz/Core/FVizError.h>
#include <FViz/Math/FVizMat3.h>
#include <FViz/Rendering/FVizActor.h>
#include <FViz/Rendering/FVizGlyphMapper.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizActorPrivate.h>
#include <FViz/Rendering/FVizMapperPrivate.h>

static void fviz_actor_destroy(FVizObject* object);
static FVizMTime fviz_actor_mtime(const FVizObject* object);

static FVizBool fviz_actor_dependency_modified(FVizObject* caller, FVizEventId event_id, void* call_data,
                                               void* client_data)
{
    FVizActor* actor = (FVizActor*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (actor != NULL) fviz_object_modified((FVizObject*)actor);
    return FVIZ_FALSE;
}

static FVizBool fviz_actor_vec3_equal(FVizVec3 a, FVizVec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_actor_quat_equal(FVizQuat a, FVizQuat b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_actor_observe_dependency(FVizObject* dependency, FVizActor* actor, FVizObserverTag* out_tag)
{
    if (out_tag == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (dependency == NULL) return FVIZ_OK;
    return fviz_object_add_observer(dependency, FVIZ_EVENT_MODIFIED, 0.0f, fviz_actor_dependency_modified, actor,
                                    out_tag);
}

static const FVizObjectClass g_fviz_actor_class = {FVIZ_TYPE_ACTOR, "FVizActor", &g_fviz_object_class,
                                                   fviz_actor_destroy, fviz_actor_mtime};

static FVizMTime fviz_actor_mtime(const FVizObject* object)
{
    /* Mapper/glyph/transform dependencies bridge ModifiedEvent into Actor. */
    return fviz_internal_object_local_mtime(object);
}

static void fviz_actor_destroy(FVizObject* object)
{
    FVizActor* actor = (FVizActor*)object;
    if (actor->mapper != NULL && actor->mapper_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)actor->mapper, actor->mapper_modified_tag);
    if (actor->glyph_mapper != NULL && actor->glyph_mapper_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)actor->glyph_mapper, actor->glyph_mapper_modified_tag);
    if (actor->volume_mapper != NULL && actor->volume_mapper_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)actor->volume_mapper, actor->volume_mapper_modified_tag);
    if (actor->user_transform != NULL && actor->user_transform_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)actor->user_transform, actor->user_transform_modified_tag);
    fviz_release(actor->mapper);
    fviz_release(actor->glyph_mapper);
    fviz_release(actor->volume_mapper);
    fviz_release(actor->user_transform);
    actor->mapper = NULL;
    actor->glyph_mapper = NULL;
    actor->volume_mapper = NULL;
    actor->user_transform = NULL;
}

FVizResult fviz_actor_create(FVizActor** out_actor)
{
    FVizActor* actor;
    if (out_actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_actor = NULL;
    actor = (FVizActor*)fviz_internal_object_allocate(sizeof(FVizActor), &g_fviz_actor_class, NULL);
    if (actor == NULL) return fviz_last_error_code();
    if (fviz_mapper_create(&actor->mapper) != FVIZ_OK)
    {
        fviz_release(actor);
        return fviz_last_error_code();
    }
    if (fviz_actor_observe_dependency((FVizObject*)actor->mapper, actor, &actor->mapper_modified_tag) != FVIZ_OK)
    {
        fviz_release(actor);
        return fviz_last_error_code();
    }
    actor->color[0] = 0.72f;
    actor->color[1] = 0.78f;
    actor->color[2] = 0.88f;
    actor->visible = FVIZ_TRUE;
    actor->pickable = FVIZ_TRUE;
    actor->wireframe = FVIZ_FALSE;
    actor->edge_visible = FVIZ_FALSE;
    actor->opacity = 1.0f;
    actor->edge_color[0] = 0.05f;
    actor->edge_color[1] = 0.05f;
    actor->edge_color[2] = 0.05f;
    actor->line_width = 1.0f;
    actor->line_depth_bias = 0.0f;
    actor->line_cap = FVIZ_LINE_CAP_ROUND;
    actor->line_join = FVIZ_LINE_JOIN_MITER;
    actor->line_miter_limit = 4.0f;
    actor->line_dash_length = 0.0f;
    actor->line_gap_length = 0.0f;
    actor->line_dash_phase = 0.0f;
    actor->line_scalar_coloring = FVIZ_FALSE;
    actor->point_visible = FVIZ_FALSE;
    actor->point_size = 5.0f;
    actor->point_shape = FVIZ_POINT_CIRCLE;
    actor->point_color[0] = 0.95f;
    actor->point_color[1] = 0.55f;
    actor->point_color[2] = 0.12f;
    actor->point_scalar_coloring = FVIZ_FALSE;
    actor->ambient = 0.18f;
    actor->diffuse = 0.82f;
    actor->specular = 0.25f;
    actor->specular_power = 32.0f;
    actor->shading_mode = FVIZ_SHADING_SMOOTH;
    actor->cull_mode = FVIZ_CULL_BACK;
    actor->coincident_mode = FVIZ_COINCIDENT_TOPOLOGY_DEFAULT;
    actor->offset_faces = FVIZ_TRUE;
    actor->polygon_offset_factor = 1.0f;
    actor->polygon_offset_units = 1.0f;
    actor->line_offset_factor = 1.0f;
    actor->line_offset_units = 1.0f;
    actor->point_offset_units = 0.5f;
    actor->z_shift = 0.0f;
    actor->depth_test = FVIZ_TRUE;
    actor->depth_write = FVIZ_TRUE;
    actor->depth_function = FVIZ_DEPTH_FUNCTION_LEQUAL;
    actor->depth_range_minimum = 0.0f;
    actor->depth_range_maximum = 1.0f;
    actor->render_layer = 0;
    actor->render_priority = 0;
    actor->pass_order = FVIZ_RENDER_PASS_OPAQUE;
    actor->overlay_mode = FVIZ_OVERLAY_TOPOLOGY_SURFACE_EDGES;
    actor->topology_data_flags = FVIZ_TOPOLOGY_DATA_CONNECTIVITY | FVIZ_TOPOLOGY_DATA_CELL_CLASSIFICATION;
    actor->position = fviz_vec3(0.0f, 0.0f, 0.0f);
    actor->orientation = fviz_quat_identity();
    actor->scale = fviz_vec3(1.0f, 1.0f, 1.0f);
    actor->world_bounds_cache = fviz_bounds_empty();
    actor->world_bounds_geometry_mtime = 0u;
    actor->world_bounds_user_transform_mtime = 0u;
    actor->transform_revision = 1u;
    actor->world_bounds_transform_revision = 0u;
    actor->world_bounds_cache_initialized = FVIZ_FALSE;
    *out_actor = actor;
    return FVIZ_OK;
}

FVizResult fviz_actor_set_mapper(FVizActor* actor, FVizMapper* mapper)
{
    FVizObserverTag new_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (actor == NULL || mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "actor and mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (mapper == actor->mapper) return FVIZ_OK;
    if (fviz_retain(mapper) == NULL) return fviz_last_error_code();
    if (fviz_actor_observe_dependency((FVizObject*)mapper, actor, &new_tag) != FVIZ_OK)
    {
        fviz_release(mapper);
        return fviz_last_error_code();
    }
    if (actor->mapper != NULL && actor->mapper_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)actor->mapper, actor->mapper_modified_tag);
    fviz_release(actor->mapper);
    actor->mapper = mapper;
    actor->mapper_modified_tag = new_tag;
    fviz_object_modified((FVizObject*)actor);
    return FVIZ_OK;
}

FVizMapper* fviz_actor_mapper(FVizActor* actor)
{
    return actor != NULL ? actor->mapper : NULL;
}

FVizResult fviz_actor_set_glyph_mapper(FVizActor* actor, FVizGlyphMapper* mapper)
{
    FVizObserverTag new_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (actor == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (mapper == actor->glyph_mapper) return FVIZ_OK;
    if (mapper != NULL)
    {
        if (fviz_retain(mapper) == NULL) return fviz_last_error_code();
        if (fviz_actor_observe_dependency((FVizObject*)mapper, actor, &new_tag) != FVIZ_OK)
        {
            fviz_release(mapper);
            return fviz_last_error_code();
        }
    }
    if (actor->glyph_mapper != NULL && actor->glyph_mapper_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)actor->glyph_mapper, actor->glyph_mapper_modified_tag);
    fviz_release(actor->glyph_mapper);
    actor->glyph_mapper = mapper;
    actor->glyph_mapper_modified_tag = new_tag;
    fviz_object_modified((FVizObject*)actor);
    return FVIZ_OK;
}

FVizGlyphMapper* fviz_actor_glyph_mapper(FVizActor* actor)
{
    return actor != NULL ? actor->glyph_mapper : NULL;
}

const FVizGlyphMapper* fviz_actor_const_glyph_mapper(const FVizActor* actor)
{
    return actor != NULL ? actor->glyph_mapper : NULL;
}

FVizResult fviz_actor_set_volume_mapper(FVizActor* actor, FVizVolumeMapper* mapper)
{
    FVizObserverTag new_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (actor == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (mapper == actor->volume_mapper) return FVIZ_OK;
    if (mapper != NULL)
    {
        if (fviz_retain(mapper) == NULL) return fviz_last_error_code();
        if (fviz_actor_observe_dependency((FVizObject*)mapper, actor, &new_tag) != FVIZ_OK)
        {
            fviz_release(mapper);
            return fviz_last_error_code();
        }
    }
    if (actor->volume_mapper != NULL && actor->volume_mapper_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)actor->volume_mapper, actor->volume_mapper_modified_tag);
    fviz_release(actor->volume_mapper);
    actor->volume_mapper = mapper;
    actor->volume_mapper_modified_tag = new_tag;
    fviz_object_modified((FVizObject*)actor);
    return FVIZ_OK;
}

FVizVolumeMapper* fviz_actor_volume_mapper(FVizActor* actor)
{
    return actor != NULL ? actor->volume_mapper : NULL;
}

const FVizVolumeMapper* fviz_actor_const_volume_mapper(const FVizActor* actor)
{
    return actor != NULL ? actor->volume_mapper : NULL;
}

FVizResult fviz_actor_set_poly_data(FVizActor* actor, FVizPolyData* poly_data)
{
    if (actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (actor->glyph_mapper != NULL)
    {
        if (actor->glyph_mapper_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)actor->glyph_mapper, actor->glyph_mapper_modified_tag);
        fviz_release(actor->glyph_mapper);
        actor->glyph_mapper = NULL;
        actor->glyph_mapper_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    }
    if (actor->volume_mapper != NULL)
    {
        if (actor->volume_mapper_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)actor->volume_mapper, actor->volume_mapper_modified_tag);
        fviz_release(actor->volume_mapper);
        actor->volume_mapper = NULL;
        actor->volume_mapper_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    }
    return fviz_mapper_set_poly_data(actor->mapper, poly_data);
}

FVizPolyData* fviz_actor_poly_data(FVizActor* actor)
{
    return actor != NULL ? fviz_mapper_poly_data(actor->mapper) : NULL;
}

const FVizPolyData* fviz_actor_const_poly_data(const FVizActor* actor)
{
    return actor != NULL ? fviz_mapper_const_poly_data(actor->mapper) : NULL;
}

void fviz_actor_set_color(FVizActor* actor, float red, float green, float blue)
{
    if (actor == NULL || (actor->color[0] == red && actor->color[1] == green && actor->color[2] == blue)) return;
    actor->color[0] = red;
    actor->color[1] = green;
    actor->color[2] = blue;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_get_color(const FVizActor* actor, float* red, float* green, float* blue)
{
    if (actor == NULL) return;
    if (red != NULL) *red = actor->color[0];
    if (green != NULL) *green = actor->color[1];
    if (blue != NULL) *blue = actor->color[2];
}

void fviz_actor_set_visible(FVizActor* actor, FVizBool visible)
{
    if (actor != NULL)
    {
        const FVizBool normalized = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (actor->visible == normalized) return;
        actor->visible = normalized;
        fviz_object_modified((FVizObject*)actor);
    }
}

FVizBool fviz_actor_is_visible(const FVizActor* actor)
{
    return actor != NULL ? actor->visible : FVIZ_FALSE;
}

void fviz_actor_set_pickable(FVizActor* actor, FVizBool pickable)
{
    if (actor != NULL)
    {
        const FVizBool normalized = pickable != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (actor->pickable == normalized) return;
        actor->pickable = normalized;
        fviz_object_modified((FVizObject*)actor);
    }
}

FVizBool fviz_actor_pickable(const FVizActor* actor)
{
    return actor != NULL ? actor->pickable : FVIZ_FALSE;
}

static FVizVec3 fviz_actor_transform_bounds_point(FVizMat4 matrix, FVizVec3 point)
{
    return fviz_vec3(matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
                     matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
                     matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]);
}

FVizBounds fviz_actor_bounds(const FVizActor* actor)
{
    FVizBounds local = fviz_bounds_empty();
    FVizBounds world = fviz_bounds_empty();
    FVizMTime geometry_mtime = 0u;
    FVizMTime user_transform_mtime = 0u;
    FVizMat4 model;
    unsigned int corner;
    FVizActor* mutable_actor;
    if (actor == NULL) return world;
    if (actor->glyph_mapper != NULL)
    {
        local = fviz_glyph_mapper_bounds(actor->glyph_mapper);
        geometry_mtime = fviz_object_mtime((const FVizObject*)actor->glyph_mapper);
    }
    else if (actor->volume_mapper != NULL)
    {
        local = fviz_volume_mapper_bounds(actor->volume_mapper);
        geometry_mtime = fviz_object_mtime((const FVizObject*)actor->volume_mapper);
    }
    else if (fviz_actor_const_poly_data(actor) != NULL)
    {
        const FVizPolyData* data = fviz_actor_const_poly_data(actor);
        local = fviz_poly_data_bounds(data);
        geometry_mtime = fviz_poly_data_geometry_mtime(data);
    }
    if (actor->user_transform != NULL)
        user_transform_mtime = fviz_object_mtime((const FVizObject*)actor->user_transform);
    if (actor->world_bounds_cache_initialized != FVIZ_FALSE && actor->world_bounds_geometry_mtime == geometry_mtime &&
        actor->world_bounds_user_transform_mtime == user_transform_mtime &&
        actor->world_bounds_transform_revision == actor->transform_revision)
        return actor->world_bounds_cache;
    if (local.valid != FVIZ_FALSE)
    {
        model = fviz_actor_transform_matrix(actor);
        for (corner = 0u; corner < 8u; ++corner)
        {
            const FVizVec3 point = fviz_vec3((corner & 1u) != 0u ? local.max.x : local.min.x,
                                             (corner & 2u) != 0u ? local.max.y : local.min.y,
                                             (corner & 4u) != 0u ? local.max.z : local.min.z);
            fviz_bounds_include_point(&world, fviz_actor_transform_bounds_point(model, point));
        }
    }
    mutable_actor = (FVizActor*)actor;
    mutable_actor->world_bounds_cache = world;
    mutable_actor->world_bounds_geometry_mtime = geometry_mtime;
    mutable_actor->world_bounds_user_transform_mtime = user_transform_mtime;
    mutable_actor->world_bounds_transform_revision = actor->transform_revision;
    mutable_actor->world_bounds_cache_initialized = FVIZ_TRUE;
    return world;
}

void fviz_actor_set_wireframe(FVizActor* actor, FVizBool enabled)
{
    if (actor != NULL)
    {
        const FVizBool normalized = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (actor->wireframe == normalized) return;
        actor->wireframe = normalized;
        fviz_object_modified((FVizObject*)actor);
    }
}

FVizBool fviz_actor_wireframe(const FVizActor* actor)
{
    return actor != NULL ? actor->wireframe : FVIZ_FALSE;
}

void fviz_actor_set_opacity(FVizActor* actor, float opacity)
{
    if (actor == NULL) return;
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    if (actor->opacity == opacity) return;
    actor->opacity = opacity;
    fviz_object_modified((FVizObject*)actor);
}

float fviz_actor_opacity(const FVizActor* actor)
{
    return actor != NULL ? actor->opacity : 0.0f;
}

void fviz_actor_set_edge_visibility(FVizActor* actor, FVizBool visible)
{
    if (actor == NULL) return;
    {
        const FVizBool normalized = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (actor->edge_visible == normalized) return;
        actor->edge_visible = normalized;
    }
    fviz_object_modified((FVizObject*)actor);
}

FVizBool fviz_actor_edge_visibility(const FVizActor* actor)
{
    return actor != NULL ? actor->edge_visible : FVIZ_FALSE;
}

void fviz_actor_set_edge_color(FVizActor* actor, float red, float green, float blue)
{
    if (actor == NULL || (actor->edge_color[0] == red && actor->edge_color[1] == green && actor->edge_color[2] == blue))
        return;
    actor->edge_color[0] = red;
    actor->edge_color[1] = green;
    actor->edge_color[2] = blue;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_get_edge_color(const FVizActor* actor, float* red, float* green, float* blue)
{
    if (actor == NULL) return;
    if (red != NULL) *red = actor->edge_color[0];
    if (green != NULL) *green = actor->edge_color[1];
    if (blue != NULL) *blue = actor->edge_color[2];
}

void fviz_actor_set_line_width(FVizActor* actor, float width)
{
    if (actor == NULL) return;
    if (width < 1.0f) width = 1.0f;
    if (actor->line_width == width) return;
    actor->line_width = width;
    fviz_object_modified((FVizObject*)actor);
}

float fviz_actor_line_width(const FVizActor* actor)
{
    return actor != NULL ? actor->line_width : 1.0f;
}

void fviz_actor_set_line_depth_bias(FVizActor* actor, float bias)
{
    if (actor == NULL) return;
    if (!(bias >= 0.0f)) bias = 0.0f;
    if (bias > 0.01f) bias = 0.01f;
    if (actor->line_depth_bias == bias) return;
    actor->line_depth_bias = bias;
    fviz_object_modified((FVizObject*)actor);
}

float fviz_actor_line_depth_bias(const FVizActor* actor)
{
    return actor != NULL ? actor->line_depth_bias : 0.0f;
}

void fviz_actor_set_line_cap(FVizActor* actor, FVizLineCap cap)
{
    if (actor == NULL) return;
    if (cap < FVIZ_LINE_CAP_BUTT || cap > FVIZ_LINE_CAP_ROUND) cap = FVIZ_LINE_CAP_BUTT;
    if (actor->line_cap == cap) return;
    actor->line_cap = cap;
    fviz_object_modified((FVizObject*)actor);
}

FVizLineCap fviz_actor_line_cap(const FVizActor* actor)
{
    return actor != NULL ? actor->line_cap : FVIZ_LINE_CAP_BUTT;
}

void fviz_actor_set_line_join(FVizActor* actor, FVizLineJoin join)
{
    if (actor == NULL || join < FVIZ_LINE_JOIN_MITER || join > FVIZ_LINE_JOIN_ROUND) return;
    if (actor->line_join != join)
    {
        actor->line_join = join;
        fviz_object_modified((FVizObject*)actor);
    }
}

FVizLineJoin fviz_actor_line_join(const FVizActor* actor)
{
    return actor != NULL ? actor->line_join : FVIZ_LINE_JOIN_MITER;
}

void fviz_actor_set_line_miter_limit(FVizActor* actor, float limit)
{
    if (actor == NULL) return;
    if (limit < 1.0f) limit = 1.0f;
    if (limit > 32.0f) limit = 32.0f;
    if (actor->line_miter_limit != limit)
    {
        actor->line_miter_limit = limit;
        fviz_object_modified((FVizObject*)actor);
    }
}

float fviz_actor_line_miter_limit(const FVizActor* actor)
{
    return actor != NULL ? actor->line_miter_limit : 4.0f;
}

void fviz_actor_set_line_dash(FVizActor* actor, float dash_length, float gap_length, float phase)
{
    if (actor == NULL) return;
    if (dash_length < 0.0f) dash_length = 0.0f;
    if (gap_length < 0.0f) gap_length = 0.0f;
    if (phase < 0.0f) phase = 0.0f;
    if (actor->line_dash_length == dash_length && actor->line_gap_length == gap_length &&
        actor->line_dash_phase == phase)
        return;
    actor->line_dash_length = dash_length;
    actor->line_gap_length = gap_length;
    actor->line_dash_phase = phase;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_get_line_dash(const FVizActor* actor, float* dash_length, float* gap_length, float* phase)
{
    if (actor == NULL) return;
    if (dash_length != NULL) *dash_length = actor->line_dash_length;
    if (gap_length != NULL) *gap_length = actor->line_gap_length;
    if (phase != NULL) *phase = actor->line_dash_phase;
}

void fviz_actor_set_line_scalar_coloring(FVizActor* actor, FVizBool enabled)
{
    if (actor == NULL) return;
    {
        const FVizBool normalized = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (actor->line_scalar_coloring == normalized) return;
        actor->line_scalar_coloring = normalized;
    }
    fviz_object_modified((FVizObject*)actor);
}

FVizBool fviz_actor_line_scalar_coloring(const FVizActor* actor)
{
    return actor != NULL ? actor->line_scalar_coloring : FVIZ_FALSE;
}

void fviz_actor_set_point_visibility(FVizActor* actor, FVizBool visible)
{
    if (actor == NULL) return;
    {
        const FVizBool normalized = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (actor->point_visible == normalized) return;
        actor->point_visible = normalized;
    }
    fviz_object_modified((FVizObject*)actor);
}

FVizBool fviz_actor_point_visibility(const FVizActor* actor)
{
    return actor != NULL ? actor->point_visible : FVIZ_FALSE;
}

void fviz_actor_set_point_size(FVizActor* actor, float size_pixels)
{
    if (actor == NULL) return;
    if (size_pixels < 1.0f) size_pixels = 1.0f;
    if (size_pixels > 256.0f) size_pixels = 256.0f;
    if (actor->point_size == size_pixels) return;
    actor->point_size = size_pixels;
    fviz_object_modified((FVizObject*)actor);
}

float fviz_actor_point_size(const FVizActor* actor)
{
    return actor != NULL ? actor->point_size : 1.0f;
}

void fviz_actor_set_point_shape(FVizActor* actor, FVizPointShape shape)
{
    if (actor == NULL) return;
    if (shape < FVIZ_POINT_SQUARE || shape > FVIZ_POINT_SPHERE_IMPOSTOR) shape = FVIZ_POINT_CIRCLE;
    if (actor->point_shape == shape) return;
    actor->point_shape = shape;
    fviz_object_modified((FVizObject*)actor);
}

FVizPointShape fviz_actor_point_shape(const FVizActor* actor)
{
    return actor != NULL ? actor->point_shape : FVIZ_POINT_SQUARE;
}

void fviz_actor_set_point_color(FVizActor* actor, float red, float green, float blue)
{
    if (actor == NULL ||
        (actor->point_color[0] == red && actor->point_color[1] == green && actor->point_color[2] == blue))
        return;
    actor->point_color[0] = red;
    actor->point_color[1] = green;
    actor->point_color[2] = blue;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_get_point_color(const FVizActor* actor, float* red, float* green, float* blue)
{
    if (actor == NULL) return;
    if (red != NULL) *red = actor->point_color[0];
    if (green != NULL) *green = actor->point_color[1];
    if (blue != NULL) *blue = actor->point_color[2];
}

void fviz_actor_set_point_scalar_coloring(FVizActor* actor, FVizBool enabled)
{
    if (actor == NULL) return;
    {
        const FVizBool normalized = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (actor->point_scalar_coloring == normalized) return;
        actor->point_scalar_coloring = normalized;
    }
    fviz_object_modified((FVizObject*)actor);
}

FVizBool fviz_actor_point_scalar_coloring(const FVizActor* actor)
{
    return actor != NULL ? actor->point_scalar_coloring : FVIZ_FALSE;
}

void fviz_actor_set_material(FVizActor* actor, float ambient, float diffuse, float specular, float specular_power)
{
    if (actor == NULL) return;
    if (ambient < 0.0f) ambient = 0.0f;
    if (ambient > 1.0f) ambient = 1.0f;
    if (diffuse < 0.0f) diffuse = 0.0f;
    if (diffuse > 1.0f) diffuse = 1.0f;
    if (specular < 0.0f) specular = 0.0f;
    if (specular > 1.0f) specular = 1.0f;
    if (specular_power < 1.0f) specular_power = 1.0f;
    if (specular_power > 256.0f) specular_power = 256.0f;
    if (actor->ambient == ambient && actor->diffuse == diffuse && actor->specular == specular &&
        actor->specular_power == specular_power)
        return;
    actor->ambient = ambient;
    actor->diffuse = diffuse;
    actor->specular = specular;
    actor->specular_power = specular_power;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_get_material(const FVizActor* actor, float* ambient, float* diffuse, float* specular,
                             float* specular_power)
{
    if (actor == NULL) return;
    if (ambient != NULL) *ambient = actor->ambient;
    if (diffuse != NULL) *diffuse = actor->diffuse;
    if (specular != NULL) *specular = actor->specular;
    if (specular_power != NULL) *specular_power = actor->specular_power;
}

void fviz_actor_set_shading_mode(FVizActor* actor, FVizShadingMode mode)
{
    FVizShadingMode normalized;
    if (actor == NULL) return;
    normalized = mode == FVIZ_SHADING_FLAT ? FVIZ_SHADING_FLAT : FVIZ_SHADING_SMOOTH;
    if (actor->shading_mode == normalized) return;
    actor->shading_mode = normalized;
    fviz_object_modified((FVizObject*)actor);
}

FVizShadingMode fviz_actor_shading_mode(const FVizActor* actor)
{
    return actor != NULL ? actor->shading_mode : FVIZ_SHADING_SMOOTH;
}

void fviz_actor_set_cull_mode(FVizActor* actor, FVizCullMode mode)
{
    if (actor == NULL) return;
    if (mode != FVIZ_CULL_FRONT && mode != FVIZ_CULL_BACK) mode = FVIZ_CULL_NONE;
    if (actor->cull_mode == mode) return;
    actor->cull_mode = mode;
    fviz_object_modified((FVizObject*)actor);
}

FVizCullMode fviz_actor_cull_mode(const FVizActor* actor)
{
    return actor != NULL ? actor->cull_mode : FVIZ_CULL_BACK;
}

static void fviz_actor_transform_modified(FVizActor* actor)
{
    if (actor == NULL) return;
    ++actor->transform_revision;
    if (actor->transform_revision == 0u) actor->transform_revision = 1u;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_set_position(FVizActor* actor, FVizVec3 position)
{
    if (actor != NULL && fviz_actor_vec3_equal(actor->position, position) == FVIZ_FALSE)
    {
        actor->position = position;
        fviz_actor_transform_modified(actor);
    }
}

FVizVec3 fviz_actor_position(const FVizActor* actor)
{
    return actor != NULL ? actor->position : fviz_vec3(0.0f, 0.0f, 0.0f);
}

void fviz_actor_set_orientation(FVizActor* actor, FVizQuat orientation)
{
    if (actor != NULL)
    {
        const FVizQuat normalized = fviz_quat_normalize(orientation);
        if (fviz_actor_quat_equal(actor->orientation, normalized) != FVIZ_FALSE) return;
        actor->orientation = normalized;
        fviz_actor_transform_modified(actor);
    }
}

FVizQuat fviz_actor_orientation(const FVizActor* actor)
{
    return actor != NULL ? actor->orientation : fviz_quat_identity();
}

void fviz_actor_set_scale(FVizActor* actor, FVizVec3 scale)
{
    if (actor != NULL)
    {
        FVizVec3 normalized;
        normalized.x = scale.x != 0.0f ? scale.x : 1.0f;
        normalized.y = scale.y != 0.0f ? scale.y : 1.0f;
        normalized.z = scale.z != 0.0f ? scale.z : 1.0f;
        if (fviz_actor_vec3_equal(actor->scale, normalized) != FVIZ_FALSE) return;
        actor->scale = normalized;
        fviz_actor_transform_modified(actor);
    }
}

FVizVec3 fviz_actor_scale(const FVizActor* actor)
{
    return actor != NULL ? actor->scale : fviz_vec3(1.0f, 1.0f, 1.0f);
}

FVizMat4 fviz_actor_transform_matrix(const FVizActor* actor)
{
    FVizMat3 rotation;
    FVizMat4 result = fviz_mat4_identity();
    if (actor == NULL) return result;
    rotation = fviz_mat3_from_quaternion(actor->orientation);

    result.m[0] = rotation.m[0] * actor->scale.x;
    result.m[1] = rotation.m[1] * actor->scale.x;
    result.m[2] = rotation.m[2] * actor->scale.x;
    result.m[4] = rotation.m[3] * actor->scale.y;
    result.m[5] = rotation.m[4] * actor->scale.y;
    result.m[6] = rotation.m[5] * actor->scale.y;
    result.m[8] = rotation.m[6] * actor->scale.z;
    result.m[9] = rotation.m[7] * actor->scale.z;
    result.m[10] = rotation.m[8] * actor->scale.z;
    result.m[12] = actor->position.x;
    result.m[13] = actor->position.y;
    result.m[14] = actor->position.z;
    return actor->user_transform != NULL ? fviz_mat4_multiply(fviz_transform_matrix(actor->user_transform), result)
                                         : result;
}

FVizResult fviz_actor_set_user_transform(FVizActor* actor, FVizTransform* transform)
{
    FVizObserverTag new_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (transform == actor->user_transform) return FVIZ_OK;
    if (transform != NULL)
    {
        if (fviz_retain(transform) == NULL) return fviz_last_error_code();
        if (fviz_actor_observe_dependency((FVizObject*)transform, actor, &new_tag) != FVIZ_OK)
        {
            fviz_release(transform);
            return fviz_last_error_code();
        }
    }
    if (actor->user_transform != NULL && actor->user_transform_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)actor->user_transform, actor->user_transform_modified_tag);
    fviz_release(actor->user_transform);
    actor->user_transform = transform;
    actor->user_transform_modified_tag = new_tag;
    fviz_actor_transform_modified(actor);
    return FVIZ_OK;
}

FVizTransform* fviz_actor_user_transform(FVizActor* actor)
{
    return actor != NULL ? actor->user_transform : NULL;
}


void fviz_topology_render_options_initialize(FVizTopologyRenderOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->coincident_mode = FVIZ_COINCIDENT_TOPOLOGY_DEFAULT;
    options->offset_faces = FVIZ_TRUE;
    options->polygon_offset_factor = 1.0f;
    options->polygon_offset_units = 1.0f;
    options->line_offset_factor = 1.0f;
    options->line_offset_units = 1.0f;
    options->point_offset_units = 0.5f;
    options->z_shift = 0.0f;
    options->depth_test = FVIZ_TRUE;
    options->depth_write = FVIZ_TRUE;
    options->depth_function = FVIZ_DEPTH_FUNCTION_LEQUAL;
    options->depth_range_minimum = 0.0f;
    options->depth_range_maximum = 1.0f;
    options->render_layer = 0;
    options->render_priority = 0;
    options->pass_order = FVIZ_RENDER_PASS_OPAQUE;
    options->overlay_mode = FVIZ_OVERLAY_TOPOLOGY_SURFACE_EDGES;
    options->topology_data_flags = FVIZ_TOPOLOGY_DATA_CONNECTIVITY | FVIZ_TOPOLOGY_DATA_CELL_CLASSIFICATION;
}

void fviz_actor_set_topology_render_options(FVizActor* actor, const FVizTopologyRenderOptions* options)
{
    if (actor == NULL || options == NULL) return;
    actor->coincident_mode = options->coincident_mode;
    actor->offset_faces = options->offset_faces != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    actor->polygon_offset_factor = options->polygon_offset_factor;
    actor->polygon_offset_units = options->polygon_offset_units;
    actor->line_offset_factor = options->line_offset_factor;
    actor->line_offset_units = options->line_offset_units;
    actor->point_offset_units = options->point_offset_units;
    actor->z_shift = options->z_shift;
    actor->depth_test = options->depth_test != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    actor->depth_write = options->depth_write != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    actor->depth_function = options->depth_function;
    actor->depth_range_minimum = options->depth_range_minimum;
    actor->depth_range_maximum = options->depth_range_maximum;
    actor->render_layer = options->render_layer;
    actor->render_priority = options->render_priority;
    actor->pass_order = options->pass_order;
    actor->overlay_mode = options->overlay_mode;
    actor->topology_data_flags = options->topology_data_flags;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_topology_render_options(const FVizActor* actor, FVizTopologyRenderOptions* out_options)
{
    if (actor == NULL || out_options == NULL) return;
    fviz_topology_render_options_initialize(out_options);
    out_options->coincident_mode = actor->coincident_mode;
    out_options->offset_faces = actor->offset_faces;
    out_options->polygon_offset_factor = actor->polygon_offset_factor;
    out_options->polygon_offset_units = actor->polygon_offset_units;
    out_options->line_offset_factor = actor->line_offset_factor;
    out_options->line_offset_units = actor->line_offset_units;
    out_options->point_offset_units = actor->point_offset_units;
    out_options->z_shift = actor->z_shift;
    out_options->depth_test = actor->depth_test;
    out_options->depth_write = actor->depth_write;
    out_options->depth_function = actor->depth_function;
    out_options->depth_range_minimum = actor->depth_range_minimum;
    out_options->depth_range_maximum = actor->depth_range_maximum;
    out_options->render_layer = actor->render_layer;
    out_options->render_priority = actor->render_priority;
    out_options->pass_order = actor->pass_order;
    out_options->overlay_mode = actor->overlay_mode;
    out_options->topology_data_flags = actor->topology_data_flags;
}

void fviz_actor_set_coincident_topology_mode(FVizActor* actor, FVizCoincidentTopologyMode mode)
{
    if (actor == NULL) return;
    actor->coincident_mode = mode;
    fviz_object_modified((FVizObject*)actor);
}

FVizCoincidentTopologyMode fviz_actor_coincident_topology_mode(const FVizActor* actor)
{
    return actor != NULL ? actor->coincident_mode : FVIZ_COINCIDENT_TOPOLOGY_DEFAULT;
}

void fviz_actor_set_polygon_offset(FVizActor* actor, float factor, float units)
{
    if (actor == NULL) return;
    actor->polygon_offset_factor = factor;
    actor->polygon_offset_units = units;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_set_line_offset(FVizActor* actor, float factor, float units)
{
    if (actor == NULL) return;
    actor->line_offset_factor = factor;
    actor->line_offset_units = units;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_set_point_offset(FVizActor* actor, float units)
{
    if (actor == NULL) return;
    actor->point_offset_units = units;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_set_z_shift(FVizActor* actor, float z_shift)
{
    if (actor == NULL) return;
    actor->z_shift = z_shift;
    fviz_object_modified((FVizObject*)actor);
}

float fviz_actor_z_shift(const FVizActor* actor)
{
    return actor != NULL ? actor->z_shift : 0.0f;
}

void fviz_actor_set_depth_test(FVizActor* actor, FVizBool enabled)
{
    if (actor == NULL) return;
    actor->depth_test = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_object_modified((FVizObject*)actor);
}

FVizBool fviz_actor_depth_test(const FVizActor* actor)
{
    return actor != NULL && actor->depth_test != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_actor_set_depth_write(FVizActor* actor, FVizBool enabled)
{
    if (actor == NULL) return;
    actor->depth_write = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_object_modified((FVizObject*)actor);
}

FVizBool fviz_actor_depth_write(const FVizActor* actor)
{
    return actor != NULL && actor->depth_write != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_actor_set_depth_function(FVizActor* actor, FVizDepthFunction function)
{
    if (actor == NULL) return;
    actor->depth_function = function;
    fviz_object_modified((FVizObject*)actor);
}

FVizDepthFunction fviz_actor_depth_function(const FVizActor* actor)
{
    return actor != NULL ? actor->depth_function : FVIZ_DEPTH_FUNCTION_LEQUAL;
}

void fviz_actor_set_depth_range(FVizActor* actor, float minimum, float maximum)
{
    if (actor == NULL) return;
    actor->depth_range_minimum = minimum;
    actor->depth_range_maximum = maximum;
    fviz_object_modified((FVizObject*)actor);
}

void fviz_actor_get_depth_range(const FVizActor* actor, float* minimum, float* maximum)
{
    if (actor == NULL) return;
    if (minimum != NULL) *minimum = actor->depth_range_minimum;
    if (maximum != NULL) *maximum = actor->depth_range_maximum;
}

void fviz_actor_set_render_layer(FVizActor* actor, int32_t layer)
{
    if (actor == NULL) return;
    actor->render_layer = layer;
    fviz_object_modified((FVizObject*)actor);
}

int32_t fviz_actor_render_layer(const FVizActor* actor)
{
    return actor != NULL ? actor->render_layer : 0;
}

void fviz_actor_set_render_priority(FVizActor* actor, int32_t priority)
{
    if (actor == NULL) return;
    actor->render_priority = priority;
    fviz_object_modified((FVizObject*)actor);
}

int32_t fviz_actor_render_priority(const FVizActor* actor)
{
    return actor != NULL ? actor->render_priority : 0;
}

void fviz_actor_set_pass_order(FVizActor* actor, FVizRenderPassStage pass_order)
{
    if (actor == NULL) return;
    actor->pass_order = pass_order;
    fviz_object_modified((FVizObject*)actor);
}

FVizRenderPassStage fviz_actor_pass_order(const FVizActor* actor)
{
    return actor != NULL ? actor->pass_order : FVIZ_RENDER_PASS_OPAQUE;
}

void fviz_actor_set_overlay_topology_mode(FVizActor* actor, FVizOverlayTopologyMode mode)
{
    if (actor == NULL) return;
    actor->overlay_mode = mode;
    fviz_object_modified((FVizObject*)actor);
}

FVizOverlayTopologyMode fviz_actor_overlay_topology_mode(const FVizActor* actor)
{
    return actor != NULL ? actor->overlay_mode : FVIZ_OVERLAY_TOPOLOGY_SURFACE_EDGES;
}

void fviz_actor_set_topology_data_flags(FVizActor* actor, uint32_t flags)
{
    if (actor == NULL) return;
    actor->topology_data_flags = flags;
    fviz_object_modified((FVizObject*)actor);
}

uint32_t fviz_actor_topology_data_flags(const FVizActor* actor)
{
    return actor != NULL ? actor->topology_data_flags : 0u;
}

void fviz_actor_polygon_offset(const FVizActor* actor, float* factor, float* units)
{
    if (actor == NULL) return;
    if (factor != NULL) *factor = actor->polygon_offset_factor;
    if (units != NULL) *units = actor->polygon_offset_units;
}

void fviz_actor_line_offset(const FVizActor* actor, float* factor, float* units)
{
    if (actor == NULL) return;
    if (factor != NULL) *factor = actor->line_offset_factor;
    if (units != NULL) *units = actor->line_offset_units;
}

float fviz_actor_point_offset(const FVizActor* actor)
{
    return actor != NULL ? actor->point_offset_units : 0.0f;
}

void fviz_actor_set_offset_faces(FVizActor* actor, FVizBool enabled)
{
    if (actor == NULL) return;
    actor->offset_faces = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_object_modified((FVizObject*)actor);
}

FVizBool fviz_actor_offset_faces(const FVizActor* actor)
{
    return actor != NULL && actor->offset_faces != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}
