# Renderer and host integration

FEAViz supports standalone, embedded native-child, host-owned external OpenGL, and
offscreen render-window ownership. The renderer/pipeline API is shared across these
modes; only native/context ownership, message-loop policy, and host lifecycle differ.

## Standalone

Standalone applications may create `FVizRendererWidget` / `FVizRenderWindow` and call
`start()` / `run()`. FEAViz owns the top-level native window and the blocking Win32
message loop.

## Embedded native child

`fviz_renderer_widget_create_attached()` or
`fviz_renderer_widget_create_attached_with_options()` receives a host native handle. On
Windows that handle is an `HWND`; FEAViz owns a dedicated `WS_CHILD` WGL renderer under
the host while the application or GUI toolkit keeps ownership of the host HWND.

The GUI owns the message loop. `fviz_render_window_run()` and
`fviz_renderer_widget_start()` return `FVIZ_ERROR_INVALID_STATE` for attached windows.
Do not pump the whole thread queue from an embedded renderer. The Win32 backend filters
non-blocking processing to the FEAViz child HWND when explicit processing is requested.

`fviz_renderer_widget_sync_host_size()` queries the actual host client rectangle, so
sizing occurs in native pixels and remains correct under per-monitor DPI. Resize, focus,
DPI change, close, rendering, and reparent operations publish generic `FVizObject`
events described in `INTERACTION_OBSERVERS.md`.

An attached renderer can move to a replacement host without destroying its render
window/context:

```c
fviz_renderer_widget_reparent(view, new_host_hwnd);
fviz_renderer_widget_sync_host_size(view);
```

This is important for docking systems and toolkits such as Qt that may recreate native
window IDs while the logical widget survives. The Win32 backend updates `SetParent`,
child styles, DPI, and client size while FEAViz preserves renderer/interactor state. If
the toolkit destroyed the old host first (and Windows consequently destroyed the child
HWND), the same reparent API recreates only the native/WGL surface on the replacement
host; the FEAViz scene, renderer, camera, and interactor objects remain intact.

Escape never destroys an embedded child. It cancels current pointer/interactor capture;
window lifetime remains controlled by the host GUI.

## `FVizWin32RenderControl`

Raw Win32 applications can use the reusable FEAviz render control instead of repeating
host glue in every window procedure:

```c
void* control = NULL;
fviz_win32_render_control_create(
    parent_hwnd, 1001, 0, 0, width, height, NULL, &control);

FVizRendererWidget* view =
    fviz_win32_render_control_renderer_widget(control); /* borrowed */
```

The control owns its attached `FVizRendererWidget`, resizes it on `WM_SIZE`, forwards
focus to the native render child, propagates enabled/visible state, and by default pumps
interactor timers at approximately 60 Hz through a host-owned `WM_TIMER`. The timer can
be disabled or configured with `fviz_win32_render_control_set_timer_pump()`.

GUI-driven changes should normally use `fviz_win32_render_control_request_render()` rather
than immediate `render()`. The core collapses repeated requests into one pending frame.
On Win32 the renderer posts a private request message and invalidates the child only when
that message is dispatched; this prevents redraws requested from inside `WM_PAINT`/
`RenderEndEvent` from being erased by the current `EndPaint()` validation.

Destroying the control destroys its renderer child; callers must not release the
borrowed renderer-widget pointer returned by the control.

## QtGui `QWindow` adapter

`integrations/Qt/FVizQtWindow` is the QtGui-only path for applications that use
`QGuiApplication` without Qt Widgets. Enable it with `FVIZ_BUILD_QT_GUI=ON`. The QWindow
is the native host; FEAViz owns the WGL child/context below it. Resize, expose, focus,
visibility, native-ID replacement, timer pumping, observer registration, and coalesced
render requests are handled by the adapter.

When a Widgets application wants QWindow ownership semantics, the same object can be
placed into a layout through `QWidget::createWindowContainer()`. This is intentionally a
native-child architecture, not a QOpenGLWindow/QOpenGLWidget shared-framebuffer path.

## Qt Widgets adapter

`integrations/Qt/FVizQtWidget` supports Qt 5 and Qt 6 Widgets on Windows while keeping
Qt out of the C17 core. It is a native QWidget host and deliberately has no Qt paint
engine/backing-store rendering for its viewport area; FEAViz paints the WGL child HWND.
Qt continues to own `QApplication::exec()`.

The adapter:

- synchronizes from the native HWND client rect rather than Qt logical dimensions;
- forwards keyboard focus to the render child;
- lazily pumps FEAViz interactor timers with a `QTimer` only while visible;
- detects `WinIdChange`, parent/show/window-state changes and reparents the existing
  FEAViz child when the native host changes;
- exposes convenience access to the render window, renderer, interactor, native handles,
  generic/function or `FVizCommand` observer registration, and coalesced render requests.

This native-child design is reliable for a primary FEA viewport and avoids requiring Qt
to own FEAViz's OpenGL context.

## Host-owned external OpenGL

`FVizExternalOpenGLSurface` is the toolkit-neutral contract for a host that owns the
OpenGL context/surface. The host supplies callbacks to make the context current, report
physical framebuffer dimensions, return the default framebuffer object, optionally
present, and schedule a GUI redraw. FEAViz never destroys the host context.

```c
FVizExternalOpenGLSurface surface;
fviz_external_opengl_surface_initialize(&surface);
surface.user_data = host;
surface.make_current = host_make_current;
surface.get_framebuffer_size = host_framebuffer_size;
surface.get_default_framebuffer = host_default_fbo;
surface.request_render = host_schedule_repaint;

fviz_renderer_widget_create_external_opengl(
    width_px, height_px, &surface, &view);
```

The modern GL path binds the host-provided framebuffer for opaque rendering, weighted
OIT composition, FXAA, readback and hardware picking. It therefore does not assume
framebuffer zero. `request_render` integrates the existing FEAViz coalescing flag with
the GUI toolkit's own paint scheduling.

Hosts that may recreate a context must call
`fviz_render_window_release_external_opengl_resources()` while the old context is still
valid, followed by `fviz_render_window_reinitialize_external_opengl()` after the new
context becomes current. This destroys/rebuilds only GPU-side objects; the FEAViz
render-window object graph remains intact.

### Qt-owned OpenGL adapters

`FVizQtOpenGLWindow` uses `QOpenGLWindow` and belongs to the `FEAViz::QtGui` target.
`FVizQtOpenGLWidget` uses `QOpenGLWidget` and belongs to `FEAViz::QtWidgets`. Both route
Qt mouse/wheel/keyboard/focus events into `FVizRenderWindowInteractor`, use physical
DPI-scaled coordinates, pump FEAViz timers, and translate FEAViz render requests to
Qt `update()` calls.

The QWidget external mode avoids a separate native child window and is therefore the
preferred integration when Qt widgets/overlays must compose with the viewport in the
same GUI hierarchy. The adapters observe Qt context destruction and rebuild FEAViz GL
resources on replacement contexts without replacing the scene/camera/interactor state.

## Offscreen

Offscreen users create a hidden render window and use color/depth readback without
showing native UI. Offscreen windows do not participate in a GUI message loop.

Render-window state transitions are Created -> Initialized/Offscreen -> Visible ->
Finalized. Initialize after Finalize recreates platform resources. Finalize is
idempotent. Child and offscreen destruction never posts `WM_QUIT` to the host.

All scene, renderer, interactor, and OpenGL work belongs on the render thread. Background
workers may prepare independent datasets and hand completed owned objects to the render
thread under application-level synchronization.
