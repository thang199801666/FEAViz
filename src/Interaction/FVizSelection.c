#include <stdint.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Interaction/FVizSelection.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizSelectionPrivate.h>

static void fviz_selection_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_selection_class = {
    FVIZ_TYPE_SELECTION,
    "FVizSelection",
    &g_fviz_object_class,
    fviz_selection_destroy,
    NULL
};

static void fviz_selection_destroy(FVizObject* object)
{
    FVizSelection* selection = (FVizSelection*)object;
    fviz_selection_clear(selection);
    fviz_release(selection->items);
    selection->items = NULL;
}

FVizResult fviz_selection_create(FVizSelection** out_selection)
{
    FVizSelection* selection;
    if (out_selection == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_selection must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_selection = NULL;
    selection = (FVizSelection*)fviz_internal_object_allocate(
        sizeof(FVizSelection), &g_fviz_selection_class, NULL);
    if (selection == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizSelectionItem), &selection->items) != FVIZ_OK)
    {
        fviz_release(selection);
        return fviz_last_error_code();
    }
    *out_selection = selection;
    return FVIZ_OK;
}

void fviz_selection_clear(FVizSelection* selection)
{
    FVizSize i;
    if (selection == NULL || selection->items == NULL) return;
    for (i = 0u; i < fviz_array_count(selection->items); ++i)
    {
        FVizSelectionItem* item = (FVizSelectionItem*)fviz_array_at(selection->items, i);
        fviz_release(item->actor);
    }
    fviz_array_clear(selection->items);
    fviz_object_modified((FVizObject*)selection);
}

FVizResult fviz_selection_add(
    FVizSelection* selection,
    FVizActor* actor,
    FVizSelectionAssociation association,
    FVizSize id)
{
    FVizSelectionItem item;
    if (selection == NULL || actor == NULL || association < FVIZ_SELECTION_ACTOR ||
        association > FVIZ_SELECTION_CELL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "selection item is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_retain(actor) == NULL) return fviz_last_error_code();
    item.actor = actor;
    item.association = association;
    item.id = id;
    if (fviz_array_push(selection->items, &item) != FVIZ_OK)
    {
        fviz_release(actor);
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)selection);
    return FVIZ_OK;
}

FVizSize fviz_selection_count(const FVizSelection* selection)
{
    return selection != NULL ? fviz_array_count(selection->items) : 0u;
}

FVizActor* fviz_selection_actor(FVizSelection* selection, FVizSize index)
{
    FVizSelectionItem* item = selection != NULL
        ? (FVizSelectionItem*)fviz_array_at(selection->items, index)
        : NULL;
    return item != NULL ? item->actor : NULL;
}

FVizSelectionAssociation fviz_selection_association(
    const FVizSelection* selection,
    FVizSize index)
{
    const FVizSelectionItem* item = selection != NULL
        ? (const FVizSelectionItem*)fviz_array_const_at(selection->items, index)
        : NULL;
    return item != NULL ? item->association : FVIZ_SELECTION_ACTOR;
}

FVizSize fviz_selection_id(const FVizSelection* selection, FVizSize index)
{
    const FVizSelectionItem* item = selection != NULL
        ? (const FVizSelectionItem*)fviz_array_const_at(selection->items, index)
        : NULL;
    return item != NULL ? item->id : SIZE_MAX;
}
