#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizRendererWidget.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRendererWidgetPrivate.h>

static void fviz_renderer_widget_destroy(FVizObject* object)
{
    FVizRendererWidget* widget = (FVizRendererWidget*)object;
    fviz_release(widget->window);
    widget->window = NULL;
}

static const FVizObjectClass g_fviz_renderer_widget_class = {
    FVIZ_TYPE_RENDERER_WIDGET,
    "FVizRendererWidget",
    &g_fviz_object_class,
    fviz_renderer_widget_destroy
};

FVizResult fviz_renderer_widget_create(
    int width,
    int height,
    const char* title,
    FVizRendererWidget** out_widget)
{
    FVizRendererWidget* widget;
    if (out_widget == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_widget must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_widget = NULL;
    widget = (FVizRendererWidget*)fviz_internal_object_allocate(
        sizeof(FVizRendererWidget), &g_fviz_renderer_widget_class, NULL);
    if (widget == NULL) return fviz_last_error_code();
    if (fviz_render_window_create(width, height, title, &widget->window) != FVIZ_OK)
    {
        fviz_release(widget);
        return fviz_last_error_code();
    }
    *out_widget = widget;
    return FVIZ_OK;
}

FVizRenderWindow* fviz_renderer_widget_window(FVizRendererWidget* widget)
{
    return widget != NULL ? widget->window : NULL;
}

FVizRenderer* fviz_renderer_widget_renderer(FVizRendererWidget* widget)
{
    return widget != NULL ? fviz_render_window_renderer(widget->window) : NULL;
}

FVizRenderWindowInteractor* fviz_renderer_widget_interactor(FVizRendererWidget* widget)
{
    return widget != NULL ? fviz_render_window_interactor(widget->window) : NULL;
}

FVizResult fviz_renderer_widget_set_interactor_style(
    FVizRendererWidget* widget,
    FVizInteractorStyle* style)
{
    if (widget == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "widget must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_render_window_interactor_set_style(fviz_renderer_widget_interactor(widget), style);
}

FVizResult fviz_renderer_widget_add_observer(
    FVizRendererWidget* widget,
    FVizInteractionEventType event_type,
    int priority,
    FVizInteractorEventCallbackFn callback,
    void* user_data,
    FVizObserverId* out_observer_id)
{
    if (widget == NULL)
    {
        if (out_observer_id != NULL) *out_observer_id = FVIZ_OBSERVER_ID_INVALID;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "widget must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_render_window_interactor_add_observer(
        fviz_renderer_widget_interactor(widget),
        event_type,
        priority,
        callback,
        user_data,
        out_observer_id);
}

FVizResult fviz_renderer_widget_remove_observer(
    FVizRendererWidget* widget,
    FVizObserverId observer_id)
{
    if (widget == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "widget must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_render_window_interactor_remove_observer(
        fviz_renderer_widget_interactor(widget), observer_id);
}

FVizResult fviz_renderer_widget_add_actor(FVizRendererWidget* widget, FVizActor* actor)
{
    FVizRenderer* renderer;
    if (widget == NULL || actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "widget and actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    renderer = fviz_renderer_widget_renderer(widget);
    return fviz_scene_add_actor(fviz_renderer_scene(renderer), actor);
}

FVizResult fviz_renderer_widget_show(FVizRendererWidget* widget)
{
    if (widget == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "widget must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_render_window_show(widget->window);
}

FVizResult fviz_renderer_widget_render(FVizRendererWidget* widget)
{
    if (widget == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "widget must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_render_window_render(widget->window);
}

FVizResult fviz_renderer_widget_process_events(FVizRendererWidget* widget)
{
    if (widget == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "widget must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_render_window_process_events(widget->window);
}

FVizResult fviz_renderer_widget_start(FVizRendererWidget* widget)
{
    if (widget == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "widget must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_render_window_interactor_start(fviz_renderer_widget_interactor(widget));
}
