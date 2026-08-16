#ifndef FVIZ_INTERACTION_WIDGET_H
#define FVIZ_INTERACTION_WIDGET_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Interaction/FVizEvent.h>
#include <FViz/Interaction/FVizRenderWindowInteractor.h>
#include <FViz/Interaction/FVizWidgetRepresentation.h>
#include <FViz/Rendering/FVizRenderer.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizWidget FVizWidget;
#define FVIZ_TYPE_WIDGET UINT64_C(0x4A92E1B7C35D608F)

typedef enum FVizWidgetState
{
    FVIZ_WIDGET_STATE_DISABLED = 0,
    FVIZ_WIDGET_STATE_START = 1,
    FVIZ_WIDGET_STATE_HOVER = 2,
    FVIZ_WIDGET_STATE_ACTIVE = 3
} FVizWidgetState;

typedef enum FVizWidgetNotification
{
    FVIZ_WIDGET_ENABLED = 0,
    FVIZ_WIDGET_DISABLED = 1,
    FVIZ_WIDGET_START_INTERACTION = 2,
    FVIZ_WIDGET_INTERACTION = 3,
    FVIZ_WIDGET_END_INTERACTION = 4,
    FVIZ_WIDGET_VALUE_CHANGED = 5,
    FVIZ_WIDGET_CANCELLED = 6
} FVizWidgetNotification;

typedef FVizBool (*FVizWidgetEventHandlerFn)(FVizWidget* widget, const FVizInteractionEvent* event, void* user_data);

typedef void (*FVizWidgetCallbackFn)(FVizWidget* widget, FVizWidgetNotification notification, void* user_data);

FVIZ_INTERACTION_API FVizResult fviz_widget_create(FVizRenderWindowInteractor* interactor, FVizRenderer* renderer,
                                       FVizWidgetRepresentation* representation, FVizWidget** out_widget);
FVIZ_INTERACTION_API FVizRenderWindowInteractor* fviz_widget_interactor(FVizWidget* widget);
FVIZ_INTERACTION_API FVizResult fviz_widget_set_interactor(FVizWidget* widget, FVizRenderWindowInteractor* interactor);
FVIZ_INTERACTION_API FVizRenderer* fviz_widget_renderer(FVizWidget* widget);
FVIZ_INTERACTION_API FVizWidgetRepresentation* fviz_widget_representation(FVizWidget* widget);
FVIZ_INTERACTION_API FVizResult fviz_widget_set_representation(FVizWidget* widget, FVizWidgetRepresentation* representation);
FVIZ_INTERACTION_API FVizResult fviz_widget_set_enabled(FVizWidget* widget, FVizBool enabled);
FVIZ_INTERACTION_API FVizBool fviz_widget_enabled(const FVizWidget* widget);
FVIZ_INTERACTION_API void fviz_widget_set_process_events(FVizWidget* widget, FVizBool process_events);
FVIZ_INTERACTION_API FVizBool fviz_widget_process_events(const FVizWidget* widget);
FVIZ_INTERACTION_API FVizResult fviz_widget_set_priority(FVizWidget* widget, int priority);
FVIZ_INTERACTION_API int fviz_widget_priority(const FVizWidget* widget);
FVIZ_INTERACTION_API FVizWidgetState fviz_widget_state(const FVizWidget* widget);
FVIZ_INTERACTION_API void fviz_widget_set_event_handler(FVizWidget* widget, FVizWidgetEventHandlerFn handler, void* user_data);
FVIZ_INTERACTION_API void fviz_widget_set_callback(FVizWidget* widget, FVizWidgetCallbackFn callback, void* user_data);
FVIZ_INTERACTION_API FVizBool fviz_widget_process_event(FVizWidget* widget, const FVizInteractionEvent* event);
FVIZ_INTERACTION_API void fviz_widget_begin_interaction(FVizWidget* widget);
FVIZ_INTERACTION_API void fviz_widget_interaction(FVizWidget* widget);
FVIZ_INTERACTION_API void fviz_widget_value_changed(FVizWidget* widget);
FVIZ_INTERACTION_API void fviz_widget_end_interaction(FVizWidget* widget);
FVIZ_INTERACTION_API void fviz_widget_cancel_interaction(FVizWidget* widget);
FVIZ_INTERACTION_API FVizResult fviz_widget_request_render(FVizWidget* widget);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_WIDGET_H */
