# Simple 3D Model

This example constructs a cube entirely through the public FEAViz C API and opens an interactive scene.

## Build

From an x64 Developer Command Prompt on Windows:

```text
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

Run:

```text
out\build\windows-msvc-release\bin\FEAVizSimpleModel.exe
```

Controls:

- Left mouse drag: orbit
- Middle mouse drag: pan
- Mouse wheel: zoom
- `F`: fit model
- `W`: toggle wireframe
- `Esc`: close
