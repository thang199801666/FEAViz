#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizScene.h>
#include <FViz/Rendering/FVizGlyphMapper.h>

#include <FViz/Core/FVizArrayPrivate.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizScenePrivate.h>

static void fviz_scene_destroy(FVizObject* object);
static FVizMTime fviz_scene_mtime(const FVizObject* object);

static FVizBool fviz_scene_actor_modified(FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    FVizScene* scene = (FVizScene*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (scene != NULL) fviz_object_modified((FVizObject*)scene);
    return FVIZ_FALSE;
}

static const FVizObjectClass g_fviz_scene_class = {FVIZ_TYPE_SCENE, "FVizScene", &g_fviz_object_class,
                                                   fviz_scene_destroy, fviz_scene_mtime};

static FVizMTime fviz_scene_mtime(const FVizObject* object)
{
    /* Every attached actor is observed, so Scene MTime is independent of actor count. */
    return fviz_internal_object_local_mtime(object);
}

static void fviz_scene_release_actors(FVizScene* scene)
{
    FVizSize i;
    FVizActor** actors = (FVizActor**)fviz_array_data(scene->actors);
    FVizObserverTag* tags =
        scene->actor_modified_tags != NULL ? (FVizObserverTag*)fviz_array_data(scene->actor_modified_tags) : NULL;
    for (i = 0u; i < fviz_array_count(scene->actors); ++i)
    {
        if (actors[i] != NULL && tags != NULL && tags[i] != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)actors[i], tags[i]);
        fviz_release(actors[i]);
    }
    fviz_internal_array_clear(scene->actors);
    if (scene->actor_modified_tags != NULL) fviz_internal_array_clear(scene->actor_modified_tags);
}

static void fviz_scene_destroy(FVizObject* object)
{
    FVizScene* scene = (FVizScene*)object;
    fviz_scene_release_actors(scene);
    fviz_release(scene->actors);
    fviz_release(scene->actor_modified_tags);
    scene->actors = NULL;
    scene->actor_modified_tags = NULL;
}

FVizResult fviz_scene_create(FVizScene** out_scene)
{
    FVizScene* scene;
    if (out_scene == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_scene must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_scene = NULL;
    scene = (FVizScene*)fviz_internal_object_allocate(sizeof(FVizScene), &g_fviz_scene_class, NULL);
    if (scene == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizActor*), &scene->actors) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizObserverTag), &scene->actor_modified_tags) != FVIZ_OK)
    {
        fviz_release(scene);
        return fviz_last_error_code();
    }
    *out_scene = scene;
    return FVIZ_OK;
}

