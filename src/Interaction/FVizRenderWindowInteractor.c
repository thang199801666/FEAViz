#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Interaction/FVizRenderWindowInteractor.h>
#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizRenderWindowInteractorPrivate.h>

static void fviz_render_window_interactor_destroy(FVizObject* object)
{
    FVizRenderWindowInteractor* interactor = (FVizRenderWindowInteractor*)object;
    fviz_free(interactor->observers);
    fviz_release(interactor->style);
    interactor->observers = NULL;
    interactor->observer_count = 0u;
    interactor->observer_capacity = 0u;
    interactor->style = NULL;
    interactor->window = NULL;
}

static const FVizObjectClass g_fviz_render_window_interactor_class = {
    FVIZ_TYPE_RENDER_WINDOW_INTERACTOR,
    "FVizRenderWindowInteractor",
    &g_fviz_object_class,
    fviz_render_window_interactor_destroy,
    NULL
};

FVizResult fviz_internal_render_window_interactor_create(
    FVizRenderWindow* window,
    FVizRenderWindowInteractor** out_interactor)
{
    FVizRenderWindowInteractor* interactor;
    if (window == NULL || out_interactor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window and out_interactor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_interactor = NULL;
    interactor = (FVizRenderWindowInteractor*)fviz_internal_object_allocate(
        sizeof(FVizRenderWindowInteractor), &g_fviz_render_window_interactor_class, NULL);
    if (interactor == NULL) return fviz_last_error_code();
    interactor->window = window;
    interactor->next_observer_id = 1u;
    interactor->next_observer_sequence = 1u;
    interactor->desired_update_rate = 30.0;
    interactor->still_update_rate = 0.0001;
    interactor->initialized = FVIZ_TRUE;
    interactor->enabled = FVIZ_TRUE;
    interactor->render_enabled = FVIZ_TRUE;
    if (fviz_interactor_style_trackball_camera_create(&interactor->style) != FVIZ_OK)
    {
        fviz_release(interactor);
        return fviz_last_error_code();
    }
    *out_interactor = interactor;
    return FVIZ_OK;
}

void fviz_internal_render_window_interactor_detach(FVizRenderWindowInteractor* interactor)
{
    if (interactor != NULL) interactor->window = NULL;
}

FVizRenderWindow* fviz_render_window_interactor_window(FVizRenderWindowInteractor* interactor)
{
    return interactor != NULL ? interactor->window : NULL;
}

FVizResult fviz_render_window_interactor_initialize(FVizRenderWindowInteractor* interactor)
{
    if (interactor == NULL || interactor->window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "interactor is not attached to a render window");
        return FVIZ_ERROR_INVALID_STATE;
    }
    interactor->initialized = FVIZ_TRUE;
    interactor->enabled = FVIZ_TRUE;
    return FVIZ_OK;
}

void fviz_render_window_interactor_enable(FVizRenderWindowInteractor* interactor)
{
    if (interactor != NULL && interactor->initialized == FVIZ_TRUE)
        interactor->enabled = FVIZ_TRUE;
}

void fviz_render_window_interactor_disable(FVizRenderWindowInteractor* interactor)
{
    if (interactor != NULL) interactor->enabled = FVIZ_FALSE;
}

FVizBool fviz_render_window_interactor_enabled(const FVizRenderWindowInteractor* interactor)
{
    return interactor != NULL ? interactor->enabled : FVIZ_FALSE;
}

void fviz_render_window_interactor_set_done(FVizRenderWindowInteractor* interactor, FVizBool done)
{
    if (interactor == NULL) return;
    interactor->done = done != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (interactor->done == FVIZ_TRUE && interactor->window != NULL)
        fviz_render_window_request_close(interactor->window);
}

FVizBool fviz_render_window_interactor_done(const FVizRenderWindowInteractor* interactor)
{
    return interactor != NULL ? interactor->done : FVIZ_FALSE;
}

