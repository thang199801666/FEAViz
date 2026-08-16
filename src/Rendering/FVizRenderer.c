#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Rendering/FVizRenderer.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRendererPrivate.h>
#include <FViz/Rendering/FVizScalarLegendPrivate.h>

static void fviz_renderer_destroy(FVizObject* object);

static const char* fviz_renderer_pass_stage_name(FVizRenderPassStage stage)
{
    switch (stage)
    {
        case FVIZ_RENDER_PASS_CLEAR: return "clear";
        case FVIZ_RENDER_PASS_OPAQUE: return "opaque";
        case FVIZ_RENDER_PASS_TRANSLUCENT: return "translucent";
        case FVIZ_RENDER_PASS_EDGE: return "edge";
        case FVIZ_RENDER_PASS_SELECTION: return "selection";
        case FVIZ_RENDER_PASS_OVERLAY: return "overlay";
        default: return "pass";
    }
}

static FVizBool fviz_renderer_dependency_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    FVizRenderer* renderer = (FVizRenderer*)client_data;
    FVIZ_UNUSED(caller);
    FVIZ_UNUSED(event_id);
    FVIZ_UNUSED(call_data);
    if (renderer != NULL)
    {
        renderer->render_graph_dirty = FVIZ_TRUE;
        fviz_object_modified((FVizObject*)renderer);
    }
    return FVIZ_FALSE;
}

static FVizResult fviz_renderer_observe_dependency(
    FVizRenderer* renderer, FVizObject* dependency, FVizObserverTag* tag)
{
    if (renderer == NULL || dependency == NULL || tag == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *tag = FVIZ_OBSERVER_TAG_INVALID;
    return fviz_object_add_observer(
        dependency, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_renderer_dependency_modified, renderer, tag);
}

static FVizResult fviz_renderer_observe_child(
    FVizRenderer* renderer, FVizObject* child, FVizArray* owner_array)
{
    FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizRendererChildObserver* record;
    FVizSize count;
    FVizResult result;
    if (renderer == NULL || child == NULL || renderer->child_dependency_observers == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_object_add_observer(
        child, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_renderer_dependency_modified, renderer, &tag);
    if (result != FVIZ_OK) return result;
    count = fviz_array_count(renderer->child_dependency_observers);
    result = fviz_array_resize(renderer->child_dependency_observers, count + 1u);
    if (result != FVIZ_OK)
    {
        (void)fviz_object_remove_observer(child, tag);
        return result;
    }
    record = (FVizRendererChildObserver*)fviz_array_at(
        renderer->child_dependency_observers, count);
    record->object = child;
    record->owner_array = owner_array;
    record->tag = tag;
    return FVIZ_OK;
}

static void fviz_renderer_unobserve_child(
    FVizRenderer* renderer, FVizObject* child, FVizArray* owner_array)
{
    FVizSize i;
    FVizSize count;
    FVizRendererChildObserver* records;
    if (renderer == NULL || child == NULL || renderer->child_dependency_observers == NULL) return;
    count = fviz_array_count(renderer->child_dependency_observers);
    records = (FVizRendererChildObserver*)fviz_array_data(renderer->child_dependency_observers);
    for (i = 0u; i < count; ++i)
    {
        if (records[i].object == child && records[i].owner_array == owner_array)
        {
            if (records[i].tag != FVIZ_OBSERVER_TAG_INVALID)
                (void)fviz_object_remove_observer(child, records[i].tag);
            if (i + 1u < count)
                (void)memmove(&records[i], &records[i + 1u],
                    (size_t)(count - i - 1u) * sizeof(*records));
            (void)fviz_array_resize(renderer->child_dependency_observers, count - 1u);
            return;
        }
    }
}

static void fviz_renderer_unobserve_all_children(FVizRenderer* renderer)
{
    FVizSize i;
    if (renderer == NULL || renderer->child_dependency_observers == NULL) return;
    for (i = 0u; i < fviz_array_count(renderer->child_dependency_observers); ++i)
    {
        FVizRendererChildObserver* record = (FVizRendererChildObserver*)fviz_array_at(
            renderer->child_dependency_observers, i);
        if (record != NULL && record->object != NULL &&
            record->tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer(record->object, record->tag);
    }
    fviz_array_clear(renderer->child_dependency_observers);
}
static const FVizObjectClass g_fviz_renderer_class = {
    FVIZ_TYPE_RENDERER,
    "FVizRenderer",
    &g_fviz_object_class,
    fviz_renderer_destroy,
    NULL
};

static void fviz_renderer_destroy(FVizObject* object)
{
    FVizRenderer* renderer = (FVizRenderer*)object;
    if (renderer->scene != NULL && renderer->scene_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)renderer->scene, renderer->scene_modified_tag);
    if (renderer->camera != NULL && renderer->camera_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)renderer->camera, renderer->camera_modified_tag);
    fviz_renderer_unobserve_all_children(renderer);
    fviz_release(renderer->scene);
    fviz_release(renderer->camera);
    fviz_release(renderer->scalar_legend);
    fviz_release(renderer->render_graph);
    if (renderer->passes != NULL)
    {
        FVizSize i;
        for (i = 0u; i < fviz_array_count(renderer->passes); ++i)
            fviz_release(*(FVizRenderPass**)fviz_array_at(renderer->passes, i));
    }
    if (renderer->text_actors_2d != NULL)
    {
        FVizSize i;
        for (i = 0u; i < fviz_array_count(renderer->text_actors_2d); ++i)
            fviz_release(*(FVizTextActor2D**)fviz_array_at(renderer->text_actors_2d, i));
    }
    if (renderer->billboard_text_actors_3d != NULL)
    {
        FVizSize i;
        for (i = 0u; i < fviz_array_count(renderer->billboard_text_actors_3d); ++i)
            fviz_release(*(FVizBillboardTextActor3D**)fviz_array_at(renderer->billboard_text_actors_3d, i));
    }
    if (renderer->label_sets_3d != NULL)
    {
        FVizSize i;
        for (i = 0u; i < fviz_array_count(renderer->label_sets_3d); ++i)
            fviz_release(*(FVizLabelSet3D**)fviz_array_at(renderer->label_sets_3d, i));
    }
    if (renderer->lights != NULL)
    {
        FVizSize i;
        for (i = 0u; i < fviz_array_count(renderer->lights); ++i)
            fviz_release(*(FVizLight**)fviz_array_at(renderer->lights, i));
    }
    fviz_release(renderer->passes);
    fviz_release(renderer->lights);
    fviz_release(renderer->text_actors_2d);
    fviz_release(renderer->billboard_text_actors_3d);
    fviz_release(renderer->label_sets_3d);
    fviz_release(renderer->child_dependency_observers);
    renderer->scene = NULL;
    renderer->camera = NULL;
    renderer->scalar_legend = NULL;
    renderer->render_graph = NULL;
    renderer->passes = NULL;
    renderer->lights = NULL;
    renderer->text_actors_2d = NULL;
    renderer->billboard_text_actors_3d = NULL;
    renderer->label_sets_3d = NULL;
    renderer->child_dependency_observers = NULL;
}

FVizResult fviz_renderer_create(FVizRenderer** out_renderer)
{
    FVizRenderer* renderer;
    if (out_renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_renderer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_renderer = NULL;
    renderer = (FVizRenderer*)fviz_internal_object_allocate(sizeof(FVizRenderer), &g_fviz_renderer_class, NULL);
    if (renderer == NULL) return fviz_last_error_code();
    if (fviz_scene_create(&renderer->scene) != FVIZ_OK ||
        fviz_camera_create(&renderer->camera) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizRenderPass*), &renderer->passes) != FVIZ_OK ||
        fviz_render_graph_create(&renderer->render_graph) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizLight*), &renderer->lights) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizTextActor2D*), &renderer->text_actors_2d) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizBillboardTextActor3D*), &renderer->billboard_text_actors_3d) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizLabelSet3D*), &renderer->label_sets_3d) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizRendererChildObserver), &renderer->child_dependency_observers) != FVIZ_OK ||
        fviz_renderer_reset_standard_passes(renderer) != FVIZ_OK)
    {
        fviz_release(renderer);
        return fviz_last_error_code();
    }
    renderer->background[0] = 0.10f;
    renderer->background[1] = 0.12f;
    renderer->background[2] = 0.16f;
    renderer->background2[0] = 0.22f;
    renderer->background2[1] = 0.28f;
    renderer->background2[2] = 0.38f;
    renderer->gradient_background = FVIZ_FALSE;
    renderer->transparency_mode = FVIZ_TRANSPARENCY_SORTED;
    fviz_weighted_oit_options_initialize(&renderer->weighted_oit_options);
    renderer->viewport[0] = 0.0f;
    renderer->viewport[1] = 0.0f;
    renderer->viewport[2] = 1.0f;
    renderer->viewport[3] = 1.0f;
    renderer->interactive = FVIZ_TRUE;
    renderer->frustum_culling = FVIZ_TRUE;
    renderer->small_object_culling = FVIZ_FALSE;
    renderer->small_object_threshold_pixels = 1.0f;
    renderer->render_graph_dirty = FVIZ_TRUE;
    renderer->frustum_cache.valid = FVIZ_FALSE;
    renderer->frustum_camera_mtime = 0u;
    renderer->frustum_aspect_ratio = 0.0f;
    renderer->frustum_cache_valid = FVIZ_FALSE;
    if (fviz_renderer_observe_dependency(
            renderer, (FVizObject*)renderer->scene, &renderer->scene_modified_tag) != FVIZ_OK ||
        fviz_renderer_observe_dependency(
            renderer, (FVizObject*)renderer->camera, &renderer->camera_modified_tag) != FVIZ_OK)
    {
        fviz_release(renderer);
        return fviz_last_error_code();
    }
    {
        FVizLight* headlight = NULL;
        if (fviz_light_create(&headlight) != FVIZ_OK ||
            fviz_renderer_add_light(renderer, headlight) != FVIZ_OK)
        {
            fviz_release(headlight);
            fviz_release(renderer);
            return fviz_last_error_code();
        }
        fviz_release(headlight);
    }
    *out_renderer = renderer;
    return FVIZ_OK;
}

