# FEAViz

FEAViz is a from-scratch C17 visualization and FEA data-processing library. The public naming convention is:

- Public types: `FViz*`
- Public functions: `fviz_*`
- Public constants/macros: `FVIZ_*`

Version **0.1.0** is the first 3D-scene milestone. FEAViz can load triangle surface meshes from OBJ/STL, place them in an Actor/Scene/Renderer hierarchy, fit a camera, and display the scene through the native Windows OpenGL viewer backend.

## Current 0.1.0 milestone

Implemented:

- Phase 0 repository/CMake/public-private ABI foundation.
- Phase 1 allocator, object runtime, atomic reference counting, TLS error state, logging.
- Core container foundation: `FVizBuffer`, zero-copy external buffer wrapping, `FVizArray`, `FVizString`.
- Typed numeric `FVizDataArray` for future FEA fields.
- Math foundation: `FVizVec3`, `FVizMat4`, perspective/view matrices, `FVizBounds`.
- Triangle surface model: `FVizPolyData`, bounds, validation, smooth vertex normals.
- Mesh IO: OBJ and ASCII/binary STL readers.
- Scene model: `FVizActor`, `FVizScene`, `FVizRenderer`, `FVizCamera`.
- Windows native render window using Win32/WGL and OpenGL.
- Viewer interaction: orbit, pan, zoom, fit, wireframe toggle.
- CTest coverage for core/data/math/mesh/IO/scene.

The first renderer intentionally uses a compatibility OpenGL path so the initial scene milestone has no third-party graphics dependency. A modern VBO/shader backend is planned next without changing the public scene API.

## Windows build baseline

FEAViz does **not** require opening the Visual Studio IDE and does not use a Visual Studio CMake generator. The normal Windows build path is:

```text
CMake CLI
  -> NMake Makefiles
  -> nmake.exe
  -> MSVC cl.exe / link.exe from Visual Studio 2026
  -> bin/FEAViz.dll + bin/FEAVizViewer.exe
```

Requirements:

- CMake 3.24 or newer (CMake 3.30 is supported)
- Visual Studio 2026 Professional with **Desktop development with C++**
- MSVC Platform Toolset v145
- Windows SDK
- **x64 Developer Command Prompt for Visual Studio 2026**

No Ninja, GCC, MinGW, `.sln`, project-local `.bat`, or `.cmd` file is required for the normal Windows build.

Open **x64 Developer Command Prompt for Visual Studio 2026**, then:

```text
cd /d D:\Code\CPlusPlus\FEAViz
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

Debug:

```text
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

## Run the 3D viewer

Built-in cube:

```text
out\build\windows-msvc-release\bin\FEAVizViewer.exe
```

Load OBJ:

```text
out\build\windows-msvc-release\bin\FEAVizViewer.exe assets\testdata\cube.obj
```

Load STL:

```text
out\build\windows-msvc-release\bin\FEAVizViewer.exe D:\Models\part.stl
```

Controls:

```text
Left mouse drag    Orbit
Middle mouse drag  Pan
Mouse wheel         Zoom/dolly
F                   Fit scene
W                   Toggle wireframe
Esc                 Close
```

The build tree deliberately places `FEAViz.dll` and `FEAVizViewer.exe` together under `bin/`, so no manual DLL copying is required.

## Build without presets

From an x64 Developer Command Prompt:

```text
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DFVIZ_BUILD_SHARED=ON -DFVIZ_BUILD_TESTS=ON -DFVIZ_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Optional portable/CI build

The portable Ninja presets are retained for development and CI on Linux/macOS:

```sh
cmake --preset portable-ninja-debug
cmake --build --preset portable-ninja-debug
ctest --preset portable-ninja-debug
```

## Important CMake options

- `FVIZ_BUILD_SHARED=ON|OFF`
- `FVIZ_BUILD_TESTS=ON|OFF`
- `FVIZ_BUILD_EXAMPLES=ON|OFF`
- `FVIZ_WARNINGS_AS_ERRORS=ON|OFF`
- `FVIZ_ENABLE_ASAN=ON|OFF`
- `FVIZ_ENABLE_UBSAN=ON|OFF`
- `FVIZ_ENABLE_LTO=ON|OFF`
- `FVIZ_REQUIRE_MSVC_V145=ON|OFF`

See `docs/architecture/ROADMAP.md` and `docs/verification/PHASE8_3D_SCENE_MILESTONE.md` for implementation status and the next development steps.
## MSVC 19.50 / Visual Studio 2026 notes

FEAViz 0.1.3 removes the runtime dependency on the CRT `max_align_t` typedef so the C17 core builds with MSVC 19.50/v145 configurations where that typedef is not exposed.

When using the `NMake Makefiles` presets, the active Visual Studio developer shell chooses the target architecture. Configure output now prints either `FEAViz target architecture: 32-bit` or `64-bit`. For large FEA models, use an x64 Developer Command Prompt before configuring.

CMake 3.30 can report `MSVC_TOOLSET_VERSION=v143` even when `cl.exe` is MSVC 19.50 from Visual Studio 2026. FEAViz therefore validates v145 from `MSVC_VERSION` rather than trusting that stale CMake variable.

