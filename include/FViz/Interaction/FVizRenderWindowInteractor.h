#ifndef FVIZ_INTERACTION_RENDER_WINDOW_INTERACTOR_H
#define FVIZ_INTERACTION_RENDER_WINDOW_INTERACTOR_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Interaction/FVizEvent.h>
#include <FViz/Interaction/FVizInteractorStyle.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRenderWindow FVizRenderWindow;
typedef struct FVizRenderWindowInteractor FVizRenderWindowInteractor;
typedef FVizId FVizObserverId;
typedef FVizId FVizTimerId;
#define FVIZ_TYPE_RENDER_WINDOW_INTERACTOR UINT64_C(0xC43E17A962B850DF)
#define FVIZ_OBSERVER_ID_INVALID UINT64_C(0)
#define FVIZ_TIMER_ID_INVALID UINT64_C(0)

typedef FVizBool (*FVizInteractorEventCallbackFn)(
    FVizRenderWindowInteractor* interactor,
    const FVizInteractionEvent* event,
    void* user_data);

FVIZ_API FVizRenderWindow* fviz_render_window_interactor_window(FVizRenderWindowInteractor* interactor);
FVIZ_API FVizResult fviz_render_window_interactor_initialize(FVizRenderWindowInteractor* interactor);
FVIZ_API void fviz_render_window_interactor_enable(FVizRenderWindowInteractor* interactor);
FVIZ_API void fviz_render_window_interactor_disable(FVizRenderWindowInteractor* interactor);
FVIZ_API FVizBool fviz_render_window_interactor_enabled(const FVizRenderWindowInteractor* interactor);
FVIZ_API void fviz_render_window_interactor_set_done(FVizRenderWindowInteractor* interactor, FVizBool done);
FVIZ_API FVizBool fviz_render_window_interactor_done(const FVizRenderWindowInteractor* interactor);
FVIZ_API void fviz_render_window_interactor_set_render_enabled(
    FVizRenderWindowInteractor* interactor,
    FVizBool enabled);
FVIZ_API FVizBool fviz_render_window_interactor_render_enabled(
    const FVizRenderWindowInteractor* interactor);
FVIZ_API void fviz_render_window_interactor_set_update_rates(
    FVizRenderWindowInteractor* interactor,
    double desired,
    double still);
FVIZ_API void fviz_render_window_interactor_get_update_rates(
    const FVizRenderWindowInteractor* interactor,
    double* desired,
    double* still);
FVIZ_API FVizRenderer* fviz_render_window_interactor_poked_renderer(
    FVizRenderWindowInteractor* interactor);
FVIZ_API FVizRenderer* fviz_render_window_interactor_captured_renderer(
    FVizRenderWindowInteractor* interactor);
FVIZ_API void fviz_render_window_interactor_grab_focus(FVizRenderWindowInteractor* interactor);
FVIZ_API void fviz_render_window_interactor_release_focus(FVizRenderWindowInteractor* interactor);
FVIZ_API void fviz_render_window_interactor_cancel_interaction(FVizRenderWindowInteractor* interactor);
FVIZ_API FVizBool fviz_render_window_interactor_has_focus(
    const FVizRenderWindowInteractor* interactor);
FVIZ_API FVizResult fviz_render_window_interactor_create_timer(
    FVizRenderWindowInteractor* interactor,
    double interval_seconds,
    FVizBool repeating,
    double now_seconds,
    FVizTimerId* out_timer_id);
FVIZ_API FVizResult fviz_render_window_interactor_reset_timer(
    FVizRenderWindowInteractor* interactor,
    FVizTimerId timer_id,
    double now_seconds);
FVIZ_API FVizResult fviz_render_window_interactor_destroy_timer(
    FVizRenderWindowInteractor* interactor,
    FVizTimerId timer_id);
FVIZ_API FVizSize fviz_render_window_interactor_process_timers(
    FVizRenderWindowInteractor* interactor,
    double now_seconds);
FVIZ_API FVizResult fviz_render_window_interactor_set_style(
    FVizRenderWindowInteractor* interactor,
    FVizInteractorStyle* style);
FVIZ_API FVizInteractorStyle* fviz_render_window_interactor_style(FVizRenderWindowInteractor* interactor);
FVIZ_API void fviz_render_window_interactor_set_event_callback(
    FVizRenderWindowInteractor* interactor,
    FVizInteractorEventCallbackFn callback,
    void* user_data);
FVIZ_API FVizResult fviz_render_window_interactor_add_observer(
    FVizRenderWindowInteractor* interactor,
    FVizInteractionEventType event_type,
    int priority,
    FVizInteractorEventCallbackFn callback,
    void* user_data,
    FVizObserverId* out_observer_id);
FVIZ_API FVizResult fviz_render_window_interactor_remove_observer(
    FVizRenderWindowInteractor* interactor,
    FVizObserverId observer_id);
FVIZ_API void fviz_render_window_interactor_remove_all_observers(
    FVizRenderWindowInteractor* interactor);
FVIZ_API FVizSize fviz_render_window_interactor_observer_count(
    const FVizRenderWindowInteractor* interactor);
FVIZ_API FVizBool fviz_render_window_interactor_process_event(
    FVizRenderWindowInteractor* interactor,
    const FVizInteractionEvent* event);
FVIZ_API FVizResult fviz_render_window_interactor_start(FVizRenderWindowInteractor* interactor);
FVIZ_API FVizResult fviz_render_window_interactor_render(FVizRenderWindowInteractor* interactor);
FVIZ_API FVizResult fviz_render_window_interactor_request_render(
    FVizRenderWindowInteractor* interactor);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_RENDER_WINDOW_INTERACTOR_H */
