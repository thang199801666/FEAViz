# Interaction observers

FEAViz 0.7.0 extends the render-window interactor from one application callback to a VTK-style observer list.

```text
native event
    |
legacy callback
    |
observers (priority high -> low)
    |
built-in window action
    |
active interactor style
```

Any stage that returns `FVIZ_TRUE` consumes the event and stops propagation. The legacy callback remains first for source compatibility. Escape-to-close is the current built-in window action.

## Registering an observer

```c
FVizObserverId observer_id = FVIZ_OBSERVER_ID_INVALID;

fviz_renderer_widget_add_observer(
    widget,
    FVIZ_INTERACTION_MOUSE_BUTTON_DOWN,
    100,
    on_mouse_down,
    application_state,
    &observer_id);

/* Later: */
fviz_renderer_widget_remove_observer(widget, observer_id);
```

Use `FVIZ_INTERACTION_EVENT_ANY` to receive every interaction event. Larger priority values run first. Equal priorities preserve registration order.

## Dispatch and mutation rules

- Removing an observer from a callback deactivates it immediately, including observers that have not yet run in the current dispatch.
- Adding an observer from a callback is safe, but the new observer starts on the next outermost dispatch.
- Nested event dispatch uses the same mutation boundary, so new listeners never appear halfway through an existing event stack.
- Inactive records are compacted and the priority order is restored when the outermost dispatch finishes.
- The observer list owns callback records, not `user_data`; applications must keep user data alive until the observer is removed or the interactor is released.

Interaction event dispatch is intended for the render window's UI thread. Cross-thread registration or event dispatch requires application-level synchronization.
