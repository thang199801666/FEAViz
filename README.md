# FEAViz

FEAViz is a from-scratch C17 visualization and FEA data-processing library. The public naming convention is:

- Public types: `FViz*`
- Public functions: `fviz_*`
- Public constants/macros: `FVIZ_*`

Version **0.13.0** adds an ordered render-pass architecture, opacity/edge/direct
RGBA/clipping support, viewport coordinate transforms, offscreen readback and
image output, explicit context lifecycle, and depth-tested GPU ID selection
with original FEA provenance.

The active implementation roadmap is
[`docs/architecture/VTK_CONVERGENCE_PLAN.md`](docs/architecture/VTK_CONVERGENCE_PLAN.md),
and the default ownership, error, MTime, and thread-safety rules are documented
in [`docs/api/PUBLIC_API_CONTRACTS.md`](docs/api/PUBLIC_API_CONTRACTS.md).

## Current 0.13.0 milestone

Implemented:

- Phase 0 repository/CMake/public-private ABI foundation.
- Phase 1 allocator, object runtime, atomic reference counting, TLS error state, logging.
- Core container foundation: `FVizBuffer`, zero-copy external buffer wrapping, `FVizArray`, `FVizString`.
- Typed numeric `FVizDataArray` for future FEA fields.
- Global monotonic 64-bit `FVizMTime`, automatic mutation tracking, and composite child MTime propagation through attributes, datasets, grids, and polygonal data.
- Math foundation: `FVizVec3`, `FVizMat4`, perspective/view matrices, `FVizBounds`.
- Triangle surface model: `FVizPolyData`, bounds, validation, smooth vertex normals.
- Mesh IO: OBJ and ASCII/binary STL readers.
- VTK IO: ASCII/binary VTU and ASCII/big-endian binary legacy `.vtk` unstructured grids, preserving named typed point/cell arrays.
- FEA pipeline: surface extraction, slicing, threshold, warp-by-vector, cell-to-point interpolation, and contour lines.
- VTK-style filter connections with executive-owned traversal, output-specific caching, cycle detection, mutable filter parameters, and typed unstructured-grid or polygonal outputs.
- Public custom source/filter algorithms with installed-header callbacks, versioned piece/extent/time requests, typed ports, resolved inputs, output publication, progress/cancellation, stable diagnostic IDs, and DOT graph export.
- Pipeline-aware mappers and renderers: rendering, camera fitting, and picking automatically pull current polygonal data from connected producers.
- VTK-style mapper, lookup table, scalar coloring, scalar legend, spatial picking, and point probing.
- Rainbow colormap preset and `FEAVizBentBeam`, a deformed hexahedral cantilever result viewer with a non-GUI validation mode.
- `FVizRenderWindowInteractor` with replaceable `FVizInteractorStyleTrackballCamera`; native backends translate events instead of owning camera behavior.
- VTK-style interactor observers with event filtering, stable priority ordering, consumable propagation, observer IDs, and mutation-safe dispatch.
- `FVizRendererWidget`, a VTK-widget-style facade for renderer/window/interactor ownership and lifecycle.
- Explicit parallel contexts with independent persistent worker pools, task
  groups, cancellation/error propagation, deterministic sum/scan/stable-sort,
  callback-local scratch, runtime statistics, nested-call safety, and the
  compatible default `fviz_parallel_for` wrapper.
- Multi-renderer windows with normalized viewports, layers, viewport-aware picking/event routing, and non-blocking widget event processing.
- Backend-neutral clear/opaque/translucent/edge/selection/overlay render passes,
  offscreen color/depth readback, PPM output, host child-window attachment,
  renderer coordinate conversion, and depth-tested hardware provenance picks.
- Interactor lifecycle/render gating and a headless-testable rubber-band style.
- General `FVizTransform` composition with actor user transforms, plus NaN/below/above-range lookup-table colors.
- Scene model: `FVizActor`, `FVizScene`, `FVizRenderer`, `FVizCamera`.
- Windows native render window using Win32/WGL and OpenGL.
- Viewer interaction: orbit, pan, zoom, fit, wireframe toggle.
- **Modern GPU renderer (0.1.4):** OpenGL 3.3 core-profile context with per-actor VBO/VAO/EBO caching and GLSL per-pixel lighting, with an automatic legacy compatibility fallback.
- CTest coverage for core/data/math/mesh/IO/scene.

The renderer requests an OpenGL 3.3 core-profile context at startup (probe-context based, using `wglChoosePixelFormatARB`/`wglCreateContextAttribsARB`) and only falls back to the legacy fixed-function path when a 3.3 context is unavailable. Geometry is uploaded to the GPU once per mesh and reused every frame, rebuilding only when the underlying `FVizPolyData` changes.

## Connected pipeline

The renderer is the demand-driven endpoint. No manual update is required before rendering:

```c
FVizFilter* smooth = NULL;
FVizFilter* warp = NULL;
FVizFilter* surface = NULL;
FVizActor* actor = NULL;

fviz_cell_data_to_point_filter_create(&smooth);
fviz_warp_filter_create("displacement", 2.0, &warp);
fviz_surface_filter_create(FVIZ_TRUE, &surface);
fviz_actor_create(&actor);

fviz_filter_set_input(smooth, grid);
fviz_filter_set_input_connection(warp, smooth);
fviz_filter_set_input_connection(surface, warp);
fviz_mapper_set_input_connection(fviz_actor_mapper(actor), surface);
fviz_scene_add_actor(fviz_renderer_scene(renderer), actor);
fviz_render_window_render(window);
```

Changing a parameter such as `fviz_warp_filter_set_scale()` invalidates that stage. The next renderer update recomputes only the affected downstream path. See `docs/design/CONNECTION_PIPELINE.md` for ownership and update semantics.

Mutating APIs call `fviz_object_modified()` automatically. After writing through a mutable raw-data pointer returned by the library, call `fviz_object_modified()` on the owning object so pipeline and rendering caches observe the change. See `docs/design/MODIFICATION_TIME.md`.

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

Run the bent-beam FEA result viewer:

```text
out\build\windows-msvc-release\bin\FEAVizBentBeam.exe
```

Validate its generated HEX8 mesh, deformation, stresses, surface and grid edges without opening a window:

```text
out\build\windows-msvc-release\bin\FEAVizBentBeam.exe --validate
```

Controls:

```text
Left mouse drag    Orbit
Middle mouse drag  Pan
Right mouse drag   Dolly
Mouse wheel         Zoom/dolly
F / R               Fit scene
W                   Wireframe
S                   Surface
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

