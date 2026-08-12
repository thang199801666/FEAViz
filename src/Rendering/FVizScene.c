#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizScene.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizScenePrivate.h>

static void fviz_scene_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_scene_class = {
    FVIZ_TYPE_SCENE,
    "FVizScene",
    &g_fviz_object_class,
    fviz_scene_destroy
};

static void fviz_scene_release_actors(FVizScene* scene)
{
    FVizSize i;
    FVizActor** actors = (FVizActor**)fviz_array_data(scene->actors);
    for (i = 0u; i < fviz_array_count(scene->actors); ++i) fviz_release(actors[i]);
    fviz_array_clear(scene->actors);
}

static void fviz_scene_destroy(FVizObject* object)
{
    FVizScene* scene = (FVizScene*)object;
    fviz_scene_release_actors(scene);
    fviz_release(scene->actors);
    scene->actors = NULL;
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
    if (fviz_array_create(sizeof(FVizActor*), &scene->actors) != FVIZ_OK)
    {
        fviz_release(scene);
        return fviz_last_error_code();
    }
    *out_scene = scene;
    return FVIZ_OK;
}

FVizResult fviz_scene_add_actor(FVizScene* scene, FVizActor* actor)
{
    FVizActor* retained;
    if (scene == NULL || actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "scene and actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    retained = (FVizActor*)fviz_retain(actor);
    if (retained == NULL) return fviz_last_error_code();
    if (fviz_array_push(scene->actors, &retained) != FVIZ_OK)
    {
        fviz_release(retained);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

FVizResult fviz_scene_remove_actor(FVizScene* scene, FVizActor* actor)
{
    FVizSize i;
    FVizActor** actors;
    if (scene == NULL || actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "scene and actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    actors = (FVizActor**)fviz_array_data(scene->actors);
    for (i = 0u; i < fviz_array_count(scene->actors); ++i)
    {
        if (actors[i] == actor)
        {
            const FVizSize count = fviz_array_count(scene->actors);
            fviz_release(actors[i]);
            if (i + 1u < count)
            {
                (void)memmove(&actors[i], &actors[i + 1u], (count - i - 1u) * sizeof(FVizActor*));
            }
            (void)fviz_array_resize(scene->actors, count - 1u);
            return FVIZ_OK;
        }
    }
    fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "actor is not part of the scene");
    return FVIZ_ERROR_NOT_FOUND;
}

void fviz_scene_clear(FVizScene* scene) { if (scene != NULL) fviz_scene_release_actors(scene); }
FVizSize fviz_scene_actor_count(const FVizScene* scene) { return scene != NULL ? fviz_array_count(scene->actors) : 0u; }

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
        if (fviz_actor_is_visible(actor) == FVIZ_TRUE)
        {
            const FVizPolyData* data = fviz_actor_const_poly_data(actor);
            if (data != NULL)
            {
                const FVizBounds actor_bounds = fviz_poly_data_bounds(data);
                fviz_bounds_include_bounds(&bounds, &actor_bounds);
            }
        }
    }
    return bounds;
}