void fviz_render_window_interactor_set_render_enabled(
    FVizRenderWindowInteractor* interactor,
    FVizBool enabled)
{
    if (interactor != NULL)
        interactor->render_enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_render_window_interactor_render_enabled(
    const FVizRenderWindowInteractor* interactor)
{
    return interactor != NULL ? interactor->render_enabled : FVIZ_FALSE;
}

void fviz_render_window_interactor_set_update_rates(
    FVizRenderWindowInteractor* interactor,
    double desired,
    double still)
{
    if (interactor == NULL) return;
    if (desired > 0.0) interactor->desired_update_rate = desired;
    if (still > 0.0) interactor->still_update_rate = still;
}

void fviz_render_window_interactor_get_update_rates(
    const FVizRenderWindowInteractor* interactor,
    double* desired,
    double* still)
{
    if (interactor == NULL) return;
    if (desired != NULL) *desired = interactor->desired_update_rate;
    if (still != NULL) *still = interactor->still_update_rate;
}

FVizRenderer* fviz_render_window_interactor_poked_renderer(
    FVizRenderWindowInteractor* interactor)
{
    return interactor != NULL ? interactor->poked_renderer : NULL;
}

FVizResult fviz_render_window_interactor_set_style(
    FVizRenderWindowInteractor* interactor,
    FVizInteractorStyle* style)
{
    if (interactor == NULL || style == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "interactor and style must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_retain(style) == NULL) return fviz_last_error_code();
    fviz_release(interactor->style);
    interactor->style = style;
    return FVIZ_OK;
}

FVizInteractorStyle* fviz_render_window_interactor_style(FVizRenderWindowInteractor* interactor)
{
    return interactor != NULL ? interactor->style : NULL;
}

void fviz_render_window_interactor_set_event_callback(
    FVizRenderWindowInteractor* interactor,
    FVizInteractorEventCallbackFn callback,
    void* user_data)
{
    if (interactor == NULL) return;
    interactor->event_callback = callback;
    interactor->event_user_data = user_data;
}

static FVizResult fviz_interactor_observers_reserve(
    FVizRenderWindowInteractor* interactor,
    FVizSize required)
{
    FVizInteractorObserver* observers;
    FVizSize capacity;
    FVizSize bytes;
    if (required <= interactor->observer_capacity) return FVIZ_OK;
    capacity = interactor->observer_capacity == 0u ? 8u : interactor->observer_capacity;
    while (capacity < required)
    {
        if (capacity > SIZE_MAX / 2u)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "observer capacity overflow");
            return FVIZ_ERROR_OVERFLOW;
        }
        capacity *= 2u;
    }
    if (fviz_size_multiply(capacity, sizeof(FVizInteractorObserver), &bytes) != FVIZ_OK)
        return fviz_last_error_code();
    observers = (FVizInteractorObserver*)fviz_realloc(interactor->observers, bytes);
    if (observers == NULL) return fviz_last_error_code();
    interactor->observers = observers;
    interactor->observer_capacity = capacity;
    return FVIZ_OK;
}

static FVizBool fviz_interactor_observer_precedes(
    const FVizInteractorObserver* left,
    const FVizInteractorObserver* right)
{
    if (left->priority != right->priority)
        return left->priority > right->priority ? FVIZ_TRUE : FVIZ_FALSE;
    return left->sequence < right->sequence ? FVIZ_TRUE : FVIZ_FALSE;
}

