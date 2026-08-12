# Renderer and host integration

Standalone applications may use `FVizRendererWidget` and `start()`. Embedded
applications create a child render window, call non-blocking `process_events`,
and retain ownership of their main loop. Offscreen users create a hidden render
window and use color/depth readback without showing native UI.

Render-window state transitions are Created → Initialized/Offscreen → Visible
→ Finalized. Initialize after Finalize recreates platform resources. Finalize is
idempotent. Child and offscreen destruction never posts `WM_QUIT` to the host.

All scene, renderer, interactor, and OpenGL work belongs on the render thread.
Background workers may prepare independent datasets and request cancellation,
then hand completed owned objects to the render thread under application-level
synchronization.
