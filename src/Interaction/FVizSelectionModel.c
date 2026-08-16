#include <FViz/Core/FVizError.h>
#include <FViz/Interaction/FVizSelectionModel.h>
#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizSelectionModelPrivate.h>

static void fviz_selection_model_destroy(FVizObject* object)
{
    FVizSelectionModel* model = (FVizSelectionModel*)object;
    fviz_release(model->hover_selection);
    fviz_release(model->selection);
    model->hover_selection = NULL;
    model->selection = NULL;
}

static const FVizObjectClass g_fviz_selection_model_class = {FVIZ_TYPE_SELECTION_MODEL, "FVizSelectionModel",
                                                             &g_fviz_object_class, fviz_selection_model_destroy, NULL};

static FVizBool fviz_selection_model_valid_association(FVizSelectionAssociation association)
{
    return association >= FVIZ_SELECTION_ACTOR && association <= FVIZ_SELECTION_GLYPH_INSTANCE ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_selection_model_valid_modifier(FVizSelectionModifier modifier)
{
    return modifier >= FVIZ_SELECTION_REPLACE && modifier <= FVIZ_SELECTION_TOGGLE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizResult fviz_selection_model_create(FVizSelectionModel** out_model)
{
    FVizSelectionModel* model;
    if (out_model == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_model = NULL;
    model = (FVizSelectionModel*)fviz_internal_object_allocate(sizeof(*model), &g_fviz_selection_model_class, NULL);
    if (model == NULL) return fviz_last_error_code();
    model->association = FVIZ_SELECTION_CELL;
    model->modifier = FVIZ_SELECTION_REPLACE;
    model->hover_update_rate = 30.0f;
    model->last_hover_timestamp = 0.0;
    model->last_hover_x = 0;
    model->last_hover_y = 0;
    model->have_hover_sample = FVIZ_FALSE;
    if (fviz_selection_create(&model->selection) != FVIZ_OK ||
        fviz_selection_create(&model->hover_selection) != FVIZ_OK)
    {
        fviz_release(model);
        return fviz_last_error_code();
    }
    *out_model = model;
    return FVIZ_OK;
}

FVizSelection* fviz_selection_model_selection(FVizSelectionModel* model)
{
    return model != NULL ? model->selection : NULL;
}

const FVizSelection* fviz_selection_model_const_selection(const FVizSelectionModel* model)
{
    return model != NULL ? model->selection : NULL;
}

FVizSelection* fviz_selection_model_hover_selection(FVizSelectionModel* model)
{
    return model != NULL ? model->hover_selection : NULL;
}

const FVizSelection* fviz_selection_model_const_hover_selection(const FVizSelectionModel* model)
{
    return model != NULL ? model->hover_selection : NULL;
}

void fviz_selection_model_clear(FVizSelectionModel* model)
{
    if (model == NULL || fviz_selection_count(model->selection) == 0u) return;
    fviz_selection_clear(model->selection);
    fviz_object_modified((FVizObject*)model);
}

void fviz_selection_model_clear_hover(FVizSelectionModel* model)
{
    if (model == NULL || fviz_selection_count(model->hover_selection) == 0u) return;
    fviz_selection_clear(model->hover_selection);
    fviz_object_modified((FVizObject*)model);
}

void fviz_selection_model_set_association(FVizSelectionModel* model, FVizSelectionAssociation association)
{
    if (model == NULL || fviz_selection_model_valid_association(association) == FVIZ_FALSE ||
        model->association == association)
        return;
    model->association = association;
    fviz_object_modified((FVizObject*)model);
}

FVizSelectionAssociation fviz_selection_model_association(const FVizSelectionModel* model)
{
    return model != NULL ? model->association : FVIZ_SELECTION_CELL;
}

void fviz_selection_model_set_modifier(FVizSelectionModel* model, FVizSelectionModifier modifier)
{
    if (model == NULL || fviz_selection_model_valid_modifier(modifier) == FVIZ_FALSE || model->modifier == modifier)
        return;
    model->modifier = modifier;
    fviz_object_modified((FVizObject*)model);
}

FVizSelectionModifier fviz_selection_model_modifier(const FVizSelectionModel* model)
{
    return model != NULL ? model->modifier : FVIZ_SELECTION_REPLACE;
}

FVizSelectionModifier fviz_selection_modifier_from_event(const FVizInteractionEvent* event)
{
    if (event == NULL) return FVIZ_SELECTION_REPLACE;
    if (event->alt != FVIZ_FALSE) return FVIZ_SELECTION_SUBTRACT;
    if (event->control != FVIZ_FALSE) return FVIZ_SELECTION_TOGGLE;
    if (event->shift != FVIZ_FALSE) return FVIZ_SELECTION_ADD;
    return FVIZ_SELECTION_REPLACE;
}

FVizResult fviz_selection_model_apply(FVizSelectionModel* model, const FVizSelection* incoming,
                                      FVizSelectionModifier modifier)
{
    FVizResult result;
    if (model == NULL || incoming == NULL || fviz_selection_model_valid_modifier(modifier) == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_selection_apply(model->selection, incoming, modifier);
    if (result == FVIZ_OK) fviz_object_modified((FVizObject*)model);
    return result;
}

FVizResult fviz_selection_model_select_at(FVizSelectionModel* model, FVizRenderWindow* window, int x, int y,
                                          FVizSelectionModifier modifier)
{
    FVizSelection* incoming = NULL;
    FVizResult result;
    if (model == NULL || window == NULL || fviz_selection_model_valid_modifier(modifier) == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_render_window_select_at(window, x, y, model->association, &incoming);
    if (result == FVIZ_ERROR_NOT_FOUND)
    {
        if (modifier == FVIZ_SELECTION_REPLACE) fviz_selection_model_clear(model);
        return result;
    }
    if (result != FVIZ_OK) return result;
    result = fviz_selection_model_apply(model, incoming, modifier);
    fviz_release(incoming);
    return result;
}

FVizResult fviz_selection_model_select_rectangle(FVizSelectionModel* model, FVizRenderWindow* window, int start_x,
                                                 int start_y, int end_x, int end_y, FVizSelectionModifier modifier)
{
    FVizSelection* incoming = NULL;
    FVizResult result;
    if (model == NULL || window == NULL || fviz_selection_model_valid_modifier(modifier) == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_render_window_select_rectangle_association(window, start_x, start_y, end_x, end_y, model->association,
                                                             &incoming);
    if (result != FVIZ_OK) return result;
    result = fviz_selection_model_apply(model, incoming, modifier);
    fviz_release(incoming);
    return result;
}

FVizResult fviz_selection_model_select_polygon(FVizSelectionModel* model, FVizRenderWindow* window,
                                               const int* xy_points, FVizSize point_count,
                                               FVizSelectionModifier modifier)
{
    FVizSelection* incoming = NULL;
    FVizResult result;
    if (model == NULL || window == NULL || xy_points == NULL || point_count < 3u ||
        fviz_selection_model_valid_modifier(modifier) == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_render_window_select_polygon(window, xy_points, point_count, model->association, &incoming);
    if (result != FVIZ_OK) return result;
    result = fviz_selection_model_apply(model, incoming, modifier);
    fviz_release(incoming);
    return result;
}

FVizResult fviz_selection_model_select_frustum(FVizSelectionModel* model, FVizRenderer* renderer,
                                               const FVizFrustum* frustum, FVizSelectionModifier modifier)
{
    FVizSelection* incoming = NULL;
    FVizResult result;
    if (model == NULL || renderer == NULL || frustum == NULL || modifier < FVIZ_SELECTION_REPLACE ||
        modifier > FVIZ_SELECTION_TOGGLE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_selection_select_frustum(renderer, frustum, model->association, &incoming);
    if (result != FVIZ_OK) return result;
    result = fviz_selection_model_apply(model, incoming, modifier);
    fviz_release(incoming);
    return result;
}

void fviz_selection_model_set_hover_update_rate(FVizSelectionModel* model, float updates_per_second)
{
    if (model == NULL || updates_per_second < 0.0f || updates_per_second > 240.0f) return;
    if (model->hover_update_rate == updates_per_second) return;
    model->hover_update_rate = updates_per_second;
    model->have_hover_sample = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)model);
}

float fviz_selection_model_hover_update_rate(const FVizSelectionModel* model)
{
    return model != NULL ? model->hover_update_rate : 0.0f;
}

FVizResult fviz_selection_model_process_hover_event(FVizSelectionModel* model, FVizRenderWindow* window,
                                                    const FVizInteractionEvent* event)
{
    double minimum_interval;
    if (model == NULL || window == NULL || event == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (event->type == FVIZ_INTERACTION_LEAVE)
    {
        model->have_hover_sample = FVIZ_FALSE;
        fviz_selection_model_clear_hover(model);
        return FVIZ_OK;
    }
    if (event->type != FVIZ_INTERACTION_MOUSE_MOVE) return FVIZ_OK;
    if (model->have_hover_sample != FVIZ_FALSE && model->last_hover_x == event->x && model->last_hover_y == event->y)
        return FVIZ_OK;
    minimum_interval = model->hover_update_rate > 0.0f ? 1.0 / (double)model->hover_update_rate : 0.0;
    if (model->have_hover_sample != FVIZ_FALSE && event->timestamp_seconds > 0.0 && model->last_hover_timestamp > 0.0 &&
        event->timestamp_seconds - model->last_hover_timestamp < minimum_interval)
        return FVIZ_OK;
    model->last_hover_x = event->x;
    model->last_hover_y = event->y;
    model->last_hover_timestamp = event->timestamp_seconds;
    model->have_hover_sample = FVIZ_TRUE;
    return fviz_selection_model_update_hover(model, window, event->x, event->y);
}

FVizResult fviz_selection_model_update_hover(FVizSelectionModel* model, FVizRenderWindow* window, int x, int y)
{
    FVizSelection* incoming = NULL;
    FVizResult result;
    if (model == NULL || window == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_render_window_select_at(window, x, y, model->association, &incoming);
    if (result == FVIZ_ERROR_NOT_FOUND)
    {
        fviz_selection_model_clear_hover(model);
        return FVIZ_OK;
    }
    if (result != FVIZ_OK) return result;
    result = fviz_selection_apply(model->hover_selection, incoming, FVIZ_SELECTION_REPLACE);
    fviz_release(incoming);
    if (result == FVIZ_OK) fviz_object_modified((FVizObject*)model);
    return result;
}