FVizResult fviz_renderer_set_scene(FVizRenderer* renderer, FVizScene* scene)
{
    if (renderer == NULL || scene == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "renderer and scene must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (renderer->scene == scene) return FVIZ_OK;
    {
        FVizObserverTag new_tag = FVIZ_OBSERVER_TAG_INVALID;
        if (fviz_retain(scene) == NULL) return fviz_last_error_code();
        if (fviz_renderer_observe_dependency(
                renderer, (FVizObject*)scene, &new_tag) != FVIZ_OK)
        {
            fviz_release(scene);
            return fviz_last_error_code();
        }
        if (renderer->scene != NULL && renderer->scene_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer(
                (FVizObject*)renderer->scene, renderer->scene_modified_tag);
        fviz_release(renderer->scene);
        renderer->scene = scene;
        renderer->scene_modified_tag = new_tag;
        fviz_object_modified((FVizObject*)renderer);
    }
    return FVIZ_OK;
}
FVizScene* fviz_renderer_scene(FVizRenderer* renderer) { return renderer != NULL ? renderer->scene : NULL; }
FVizCamera* fviz_renderer_camera(FVizRenderer* renderer) { return renderer != NULL ? renderer->camera : NULL; }

void fviz_renderer_set_frustum_culling(FVizRenderer* renderer, FVizBool enabled)
{
    if (renderer != NULL)
    {
        const FVizBool normalized = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (renderer->frustum_culling == normalized) return;
        renderer->frustum_culling = normalized;
        fviz_object_modified((FVizObject*)renderer);
    }
}

FVizBool fviz_renderer_frustum_culling(const FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->frustum_culling : FVIZ_FALSE;
}

FVizResult fviz_renderer_get_frustum(
    const FVizRenderer* renderer, float aspect_ratio, FVizFrustum* out_frustum)
{
    FVizRenderer* mutable_renderer;
    FVizMTime camera_mtime;
    if (renderer == NULL || out_frustum == NULL || aspect_ratio <= 0.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    camera_mtime = fviz_object_mtime((const FVizObject*)renderer->camera);
    mutable_renderer = (FVizRenderer*)renderer;
    if (renderer->frustum_cache_valid == FVIZ_FALSE ||
        renderer->frustum_camera_mtime != camera_mtime ||
        renderer->frustum_aspect_ratio != aspect_ratio)
    {
        const FVizMat4 view_projection = fviz_mat4_multiply(
            fviz_camera_projection_matrix(renderer->camera, aspect_ratio),
            fviz_camera_view_matrix(renderer->camera));
        mutable_renderer->frustum_cache = fviz_frustum_from_view_projection(view_projection);
        mutable_renderer->frustum_camera_mtime = camera_mtime;
        mutable_renderer->frustum_aspect_ratio = aspect_ratio;
        mutable_renderer->frustum_cache_valid = FVIZ_TRUE;
    }
    *out_frustum = renderer->frustum_cache;
    return renderer->frustum_cache.valid != FVIZ_FALSE ? FVIZ_OK : FVIZ_ERROR_INVALID_STATE;
}

FVizBool fviz_renderer_actor_in_frustum(
    const FVizRenderer* renderer, const FVizActor* actor, float aspect_ratio)
{
    FVizFrustum frustum;
    if (renderer == NULL || actor == NULL || aspect_ratio <= 0.0f) return FVIZ_FALSE;
    if (renderer->frustum_culling == FVIZ_FALSE) return FVIZ_TRUE;
    if (fviz_renderer_get_frustum(renderer, aspect_ratio, &frustum) != FVIZ_OK)
        return FVIZ_TRUE;
    return fviz_frustum_intersects_bounds(&frustum, fviz_actor_bounds(actor));
}

void fviz_renderer_set_small_object_culling(FVizRenderer* renderer, FVizBool enabled)
{
    const FVizBool normalized = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (renderer != NULL && renderer->small_object_culling != normalized)
    {
        renderer->small_object_culling = normalized;
        fviz_object_modified((FVizObject*)renderer);
    }
}

FVizBool fviz_renderer_small_object_culling(const FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->small_object_culling : FVIZ_FALSE;
}

FVizResult fviz_renderer_set_small_object_threshold_pixels(
    FVizRenderer* renderer, float diameter_pixels)
{
    if (renderer == NULL || !isfinite(diameter_pixels) || diameter_pixels < 0.0f ||
        diameter_pixels > 4096.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (renderer->small_object_threshold_pixels != diameter_pixels)
    {
        renderer->small_object_threshold_pixels = diameter_pixels;
        fviz_object_modified((FVizObject*)renderer);
    }
    return FVIZ_OK;
}

float fviz_renderer_small_object_threshold_pixels(const FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->small_object_threshold_pixels : 0.0f;
}

float fviz_renderer_actor_projected_diameter_pixels(
    const FVizRenderer* renderer,
    const FVizActor* actor,
    float aspect_ratio,
    int viewport_height)
{
    FVizBounds bounds;
    FVizVec3 center;
    FVizVec3 half;
    float radius;
    float diameter;
    if (renderer == NULL || actor == NULL || aspect_ratio <= 0.0f || viewport_height <= 0)
        return 0.0f;
    bounds = fviz_actor_bounds(actor);
    if (bounds.valid == FVIZ_FALSE) return 0.0f;
    center = fviz_bounds_center(&bounds);
    half = fviz_vec3_scale(fviz_bounds_size(&bounds), 0.5f);
    radius = fviz_vec3_length(half);
    if (!isfinite(radius) || radius < 0.0f) return 0.0f;
    diameter = radius * 2.0f;
    if (diameter <= 1.0e-12f) return 0.0f;
    if (fviz_camera_projection_mode(renderer->camera) == FVIZ_CAMERA_PARALLEL)
    {
        const float half_height = fviz_camera_parallel_scale(renderer->camera);
        if (half_height <= 1.0e-12f) return (float)viewport_height;
        return diameter * (float)viewport_height / (2.0f * half_height);
    }
    else
    {
        const FVizVec3 position = fviz_camera_position(renderer->camera);
        const FVizVec3 target = fviz_camera_target(renderer->camera);
        const FVizVec3 forward = fviz_vec3_normalize(fviz_vec3_sub(target, position));
        const float depth = fviz_vec3_dot(fviz_vec3_sub(center, position), forward);
        const float half_fov = fviz_camera_fov_degrees(renderer->camera) *
            (3.14159265358979323846f / 360.0f);
        float tangent;
        if (depth <= radius + 1.0e-6f) return (float)viewport_height;
        tangent = tanf(half_fov);
        if (!isfinite(tangent) || tangent <= 1.0e-12f) return (float)viewport_height;
        return diameter * (float)viewport_height / (2.0f * depth * tangent);
    }
}

FVizBool fviz_renderer_actor_is_renderable(
    const FVizRenderer* renderer,
    const FVizActor* actor,
    float aspect_ratio,
    int viewport_height)
{
    if (renderer == NULL || actor == NULL || aspect_ratio <= 0.0f || viewport_height <= 0)
        return FVIZ_FALSE;
    if (fviz_renderer_actor_in_frustum(renderer, actor, aspect_ratio) == FVIZ_FALSE)
        return FVIZ_FALSE;
    if (renderer->small_object_culling != FVIZ_FALSE &&
        renderer->small_object_threshold_pixels > 0.0f &&
        fviz_renderer_actor_projected_diameter_pixels(
            renderer, actor, aspect_ratio, viewport_height) < renderer->small_object_threshold_pixels)
        return FVIZ_FALSE;
    return FVIZ_TRUE;
}

void fviz_weighted_oit_options_initialize(FVizWeightedOITOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->weight_scale = 8.0f;
    options->depth_weight = 3.0f;
    options->minimum_weight = 0.01f;
    options->alpha_cutoff = 1.0f / 255.0f;
}

void fviz_renderer_set_transparency_mode(FVizRenderer* renderer, FVizTransparencyMode mode)
{
    if (renderer == NULL) return;
    if (mode < FVIZ_TRANSPARENCY_SORTED || mode > FVIZ_TRANSPARENCY_DEPTH_PEELING) return;
    if (renderer->transparency_mode != mode)
    {
        renderer->transparency_mode = mode;
        fviz_object_modified((FVizObject*)renderer);
    }
}

FVizTransparencyMode fviz_renderer_transparency_mode(const FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->transparency_mode : FVIZ_TRANSPARENCY_SORTED;
}

FVizResult fviz_renderer_set_weighted_oit_options(
    FVizRenderer* renderer, const FVizWeightedOITOptions* options)
{
    if (renderer == NULL || options == NULL ||
        (options->struct_size != 0u && options->struct_size < sizeof(FVizWeightedOITOptions)) ||
        options->weight_scale <= 0.0f || options->weight_scale > 64.0f ||
        options->depth_weight < 0.0f || options->depth_weight > 16.0f ||
        options->minimum_weight <= 0.0f || options->minimum_weight > 1.0f ||
        options->alpha_cutoff < 0.0f || options->alpha_cutoff >= 1.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    renderer->weighted_oit_options = *options;
    renderer->weighted_oit_options.struct_size = (uint32_t)sizeof(renderer->weighted_oit_options);
    fviz_object_modified((FVizObject*)renderer);
    return FVIZ_OK;
}

void fviz_renderer_get_weighted_oit_options(
    const FVizRenderer* renderer, FVizWeightedOITOptions* out_options)
{
    if (out_options == NULL) return;
    fviz_weighted_oit_options_initialize(out_options);
    if (renderer != NULL) *out_options = renderer->weighted_oit_options;
}

FVizResult fviz_renderer_add_light(FVizRenderer* renderer, FVizLight* light)
{
    FVizSize i;
    FVizSize count;
    FVizLight** slot;
    if (renderer == NULL || light == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_array_count(renderer->lights);
    for (i = 0u; i < count; ++i)
        if (*(FVizLight**)fviz_array_at(renderer->lights, i) == light) return FVIZ_OK;
    if (count >= FVIZ_RENDERER_MAX_LIGHTS) return FVIZ_ERROR_OVERFLOW;
    if (fviz_retain(light) == NULL) return fviz_last_error_code();
    if (fviz_renderer_observe_child(renderer, (FVizObject*)light, renderer->lights) != FVIZ_OK)
    {
        fviz_release(light);
        return fviz_last_error_code();
    }
    if (fviz_array_resize(renderer->lights, count + 1u) != FVIZ_OK)
    {
        fviz_renderer_unobserve_child(renderer, (FVizObject*)light, renderer->lights);
        fviz_release(light);
        return fviz_last_error_code();
    }
    slot = (FVizLight**)fviz_array_at(renderer->lights, count);
    *slot = light;
    fviz_object_modified((FVizObject*)renderer);
    return FVIZ_OK;
}

FVizResult fviz_renderer_remove_light(FVizRenderer* renderer, FVizLight* light)
{
    FVizSize i;
    FVizSize count;
    FVizLight** items;
    if (renderer == NULL || light == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_array_count(renderer->lights);
    items = (FVizLight**)fviz_array_data(renderer->lights);
    for (i = 0u; i < count; ++i)
    {
        if (items[i] == light)
        {
            fviz_renderer_unobserve_child(renderer, (FVizObject*)items[i], renderer->lights);
            fviz_release(items[i]);
            if (i + 1u < count)
                (void)memmove(&items[i], &items[i + 1u],
                    (size_t)(count - i - 1u) * sizeof(*items));
            (void)fviz_array_resize(renderer->lights, count - 1u);
            fviz_object_modified((FVizObject*)renderer);
            return FVIZ_OK;
        }
    }
    return FVIZ_ERROR_NOT_FOUND;
}

void fviz_renderer_remove_all_lights(FVizRenderer* renderer)
{
    FVizSize i;
    if (renderer == NULL || renderer->lights == NULL) return;
    for (i = 0u; i < fviz_array_count(renderer->lights); ++i)
    {
        FVizLight* light = *(FVizLight**)fviz_array_at(renderer->lights, i);
        fviz_renderer_unobserve_child(renderer, (FVizObject*)light, renderer->lights);
        fviz_release(light);
    }
    fviz_array_clear(renderer->lights);
    fviz_object_modified((FVizObject*)renderer);
}

FVizSize fviz_renderer_light_count(const FVizRenderer* renderer)
{
    return renderer != NULL ? fviz_array_count(renderer->lights) : 0u;
}

FVizLight* fviz_renderer_light_at(FVizRenderer* renderer, FVizSize index)
{
    FVizLight** slot = renderer != NULL ? (FVizLight**)fviz_array_at(renderer->lights, index) : NULL;
    return slot != NULL ? *slot : NULL;
}

void fviz_renderer_reset_clipping_range(FVizRenderer* renderer)
{
    FVizBounds bounds;
    FVizVec3 target;
    FVizVec3 eye;
    float radius;
    float distance;
    float near_plane;
    float far_plane;
    if (renderer == NULL || renderer->camera == NULL || renderer->scene == NULL) return;
    bounds = fviz_scene_bounds(renderer->scene);
    if (bounds.valid == FVIZ_FALSE) return;
    radius = fviz_bounds_radius(&bounds);
    if (radius < 1.0e-4f) radius = 1.0f;
    target = fviz_bounds_center(&bounds);
    eye = fviz_camera_position(renderer->camera);
    distance = fviz_vec3_length(fviz_vec3_sub(eye, target));
    if (distance < radius * 0.01f) distance = radius * 0.01f;
    near_plane = distance > radius * 2.0f ? distance - radius * 2.0f : radius * 0.001f;
    if (near_plane < radius * 0.001f) near_plane = radius * 0.001f;
    far_plane = distance + radius * 4.0f;
    fviz_camera_set_clipping_range(renderer->camera, near_plane, far_plane);
}

FVizResult fviz_renderer_update(FVizRenderer* renderer)
{
    FVizSize i;
    if (renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "renderer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; renderer->scene != NULL && i < fviz_scene_actor_count(renderer->scene); ++i)
    {
        FVizActor* actor = fviz_scene_actor(renderer->scene, i);
        if (actor != NULL && fviz_mapper_update(fviz_actor_mapper(actor)) != FVIZ_OK)
            return fviz_last_error_code();
    }
    /* Keep the near/far planes in sync with the camera so zooming, panning and
     * rotating never clip the model (VTK-style clipping-range update). */
    fviz_renderer_reset_clipping_range(renderer);
    return FVIZ_OK;
}

void fviz_renderer_set_background(FVizRenderer* renderer, float red, float green, float blue)
{
    if (renderer == NULL) return;
    if (renderer->background[0] == red && renderer->background[1] == green &&
        renderer->background[2] == blue) return;
    renderer->background[0] = red; renderer->background[1] = green; renderer->background[2] = blue;
    fviz_object_modified((FVizObject*)renderer);
}
void fviz_renderer_get_background(const FVizRenderer* renderer, float* red, float* green, float* blue)
{
    if (renderer == NULL) return;
    if (red != NULL) *red = renderer->background[0];
    if (green != NULL) *green = renderer->background[1];
    if (blue != NULL) *blue = renderer->background[2];
}

void fviz_renderer_set_background2(FVizRenderer* renderer, float red, float green, float blue)
{
    if (renderer == NULL) return;
    if (renderer->background2[0] == red && renderer->background2[1] == green &&
        renderer->background2[2] == blue) return;
    renderer->background2[0] = red;
    renderer->background2[1] = green;
    renderer->background2[2] = blue;
    fviz_object_modified((FVizObject*)renderer);
}

void fviz_renderer_get_background2(
    const FVizRenderer* renderer, float* red, float* green, float* blue)
{
    if (renderer == NULL) return;
    if (red != NULL) *red = renderer->background2[0];
    if (green != NULL) *green = renderer->background2[1];
    if (blue != NULL) *blue = renderer->background2[2];
}

void fviz_renderer_set_gradient_background(FVizRenderer* renderer, FVizBool enabled)
{
    if (renderer == NULL) return;
    {
        const FVizBool normalized = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (renderer->gradient_background == normalized) return;
        renderer->gradient_background = normalized;
    }
    fviz_object_modified((FVizObject*)renderer);
}

FVizBool fviz_renderer_gradient_background(const FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->gradient_background : FVIZ_FALSE;
}

FVizResult fviz_renderer_set_viewport(
    FVizRenderer* renderer,
    float minimum_x,
    float minimum_y,
    float maximum_x,
    float maximum_y)
{
    if (renderer == NULL || minimum_x < 0.0f || minimum_y < 0.0f ||
        maximum_x > 1.0f || maximum_y > 1.0f ||
        minimum_x >= maximum_x || minimum_y >= maximum_y)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "renderer viewport must be a non-empty normalized rectangle");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (renderer->viewport[0] == minimum_x && renderer->viewport[1] == minimum_y &&
        renderer->viewport[2] == maximum_x && renderer->viewport[3] == maximum_y)
        return FVIZ_OK;
    renderer->viewport[0] = minimum_x;
    renderer->viewport[1] = minimum_y;
    renderer->viewport[2] = maximum_x;
    renderer->viewport[3] = maximum_y;
    fviz_object_modified((FVizObject*)renderer);
    return FVIZ_OK;
}

void fviz_renderer_get_viewport(
    const FVizRenderer* renderer,
    float* minimum_x,
    float* minimum_y,
    float* maximum_x,
    float* maximum_y)
{
    if (renderer == NULL) return;
    if (minimum_x != NULL) *minimum_x = renderer->viewport[0];
    if (minimum_y != NULL) *minimum_y = renderer->viewport[1];
    if (maximum_x != NULL) *maximum_x = renderer->viewport[2];
    if (maximum_y != NULL) *maximum_y = renderer->viewport[3];
}

void fviz_renderer_set_layer(FVizRenderer* renderer, int layer)
{
    if (layer < 0) layer = 0;
    if (renderer != NULL && renderer->layer != layer)
    {
        renderer->layer = layer;
        fviz_object_modified((FVizObject*)renderer);
    }
}

int fviz_renderer_layer(const FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->layer : 0;
}

void fviz_renderer_set_interactive(FVizRenderer* renderer, FVizBool interactive)
{
    if (renderer != NULL)
        renderer->interactive = interactive != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_renderer_interactive(const FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->interactive : FVIZ_FALSE;
}

FVizBool fviz_renderer_contains_normalized_point(
    const FVizRenderer* renderer,
    float x,
    float y)
{
    return renderer != NULL && x >= renderer->viewport[0] && x <= renderer->viewport[2] &&
        y >= renderer->viewport[1] && y <= renderer->viewport[3]
        ? FVIZ_TRUE
        : FVIZ_FALSE;
}
void fviz_renderer_fit_camera(FVizRenderer* renderer, float padding)
{
    if (renderer != NULL)
    {
        (void)fviz_renderer_update(renderer);
        const FVizBounds bounds = fviz_scene_bounds(renderer->scene);
        fviz_camera_fit_bounds(renderer->camera, &bounds, padding);
    }
}

void fviz_renderer_set_scalar_legend(FVizRenderer* renderer, FVizScalarLegend* legend)
{
    if (renderer == NULL || renderer->scalar_legend == legend) return;
    if (legend != NULL)
    {
        if (fviz_retain(legend) == NULL) return;
        if (fviz_renderer_observe_child(renderer, (FVizObject*)legend, NULL) != FVIZ_OK)
        {
            fviz_release(legend);
            return;
        }
    }
    if (renderer->scalar_legend != NULL)
        fviz_renderer_unobserve_child(renderer, (FVizObject*)renderer->scalar_legend, NULL);
    fviz_release(renderer->scalar_legend);
    renderer->scalar_legend = legend;
    fviz_object_modified((FVizObject*)renderer);
}

FVizScalarLegend* fviz_renderer_scalar_legend(FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->scalar_legend : NULL;
}

FVizResult fviz_renderer_add_pass(FVizRenderer* renderer, FVizRenderPass* pass)
{
    FVizSize count;
    FVizSize index;
    FVizRenderPass** items;
    if (renderer == NULL || pass == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "renderer and render pass are required");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_array_count(renderer->passes);
    for (index = 0u; index < count; ++index)
        if (*(FVizRenderPass**)fviz_array_at(renderer->passes, index) == pass) return FVIZ_OK;
    if (fviz_retain(pass) == NULL) return fviz_last_error_code();
    if (fviz_renderer_observe_child(renderer, (FVizObject*)pass, renderer->passes) != FVIZ_OK)
    {
        fviz_release(pass);
        return fviz_last_error_code();
    }
    if (fviz_array_resize(renderer->passes, count + 1u) != FVIZ_OK)
    {
        fviz_renderer_unobserve_child(renderer, (FVizObject*)pass, renderer->passes);
        fviz_release(pass);
        return fviz_last_error_code();
    }
    items = (FVizRenderPass**)fviz_array_data(renderer->passes);
    index = count;
    while (index > 0u &&
           fviz_render_pass_stage(items[index - 1u]) > fviz_render_pass_stage(pass))
    {
        items[index] = items[index - 1u];
        --index;
    }
    items[index] = pass;
    renderer->render_graph_dirty = FVIZ_TRUE;
    fviz_object_modified((FVizObject*)renderer);
    return FVIZ_OK;
}

FVizResult fviz_renderer_remove_pass(FVizRenderer* renderer, FVizRenderPass* pass)
{
    FVizSize count;
    FVizSize index;
    FVizRenderPass** items;
    if (renderer == NULL || pass == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_array_count(renderer->passes);
    items = (FVizRenderPass**)fviz_array_data(renderer->passes);
    for (index = 0u; index < count; ++index)
    {
        if (items[index] == pass)
        {
            fviz_renderer_unobserve_child(renderer, (FVizObject*)items[index], renderer->passes);
            fviz_release(items[index]);
            if (index + 1u < count)
                (void)memmove(
                    &items[index], &items[index + 1u],
                    (size_t)(count - index - 1u) * sizeof(*items));
            (void)fviz_array_resize(renderer->passes, count - 1u);
            renderer->render_graph_dirty = FVIZ_TRUE;
            fviz_object_modified((FVizObject*)renderer);
            return FVIZ_OK;
        }
    }
    return FVIZ_ERROR_NOT_FOUND;
}


static FVizResult fviz_renderer_add_object_to_array(FVizRenderer* renderer, FVizArray* array, FVizObject* object)
{
    FVizSize i;
    FVizSize count;
    FVizObject** slot;
    if (renderer == NULL || array == NULL || object == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_array_count(array);
    for (i = 0u; i < count; ++i)
        if (*(FVizObject**)fviz_array_at(array, i) == object) return FVIZ_OK;
    if (fviz_retain(object) == NULL) return fviz_last_error_code();
    if (fviz_renderer_observe_child(renderer, object, array) != FVIZ_OK)
    {
        fviz_release(object);
        return fviz_last_error_code();
    }
    if (fviz_array_resize(array, count + 1u) != FVIZ_OK)
    {
        fviz_renderer_unobserve_child(renderer, object, array);
        fviz_release(object);
        return fviz_last_error_code();
    }
    slot = (FVizObject**)fviz_array_at(array, count);
    *slot = object;
    fviz_object_modified((FVizObject*)renderer);
    return FVIZ_OK;
}

static FVizResult fviz_renderer_remove_object_from_array(FVizRenderer* renderer, FVizArray* array, FVizObject* object)
{
    FVizSize i;
    FVizSize count;
    FVizObject** items;
    if (renderer == NULL || array == NULL || object == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_array_count(array);
    items = (FVizObject**)fviz_array_data(array);
    for (i = 0u; i < count; ++i)
    {
        if (items[i] == object)
        {
            fviz_renderer_unobserve_child(renderer, items[i], array);
            fviz_release(items[i]);
            if (i + 1u < count)
                (void)memmove(&items[i], &items[i + 1u], (size_t)(count - i - 1u) * sizeof(*items));
            (void)fviz_array_resize(array, count - 1u);
            fviz_object_modified((FVizObject*)renderer);
            return FVIZ_OK;
        }
    }
    return FVIZ_ERROR_NOT_FOUND;
}

static void fviz_renderer_clear_object_array(FVizRenderer* renderer, FVizArray* array)
{
    FVizSize i;
    if (renderer == NULL || array == NULL) return;
    for (i = 0u; i < fviz_array_count(array); ++i)
    {
        FVizObject* child = *(FVizObject**)fviz_array_at(array, i);
        fviz_renderer_unobserve_child(renderer, child, array);
        fviz_release(child);
    }
    if (fviz_array_count(array) != 0u)
    {
        fviz_array_clear(array);
        fviz_object_modified((FVizObject*)renderer);
    }
}

FVizResult fviz_renderer_add_text_actor_2d(FVizRenderer* renderer, FVizTextActor2D* actor)
{
    return fviz_renderer_add_object_to_array(renderer, renderer != NULL ? renderer->text_actors_2d : NULL, (FVizObject*)actor);
}
FVizResult fviz_renderer_remove_text_actor_2d(FVizRenderer* renderer, FVizTextActor2D* actor)
{
    return fviz_renderer_remove_object_from_array(renderer, renderer != NULL ? renderer->text_actors_2d : NULL, (FVizObject*)actor);
}
void fviz_renderer_remove_all_text_actors_2d(FVizRenderer* renderer)
{
    fviz_renderer_clear_object_array(renderer, renderer != NULL ? renderer->text_actors_2d : NULL);
}
FVizSize fviz_renderer_text_actor_2d_count(const FVizRenderer* renderer)
{
    return renderer != NULL ? fviz_array_count(renderer->text_actors_2d) : 0u;
}
FVizTextActor2D* fviz_renderer_text_actor_2d_at(FVizRenderer* renderer, FVizSize index)
{
    FVizTextActor2D** slot = renderer != NULL ? (FVizTextActor2D**)fviz_array_at(renderer->text_actors_2d, index) : NULL;
    return slot != NULL ? *slot : NULL;
}
FVizResult fviz_renderer_add_billboard_text_actor_3d(FVizRenderer* renderer, FVizBillboardTextActor3D* actor)
{
    return fviz_renderer_add_object_to_array(renderer, renderer != NULL ? renderer->billboard_text_actors_3d : NULL, (FVizObject*)actor);
}
FVizResult fviz_renderer_remove_billboard_text_actor_3d(FVizRenderer* renderer, FVizBillboardTextActor3D* actor)
{
    return fviz_renderer_remove_object_from_array(renderer, renderer != NULL ? renderer->billboard_text_actors_3d : NULL, (FVizObject*)actor);
}
void fviz_renderer_remove_all_billboard_text_actors_3d(FVizRenderer* renderer)
{
    fviz_renderer_clear_object_array(renderer, renderer != NULL ? renderer->billboard_text_actors_3d : NULL);
}
FVizSize fviz_renderer_billboard_text_actor_3d_count(const FVizRenderer* renderer)
{
    return renderer != NULL ? fviz_array_count(renderer->billboard_text_actors_3d) : 0u;
}
FVizBillboardTextActor3D* fviz_renderer_billboard_text_actor_3d_at(FVizRenderer* renderer, FVizSize index)
{
    FVizBillboardTextActor3D** slot = renderer != NULL ? (FVizBillboardTextActor3D**)fviz_array_at(renderer->billboard_text_actors_3d, index) : NULL;
    return slot != NULL ? *slot : NULL;
}

FVizResult fviz_renderer_add_label_set_3d(FVizRenderer* renderer, FVizLabelSet3D* label_set)
{
    return fviz_renderer_add_object_to_array(renderer, renderer != NULL ? renderer->label_sets_3d : NULL, (FVizObject*)label_set);
}
FVizResult fviz_renderer_remove_label_set_3d(FVizRenderer* renderer, FVizLabelSet3D* label_set)
{
    return fviz_renderer_remove_object_from_array(renderer, renderer != NULL ? renderer->label_sets_3d : NULL, (FVizObject*)label_set);
}
void fviz_renderer_remove_all_label_sets_3d(FVizRenderer* renderer)
{
    fviz_renderer_clear_object_array(renderer, renderer != NULL ? renderer->label_sets_3d : NULL);
}
FVizSize fviz_renderer_label_set_3d_count(const FVizRenderer* renderer)
{
    return renderer != NULL ? fviz_array_count(renderer->label_sets_3d) : 0u;
}
FVizLabelSet3D* fviz_renderer_label_set_3d_at(FVizRenderer* renderer, FVizSize index)
{
    FVizLabelSet3D** slot = renderer != NULL ? (FVizLabelSet3D**)fviz_array_at(renderer->label_sets_3d, index) : NULL;
    return slot != NULL ? *slot : NULL;
}

FVizResult fviz_renderer_reset_standard_passes(FVizRenderer* renderer)
{
    static const FVizRenderPassStage stages[] = {
        FVIZ_RENDER_PASS_CLEAR,
        FVIZ_RENDER_PASS_OPAQUE,
        FVIZ_RENDER_PASS_TRANSLUCENT,
        FVIZ_RENDER_PASS_EDGE,
        FVIZ_RENDER_PASS_SELECTION,
        FVIZ_RENDER_PASS_OVERLAY
    };
    FVizSize i;
    if (renderer == NULL || renderer->passes == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < fviz_array_count(renderer->passes); ++i)
    {
        FVizRenderPass* pass = *(FVizRenderPass**)fviz_array_at(renderer->passes, i);
        fviz_renderer_unobserve_child(renderer, (FVizObject*)pass, renderer->passes);
        fviz_release(pass);
    }
    fviz_array_clear(renderer->passes);
    for (i = 0u; i < sizeof(stages) / sizeof(stages[0]); ++i)
    {
        FVizRenderPass* pass = NULL;
        if (fviz_render_pass_create(stages[i], NULL, NULL, NULL, &pass) != FVIZ_OK ||
            fviz_renderer_add_pass(renderer, pass) != FVIZ_OK)
        {
            fviz_release(pass);
            return fviz_last_error_code();
        }
        fviz_release(pass);
    }
    return FVIZ_OK;
}

FVizSize fviz_renderer_pass_count(const FVizRenderer* renderer)
{
    return renderer != NULL ? fviz_array_count(renderer->passes) : 0u;
}

FVizRenderPass* fviz_renderer_pass_at(FVizRenderer* renderer, FVizSize index)
{
    FVizRenderPass** pass = renderer != NULL
        ? (FVizRenderPass**)fviz_array_at(renderer->passes, index)
        : NULL;
    return pass != NULL ? *pass : NULL;
}

FVizResult fviz_renderer_compile_render_graph(FVizRenderer* renderer)
{
    FVizRenderGraphPassId previous = FVIZ_RENDER_GRAPH_PASS_ID_INVALID;
    FVizSize i;
    FVizResult result;
    if (renderer == NULL || renderer->render_graph == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (renderer->render_graph_dirty == FVIZ_FALSE &&
        fviz_render_graph_is_compiled(renderer->render_graph) != FVIZ_FALSE) return FVIZ_OK;
    fviz_render_graph_clear(renderer->render_graph);
    for (i = 0u; i < fviz_array_count(renderer->passes); ++i)
    {
        FVizRenderPass* pass = *(FVizRenderPass**)fviz_array_at(renderer->passes, i);
        FVizRenderGraphPassId current = FVIZ_RENDER_GRAPH_PASS_ID_INVALID;
        result = fviz_render_graph_add_pass(
            renderer->render_graph,
            fviz_renderer_pass_stage_name(fviz_render_pass_stage(pass)),
            pass,
            &current);
        if (result != FVIZ_OK) return result;
        if (previous != FVIZ_RENDER_GRAPH_PASS_ID_INVALID)
        {
            result = fviz_render_graph_add_dependency(renderer->render_graph, previous, current);
            if (result != FVIZ_OK) return result;
        }
        previous = current;
    }
    result = fviz_render_graph_compile(renderer->render_graph);
    if (result == FVIZ_OK) renderer->render_graph_dirty = FVIZ_FALSE;
    return result;
}

const FVizRenderGraph* fviz_renderer_render_graph(FVizRenderer* renderer)
{
    if (fviz_renderer_compile_render_graph(renderer) != FVIZ_OK) return NULL;
    return renderer->render_graph;
}

static FVizResult fviz_renderer_transform_point(
    FVizMat4 matrix,
    FVizVec3 input,
    FVizBool divide,
    FVizVec3* output)
{
    float x;
    float y;
    float z;
    float w;
    if (output == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    x = matrix.m[0] * input.x + matrix.m[4] * input.y + matrix.m[8] * input.z + matrix.m[12];
    y = matrix.m[1] * input.x + matrix.m[5] * input.y + matrix.m[9] * input.z + matrix.m[13];
    z = matrix.m[2] * input.x + matrix.m[6] * input.y + matrix.m[10] * input.z + matrix.m[14];
    w = matrix.m[3] * input.x + matrix.m[7] * input.y + matrix.m[11] * input.z + matrix.m[15];
    if (divide != FVIZ_FALSE)
    {
        if (w == 0.0f) return FVIZ_ERROR_INVALID_STATE;
        x /= w;
        y /= w;
        z /= w;
    }
    *output = fviz_vec3(x, y, z);
    return FVIZ_OK;
}

FVizResult fviz_renderer_world_to_view(
    const FVizRenderer* renderer,
    FVizVec3 world,
    FVizVec3* out_view)
{
    if (renderer == NULL || renderer->camera == NULL || out_view == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_renderer_transform_point(
        fviz_camera_view_matrix(renderer->camera), world, FVIZ_FALSE, out_view);
}

FVizResult fviz_renderer_view_to_ndc(
    const FVizRenderer* renderer,
    FVizVec3 view,
    float aspect_ratio,
    FVizVec3* out_ndc)
{
    if (renderer == NULL || renderer->camera == NULL || out_ndc == NULL || aspect_ratio <= 0.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_renderer_transform_point(
        fviz_camera_projection_matrix(renderer->camera, aspect_ratio),
        view, FVIZ_TRUE, out_ndc);
}

FVizResult fviz_renderer_ndc_to_display(
    const FVizRenderer* renderer,
    FVizVec3 ndc,
    int window_width,
    int window_height,
    FVizVec3* out_display)
{
    float viewport_width;
    float viewport_height;
    if (renderer == NULL || out_display == NULL || window_width <= 0 || window_height <= 0)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    viewport_width = (renderer->viewport[2] - renderer->viewport[0]) * (float)window_width;
    viewport_height = (renderer->viewport[3] - renderer->viewport[1]) * (float)window_height;
    *out_display = fviz_vec3(
        renderer->viewport[0] * (float)window_width + (ndc.x * 0.5f + 0.5f) * viewport_width,
        (1.0f - renderer->viewport[3]) * (float)window_height +
            (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_height,
        ndc.z * 0.5f + 0.5f);
    return FVIZ_OK;
}

FVizResult fviz_renderer_world_to_display(
    const FVizRenderer* renderer,
    FVizVec3 world,
    int window_width,
    int window_height,
    FVizVec3* out_display)
{
    FVizVec3 view;
    FVizVec3 ndc;
    float viewport_width;
    float viewport_height;
    if (renderer == NULL || out_display == NULL || window_width <= 0 || window_height <= 0)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    viewport_width = (renderer->viewport[2] - renderer->viewport[0]) * (float)window_width;
    viewport_height = (renderer->viewport[3] - renderer->viewport[1]) * (float)window_height;
    if (viewport_width < 1.0f || viewport_height < 1.0f) return FVIZ_ERROR_INVALID_STATE;
    if (fviz_renderer_world_to_view(renderer, world, &view) != FVIZ_OK ||
        fviz_renderer_view_to_ndc(renderer, view, viewport_width / viewport_height, &ndc) != FVIZ_OK)
        return fviz_last_error_code();
    return fviz_renderer_ndc_to_display(renderer, ndc, window_width, window_height, out_display);
}

FVizResult fviz_renderer_display_to_world_ray(
    const FVizRenderer* renderer,
    float display_x,
    float display_y,
    int window_width,
    int window_height,
    FVizRay* out_ray)
{
    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
    if (renderer == NULL || renderer->camera == NULL || out_ray == NULL ||
        window_width <= 0 || window_height <= 0)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    viewport_x = (int)(renderer->viewport[0] * (float)window_width);
    viewport_y = (int)((1.0f - renderer->viewport[3]) * (float)window_height);
    viewport_width = (int)((renderer->viewport[2] - renderer->viewport[0]) * (float)window_width);
    viewport_height = (int)((renderer->viewport[3] - renderer->viewport[1]) * (float)window_height);
    if (viewport_width < 1 || viewport_height < 1 ||
        display_x < (float)viewport_x || display_y < (float)viewport_y ||
        display_x >= (float)(viewport_x + viewport_width) ||
        display_y >= (float)(viewport_y + viewport_height))
        return FVIZ_ERROR_NOT_FOUND;
    *out_ray = fviz_camera_pick_ray(
        renderer->camera,
        viewport_width,
        viewport_height,
        (int)(display_x - (float)viewport_x),
        (int)(display_y - (float)viewport_y));
    return FVIZ_OK;
}
