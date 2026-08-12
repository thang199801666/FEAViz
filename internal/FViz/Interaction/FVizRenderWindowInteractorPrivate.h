#ifndef FVIZ_INTERNAL_INTERACTION_RENDER_WINDOW_INTERACTOR_PRIVATE_H
#define FVIZ_INTERNAL_INTERACTION_RENDER_WINDOW_INTERACTOR_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Interaction/FVizRenderWindowInteractor.h>

typedef struct FVizInteractorObserver
{
    FVizObserverId id;
    FVizInteractionEventType event_type;
    int priority;
    uint64_t sequence;
    uint64_t activation_dispatch;
    FVizInteractorEventCallbackFn callback;
    void* user_data;
    FVizBool active;
} FVizInteractorObserver;

struct FVizRenderWindowInteractor
{
    FVizObject base;
    FVizRenderWindow* window;
    FVizInteractorStyle* style;
    FVizInteractorEventCallbackFn event_callback;
    void* event_user_data;
    FVizInteractorObserver* observers;
    FVizSize observer_count;
    FVizSize observer_capacity;
    FVizObserverId next_observer_id;
    uint64_t next_observer_sequence;
    uint64_t dispatch_serial;
    uint32_t dispatch_depth;
    FVizRenderer* poked_renderer;
    double desired_update_rate;
    double still_update_rate;
    FVizBool initialized;
    FVizBool enabled;
    FVizBool done;
    FVizBool render_enabled;
    FVizBool observers_need_compaction;
    FVizBool observers_need_sort;
};

FVizResult fviz_internal_render_window_interactor_create(
    FVizRenderWindow* window,
    FVizRenderWindowInteractor** out_interactor);
void fviz_internal_render_window_interactor_detach(FVizRenderWindowInteractor* interactor);

#endif /* FVIZ_INTERNAL_INTERACTION_RENDER_WINDOW_INTERACTOR_PRIVATE_H */
