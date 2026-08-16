#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Interaction/FVizWidgetRepresentation.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizWidgetRepresentationPrivate.h>

typedef enum FVizWidgetRepresentationKind
{
    FVIZ_WIDGET_REPRESENTATION_ACTOR = 0,
    FVIZ_WIDGET_REPRESENTATION_TEXT_2D = 1,
    FVIZ_WIDGET_REPRESENTATION_BILLBOARD_3D = 2,
    FVIZ_WIDGET_REPRESENTATION_LABEL_SET_3D = 3
} FVizWidgetRepresentationKind;

static FVizMTime fviz_widget_representation_entries_mtime(
    const FVizWidgetRepresentationEntry* entries,
    FVizSize count,
    FVizMTime mtime)
{
    FVizSize i;
    for (i = 0u; i < count; ++i)
    {
        const FVizMTime child = fviz_object_mtime(entries[i].object);
        if (child > mtime) mtime = child;
    }
    return mtime;
}

static FVizMTime fviz_widget_representation_mtime(const FVizObject* object)
{
    const FVizWidgetRepresentation* representation = (const FVizWidgetRepresentation*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    mtime = fviz_widget_representation_entries_mtime(
        representation->actors, representation->actor_count, mtime);
    mtime = fviz_widget_representation_entries_mtime(
        representation->text_actors_2d, representation->text_actor_2d_count, mtime);
    mtime = fviz_widget_representation_entries_mtime(
        representation->billboard_text_actors_3d,
        representation->billboard_text_actor_3d_count, mtime);
    return fviz_widget_representation_entries_mtime(
        representation->label_sets_3d, representation->label_set_3d_count, mtime);
}

static FVizResult fviz_widget_representation_entries_reserve(
    FVizWidgetRepresentationEntry** entries,
    FVizSize* capacity,
    FVizSize required)
{
    FVizSize new_capacity;
    FVizSize bytes;
    FVizWidgetRepresentationEntry* replacement;
    if (entries == NULL || capacity == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (required <= *capacity) return FVIZ_OK;
    new_capacity = *capacity == 0u ? 4u : *capacity;
    while (new_capacity < required)
    {
        if (new_capacity > SIZE_MAX / 2u) return FVIZ_ERROR_OVERFLOW;
        new_capacity *= 2u;
    }
    if (fviz_size_multiply(new_capacity, sizeof(**entries), &bytes) != FVIZ_OK)
        return fviz_last_error_code();
    replacement = (FVizWidgetRepresentationEntry*)fviz_realloc(*entries, bytes);
    if (replacement == NULL) return fviz_last_error_code();
    *entries = replacement;
    *capacity = new_capacity;
    return FVIZ_OK;
}

static FVizBool fviz_widget_representation_object_visible(
    FVizWidgetRepresentationKind kind,
    FVizObject* object)
{
    switch (kind)
    {
    case FVIZ_WIDGET_REPRESENTATION_ACTOR:
        return fviz_actor_is_visible((FVizActor*)object);
    case FVIZ_WIDGET_REPRESENTATION_TEXT_2D:
        return fviz_text_actor_2d_is_visible((FVizTextActor2D*)object);
    case FVIZ_WIDGET_REPRESENTATION_BILLBOARD_3D:
        return fviz_billboard_text_actor_3d_is_visible((FVizBillboardTextActor3D*)object);
    case FVIZ_WIDGET_REPRESENTATION_LABEL_SET_3D:
        return fviz_label_set_3d_visible((FVizLabelSet3D*)object);
    default:
        return FVIZ_FALSE;
    }
}

static void fviz_widget_representation_apply_visibility(
    FVizWidgetRepresentation* representation,
    FVizWidgetRepresentationKind kind,
    FVizWidgetRepresentationEntry* entry)
{
    const FVizBool visible = representation->visible != FVIZ_FALSE &&
        entry->local_visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    switch (kind)
    {
    case FVIZ_WIDGET_REPRESENTATION_ACTOR:
        fviz_actor_set_visible((FVizActor*)entry->object, visible);
        break;
    case FVIZ_WIDGET_REPRESENTATION_TEXT_2D:
        fviz_text_actor_2d_set_visible((FVizTextActor2D*)entry->object, visible);
        break;
    case FVIZ_WIDGET_REPRESENTATION_BILLBOARD_3D:
        fviz_billboard_text_actor_3d_set_visible((FVizBillboardTextActor3D*)entry->object, visible);
        break;
    case FVIZ_WIDGET_REPRESENTATION_LABEL_SET_3D:
        fviz_label_set_3d_set_visible((FVizLabelSet3D*)entry->object, visible);
        break;
    default:
        break;
    }
}

static FVizResult fviz_widget_representation_add_to_renderer(
    FVizWidgetRepresentation* representation,
    FVizWidgetRepresentationKind kind,
    FVizObject* object)
{
    switch (kind)
    {
    case FVIZ_WIDGET_REPRESENTATION_ACTOR:
        return fviz_scene_add_actor(fviz_renderer_scene(representation->renderer), (FVizActor*)object);
    case FVIZ_WIDGET_REPRESENTATION_TEXT_2D:
        return fviz_renderer_add_text_actor_2d(representation->renderer, (FVizTextActor2D*)object);
    case FVIZ_WIDGET_REPRESENTATION_BILLBOARD_3D:
        return fviz_renderer_add_billboard_text_actor_3d(
            representation->renderer, (FVizBillboardTextActor3D*)object);
    case FVIZ_WIDGET_REPRESENTATION_LABEL_SET_3D:
        return fviz_renderer_add_label_set_3d(representation->renderer, (FVizLabelSet3D*)object);
    default:
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
}

static void fviz_widget_representation_remove_from_renderer(
    FVizWidgetRepresentation* representation,
    FVizWidgetRepresentationKind kind,
    FVizObject* object)
{
    switch (kind)
    {
    case FVIZ_WIDGET_REPRESENTATION_ACTOR:
        (void)fviz_scene_remove_actor(fviz_renderer_scene(representation->renderer), (FVizActor*)object);
        break;
    case FVIZ_WIDGET_REPRESENTATION_TEXT_2D:
        (void)fviz_renderer_remove_text_actor_2d(representation->renderer, (FVizTextActor2D*)object);
        break;
    case FVIZ_WIDGET_REPRESENTATION_BILLBOARD_3D:
        (void)fviz_renderer_remove_billboard_text_actor_3d(
            representation->renderer, (FVizBillboardTextActor3D*)object);
        break;
    case FVIZ_WIDGET_REPRESENTATION_LABEL_SET_3D:
        (void)fviz_renderer_remove_label_set_3d(representation->renderer, (FVizLabelSet3D*)object);
        break;
    default:
        break;
    }
}

static FVizResult fviz_widget_representation_add_entry(
    FVizWidgetRepresentation* representation,
    FVizWidgetRepresentationKind kind,
    FVizWidgetRepresentationEntry** entries,
    FVizSize* count,
    FVizSize* capacity,
    FVizObject* object)
{
    FVizSize i;
    FVizWidgetRepresentationEntry* entry;
    if (representation == NULL || entries == NULL || count == NULL || capacity == NULL || object == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < *count; ++i)
        if ((*entries)[i].object == object) return FVIZ_OK;
    if (fviz_widget_representation_entries_reserve(entries, capacity, *count + 1u) != FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_retain(object) == NULL) return fviz_last_error_code();
    /* Widget representation actors perform their own screen-space hit testing.
       Keep them out of scene-selection passes so helpers never mask FEA geometry. */
    if (kind == FVIZ_WIDGET_REPRESENTATION_ACTOR)
        fviz_actor_set_pickable((FVizActor*)object, FVIZ_FALSE);
    if (fviz_widget_representation_add_to_renderer(representation, kind, object) != FVIZ_OK)
    {
        fviz_release(object);
        return fviz_last_error_code();
    }
    entry = &(*entries)[(*count)++];
    entry->object = object;
    entry->local_visible = fviz_widget_representation_object_visible(kind, object);
    fviz_widget_representation_apply_visibility(representation, kind, entry);
    fviz_object_modified((FVizObject*)representation);
    return FVIZ_OK;
}

static FVizResult fviz_widget_representation_remove_entry(
    FVizWidgetRepresentation* representation,
    FVizWidgetRepresentationKind kind,
    FVizWidgetRepresentationEntry* entries,
    FVizSize* count,
    FVizObject* object)
{
    FVizSize i;
    if (representation == NULL || count == NULL || object == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < *count; ++i)
    {
        if (entries[i].object == object)
        {
            fviz_widget_representation_remove_from_renderer(representation, kind, object);
            fviz_release(object);
            if (i + 1u < *count)
                (void)memmove(&entries[i], &entries[i + 1u],
                    (size_t)(*count - i - 1u) * sizeof(entries[0]));
            --(*count);
            fviz_object_modified((FVizObject*)representation);
            return FVIZ_OK;
        }
    }
    return FVIZ_ERROR_NOT_FOUND;
}

static FVizResult fviz_widget_representation_set_entry_visible(
    FVizWidgetRepresentation* representation,
    FVizWidgetRepresentationKind kind,
    FVizWidgetRepresentationEntry* entries,
    FVizSize count,
    FVizObject* object,
    FVizBool visible)
{
    FVizSize i;
    if (representation == NULL || object == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    visible = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    for (i = 0u; i < count; ++i)
    {
        if (entries[i].object == object)
        {
            if (entries[i].local_visible != visible)
            {
                entries[i].local_visible = visible;
                fviz_widget_representation_apply_visibility(representation, kind, &entries[i]);
                fviz_object_modified((FVizObject*)representation);
            }
            return FVIZ_OK;
        }
    }
    return FVIZ_ERROR_NOT_FOUND;
}

static void fviz_widget_representation_clear_entries(
    FVizWidgetRepresentation* representation,
    FVizWidgetRepresentationKind kind,
    FVizWidgetRepresentationEntry* entries,
    FVizSize* count)
{
    FVizSize i;
    if (representation == NULL || count == NULL) return;
    for (i = 0u; i < *count; ++i)
    {
        fviz_widget_representation_remove_from_renderer(representation, kind, entries[i].object);
        fviz_release(entries[i].object);
    }
    if (*count != 0u)
    {
        *count = 0u;
        fviz_object_modified((FVizObject*)representation);
    }
}

static void fviz_widget_representation_destroy(FVizObject* object)
{
    FVizWidgetRepresentation* representation = (FVizWidgetRepresentation*)object;
    fviz_widget_representation_remove_all_actors(representation);
    fviz_widget_representation_remove_all_annotations(representation);
    fviz_free(representation->actors);
    fviz_free(representation->text_actors_2d);
    fviz_free(representation->billboard_text_actors_3d);
    fviz_free(representation->label_sets_3d);
    representation->actors = NULL;
    representation->text_actors_2d = NULL;
    representation->billboard_text_actors_3d = NULL;
    representation->label_sets_3d = NULL;
    fviz_release(representation->renderer);
    representation->renderer = NULL;
}

static const FVizObjectClass g_fviz_widget_representation_class = {
    FVIZ_TYPE_WIDGET_REPRESENTATION,
    "FVizWidgetRepresentation",
    &g_fviz_object_class,
    fviz_widget_representation_destroy,
    fviz_widget_representation_mtime
};

FVizResult fviz_widget_representation_create(
    FVizRenderer* renderer,
    FVizWidgetRepresentation** out_representation)
{
    FVizWidgetRepresentation* representation;
    if (renderer == NULL || out_representation == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_representation = NULL;
    representation = (FVizWidgetRepresentation*)fviz_internal_object_allocate(
        sizeof(*representation), &g_fviz_widget_representation_class, NULL);
    if (representation == NULL) return fviz_last_error_code();
    representation->renderer = (FVizRenderer*)fviz_retain(renderer);
    representation->visible = FVIZ_TRUE;
    if (representation->renderer == NULL)
    {
        fviz_release(representation);
        return fviz_last_error_code();
    }
    *out_representation = representation;
    return FVIZ_OK;
}

FVizRenderer* fviz_widget_representation_renderer(FVizWidgetRepresentation* representation)
{
    return representation != NULL ? representation->renderer : NULL;
}

FVizResult fviz_widget_representation_add_actor(FVizWidgetRepresentation* representation, FVizActor* actor)
{
    return fviz_widget_representation_add_entry(representation, FVIZ_WIDGET_REPRESENTATION_ACTOR,
        representation != NULL ? &representation->actors : NULL,
        representation != NULL ? &representation->actor_count : NULL,
        representation != NULL ? &representation->actor_capacity : NULL, (FVizObject*)actor);
}

FVizResult fviz_widget_representation_remove_actor(FVizWidgetRepresentation* representation, FVizActor* actor)
{
    return fviz_widget_representation_remove_entry(representation, FVIZ_WIDGET_REPRESENTATION_ACTOR,
        representation != NULL ? representation->actors : NULL,
        representation != NULL ? &representation->actor_count : NULL, (FVizObject*)actor);
}

void fviz_widget_representation_remove_all_actors(FVizWidgetRepresentation* representation)
{
    if (representation == NULL) return;
    fviz_widget_representation_clear_entries(representation, FVIZ_WIDGET_REPRESENTATION_ACTOR,
        representation->actors, &representation->actor_count);
}

FVizSize fviz_widget_representation_actor_count(const FVizWidgetRepresentation* representation)
{
    return representation != NULL ? representation->actor_count : 0u;
}

FVizActor* fviz_widget_representation_actor_at(FVizWidgetRepresentation* representation, FVizSize index)
{
    return representation != NULL && index < representation->actor_count
        ? (FVizActor*)representation->actors[index].object : NULL;
}

FVizResult fviz_widget_representation_set_actor_visible(
    FVizWidgetRepresentation* representation, FVizActor* actor, FVizBool visible)
{
    return fviz_widget_representation_set_entry_visible(representation, FVIZ_WIDGET_REPRESENTATION_ACTOR,
        representation != NULL ? representation->actors : NULL,
        representation != NULL ? representation->actor_count : 0u, (FVizObject*)actor, visible);
}

FVizResult fviz_widget_representation_add_text_actor_2d(
    FVizWidgetRepresentation* representation, FVizTextActor2D* actor)
{
    return fviz_widget_representation_add_entry(representation, FVIZ_WIDGET_REPRESENTATION_TEXT_2D,
        representation != NULL ? &representation->text_actors_2d : NULL,
        representation != NULL ? &representation->text_actor_2d_count : NULL,
        representation != NULL ? &representation->text_actor_2d_capacity : NULL, (FVizObject*)actor);
}

FVizResult fviz_widget_representation_remove_text_actor_2d(
    FVizWidgetRepresentation* representation, FVizTextActor2D* actor)
{
    return fviz_widget_representation_remove_entry(representation, FVIZ_WIDGET_REPRESENTATION_TEXT_2D,
        representation != NULL ? representation->text_actors_2d : NULL,
        representation != NULL ? &representation->text_actor_2d_count : NULL, (FVizObject*)actor);
}

FVizSize fviz_widget_representation_text_actor_2d_count(const FVizWidgetRepresentation* representation)
{
    return representation != NULL ? representation->text_actor_2d_count : 0u;
}

FVizTextActor2D* fviz_widget_representation_text_actor_2d_at(
    FVizWidgetRepresentation* representation, FVizSize index)
{
    return representation != NULL && index < representation->text_actor_2d_count
        ? (FVizTextActor2D*)representation->text_actors_2d[index].object : NULL;
}

FVizResult fviz_widget_representation_set_text_actor_2d_visible(
    FVizWidgetRepresentation* representation, FVizTextActor2D* actor, FVizBool visible)
{
    return fviz_widget_representation_set_entry_visible(representation, FVIZ_WIDGET_REPRESENTATION_TEXT_2D,
        representation != NULL ? representation->text_actors_2d : NULL,
        representation != NULL ? representation->text_actor_2d_count : 0u, (FVizObject*)actor, visible);
}

FVizResult fviz_widget_representation_add_billboard_text_actor_3d(
    FVizWidgetRepresentation* representation, FVizBillboardTextActor3D* actor)
{
    return fviz_widget_representation_add_entry(representation, FVIZ_WIDGET_REPRESENTATION_BILLBOARD_3D,
        representation != NULL ? &representation->billboard_text_actors_3d : NULL,
        representation != NULL ? &representation->billboard_text_actor_3d_count : NULL,
        representation != NULL ? &representation->billboard_text_actor_3d_capacity : NULL,
        (FVizObject*)actor);
}

FVizResult fviz_widget_representation_remove_billboard_text_actor_3d(
    FVizWidgetRepresentation* representation, FVizBillboardTextActor3D* actor)
{
    return fviz_widget_representation_remove_entry(representation, FVIZ_WIDGET_REPRESENTATION_BILLBOARD_3D,
        representation != NULL ? representation->billboard_text_actors_3d : NULL,
        representation != NULL ? &representation->billboard_text_actor_3d_count : NULL, (FVizObject*)actor);
}

FVizSize fviz_widget_representation_billboard_text_actor_3d_count(
    const FVizWidgetRepresentation* representation)
{
    return representation != NULL ? representation->billboard_text_actor_3d_count : 0u;
}

FVizBillboardTextActor3D* fviz_widget_representation_billboard_text_actor_3d_at(
    FVizWidgetRepresentation* representation, FVizSize index)
{
    return representation != NULL && index < representation->billboard_text_actor_3d_count
        ? (FVizBillboardTextActor3D*)representation->billboard_text_actors_3d[index].object : NULL;
}

FVizResult fviz_widget_representation_set_billboard_text_actor_3d_visible(
    FVizWidgetRepresentation* representation, FVizBillboardTextActor3D* actor, FVizBool visible)
{
    return fviz_widget_representation_set_entry_visible(representation, FVIZ_WIDGET_REPRESENTATION_BILLBOARD_3D,
        representation != NULL ? representation->billboard_text_actors_3d : NULL,
        representation != NULL ? representation->billboard_text_actor_3d_count : 0u,
        (FVizObject*)actor, visible);
}

FVizResult fviz_widget_representation_add_label_set_3d(
    FVizWidgetRepresentation* representation, FVizLabelSet3D* label_set)
{
    return fviz_widget_representation_add_entry(representation, FVIZ_WIDGET_REPRESENTATION_LABEL_SET_3D,
        representation != NULL ? &representation->label_sets_3d : NULL,
        representation != NULL ? &representation->label_set_3d_count : NULL,
        representation != NULL ? &representation->label_set_3d_capacity : NULL, (FVizObject*)label_set);
}

FVizResult fviz_widget_representation_remove_label_set_3d(
    FVizWidgetRepresentation* representation, FVizLabelSet3D* label_set)
{
    return fviz_widget_representation_remove_entry(representation, FVIZ_WIDGET_REPRESENTATION_LABEL_SET_3D,
        representation != NULL ? representation->label_sets_3d : NULL,
        representation != NULL ? &representation->label_set_3d_count : NULL, (FVizObject*)label_set);
}

FVizSize fviz_widget_representation_label_set_3d_count(const FVizWidgetRepresentation* representation)
{
    return representation != NULL ? representation->label_set_3d_count : 0u;
}

FVizLabelSet3D* fviz_widget_representation_label_set_3d_at(
    FVizWidgetRepresentation* representation, FVizSize index)
{
    return representation != NULL && index < representation->label_set_3d_count
        ? (FVizLabelSet3D*)representation->label_sets_3d[index].object : NULL;
}

FVizResult fviz_widget_representation_set_label_set_3d_visible(
    FVizWidgetRepresentation* representation, FVizLabelSet3D* label_set, FVizBool visible)
{
    return fviz_widget_representation_set_entry_visible(representation, FVIZ_WIDGET_REPRESENTATION_LABEL_SET_3D,
        representation != NULL ? representation->label_sets_3d : NULL,
        representation != NULL ? representation->label_set_3d_count : 0u, (FVizObject*)label_set, visible);
}

void fviz_widget_representation_remove_all_annotations(FVizWidgetRepresentation* representation)
{
    if (representation == NULL) return;
    fviz_widget_representation_clear_entries(representation, FVIZ_WIDGET_REPRESENTATION_TEXT_2D,
        representation->text_actors_2d, &representation->text_actor_2d_count);
    fviz_widget_representation_clear_entries(representation, FVIZ_WIDGET_REPRESENTATION_BILLBOARD_3D,
        representation->billboard_text_actors_3d, &representation->billboard_text_actor_3d_count);
    fviz_widget_representation_clear_entries(representation, FVIZ_WIDGET_REPRESENTATION_LABEL_SET_3D,
        representation->label_sets_3d, &representation->label_set_3d_count);
}

void fviz_widget_representation_set_visible(
    FVizWidgetRepresentation* representation, FVizBool visible)
{
    FVizSize i;
    if (representation == NULL) return;
    visible = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (representation->visible == visible) return;
    representation->visible = visible;
    for (i = 0u; i < representation->actor_count; ++i)
        fviz_widget_representation_apply_visibility(
            representation, FVIZ_WIDGET_REPRESENTATION_ACTOR, &representation->actors[i]);
    for (i = 0u; i < representation->text_actor_2d_count; ++i)
        fviz_widget_representation_apply_visibility(
            representation, FVIZ_WIDGET_REPRESENTATION_TEXT_2D, &representation->text_actors_2d[i]);
    for (i = 0u; i < representation->billboard_text_actor_3d_count; ++i)
        fviz_widget_representation_apply_visibility(
            representation, FVIZ_WIDGET_REPRESENTATION_BILLBOARD_3D,
            &representation->billboard_text_actors_3d[i]);
    for (i = 0u; i < representation->label_set_3d_count; ++i)
        fviz_widget_representation_apply_visibility(
            representation, FVIZ_WIDGET_REPRESENTATION_LABEL_SET_3D, &representation->label_sets_3d[i]);
    fviz_object_modified((FVizObject*)representation);
}

FVizBool fviz_widget_representation_visible(const FVizWidgetRepresentation* representation)
{
    return representation != NULL ? representation->visible : FVIZ_FALSE;
}
