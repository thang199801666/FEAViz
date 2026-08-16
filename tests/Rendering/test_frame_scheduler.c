#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizInteractionEvent event_at(
    FVizInteractionEventType type,
    FVizMouseButton button,
    int x,
    int y)
{
    FVizInteractionEvent event;
    (void)memset(&event, 0, sizeof(event));
    event.type = type;
    event.button = button;
    event.x = x;
    event.y = y;
    return event;
}

int main(void)
{
    FVizRenderWindow* window = NULL;
    FVizRenderWindowInteractor* interactor;
    FVizFrameSchedulerOptions options;
    FVizFrameSchedulerStatistics statistics;
    FVizRenderStatistics render_statistics;
    FVizInteractionEvent event;

    fviz_frame_scheduler_options_initialize(&options);
    CHECK(options.struct_size == sizeof(options));
    CHECK(options.interactive_quality == FVIZ_TRUE);
    CHECK(options.interactive_target_fps == 0.0);
    CHECK(options.still_target_fps == 0.0);

    CHECK(fviz_render_window_create_offscreen(128, 96, &window) == FVIZ_OK);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_reset_frame_scheduler_statistics(window);

    fviz_render_window_request_render_reason(window, FVIZ_RENDER_REQUEST_SCENE);
    fviz_render_window_request_render_reason(window, FVIZ_RENDER_REQUEST_CAMERA);
    CHECK(fviz_render_window_render_requested(window) == FVIZ_TRUE);
    CHECK(fviz_render_window_pending_render_reasons(window) ==
        (FVIZ_RENDER_REQUEST_SCENE | FVIZ_RENDER_REQUEST_CAMERA));
    fviz_render_window_get_frame_scheduler_statistics(window, &statistics);
    CHECK(statistics.request_count == 2u);
    CHECK(statistics.coalesced_request_count == 1u);
    CHECK(statistics.rendered_frame_count == 0u);
    CHECK(fviz_render_window_render_if_requested(window) == FVIZ_OK);
    fviz_render_window_get_frame_scheduler_statistics(window, &statistics);
    CHECK(statistics.rendered_frame_count == 1u);
    CHECK(statistics.last_frame_reasons ==
        (FVIZ_RENDER_REQUEST_SCENE | FVIZ_RENDER_REQUEST_CAMERA));
    CHECK(statistics.last_frame_quality == FVIZ_FRAME_QUALITY_STILL);
    CHECK(statistics.pending_reasons == FVIZ_RENDER_REQUEST_NONE);
    fviz_render_window_get_statistics(window, &render_statistics);
    CHECK(render_statistics.request_reasons == statistics.last_frame_reasons);
    CHECK(render_statistics.frame_quality == FVIZ_FRAME_QUALITY_STILL);

    interactor = fviz_render_window_interactor(window);
    CHECK(interactor != NULL);
    event = event_at(
        FVIZ_INTERACTION_MOUSE_BUTTON_DOWN,
        FVIZ_MOUSE_BUTTON_LEFT,
        32,
        32);
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_TRUE);
    CHECK(fviz_render_window_interaction_active(window) == FVIZ_TRUE);
    CHECK(fviz_render_window_frame_quality(window) == FVIZ_FRAME_QUALITY_INTERACTIVE);
    CHECK((fviz_render_window_pending_render_reasons(window) &
        FVIZ_RENDER_REQUEST_INTERACTION) != 0u);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &render_statistics);
    CHECK(render_statistics.frame_quality == FVIZ_FRAME_QUALITY_INTERACTIVE);

    event = event_at(
        FVIZ_INTERACTION_MOUSE_BUTTON_UP,
        FVIZ_MOUSE_BUTTON_LEFT,
        32,
        32);
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_TRUE);
    CHECK(fviz_render_window_interaction_active(window) == FVIZ_FALSE);
    CHECK(fviz_render_window_frame_quality(window) == FVIZ_FRAME_QUALITY_STILL);
    CHECK((fviz_render_window_pending_render_reasons(window) &
        FVIZ_RENDER_REQUEST_SCENE) != 0u);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    fviz_render_window_get_statistics(window, &render_statistics);
    CHECK(render_statistics.frame_quality == FVIZ_FRAME_QUALITY_STILL);

    fviz_render_window_get_frame_scheduler_options(window, &options);
    options.interactive_target_fps = 120.0;
    options.still_target_fps = 30.0;
    options.interactive_quality = FVIZ_FALSE;
    CHECK(fviz_render_window_set_frame_scheduler_options(window, &options) == FVIZ_OK);
    fviz_render_window_get_frame_scheduler_options(window, &options);
    CHECK(options.interactive_target_fps == 120.0);
    CHECK(options.still_target_fps == 30.0);
    CHECK(options.interactive_quality == FVIZ_FALSE);
    event = event_at(
        FVIZ_INTERACTION_MOUSE_BUTTON_DOWN,
        FVIZ_MOUSE_BUTTON_LEFT,
        32,
        32);
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_TRUE);
    CHECK(fviz_render_window_frame_quality(window) == FVIZ_FRAME_QUALITY_STILL);
    fviz_render_window_interactor_cancel_interaction(interactor);

    fviz_release(window);
    return 0;
}