static void fviz_interactor_observers_maintain(FVizRenderWindowInteractor* interactor)
{
    FVizSize source;
    FVizSize destination = 0u;
    if (interactor->observers_need_compaction == FVIZ_TRUE)
    {
        for (source = 0u; source < interactor->observer_count; ++source)
        {
            if (interactor->observers[source].active == FVIZ_TRUE)
            {
                if (source != destination)
                    interactor->observers[destination] = interactor->observers[source];
                ++destination;
            }
        }
        interactor->observer_count = destination;
        interactor->observers_need_compaction = FVIZ_FALSE;
        interactor->observers_need_sort = FVIZ_TRUE;
    }
    if (interactor->observers_need_sort == FVIZ_TRUE)
    {
        for (source = 1u; source < interactor->observer_count; ++source)
        {
            const FVizInteractorObserver value = interactor->observers[source];
            FVizSize position = source;
            while (position > 0u &&
                fviz_interactor_observer_precedes(&value, &interactor->observers[position - 1u]) == FVIZ_TRUE)
            {
                interactor->observers[position] = interactor->observers[position - 1u];
                --position;
            }
            interactor->observers[position] = value;
        }
        interactor->observers_need_sort = FVIZ_FALSE;
    }
}

FVizResult fviz_render_window_interactor_add_observer(
    FVizRenderWindowInteractor* interactor,
    FVizInteractionEventType event_type,
    int priority,
    FVizInteractorEventCallbackFn callback,
    void* user_data,
    FVizObserverId* out_observer_id)
{
    FVizInteractorObserver* observer;
    if (out_observer_id != NULL) *out_observer_id = FVIZ_OBSERVER_ID_INVALID;
    if (interactor == NULL || callback == NULL || out_observer_id == NULL ||
        event_type < FVIZ_INTERACTION_EVENT_ANY || event_type > FVIZ_INTERACTION_RESIZE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid interactor observer arguments");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (interactor->next_observer_id == FVIZ_OBSERVER_ID_INVALID)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "observer identifier space exhausted");
        return FVIZ_ERROR_OVERFLOW;
    }
    if (fviz_interactor_observers_reserve(interactor, interactor->observer_count + 1u) != FVIZ_OK)
        return fviz_last_error_code();
    observer = &interactor->observers[interactor->observer_count++];
    (void)memset(observer, 0, sizeof(*observer));
    observer->id = interactor->next_observer_id++;
    observer->event_type = event_type;
    observer->priority = priority;
    observer->sequence = interactor->next_observer_sequence++;
    observer->activation_dispatch = interactor->dispatch_serial;
    observer->callback = callback;
    observer->user_data = user_data;
    observer->active = FVIZ_TRUE;
    interactor->observers_need_sort = FVIZ_TRUE;
    *out_observer_id = observer->id;
    if (interactor->dispatch_depth == 0u) fviz_interactor_observers_maintain(interactor);
    return FVIZ_OK;
}

FVizResult fviz_render_window_interactor_remove_observer(
    FVizRenderWindowInteractor* interactor,
    FVizObserverId observer_id)
{
    FVizSize i;
    if (interactor == NULL || observer_id == FVIZ_OBSERVER_ID_INVALID)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid interactor or observer identifier");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < interactor->observer_count; ++i)
    {
        if (interactor->observers[i].active == FVIZ_TRUE && interactor->observers[i].id == observer_id)
        {
            interactor->observers[i].active = FVIZ_FALSE;
            interactor->observers_need_compaction = FVIZ_TRUE;
            if (interactor->dispatch_depth == 0u) fviz_interactor_observers_maintain(interactor);
            return FVIZ_OK;
        }
    }
    fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "interactor observer was not found");
    return FVIZ_ERROR_NOT_FOUND;
}

void fviz_render_window_interactor_remove_all_observers(FVizRenderWindowInteractor* interactor)
{
    FVizSize i;
    if (interactor == NULL) return;
    for (i = 0u; i < interactor->observer_count; ++i)
        interactor->observers[i].active = FVIZ_FALSE;
    interactor->observers_need_compaction = FVIZ_TRUE;
    if (interactor->dispatch_depth == 0u) fviz_interactor_observers_maintain(interactor);
}

