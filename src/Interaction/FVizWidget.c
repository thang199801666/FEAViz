#include <FViz/Core/FVizError.h>
#include <FViz/Interaction/FVizWidget.h>
#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizWidgetPrivate.h>

static FVizMTime fviz_widget_mtime(const FVizObject* object)
{
    const FVizWidget* widget = (const FVizWidget*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    const FVizMTime child = fviz_object_mtime((const FVizObject*)widget->representation);
    return child > mtime ? child : mtime;
}

static FVizBool fviz_widget_interactor_observer(
    FVizRenderWindowInteractor* interactor,
    const FVizInteractionEvent* event,
    void* user_data)
{
    FVizWidget* widget = (FVizWidget*)user_data;
    FVizWidget* guard;
    FVizBool handled;
    (void)interactor;
    guard = (FVizWidget*)fviz_retain(widget);
    if (guard == NULL) return FVIZ_FALSE;
    handled = fviz_widget_process_event(guard, event);
    fviz_release(guard);
    return handled;
}

static void fviz_widget_destroy(FVizObject* object)
{
    FVizWidget* widget = (FVizWidget*)object;
    if (widget->observer_id != FVIZ_OBSERVER_ID_INVALID && widget->interactor != NULL)
        (void)fviz_render_window_interactor_remove_observer(widget->interactor, widget->observer_id);
    widget->observer_id = FVIZ_OBSERVER_ID_INVALID;
    fviz_release(widget->representation);
    fviz_release(widget->renderer);
    fviz_release(widget->interactor);
    widget->representation = NULL;
    widget->renderer = NULL;
    widget->interactor = NULL;
}

static const FVizObjectClass g_fviz_widget_class = {
    FVIZ_TYPE_WIDGET,
    "FVizWidget",
    &g_fviz_object_class,
    fviz_widget_destroy,
    fviz_widget_mtime
};

static FVizResult fviz_widget_attach_observer(FVizWidget* widget)
{
    if (widget->observer_id != FVIZ_OBSERVER_ID_INVALID || widget->interactor == NULL)
        return FVIZ_OK;
    return fviz_render_window_interactor_add_observer(
        widget->interactor,
        FVIZ_INTERACTION_EVENT_ANY,
        widget->priority,
        fviz_widget_interactor_observer,
        widget,
        &widget->observer_id);
}

static void fviz_widget_detach_observer(FVizWidget* widget)
{
    if (widget->observer_id == FVIZ_OBSERVER_ID_INVALID || widget->interactor == NULL) return;
    (void)fviz_render_window_interactor_remove_observer(widget->interactor, widget->observer_id);
    widget->observer_id = FVIZ_OBSERVER_ID_INVALID;
}

FVizResult fviz_widget_create(
    FVizRenderWindowInteractor* interactor,
    FVizRenderer* renderer,
    FVizWidgetRepresentation* representation,
    FVizWidget** out_widget)
{
    FVizWidget* widget;
    if (renderer == NULL || out_widget == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_widget = NULL;
    if (representation != NULL && fviz_widget_representation_renderer(representation) != renderer)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    widget = (FVizWidget*)fviz_internal_object_allocate(
        sizeof(*widget), &g_fviz_widget_class, NULL);
    if (widget == NULL) return fviz_last_error_code();
    widget->interactor = (FVizRenderWindowInteractor*)fviz_retain(interactor);
    widget->renderer = (FVizRenderer*)fviz_retain(renderer);
    widget->representation = (FVizWidgetRepresentation*)fviz_retain(representation);
    widget->priority = 100;
    widget->state = FVIZ_WIDGET_STATE_START;
    widget->enabled = FVIZ_TRUE;
    widget->process_events = FVIZ_TRUE;
    widget->observer_id = FVIZ_OBSERVER_ID_INVALID;
    if (widget->renderer == NULL ||
        (interactor != NULL && widget->interactor == NULL) ||
        (representation != NULL && widget->representation == NULL) ||
        fviz_widget_attach_observer(widget) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    if (widget->representation != NULL)
        fviz_widget_representation_set_visible(widget->representation, FVIZ_TRUE);
    *out_widget = widget;
    return FVIZ_OK;
}

FVizRenderWindowInteractor* fviz_widget_interactor(FVizWidget* widget)
{
    return widget != NULL ? widget->interactor : NULL;
}

FVizResult fviz_widget_set_interactor(FVizWidget* widget, FVizRenderWindowInteractor* interactor)
{
    FVizRenderWindowInteractor* replacement;
    FVizRenderWindowInteractor* previous;
    FVizResult result;
    if (widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (widget->interactor == interactor) return FVIZ_OK;
    replacement = (FVizRenderWindowInteractor*)fviz_retain(interactor);
    if (interactor != NULL && replacement == NULL) return fviz_last_error_code();
    previous = widget->interactor;
    fviz_widget_detach_observer(widget);
    widget->interactor = replacement;
    if (widget->enabled != FVIZ_FALSE)
    {
        result = fviz_widget_attach_observer(widget);
        if (result != FVIZ_OK)
        {
            widget->interactor = previous;
            widget->observer_id = FVIZ_OBSERVER_ID_INVALID;
            (void)fviz_widget_attach_observer(widget);
            fviz_release(replacement);
            return result;
        }
    }
    fviz_release(previous);
    fviz_object_modified((FVizObject*)widget);
    return FVIZ_OK;
}

FVizRenderer* fviz_widget_renderer(FVizWidget* widget)
{
    return widget != NULL ? widget->renderer : NULL;
}

FVizWidgetRepresentation* fviz_widget_representation(FVizWidget* widget)
{
    return widget != NULL ? widget->representation : NULL;
}

FVizResult fviz_widget_set_representation(
    FVizWidget* widget, FVizWidgetRepresentation* representation)
{
    if (widget == NULL || (representation != NULL &&
        fviz_widget_representation_renderer(representation) != widget->renderer))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (representation != NULL && fviz_retain(representation) == NULL)
        return fviz_last_error_code();
    if (widget->representation != NULL)
        fviz_widget_representation_set_visible(widget->representation, FVIZ_FALSE);
    fviz_release(widget->representation);
    widget->representation = representation;
    if (representation != NULL)
        fviz_widget_representation_set_visible(representation, widget->enabled);
    fviz_object_modified((FVizObject*)widget);
    return FVIZ_OK;
}

void fviz_internal_widget_notify(FVizWidget* widget, FVizWidgetNotification notification)
{
    if (widget != NULL && widget->callback != NULL)
        widget->callback(widget, notification, widget->callback_user_data);
}

FVizResult fviz_widget_set_enabled(FVizWidget* widget, FVizBool enabled)
{
    if (widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (widget->enabled == enabled) return FVIZ_OK;
    if (enabled == FVIZ_TRUE)
    {
        widget->enabled = FVIZ_TRUE;
        widget->state = FVIZ_WIDGET_STATE_START;
        if (fviz_widget_attach_observer(widget) != FVIZ_OK)
        {
            widget->enabled = FVIZ_FALSE;
            widget->state = FVIZ_WIDGET_STATE_DISABLED;
            return fviz_last_error_code();
        }
        if (widget->representation != NULL)
            fviz_widget_representation_set_visible(widget->representation, FVIZ_TRUE);
        fviz_internal_widget_notify(widget, FVIZ_WIDGET_ENABLED);
    }
    else
    {
        if (widget->state == FVIZ_WIDGET_STATE_ACTIVE)
            fviz_widget_cancel_interaction(widget);
        widget->enabled = FVIZ_FALSE;
        widget->state = FVIZ_WIDGET_STATE_DISABLED;
        fviz_widget_detach_observer(widget);
        if (widget->representation != NULL)
            fviz_widget_representation_set_visible(widget->representation, FVIZ_FALSE);
        fviz_internal_widget_notify(widget, FVIZ_WIDGET_DISABLED);
    }
    fviz_object_modified((FVizObject*)widget);
    return FVIZ_OK;
}

FVizBool fviz_widget_enabled(const FVizWidget* widget)
{
    return widget != NULL ? widget->enabled : FVIZ_FALSE;
}

void fviz_widget_set_process_events(FVizWidget* widget, FVizBool process_events)
{
    if (widget == NULL) return;
    widget->process_events = process_events != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_widget_process_events(const FVizWidget* widget)
{
    return widget != NULL ? widget->process_events : FVIZ_FALSE;
}

FVizResult fviz_widget_set_priority(FVizWidget* widget, int priority)
{
    FVizBool attached;
    int previous_priority;
    FVizResult result;
    if (widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    attached = widget->observer_id != FVIZ_OBSERVER_ID_INVALID ? FVIZ_TRUE : FVIZ_FALSE;
    if (widget->priority == priority) return FVIZ_OK;
    previous_priority = widget->priority;
    if (attached == FVIZ_TRUE) fviz_widget_detach_observer(widget);
    widget->priority = priority;
    if (attached == FVIZ_TRUE)
    {
        result = fviz_widget_attach_observer(widget);
        if (result != FVIZ_OK)
        {
            widget->priority = previous_priority;
            (void)fviz_widget_attach_observer(widget);
            return result;
        }
    }
    fviz_object_modified((FVizObject*)widget);
    return FVIZ_OK;
}

int fviz_widget_priority(const FVizWidget* widget)
{
    return widget != NULL ? widget->priority : 0;
}

FVizWidgetState fviz_widget_state(const FVizWidget* widget)
{
    return widget != NULL ? widget->state : FVIZ_WIDGET_STATE_DISABLED;
}

void fviz_widget_set_event_handler(
    FVizWidget* widget, FVizWidgetEventHandlerFn handler, void* user_data)
{
    if (widget == NULL) return;
    widget->event_handler = handler;
    widget->event_user_data = user_data;
}

void fviz_widget_set_callback(
    FVizWidget* widget, FVizWidgetCallbackFn callback, void* user_data)
{
    if (widget == NULL) return;
    widget->callback = callback;
    widget->callback_user_data = user_data;
}

static FVizBool fviz_widget_event_is_positional(FVizInteractionEventType type)
{
    return type == FVIZ_INTERACTION_MOUSE_BUTTON_DOWN ||
        type == FVIZ_INTERACTION_MOUSE_BUTTON_UP ||
        type == FVIZ_INTERACTION_MOUSE_MOVE ||
        type == FVIZ_INTERACTION_MOUSE_WHEEL ||
        type == FVIZ_INTERACTION_DOUBLE_CLICK;
}

FVizBool fviz_widget_process_event(FVizWidget* widget, const FVizInteractionEvent* event)
{
    FVizRenderWindow* window;
    FVizRenderer* renderer;
    if (widget == NULL || event == NULL || widget->enabled == FVIZ_FALSE ||
        widget->process_events == FVIZ_FALSE || widget->event_handler == NULL)
        return FVIZ_FALSE;
    if (event->type == FVIZ_INTERACTION_FOCUS_OUT ||
        (event->type == FVIZ_INTERACTION_KEY_DOWN && event->key == FVIZ_KEY_ESCAPE))
    {
        if (widget->state == FVIZ_WIDGET_STATE_ACTIVE)
        {
            const FVizBool handled = widget->event_handler(
                widget, event, widget->event_user_data);
            if (widget->state == FVIZ_WIDGET_STATE_ACTIVE)
                fviz_widget_cancel_interaction(widget);
            (void)handled;
            return FVIZ_TRUE;
        }
        return FVIZ_FALSE;
    }
    if (widget->state != FVIZ_WIDGET_STATE_ACTIVE && fviz_widget_event_is_positional(event->type) == FVIZ_TRUE)
    {
        window = fviz_render_window_interactor_window(widget->interactor);
        renderer = window != NULL ? fviz_render_window_find_renderer(window, event->x, event->y) : NULL;
        if (renderer != NULL && renderer != widget->renderer) return FVIZ_FALSE;
    }
    return widget->event_handler(widget, event, widget->event_user_data);
}

void fviz_widget_begin_interaction(FVizWidget* widget)
{
    if (widget == NULL || widget->enabled == FVIZ_FALSE) return;
    widget->state = FVIZ_WIDGET_STATE_ACTIVE;
    if (widget->interactor != NULL) fviz_render_window_interactor_grab_focus(widget->interactor);
    fviz_internal_widget_notify(widget, FVIZ_WIDGET_START_INTERACTION);
    fviz_object_modified((FVizObject*)widget);
}

void fviz_widget_interaction(FVizWidget* widget)
{
    if (widget == NULL || widget->state != FVIZ_WIDGET_STATE_ACTIVE) return;
    fviz_internal_widget_notify(widget, FVIZ_WIDGET_INTERACTION);
}

void fviz_widget_value_changed(FVizWidget* widget)
{
    if (widget == NULL) return;
    fviz_internal_widget_notify(widget, FVIZ_WIDGET_VALUE_CHANGED);
    fviz_object_modified((FVizObject*)widget);
}

void fviz_widget_end_interaction(FVizWidget* widget)
{
    if (widget == NULL || widget->state != FVIZ_WIDGET_STATE_ACTIVE) return;
    widget->state = FVIZ_WIDGET_STATE_START;
    if (widget->interactor != NULL) fviz_render_window_interactor_release_focus(widget->interactor);
    fviz_internal_widget_notify(widget, FVIZ_WIDGET_END_INTERACTION);
    fviz_object_modified((FVizObject*)widget);
}

void fviz_widget_cancel_interaction(FVizWidget* widget)
{
    if (widget == NULL || widget->state != FVIZ_WIDGET_STATE_ACTIVE) return;
    widget->state = FVIZ_WIDGET_STATE_START;
    if (widget->interactor != NULL) fviz_render_window_interactor_release_focus(widget->interactor);
    fviz_internal_widget_notify(widget, FVIZ_WIDGET_CANCELLED);
    fviz_object_modified((FVizObject*)widget);
}

FVizResult fviz_widget_request_render(FVizWidget* widget)
{
    if (widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (widget->interactor == NULL) return FVIZ_ERROR_INVALID_STATE;
    return fviz_render_window_interactor_request_render(widget->interactor);
}
