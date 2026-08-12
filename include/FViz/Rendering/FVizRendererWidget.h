#ifndef FVIZ_RENDERING_RENDERER_WIDGET_H
#define FVIZ_RENDERING_RENDERER_WIDGET_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Interaction/FVizInteractorStyle.h>
#include <FViz/Interaction/FVizRenderWindowInteractor.h>
#include <FViz/Rendering/FVizActor.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizRenderWindow.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRendererWidget FVizRendererWidget;
#define FVIZ_TYPE_RENDERER_WIDGET UINT64_C(0x94D2B7E136CA580F)

FVIZ_API FVizResult fviz_renderer_widget_create(
    int width,
    int height,
    const char* title,
    FVizRendererWidget** out_widget);
FVIZ_API FVizRenderWindow* fviz_renderer_widget_window(FVizRendererWidget* widget);
FVIZ_API FVizRenderer* fviz_renderer_widget_renderer(FVizRendererWidget* widget);
FVIZ_API FVizRenderWindowInteractor* fviz_renderer_widget_interactor(FVizRendererWidget* widget);
FVIZ_API FVizResult fviz_renderer_widget_set_interactor_style(
    FVizRendererWidget* widget,
    FVizInteractorStyle* style);
FVIZ_API FVizResult fviz_renderer_widget_add_observer(
    FVizRendererWidget* widget,
    FVizInteractionEventType event_type,
    int priority,
    FVizInteractorEventCallbackFn callback,
    void* user_data,
    FVizObserverId* out_observer_id);
FVIZ_API FVizResult fviz_renderer_widget_remove_observer(
    FVizRendererWidget* widget,
    FVizObserverId observer_id);
FVIZ_API FVizResult fviz_renderer_widget_add_actor(FVizRendererWidget* widget, FVizActor* actor);
FVIZ_API FVizResult fviz_renderer_widget_show(FVizRendererWidget* widget);
FVIZ_API FVizResult fviz_renderer_widget_render(FVizRendererWidget* widget);
FVIZ_API FVizResult fviz_renderer_widget_process_events(FVizRendererWidget* widget);
FVIZ_API FVizResult fviz_renderer_widget_start(FVizRendererWidget* widget);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_RENDERER_WIDGET_H */
