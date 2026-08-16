# Object events and interaction observers

FEAViz 0.30+ promotes events from an interactor-only facility to a VTK-style observer
service on every `FVizObject`. Existing interactor observers remain source-compatible,
but new code can observe render windows, interactors, renderers, cameras, actors,
datasets, and future object types through one API.

## Generic object observers

```c
static FVizBool on_event(
    FVizObject* caller,
    FVizEventId event_id,
    void* call_data,
    void* client_data)
{
    AppState* app = (AppState*)client_data;
    if (event_id == FVIZ_EVENT_RENDER_END)
        app->frame_count++;

    /* FVIZ_TRUE aborts lower-priority observers for this invocation. */
    return FVIZ_FALSE;
}

FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
fviz_object_add_observer(
    (FVizObject*)render_window,
    FVIZ_EVENT_RENDER_END,
    10.0f,
    on_event,
    &app,
    &tag);

/* Later: */
fviz_object_remove_observer((FVizObject*)render_window, tag);
```

The object API also provides `remove_observers(event)`, `remove_all_observers()`,
`has_observer()`, `observer_count()`, and `invoke_event()`. `FVIZ_EVENT_ANY` observes all
object events. `FVIZ_EVENT_USER` is the first application-defined event ID.

### Reusable `FVizCommand` objects

For callback logic that is shared by several objects or event IDs, FEAViz 0.31 adds a
ref-counted command object analogous to `vtkCommand`:

```c
FVizCommand* command = NULL;
FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
fviz_command_create(on_command, app_state, &command);
fviz_object_add_command_observer(
    (FVizObject*)camera, FVIZ_EVENT_MODIFIED, 5.0f, command, &tag);

/* Registration owns a reference, so the application's reference may be dropped. */
fviz_release(command);
```

A command owns neither its client-data pointer nor event call data. The registration
retains the command itself. The callback may return `FVIZ_TRUE` or call
`fviz_command_set_abort_flag(command, FVIZ_TRUE)` to stop lower-priority observers. The
abort flag is reset before every command execution, matching one-invocation semantics.

`fviz_object_modified()` emits `FVIZ_EVENT_MODIFIED`. Final release emits
`FVIZ_EVENT_DELETE` immediately before class destruction. A DeleteEvent callback must
not attempt to retain/resurrect the caller; its reference count has already reached
zero.

## Interaction events

Native interaction is mapped to the same event ID namespace. For example
`FVIZ_INTERACTION_MOUSE_MOVE` maps to `FVIZ_EVENT_MOUSE_MOVE`. Register
`FVIZ_EVENT_INTERACTION_ANY` on an interactor to receive all concrete interaction
events while keeping ordinary object lifecycle events separate.

The `call_data` pointer for a generic interactor event is the current
`FVizInteractionEvent*`. It is borrowed and valid only for the callback invocation.

The compatibility dispatch chain is:

```text
native event
    |
legacy single callback
    |
legacy interactor observers (integer priority)
    |
generic FVizObject observers (float priority / observer tags)
    |
built-in host action
    |
active interactor style
```

Any callback that returns `FVIZ_TRUE` consumes the event and stops propagation to later
stages. For new code, the generic object observer API is preferred because it matches
the rest of the object model and can observe non-interactor objects.

The interactor also publishes VTK-style semantic lifecycle events around style state:
`StartInteractionEvent` when a drag/manipulation begins, `InteractionEvent` while it is
active, and `EndInteractionEvent` when it returns to the idle state. `EnableEvent` and
`DisableEvent` are emitted when an initialized interactor changes enabled state. These
are separate from the low-level mouse/key events and are useful for adaptive render
quality, undo grouping, linked views, and GUI status updates.

Escape is host-aware: in a standalone window it requests close; in either a native-child
embedded window or an external-OpenGL hosted surface it cancels the active interaction/capture and
leaves the GUI-owned viewport alive.

## Priority and mutation rules

- Higher priority runs first; equal priorities preserve registration order.
- Removing an observer during a callback deactivates it immediately, including a target
  that has not yet executed in the current dispatch.
- Adding an observer during dispatch is safe; it becomes eligible only after the
  outermost dispatch returns.
- Nested dispatch follows the same mutation boundary, avoiding listeners appearing
  halfway through an existing event stack.
- Returning `FVIZ_TRUE` aborts remaining matching lower-priority observers.
- Observer records own neither `client_data` nor `call_data`; callers control those
  lifetimes.
- Event dispatch is intended for the object's GUI/render thread unless an application
  provides its own cross-thread synchronization.

## Render-window lifecycle events

The current render-window event set includes `RenderStart`, `RenderEnd`, `WindowResize`,
`WindowClose`, `WindowFocusIn`, `WindowFocusOut`, `WindowDpiChanged`, and
`WindowReparented`. These allow GUI integration code to react without backend-specific
hooks. `RenderEnd` receives a borrowed pointer to the render result as call data;
`WindowDpiChanged` receives a borrowed `uint32_t*` to the effective DPI.
## Pipeline, pick, and redraw events

Algorithms publish `StartEvent` immediately before a real execution, then
`ProgressEvent` and `AbortCheckEvent` as progress is reported, followed by `EndEvent`
with a borrowed `FVizResult*`. Returning true from Progress/AbortCheck requests pipeline
abortion. Executive cache hits intentionally do not synthesize Start/End events.

CPU and hardware pick entry points publish `StartPickEvent`, `PickEvent`, and
`EndPickEvent` with a borrowed `FVizPickEventData*`. The payload records display
coordinates, association, hardware/CPU path, result, and the resolved hardware-pick or
ray-hit data. A StartPick observer may cancel the operation. Successful hardware picks
also publish `PickEvent` on the resolved renderer and actor.

`fviz_render_window_request_render()` publishes `RenderRequestedEvent` only when the
window transitions from no pending frame to one pending frame. Repeated requests before
the host paints are coalesced. `fviz_render_window_render_request_serial()` is useful for
diagnostics/tests; it is not a frame counter. A render request raised while a frame is
already executing is deferred rather than recursively entering the same GL context.

On the Win32 renderer backend, each renderer also receives `RenderStartEvent` and
`RenderEndEvent` around its own viewport/pass execution. Renderer `RenderStartEvent`
receives the owning render window as borrowed call data; renderer `RenderEndEvent`
receives a borrowed `FVizResult*`. This is useful for per-viewport profiling and overlays
in multi-renderer windows.
## Dependency-driven redraw propagation

FEAViz 0.32 extends `ModifiedEvent` into the retained render/pipeline graph so GUI code does
not need to issue a render after every setter. The principal propagation paths are:

```text
custom source/filter state
        |
        v
FVizAlgorithm <- direct input / upstream producer
        |
        v
FVizMapper <- PolyData / LookupTable
        |
        v
FVizActor <- Transform / GlyphMapper
        |
        v
FVizScene
        |
        v
FVizRenderer <- Camera / Light / ScalarLegend / RenderPass / text & label overlays
        |
        v
FVizRenderWindow::RequestRender (coalesced)
```

Text actors also observe their `FVizTextProperty`; scalar legends observe their lookup table and
title/label text properties. When a retained dependency is replaced or removed, FEAViz removes the
corresponding observer tag before releasing it. This is both a lifetime rule and a correctness rule:
a detached object must never schedule a later ghost redraw on its former viewport.

MTime remains the cache-validity mechanism; observers are the scheduling/notification mechanism.
Keeping both avoids eager pipeline execution while still making interactive GUIs responsive to deep
object changes.

