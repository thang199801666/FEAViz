# FEAViz

FEAViz is a from-scratch C17 visualization and FEA data-processing library, built around a VTK-style connected pipeline. Public naming convention:

- Public types: `FViz*`
- Public functions: `fviz_*`
- Public constants/macros: `FVIZ_*`

The active implementation roadmap is
[`docs/architecture/VTK_CONVERGENCE_PLAN.md`](docs/architecture/VTK_CONVERGENCE_PLAN.md),
and the default ownership, error, MTime, and thread-safety rules are documented
in [`docs/api/PUBLIC_API_CONTRACTS.md`](docs/api/PUBLIC_API_CONTRACTS.md).

## Features

- **Core**: allocator/object runtime with atomic refcounting, TLS error state, logging, `FVizBuffer`/`FVizArray`/`FVizString`/`FVizDataArray`, `FVizBitArray`, `FVizHashMap`.
- **Math**: `FVizVec2/3/4`, `FVizMat3/4`, `FVizQuat`, `FVizBounds`, `FVizRay`, `FVizPlane`, `FVizTransform`.
- **Mesh**: `FVizPolyData` (points, triangles, lines, per-point scalars), `FVizPoints`, `FVizCellArray`, smooth normals, validation, `FVizMTime` mutation tracking.
- **FEA**: `FVizUnstructuredGrid` with tetra/hex/wedge/pyramid cells, surface extraction, slice, threshold, warp-by-vector, cell-to-point interpolation, contour lines, point probing.
- **Pipeline**: VTK-style `FVizFilter` connections with demand-driven execution, output caching, cycle detection, typed ports, progress/cancellation, and pipeline-aware mappers/renderers that pull data automatically.
- **Rendering**: OpenGL 3.3 core-profile context (legacy fallback), per-actor VBO/VAO/EBO caching, per-pixel lighting, scalar coloring with `FVizLookupTable`, `FVizScalarLegend` overlay, and multi-renderer windows with normalized viewports and render passes.
- **Interaction**: `FVizRenderWindowInteractor` with trackball style, VTK-style observers/events, `FVizRendererWidget`, spatial picking (BVH), and headless-testable styles.
- **Parallel**: explicit parallel contexts with independent worker pools, task groups, cancellation, deterministic sum/scan/stable-sort, and a compatible `fviz_parallel_for` wrapper.
- **IO**: OBJ/STL, VTK XML (`.vtu`, ASCII + binary/appended), and legacy `.vtk` unstructured grids preserving named typed point/cell arrays.
- **ABI**: public headers gate as standalone C17 and C++17; installed package consumers verify semantic/ABI versions.

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

Changing a parameter such as `fviz_warp_filter_set_scale()` invalidates that stage; the next renderer update recomputes only the affected downstream path. See `docs/design/CONNECTION_PIPELINE.md`.

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

Load OBJ or STL:

```text
out\build\windows-msvc-release\bin\FEAVizViewer.exe assets\testdata\cube.obj
out\build\windows-msvc-release\bin\FEAVizViewer.exe D:\Models\part.stl
```

Run the bent-beam FEA result viewer, or validate its HEX8 mesh, deformation, and stresses headlessly:

```text
out\build\windows-msvc-release\bin\FEAVizBentBeam.exe
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

The build tree places `FEAViz.dll` and the executables together under `bin/`, so no manual DLL copying is required.

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

## MSVC 19.50 / Visual Studio 2026 notes

FEAViz removes the runtime dependency on the CRT `max_align_t` typedef so the C17 core builds with MSVC 19.50/v145 configurations where that typedef is not exposed.

When using the `NMake Makefiles` presets, the active Visual Studio developer shell chooses the target architecture. Configure output prints either `FEAViz target architecture: 32-bit` or `64-bit`. For large FEA models, use an x64 Developer Command Prompt before configuring.

CMake can report `MSVC_TOOLSET_VERSION=v143` even when `cl.exe` is MSVC 19.50 from Visual Studio 2026. FEAViz validates v145 from `MSVC_VERSION` rather than trusting that stale variable.