FVizSize fviz_render_window_interactor_observer_count(const FVizRenderWindowInteractor* interactor)
{
    FVizSize i;
    FVizSize count = 0u;
    if (interactor == NULL) return 0u;
    for (i = 0u; i < interactor->observer_count; ++i)
        if (interactor->observers[i].active == FVIZ_TRUE) ++count;
    return count;
}

static FVizBool fviz_render_window_interactor_dispatch_observers(
    FVizRenderWindowInteractor* interactor,
    const FVizInteractionEvent* event)
{
    const FVizSize dispatch_count = interactor->observer_count;
    FVizSize i;
    for (i = 0u; i < dispatch_count; ++i)
    {
        FVizInteractorObserver* observer = &interactor->observers[i];
        if (observer->active == FVIZ_TRUE &&
            observer->activation_dispatch < interactor->dispatch_serial &&
            (observer->event_type == FVIZ_INTERACTION_EVENT_ANY || observer->event_type == event->type) &&
            observer->callback(interactor, event, observer->user_data) == FVIZ_TRUE)
            return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

FVizBool fviz_render_window_interactor_process_event(
    FVizRenderWindowInteractor* interactor,
    const FVizInteractionEvent* event)
{
    FVizRenderer* renderer;
    FVizBool handled = FVIZ_FALSE;
    FVizBool top_level;
    if (interactor == NULL || interactor->window == NULL || event == NULL) return FVIZ_FALSE;
    if (interactor->enabled == FVIZ_FALSE) return FVIZ_FALSE;
    top_level = interactor->dispatch_depth == 0u ? FVIZ_TRUE : FVIZ_FALSE;
    if (top_level == FVIZ_TRUE) ++interactor->dispatch_serial;
    ++interactor->dispatch_depth;
    if (interactor->event_callback != NULL &&
        interactor->event_callback(interactor, event, interactor->event_user_data) == FVIZ_TRUE)
        handled = FVIZ_TRUE;
    if (handled == FVIZ_FALSE)
        handled = fviz_render_window_interactor_dispatch_observers(interactor, event);
    if (handled == FVIZ_FALSE && event->type == FVIZ_INTERACTION_KEY_DOWN && event->key == FVIZ_KEY_ESCAPE)
    {
        fviz_render_window_request_close(interactor->window);
        handled = FVIZ_TRUE;
    }
    if (handled == FVIZ_FALSE)
    {
        renderer = event->type == FVIZ_INTERACTION_MOUSE_BUTTON_DOWN ||
            event->type == FVIZ_INTERACTION_MOUSE_BUTTON_UP ||
            event->type == FVIZ_INTERACTION_MOUSE_MOVE ||
            event->type == FVIZ_INTERACTION_MOUSE_WHEEL
            ? fviz_render_window_find_renderer(interactor->window, event->x, event->y)
            : fviz_render_window_renderer(interactor->window);
        if (renderer == NULL) renderer = interactor->poked_renderer;
        if (renderer == NULL) renderer = fviz_render_window_renderer(interactor->window);
        interactor->poked_renderer = renderer;
        handled = fviz_interactor_style_process_event(interactor->style, renderer, event);
    }
    --interactor->dispatch_depth;
    if (top_level == FVIZ_TRUE) fviz_interactor_observers_maintain(interactor);
    return handled;
}

FVizResult fviz_render_window_interactor_start(FVizRenderWindowInteractor* interactor)
{
    if (interactor == NULL || interactor->window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "interactor is not attached to a render window");
        return FVIZ_ERROR_INVALID_STATE;
    }
    interactor->done = FVIZ_FALSE;
    return fviz_render_window_run(interactor->window);
}

FVizResult fviz_render_window_interactor_render(FVizRenderWindowInteractor* interactor)
{
    if (interactor == NULL || interactor->window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "interactor is not attached to a render window");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (interactor->render_enabled == FVIZ_FALSE) return FVIZ_OK;
    return fviz_render_window_render(interactor->window);
}