FVizResult fviz_scene_reserve(FVizScene* scene, FVizSize actor_capacity)
{
    if (scene == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "scene must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_array_reserve(scene->actors, actor_capacity) != FVIZ_OK ||
        fviz_array_reserve(scene->actor_modified_tags, actor_capacity) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

FVizResult fviz_scene_add_actors(FVizScene* scene, FVizActor* const* actors, FVizSize actor_count)
{
    FVizSize old_count;
    FVizSize required;
    FVizSize i;
    if (scene == NULL || (actors == NULL && actor_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "scene actor batch is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (actor_count == 0u) return FVIZ_OK;
    for (i = 0u; i < actor_count; ++i)
    {
        if (actors[i] == NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "scene actor batch contains NULL");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    old_count = fviz_array_count(scene->actors);
    if (actor_count > (FVizSize)-1 - old_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "scene actor count overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    required = old_count + actor_count;
    if (fviz_scene_reserve(scene, required) != FVIZ_OK) return fviz_last_error_code();

    for (i = 0u; i < actor_count; ++i)
    {
        FVizActor* retained = (FVizActor*)fviz_retain(actors[i]);
        FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
        if (retained == NULL ||
            fviz_object_add_observer((FVizObject*)retained, FVIZ_EVENT_MODIFIED, 0.0f, fviz_scene_actor_modified, scene,
                                     &tag) != FVIZ_OK ||
            fviz_internal_array_append(scene->actors, &retained, 1u) != FVIZ_OK ||
            fviz_internal_array_append(scene->actor_modified_tags, &tag, 1u) != FVIZ_OK)
        {
            FVizSize rollback_count;
            if (tag != FVIZ_OBSERVER_TAG_INVALID && retained != NULL)
                (void)fviz_object_remove_observer((FVizObject*)retained, tag);
            if (fviz_array_count(scene->actors) > fviz_array_count(scene->actor_modified_tags))
                (void)fviz_internal_array_resize_untracked(scene->actors, fviz_array_count(scene->actors) - 1u);
            fviz_release(retained);
            rollback_count = fviz_array_count(scene->actors);
            while (rollback_count > old_count)
            {
                FVizActor** actor_slots = (FVizActor**)fviz_array_data(scene->actors);
                FVizObserverTag* tags = (FVizObserverTag*)fviz_array_data(scene->actor_modified_tags);
                const FVizSize index = rollback_count - 1u;
                if (tags[index] != FVIZ_OBSERVER_TAG_INVALID)
                    (void)fviz_object_remove_observer((FVizObject*)actor_slots[index], tags[index]);
                fviz_release(actor_slots[index]);
                --rollback_count;
            }
            (void)fviz_internal_array_resize_untracked(scene->actors, old_count);
            (void)fviz_internal_array_resize_untracked(scene->actor_modified_tags, old_count);
            return fviz_last_error_code();
        }
    }
    fviz_object_modified((FVizObject*)scene);
    return FVIZ_OK;
}

FVizResult fviz_scene_add_actor(FVizScene* scene, FVizActor* actor)
{
    FVizActor* retained;
    FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
    if (scene == NULL || actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "scene and actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    retained = (FVizActor*)fviz_retain(actor);
    if (retained == NULL) return fviz_last_error_code();
    if (fviz_object_add_observer((FVizObject*)retained, FVIZ_EVENT_MODIFIED, 0.0f, fviz_scene_actor_modified, scene,
                                 &tag) != FVIZ_OK)
    {
        fviz_release(retained);
        return fviz_last_error_code();
    }
    if (fviz_internal_array_append(scene->actors, &retained, 1u) != FVIZ_OK ||
        fviz_internal_array_append(scene->actor_modified_tags, &tag, 1u) != FVIZ_OK)
    {
        (void)fviz_object_remove_observer((FVizObject*)retained, tag);
        if (fviz_array_count(scene->actors) > fviz_array_count(scene->actor_modified_tags))
            (void)fviz_internal_array_resize_untracked(scene->actors, fviz_array_count(scene->actors) - 1u);
        fviz_release(retained);
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)scene);
    return FVIZ_OK;
}

FVizResult fviz_scene_remove_actor(FVizScene* scene, FVizActor* actor)
{
    FVizSize i;
    FVizActor** actors;
    FVizObserverTag* tags;
    if (scene == NULL || actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "scene and actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    actors = (FVizActor**)fviz_array_data(scene->actors);
    tags = (FVizObserverTag*)fviz_array_data(scene->actor_modified_tags);
    for (i = 0u; i < fviz_array_count(scene->actors); ++i)
    {
        if (actors[i] == actor)
        {
            const FVizSize count = fviz_array_count(scene->actors);
            if (tags[i] != FVIZ_OBSERVER_TAG_INVALID)
                (void)fviz_object_remove_observer((FVizObject*)actors[i], tags[i]);
            fviz_release(actors[i]);
            if (i + 1u < count)
            {
                (void)memmove(&actors[i], &actors[i + 1u], (count - i - 1u) * sizeof(FVizActor*));
                (void)memmove(&tags[i], &tags[i + 1u], (count - i - 1u) * sizeof(FVizObserverTag));
            }
            (void)fviz_internal_array_resize_untracked(scene->actors, count - 1u);
            (void)fviz_internal_array_resize_untracked(scene->actor_modified_tags, count - 1u);
            fviz_object_modified((FVizObject*)scene);
            return FVIZ_OK;
        }
    }
    fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "actor is not part of the scene");
    return FVIZ_ERROR_NOT_FOUND;
}

void fviz_scene_clear(FVizScene* scene)
{
    if (scene == NULL) return;
    if (fviz_array_count(scene->actors) == 0u) return;
    fviz_scene_release_actors(scene);
    fviz_object_modified((FVizObject*)scene);
}

FVizSize fviz_scene_actor_count(const FVizScene* scene)
{
    return scene != NULL ? fviz_array_count(scene->actors) : 0u;
}

FVizActor* fviz_scene_actor(FVizScene* scene, FVizSize index)
{
    FVizActor* const* slot = scene != NULL ? (FVizActor* const*)fviz_array_const_at(scene->actors, index) : NULL;
    return slot != NULL ? *slot : NULL;
}

const FVizActor* fviz_scene_const_actor(const FVizScene* scene, FVizSize index)
{
    FVizActor* const* slot = scene != NULL ? (FVizActor* const*)fviz_array_const_at(scene->actors, index) : NULL;
    return slot != NULL ? *slot : NULL;
}

FVizBounds fviz_scene_bounds(const FVizScene* scene)
{
    FVizBounds bounds = fviz_bounds_empty();
    FVizSize i;
    if (scene == NULL) return bounds;
    for (i = 0u; i < fviz_scene_actor_count(scene); ++i)
    {
        const FVizActor* actor = fviz_scene_const_actor(scene, i);
        FVizBounds actor_bounds;
        if (fviz_actor_is_visible(actor) == FVIZ_FALSE) continue;
        actor_bounds = fviz_actor_bounds(actor);
        fviz_bounds_include_bounds(&bounds, &actor_bounds);
    }
    return bounds;
}
