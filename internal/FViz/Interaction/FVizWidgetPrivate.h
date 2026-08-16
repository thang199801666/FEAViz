#ifndef FVIZ_INTERNAL_INTERACTION_WIDGET_PRIVATE_H
#define FVIZ_INTERNAL_INTERACTION_WIDGET_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Interaction/FVizWidget.h>

struct FVizWidget
{
    FVizObject base;
    FVizRenderWindowInteractor* interactor;
    FVizRenderer* renderer;
    FVizWidgetRepresentation* representation;
    FVizObserverId observer_id;
    int priority;
    FVizWidgetState state;
    FVizBool enabled;
    FVizBool process_events;
    FVizWidgetEventHandlerFn event_handler;
    void* event_user_data;
    FVizWidgetCallbackFn callback;
    void* callback_user_data;
};

void fviz_internal_widget_notify(FVizWidget* widget, FVizWidgetNotification notification);

#endif /* FVIZ_INTERNAL_INTERACTION_WIDGET_PRIVATE_H */
