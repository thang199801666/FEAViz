#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

typedef struct ObserverProbe
{
    int* order;
    int* order_count;
    int value;
    FVizBool consume;
    FVizObserverId remove_id;
    struct ObserverProbe* add_probe;
    FVizObserverId* added_id;
} ObserverProbe;

typedef struct ReentrantProbe
{
    int* order;
    int* order_count;
    FVizBool nested;
    ObserverProbe* added_probe;
    FVizObserverId* added_id;
} ReentrantProbe;

static FVizBool consume_event(
    FVizRenderWindowInteractor* interactor,
    const FVizInteractionEvent* event,
    void* user_data)
{
    int* count = (int*)user_data;
    (void)interactor;
    ++(*count);
    return event->key == 'X' ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool observe_event(
    FVizRenderWindowInteractor* interactor,
    const FVizInteractionEvent* event,
    void* user_data)
{
    ObserverProbe* probe = (ObserverProbe*)user_data;
    (void)event;
    probe->order[(*probe->order_count)++] = probe->value;
    if (probe->remove_id != FVIZ_OBSERVER_ID_INVALID)
    {
        (void)fviz_render_window_interactor_remove_observer(interactor, probe->remove_id);
        probe->remove_id = FVIZ_OBSERVER_ID_INVALID;
    }
    if (probe->add_probe != NULL && probe->added_id != NULL &&
        *probe->added_id == FVIZ_OBSERVER_ID_INVALID)
    {
        (void)fviz_render_window_interactor_add_observer(
            interactor,
            FVIZ_INTERACTION_KEY_DOWN,
            100,
            observe_event,
            probe->add_probe,
            probe->added_id);
    }
    return probe->consume;
}

static FVizBool observe_reentrant(
    FVizRenderWindowInteractor* interactor,
    const FVizInteractionEvent* event,
    void* user_data)
{
    ReentrantProbe* probe = (ReentrantProbe*)user_data;
    probe->order[(*probe->order_count)++] = 8;
    if (probe->nested == FVIZ_FALSE)
    {
        probe->nested = FVIZ_TRUE;
        (void)fviz_render_window_interactor_add_observer(
            interactor,
            FVIZ_INTERACTION_KEY_DOWN,
            100,
            observe_event,
            probe->added_probe,
            probe->added_id);
        (void)fviz_render_window_interactor_process_event(interactor, event);
    }
    return FVIZ_FALSE;
}

int main(void)
{
    FVizRendererWidget* widget = NULL;
    FVizInteractorStyle* style = NULL;
    FVizActor* actor = NULL;
    FVizPolyData* selection_data = NULL;
    FVizSelection* selection = NULL;
    FVizRenderer* second_renderer = NULL;
    FVizRenderer* overlay_renderer = NULL;
    FVizRenderWindow* window;
    FVizRenderWindowInteractor* interactor;
    FVizInteractionEvent event = {0};
    FVizObserverId low_id = FVIZ_OBSERVER_ID_INVALID;
    FVizObserverId high_id = FVIZ_OBSERVER_ID_INVALID;
    FVizObserverId consume_id = FVIZ_OBSERVER_ID_INVALID;
    FVizObserverId remover_id = FVIZ_OBSERVER_ID_INVALID;
    FVizObserverId removed_id = FVIZ_OBSERVER_ID_INVALID;
    FVizObserverId adder_id = FVIZ_OBSERVER_ID_INVALID;
    FVizObserverId added_id = FVIZ_OBSERVER_ID_INVALID;
    FVizObserverId reentrant_id = FVIZ_OBSERVER_ID_INVALID;
    FVizObserverId nested_added_id = FVIZ_OBSERVER_ID_INVALID;
    int order[16] = {0};
    int order_count = 0;
    ObserverProbe low = {order, &order_count, 1, FVIZ_FALSE, FVIZ_OBSERVER_ID_INVALID, NULL, NULL};
    ObserverProbe high = {order, &order_count, 2, FVIZ_FALSE, FVIZ_OBSERVER_ID_INVALID, NULL, NULL};
    ObserverProbe consume = {order, &order_count, 3, FVIZ_TRUE, FVIZ_OBSERVER_ID_INVALID, NULL, NULL};
    ObserverProbe removed = {order, &order_count, 4, FVIZ_FALSE, FVIZ_OBSERVER_ID_INVALID, NULL, NULL};
    ObserverProbe remover = {order, &order_count, 5, FVIZ_FALSE, FVIZ_OBSERVER_ID_INVALID, NULL, NULL};
    ObserverProbe added = {order, &order_count, 7, FVIZ_FALSE, FVIZ_OBSERVER_ID_INVALID, NULL, NULL};
    ObserverProbe adder = {order, &order_count, 6, FVIZ_FALSE, FVIZ_OBSERVER_ID_INVALID, &added, &added_id};
    ObserverProbe nested_added = {order, &order_count, 9, FVIZ_FALSE, FVIZ_OBSERVER_ID_INVALID, NULL, NULL};
    ReentrantProbe reentrant = {order, &order_count, FVIZ_FALSE, &nested_added, &nested_added_id};
    int callback_count = 0;
    if (fviz_render_window_supported() == FVIZ_FALSE) return 0;
    CHECK(fviz_renderer_widget_create(64, 64, "FEAViz widget test", &widget) == FVIZ_OK);
    CHECK(widget != NULL);
    CHECK(fviz_object_type_id((const FVizObject*)widget) == FVIZ_TYPE_RENDERER_WIDGET);
    CHECK(fviz_renderer_widget_window(widget) != NULL);
    CHECK(fviz_renderer_widget_renderer(widget) != NULL);
    window = fviz_renderer_widget_window(widget);
    CHECK(fviz_render_window_renderer_count(window) == 1u);
    CHECK(fviz_render_window_set_renderer(
        window, fviz_render_window_renderer(window)) == FVIZ_OK);
    CHECK(fviz_render_window_renderer_count(window) == 1u);
    CHECK(fviz_render_window_renderer(window) == fviz_renderer_widget_renderer(widget));
    CHECK(fviz_renderer_set_viewport(
        fviz_renderer_widget_renderer(widget), 0.0f, 0.0f, 0.5f, 1.0f) == FVIZ_OK);
    CHECK(fviz_renderer_create(&second_renderer) == FVIZ_OK);
    CHECK(fviz_renderer_set_viewport(second_renderer, 0.5f, 0.0f, 1.0f, 1.0f) == FVIZ_OK);
    CHECK(fviz_render_window_add_renderer(window, second_renderer) == FVIZ_OK);
    CHECK(fviz_render_window_add_renderer(window, second_renderer) == FVIZ_OK);
    CHECK(fviz_render_window_renderer_count(window) == 2u);
    CHECK(fviz_render_window_find_renderer(window, 8, 32) == fviz_renderer_widget_renderer(widget));
    CHECK(fviz_render_window_find_renderer(window, 56, 32) == second_renderer);
    CHECK(fviz_renderer_create(&overlay_renderer) == FVIZ_OK);
    fviz_renderer_set_layer(overlay_renderer, 1);
    CHECK(fviz_render_window_add_renderer(window, overlay_renderer) == FVIZ_OK);
    CHECK(fviz_render_window_find_renderer(window, 56, 32) == overlay_renderer);
    fviz_renderer_set_interactive(overlay_renderer, FVIZ_FALSE);
    CHECK(fviz_render_window_find_renderer(window, 56, 32) == second_renderer);
    CHECK(fviz_render_window_remove_renderer(window, overlay_renderer) == FVIZ_OK);
    CHECK(fviz_render_window_remove_renderer(window, overlay_renderer) == FVIZ_ERROR_NOT_FOUND);
    CHECK(fviz_render_window_renderer_count(window) == 2u);
    interactor = fviz_renderer_widget_interactor(widget);
    CHECK(interactor != NULL);
    CHECK(fviz_object_type_id((const FVizObject*)interactor) == FVIZ_TYPE_RENDER_WINDOW_INTERACTOR);
    CHECK(fviz_render_window_interactor_window(interactor) == fviz_renderer_widget_window(widget));
    CHECK(fviz_render_window_interactor_style(interactor) != NULL);
    CHECK(fviz_render_window_interactor_enabled(interactor) == FVIZ_TRUE);
    fviz_render_window_interactor_disable(interactor);
    CHECK(fviz_render_window_interactor_enabled(interactor) == FVIZ_FALSE);
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_FALSE);
    fviz_render_window_interactor_enable(interactor);
    CHECK(fviz_render_window_interactor_enabled(interactor) == FVIZ_TRUE);
    fviz_render_window_interactor_set_render_enabled(interactor, FVIZ_FALSE);
    CHECK(fviz_render_window_interactor_render_enabled(interactor) == FVIZ_FALSE);
    CHECK(fviz_render_window_interactor_render(interactor) == FVIZ_OK);
    fviz_render_window_interactor_set_render_enabled(interactor, FVIZ_TRUE);
    fviz_render_window_interactor_set_update_rates(interactor, 60.0, 0.5);
    {
        double desired = 0.0;
        double still = 0.0;
        fviz_render_window_interactor_get_update_rates(interactor, &desired, &still);
        CHECK(desired == 60.0 && still == 0.5);
    }
    CHECK(fviz_interactor_style_trackball_camera_create(&style) == FVIZ_OK);
    CHECK(fviz_renderer_widget_set_interactor_style(widget, style) == FVIZ_OK);
    CHECK(fviz_render_window_interactor_style(interactor) == style);
    fviz_render_window_interactor_set_event_callback(interactor, consume_event, &callback_count);
    event.type = FVIZ_INTERACTION_KEY_DOWN;
    event.key = 'X';
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_TRUE);
    CHECK(callback_count == 1);
    fviz_render_window_interactor_set_event_callback(interactor, NULL, NULL);

    CHECK(fviz_renderer_widget_add_observer(
        widget, FVIZ_INTERACTION_EVENT_ANY, -10, observe_event, &low, &low_id) == FVIZ_OK);
    CHECK(fviz_renderer_widget_add_observer(
        widget, FVIZ_INTERACTION_KEY_DOWN, 10, observe_event, &high, &high_id) == FVIZ_OK);
    CHECK(fviz_renderer_widget_add_observer(
        widget, FVIZ_INTERACTION_KEY_DOWN, 5, observe_event, &consume, &consume_id) == FVIZ_OK);
    CHECK(fviz_render_window_interactor_observer_count(interactor) == 3u);
    CHECK(fviz_renderer_widget_add_observer(
        widget, (FVizInteractionEventType)99, 0, observe_event, &low, &removed_id) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(removed_id == FVIZ_OBSERVER_ID_INVALID);
    event.type = FVIZ_INTERACTION_MOUSE_MOVE;
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_FALSE);
    CHECK(order_count == 1 && order[0] == 1);
    order_count = 0;
    event.type = FVIZ_INTERACTION_KEY_DOWN;
    event.key = 'Y';
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_TRUE);
    CHECK(order_count == 2 && order[0] == 2 && order[1] == 3);
    CHECK(fviz_renderer_widget_remove_observer(widget, consume_id) == FVIZ_OK);
    CHECK(fviz_renderer_widget_remove_observer(widget, consume_id) == FVIZ_ERROR_NOT_FOUND);
    order_count = 0;
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_FALSE);
    CHECK(order_count == 2 && order[0] == 2 && order[1] == 1);

    CHECK(fviz_renderer_widget_add_observer(
        widget, FVIZ_INTERACTION_KEY_DOWN, -20, observe_event, &removed, &removed_id) == FVIZ_OK);
    remover.remove_id = removed_id;
    CHECK(fviz_renderer_widget_add_observer(
        widget, FVIZ_INTERACTION_KEY_DOWN, 20, observe_event, &remover, &remover_id) == FVIZ_OK);
    order_count = 0;
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_FALSE);
    CHECK(order_count == 3 && order[0] == 5 && order[1] == 2 && order[2] == 1);

    CHECK(fviz_renderer_widget_add_observer(
        widget, FVIZ_INTERACTION_KEY_DOWN, 30, observe_event, &adder, &adder_id) == FVIZ_OK);
    order_count = 0;
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_FALSE);
    CHECK(added_id != FVIZ_OBSERVER_ID_INVALID);
    CHECK(order_count == 4 && order[0] == 6 && order[1] == 5 && order[2] == 2 && order[3] == 1);
    order_count = 0;
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_FALSE);
    CHECK(order_count == 5 && order[0] == 7 && order[1] == 6 && order[2] == 5 && order[3] == 2 && order[4] == 1);
    fviz_render_window_interactor_remove_all_observers(interactor);
    CHECK(fviz_render_window_interactor_observer_count(interactor) == 0u);
    CHECK(fviz_renderer_widget_add_observer(
        widget, FVIZ_INTERACTION_KEY_DOWN, 0, observe_reentrant, &reentrant, &reentrant_id) == FVIZ_OK);
    order_count = 0;
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_FALSE);
    CHECK(order_count == 2 && order[0] == 8 && order[1] == 8);
    CHECK(nested_added_id != FVIZ_OBSERVER_ID_INVALID);
    order_count = 0;
    CHECK(fviz_render_window_interactor_process_event(interactor, &event) == FVIZ_FALSE);
    CHECK(order_count == 2 && order[0] == 9 && order[1] == 8);
    fviz_render_window_interactor_remove_all_observers(interactor);

    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    CHECK(fviz_poly_data_create(&selection_data) == FVIZ_OK);
    {
        uint32_t a;
        uint32_t b;
        uint32_t c;
        CHECK(fviz_poly_data_add_point(selection_data, fviz_vec3(-1.0f, -1.0f, 0.0f), &a) == FVIZ_OK);
        CHECK(fviz_poly_data_add_point(selection_data, fviz_vec3(1.0f, -1.0f, 0.0f), &b) == FVIZ_OK);
        CHECK(fviz_poly_data_add_point(selection_data, fviz_vec3(0.0f, 1.0f, 0.0f), &c) == FVIZ_OK);
        CHECK(fviz_poly_data_add_triangle(selection_data, a, b, c) == FVIZ_OK);
    }
    CHECK(fviz_actor_set_poly_data(actor, selection_data) == FVIZ_OK);
    CHECK(fviz_renderer_widget_add_actor(widget, actor) == FVIZ_OK);
    CHECK(fviz_scene_actor_count(fviz_renderer_scene(fviz_renderer_widget_renderer(widget))) == 1u);
    fviz_renderer_fit_camera(fviz_renderer_widget_renderer(widget), 1.2f);
    CHECK(fviz_render_window_select_rectangle(window, 0, 0, 31, 63, &selection) == FVIZ_OK);
    CHECK(selection != NULL && fviz_selection_count(selection) == 1u);
    CHECK(fviz_selection_actor(selection, 0u) == actor);
    CHECK(fviz_selection_association(selection, 0u) == FVIZ_SELECTION_CELL);
    CHECK(fviz_selection_id(selection, 0u) == 0u);
    CHECK(fviz_selection_add(selection, actor, FVIZ_SELECTION_POINT, 2u) == FVIZ_OK);
    CHECK(fviz_selection_count(selection) == 2u);
    fviz_selection_clear(selection);
    CHECK(fviz_selection_count(selection) == 0u);
    fviz_release(actor);
    fviz_release(selection);
    fviz_release(selection_data);
    fviz_release(overlay_renderer);
    fviz_release(second_renderer);
    fviz_release(style);
    fviz_release(widget);
    return 0;
}
