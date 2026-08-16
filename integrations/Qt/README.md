# FEAViz Qt integration

FEAViz keeps Qt outside the C17 core and provides two ownership models on Windows.
Both Qt 5 and Qt 6 are supported by the adapters.

## Native-child mode

`FVizQtWidget` (Qt Widgets) and `FVizQtWindow` (QtGui) host a FEAViz-owned
`WS_CHILD` window and WGL context. This mode is simple and robust for a primary FEA
viewport. FEAViz owns the OpenGL context; Qt owns the application event loop and host
lifetime.

Enable with:

```text
-DFVIZ_BUILD_QT_WIDGETS=ON
-DFVIZ_BUILD_QT_GUI=ON
```

Do not call `fviz_renderer_widget_start()` from Qt. Qt's `exec()` remains the only GUI
message loop. The adapters synchronize native client pixels for DPI, forward focus,
pump only FEAViz interactor timers, detect host-ID/reparent changes, and use coalesced
render requests.

## Qt-owned OpenGL mode

`FVizQtOpenGLWindow` (QtGui) and `FVizQtOpenGLWidget` (Qt Widgets) let Qt own the
OpenGL context and presentation/compositing lifecycle. They provide FEAViz with an
`FVizExternalOpenGLSurface` contract from `initializeGL()/resizeGL()/paintGL()`.
FEAViz renders to the toolkit-provided `defaultFramebufferObject()` rather than
assuming framebuffer zero.

This mode is preferred when Qt content must compose naturally with the viewport or when
application architecture requires toolkit-owned OpenGL resources. The QWidget adapter
is the FEAViz integration path closest to `QVTKOpenGLNativeWidget` semantics.

Qt context destruction is observed explicitly. Before an old context disappears, the
adapter releases FEAViz GPU objects only; when Qt initializes a replacement context,
FEAViz rebuilds GL resources while preserving the render window, renderer, scene,
camera and interactor object graph.

Examples:

- `examples/27_QtEmbed` - native-child `FVizQtWidget`
- `examples/28_QtGuiEmbed` - native-child `FVizQtWindow`
- `examples/29_QtExternalOpenGL` - Qt-owned `QOpenGLWindow`
- `examples/30_QtOpenGLWidget` - Qt-owned `QOpenGLWidget`
