# FEAViz Win32 GUI Demo

A minimal desktop GUI demonstrating FEAViz embedded as a native Win32 child control.

The main Win32 application owns the window, toolbar buttons, layout and message loop.
FEAViz owns only the embedded 3D viewport.

## Features

- Native Win32 main window.
- FEAViz `FVizWin32RenderControl` embedded below a toolbar.
- Cube and sphere scene.
- Orbit/pan/zoom handled by the FEAViz interactor.
- Buttons outside the renderer for Reset Camera, Wireframe, Sphere visibility and Background.
- Viewport resizes with the parent window.

## Build from the full FEAViz tree

From the FEAViz project root on Windows with Visual Studio 2026 / v145:

```bat
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 ^
  -DFVIZ_BUILD_EXAMPLES=ON
cmake --build build --config Release --target FVizExampleWin32GuiDemo
```

The executable is emitted as:

```text
build/bin/FEAVizWin32GuiDemo.exe
```

Do not call `fviz_renderer_widget_start()` for an embedded viewport. The Win32
application message loop remains the owner of event dispatch.
