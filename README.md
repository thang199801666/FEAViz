# FEAViz

FEAViz is a from-scratch C17 visualization platform with a **domain-neutral Core** and
optional domain modules. Starting with 0.40, FEA post-processing is a separate module
rather than a responsibility of the visualization Core.

Public naming convention:

- Public types: `FViz*`
- Public functions: `fviz_*`
- Public constants/macros: `FVIZ_*`

The active cross-project roadmap is
[`docs/architecture/FEAVIZ_MODULAR_MASTER_PLAN_0_41_TO_1_0.md`](docs/architecture/FEAVIZ_MODULAR_MASTER_PLAN_0_41_TO_1_0.md).
The FEA/Abaqus-like behavioral sub-plan remains in
[`docs/architecture/ABAQUS_FEA_VISUALIZATION_MASTER_PLAN_0_39_TO_1_0.md`](docs/architecture/ABAQUS_FEA_VISUALIZATION_MASTER_PLAN_0_39_TO_1_0.md),
and the module dependency rules are documented in
[`docs/architecture/CORE_FEA_MODULE_BOUNDARY.md`](docs/architecture/CORE_FEA_MODULE_BOUNDARY.md).
Default ownership, error, MTime, and thread-safety rules are documented in
[`docs/api/PUBLIC_API_CONTRACTS.md`](docs/api/PUBLIC_API_CONTRACTS.md). Performance methodology and benchmark entry points are documented in
[`docs/performance/PERFORMANCE.md`](docs/performance/PERFORMANCE.md).

## Modules

```text
Applications / Win32 / Qt / scripting / headless
                      │
       ┌──────────────┴──────────────┐
       │                             │
  FEAViz::FEA                  future modules
  FEA post-processing         CAD / CFD / ...
       │                             │
       └──────────────┬──────────────┘
                      │
                FEAViz::Core
```

`FEAViz::Core` builds independently with `FVIZ_BUILD_FEA=OFF`. It contains no
Step/Frame/FieldOutput/integration-point/section-point policy and can be used as a generic
scientific/CAD/CFD visualization library.

`FEAViz::FEA` is optional and links Core. It supplies the solver-neutral FEA result domain
and Abaqus/Viewer-like post-processing behavior. It does **not** require the Abaqus ODB
runtime. Solver bridges are adapters that populate the neutral FEA model.

`FEAViz::FEAViz` is retained as a compatibility aggregate target. New code should link the
explicit component it needs.

### CMake usage

Generic visualization application:

```cmake
find_package(FEAViz 0.41 REQUIRED COMPONENTS Core)
target_link_libraries(my_app PRIVATE FEAViz::Core)
```

FEA post-processing application:

```cmake
find_package(FEAViz 0.41 REQUIRED COMPONENTS Core FEA)
target_link_libraries(my_app PRIVATE FEAViz::FEA)
```

Build only the generic library:

```bash
cmake -S . -B build -DFVIZ_BUILD_FEA=OFF
cmake --build build --config Release
```

## Features

### FEAViz Core

- **Runtime**: allocator/object runtime with atomic refcounting, TLS error state, logging, VTK-style observers/commands, MTime, resettable transient `FVizArena`, arrays/buffers/strings/bit arrays/hash maps and cached typed scalar ranges.
- **Math**: `FVizVec2/3/4`, `FVizMat3/4`, `FVizQuat`, bounds/ray/plane/transform utilities.
- **Generic data**: `FVizPolyData`, `FVizImageData`, `FVizStructuredGrid`, `FVizRectilinearGrid`, `FVizUnstructuredGrid`, `FVizMultiBlockDataSet`, `FVizPartitionedDataSet`, `FVizTemporalDataSet`, generic attributes and `FVizFieldStatistics`. `UnstructuredGrid` and statistics now live under the public `FViz/Data` namespace; their old `FViz/FEA` include paths are compatibility wrappers.
- **Pipeline**: connected demand-driven algorithms, composite scheduling, piece/extent/time requests, O(1) dependency cache gates, progress/cancellation, deep-graph iterative execution and byte-budgeted caches.
- **Algorithms**: sources plus generic transform/elevation/append/clean/normals/feature-edge/connectivity/clip/smooth/decimate/probe/resample/contour/threshold/slice/interpolation/calculator operations.
- **Generic deformation (0.41)**: solver-neutral vector-field metrics/auto-scale, fresh and in-place deformation for Points/PolyData/UnstructuredGrid, direct-array unstructured warp, and typed raw-vector hot paths designed for frame-by-frame geometry updates.
- **Rendering**: OpenGL renderer/window/camera/actor/mapper/material/lights, mapper-shared GPU resources, scalar mapping/legend/text, transparency/AA, clipping, glyphs, render passes and statistics.
- **Interaction**: interactor/styles, observers/events, selection/picking, generic widgets/manipulators and Win32/Qt embedding including external OpenGL context paths.
- **Parallel/memory**: worker contexts, task groups, deterministic reductions/scan/sort, arena scratch allocation and memory footprint accounting.
- **IO**: existing generic OBJ/STL/VTK XML/PVD/PVTU/legacy VTK support is retained for current workflows, but VTK-format parity is not a project goal.

### FEAViz FEA module

- **Result domain**: `ResultDatabase -> Step -> Frame -> Field -> FieldBlock`, plus HistoryRegion/HistorySeries. Blocks retain instance, result position, entity labels, local ids, section-point metadata and typed values.
- **FEA field semantics**: components, vector magnitude, Mises, Tresca, pressure and principal values.
- **Primary Variable engine (0.40)**: select component/invariant, source/target position, instance/entity subset and averaging rules; preserve raw values; extrapolate integration-point values through element-nodal representation; produce display values, discontinuity mask and display range; cache evaluation by source/grid/filter revisions.
- **Deformed Shape controller (0.41)**: resolve nodal displacement fields by frame/instance, map result labels to mesh GlobalIds, support true/uniform/auto scaling, undeformed/deformed/superimposed states, partial-coverage masks, base/deformed grids and revision-aware evaluation caching while delegating coordinate math to Core.
- **Existing FEA utilities**: integration-point extrapolation, shell/beam/high-order geometry support, mesh quality and FEA-oriented examples are linked through the optional FEA module where domain semantics are required.

## Primary Variable / Position / Averaging (0.40)

`FVizFEAPrimaryVariableEvaluator` turns raw FEA field blocks into a display-ready scalar
without destroying the source values:

```text
FieldBlock raw values
      │
      ├─ component / invariant
      ├─ result-position conversion
      ├─ entity filtering
      ├─ averaging policy
      └─ discontinuity detection
              ↓
  raw result + display result + mask + range
```

A typical stress request is:

```c
FVizFEAPrimaryVariable variable;
FVizFEAPrimaryVariableResult* result = NULL;

fviz_fea_primary_variable_initialize(&variable);
variable.operation = FVIZ_FEA_PRIMARY_INVARIANT;
variable.invariant = FVIZ_FEA_INVARIANT_MISES;
variable.source_position = FVIZ_FEA_POSITION_INTEGRATION_POINT;
variable.target_position = FVIZ_FEA_POSITION_NODAL;
variable.averaging_enabled = FVIZ_TRUE;
variable.averaging_threshold_percent = 75.0;

fviz_fea_primary_variable_evaluate(evaluator, field, grid, &variable, &result);
```

The evaluator rejects ambiguous duplicate active GlobalIds, preserves element-local/raw
values, and reports discontinuities rather than silently smoothing incompatible regions.
This output is the input contract for the upcoming deformed-shape and contour display
controllers; contour/rendering themselves remain generic Core facilities.

## FEA result-domain model

```text
FVizFEAResultDatabase
  └─ FVizFEAStep
      ├─ FVizFEAFrame
      │   └─ FVizFEAField
      │       └─ field blocks by instance / position / section point
      └─ FVizFEAHistoryRegion
          └─ FVizFEAHistorySeries
```

The result model is solver-neutral. An Abaqus ODB bridge, CalculiX reader or another
solver adapter should convert native results into these objects instead of leaking
solver-specific types into Core.

### Primitive and vector-glyph rendering

`FVizGlyphMapper` keeps one source mesh on the GPU and streams only instance transforms/colors. A common FEA vector-field workflow is:

```c
FVizArrowSource* arrow = NULL;
FVizGlyphMapper* glyphs = NULL;
FVizVectorGlyphOptions glyph_options;

fviz_arrow_source_create(&arrow);
fviz_arrow_source_update(arrow);
fviz_glyph_mapper_create(&glyphs);
fviz_glyph_mapper_set_source_poly_data(glyphs, fviz_arrow_source_output(arrow));
fviz_vector_glyph_options_initialize(&glyph_options);
glyph_options.scale_factor = 0.25f;
fviz_glyph_mapper_build_from_point_vectors(glyphs, result_points, NULL, &glyph_options);
fviz_actor_set_glyph_mapper(actor, glyphs);
```

Passing `NULL` for the vector-array name uses the active point-vector attribute. Zero/non-finite vectors are skipped. Modern OpenGL renders the retained instances with `glDrawElementsInstanced`; the source geometry is not duplicated per vector.

## Text and engineering annotations

FEAViz includes a dependency-free built-in fallback font plus a custom coverage-atlas extension point. `FVizTextActor2D` handles HUD/overlay text, `FVizBillboardTextActor3D` anchors one label in world space, and `FVizLabelSet3D` batches node/element-style labels that share one text property. Scalar legends reuse the same text-property system for titles, units, and numeric ticks.

Applications that rasterize fonts with FreeType, DirectWrite, or another library can create an `FVizFontAtlas` from owned coverage pixels and Unicode glyph metrics, then construct an `FVizFont` from that atlas; FEAViz itself does not require those external font libraries.

## Widget and engineering interaction framework

`FVizWidget` owns event/lifecycle state, `FVizWidgetRepresentation` owns the visual props attached to one renderer, and `FVizWidgetManipulator` maps display-space motion onto a view plane, explicit plane, or world-space axis. Concrete engineering widgets build on those three layers rather than installing independent platform callbacks.

The initial 0.24 family includes `FVizHandleWidget`, `FVizPlaneWidget`, `FVizBoxWidget`, `FVizLineWidget`, `FVizDistanceWidget`, `FVizAngleWidget`, `FVizSectionCutWidget`, and `FVizProbeWidget`. Widgets can be constructed without a native interactor for document/headless workflows and attached to an interactor later. Representation visibility masks per-child visibility instead of overwriting it, so hidden measurement/probe labels remain hidden across disable/enable cycles.

`FVizSectionCutWidget` uses stable mapper clipping-plane IDs: a section-cut widget updates/removes only the planes that it owns and does not clear application-created clipping planes. See `examples/21_Widgets/widgets.c` for a headless-friendly integration example.

## Selection 2.0 and large scenes

`FVizSelection` is now a deduplicated association-aware set over Actor/Point/Cell/Edge/GlyphInstance records. `FVizSelectionModel` owns current and hover selections and applies Replace/Add/Subtract/Toggle consistently across click, rectangle, lasso/polygon, and 3D-frustum workflows. The modern OpenGL picker uses an integer `RGBA32UI` target instead of RGB ID packing; point/edge picking uses a surface-depth prepass so hidden topology is not selected through the model.

Renderer large-scene policy includes cached world Actor bounds, cached camera frustum planes, pre-upload frustum rejection, and optional projected-size culling for sub-pixel props. Small-object culling is disabled by default and does not remove data from world-frustum selection. Render statistics expose culling counts and, when the driver supports timer queries, asynchronous GPU elapsed time. See `examples/22_SelectionLargeScene/selection_large_scene.c`.

## Structured data and native connectivity

`FVizImageData` models regular 0D/1D/2D/3D Cartesian data with inclusive extents, physical transforms, typed attributes, and linear sampling in physical space. `FVizImageDataGeometryFilter` extracts renderable structured boundaries while preserving point/field data and transitive cell provenance. See `examples/23_ImageData/image_data.c`.

`FVizCellArray` can store connectivity as 32-bit or 64-bit IDs. Ordinary meshes remain compact and renderer-friendly in 32-bit storage; native `FVizId` insertion auto-promotes when required. Width-agnostic `FVizCellView` lets algorithms avoid assuming a specific connectivity representation.

## Temporal, composite, and resampling data

`FVizPVDReader` publishes `TIME_STEPS`/`TIME_RANGE`-style metadata and supports both whole-time updates and demand-driven `time + piece` pulls. PVD→PVTU requests can load only the selected VTU piece through a retained manifest reader; whole grouped timesteps still materialize as `FVizPartitionedDataSet` when requested. PVD/PVTU LRUs can be bounded independently by entry count and estimated resident bytes. `FVizDataObjectMemoryInfo` exposes the same logical memory estimator to applications. `FVizTemporalDataSet` provides sorted-nearest-time semantics for in-memory workflows, adds bracket lookup/batch append, and propagates child modifications so repeated MTime queries remain O(1). `FVizMultiBlockDataSet` adds named hierarchical Assembly/Instance/Part-style trees with retained child ownership and cycle rejection.

Structured streaming is available through `FVizStructuredGridExtractFilter` and `FVizRectilinearGridExtractFilter`: downstream sub-extent requests can be expanded for ghost levels before reaching upstream producers, then materialized back to the requested region while preserving indexed attributes. Mixed FEA meshes use `FVizUnstructuredGridPieceFilter` / `FVizUnstructuredGridPartitionFilter` for balanced owned-cell ranges plus topology-aware multi-layer ghosts. `FVizCellLinks` and facet-hash `FVizCellAdjacency` drive ghost expansion, and deterministic global point ownership prevents nodal statistics from double-counting partition boundaries. `FVizCompositeGeometryFilter` converts heterogeneous MultiBlock/Partitioned assembly trees to renderable PolyData leaves while preserving names and hierarchy; unchanged leaves are cached by MTime so a hierarchy edit or one-part update does not reconvert every other part.

`FVizResampleWithDataSet` samples ImageData or UnstructuredGrid point arrays onto PolyData points. Structured sampling computes one interpolation stencil per destination point and reuses it across every source array/component. `FVizArrayCalculatorFilter` can then derive magnitudes, components, von Mises stress, or scaled arrays in the connected pipeline. See `examples/24_TemporalResample/temporal_resample.c`.

The 0.27 VTP reader/writer deliberately supports a single `Piece` with ASCII `DataArray` encoding. Multi-Piece and appended/base64/compressed VTP are rejected explicitly rather than partially decoded or silently dropping pieces.

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

Rendering and pipeline dependencies also propagate `ModifiedEvent` through the retained object graph.
Direct input data and upstream algorithm changes propagate through downstream algorithms; producer
algorithms / PolyData / lookup tables propagate through mapper -> actor -> scene -> renderer ->
render-window redraw request, while camera -> renderer follows the same path. Renderer-owned visual
dependencies (lights, scalar legends, render passes, text actors, and label overlays) are observed too;
text actors bridge their text properties and scalar legends bridge their lookup table plus title/label
text properties. Built-in custom source/filter wrappers bridge their state object into the underlying
`FVizAlgorithm`, so changing a source parameter can schedule a redraw without application glue.
Replacing or removing a dependency drops its old observer tag before release, so detached
inputs/scenes/mappers/overlays do not generate ghost redraws.

## Embedding FEAViz in Win32 and Qt

The renderer can live inside an existing GUI without taking over its event loop. Raw
Win32 applications can use the reusable `FVizWin32RenderControl`, which owns an attached
renderer child and automatically handles resize, focus, and interactor timer pumping:

```c
void* control = NULL;
fviz_win32_render_control_create(
    parent_hwnd, 1001, 0, 0, width, height, NULL, &control);

FVizRendererWidget* view =
    fviz_win32_render_control_renderer_widget(control); /* borrowed */
```

Lower-level applications can still call `fviz_renderer_widget_create_attached()`
directly. Attached views reject `start()`/`run()` because the host owns
`GetMessage`/`DispatchMessage`. A renderer can also be moved to a new native host with
`fviz_renderer_widget_reparent()` without rebuilding its scene or interaction state.
See `examples/26_Win32Embed/win32_embed.c`.

For Qt 5/Qt 6 on Windows, FEAViz now exposes both native-child and Qt-owned OpenGL
integration:

```text
# QtGui: FVizQtWindow + FVizQtOpenGLWindow
cmake --preset windows-msvc-release -DFVIZ_BUILD_QT_GUI=ON

# Qt Widgets: FVizQtWidget + FVizQtOpenGLWidget
cmake --preset windows-msvc-release -DFVIZ_BUILD_QT_WIDGETS=ON

# All Qt adapters
cmake --preset windows-msvc-release -DFVIZ_BUILD_QT_GUI=ON -DFVIZ_BUILD_QT_WIDGETS=ON
```

`FVizQtWindow` / `FVizQtWidget` use the established FEAViz-owned child HWND/WGL
architecture. `FVizQtOpenGLWindow` / `FVizQtOpenGLWidget` instead use the new
`FVizExternalOpenGLSurface` contract: Qt owns the context and framebuffer while FEAViz
renders into the host's `defaultFramebufferObject()` during `paintGL()`. This removes
the child-window stacking limitation and lets a QWidget participate naturally in Qt's
OpenGL composition path. The external adapters also release/rebuild only FEAViz GPU
resources when Qt recreates a context, preserving scene, camera and interaction state.
All four adapters keep Qt's `exec()` as the only GUI event loop and use coalesced
`requestRender()` scheduling. See `examples/27_QtEmbed`, `examples/28_QtGuiEmbed`,
`examples/29_QtExternalOpenGL`, `examples/30_QtOpenGLWidget`, and
`docs/design/RENDERER_HOST_INTEGRATION.md`.

## VTK-style object events and observers

Every `FVizObject` now supports tagged, priority-ordered observers. This extends events
beyond the interactor so render windows, renderers, cameras, actors, datasets, and future
object types can share one notification mechanism:

```c
FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
fviz_object_add_observer(
    (FVizObject*)window, FVIZ_EVENT_RENDER_END, 10.0f,
    on_render_end, app_state, &tag);

/* Later */
fviz_object_remove_observer((FVizObject*)window, tag);
```

Higher priorities run first, equal priorities are stable, callbacks may abort lower
priority propagation, and add/remove operations are safe during nested dispatch.
`Modified()` emits `ModifiedEvent`; final release emits `DeleteEvent`; interaction events
map into the same event-ID namespace with `FVIZ_EVENT_INTERACTION_ANY` available as a
category wildcard.

For VTK-style reusable callbacks, create one ref-counted `FVizCommand` and attach it to
multiple objects/events with `fviz_object_add_command_observer()`. A command callback may
set its abort flag to stop lower-priority observers. Algorithms now emit Start/Progress/
AbortCheck/End around real execution, and render-window picks emit StartPick/Pick/EndPick.
 Custom algorithms can additionally set `FVizAlgorithmCallbacks.state_object` to bridge a
state object's `ModifiedEvent` into the algorithm; the state object is borrowed and its `DeleteEvent`
automatically detaches the bridge.

GUI code can call `fviz_render_window_request_render()` repeatedly; requests are coalesced
into one pending host frame. This is the preferred redraw path for widgets, hover, camera
observers, and property panels; use `fviz_render_window_render()` only when a synchronous
frame is explicitly required. See `docs/design/INTERACTION_OBSERVERS.md`.

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
Left mouse drag          Orbit
Shift + left drag        Pan
Ctrl + left drag         Dolly
Middle mouse drag        Pan
Right mouse drag         Dolly
Mouse wheel              Zoom/dolly
Left double-click        Fit scene
F / R                    Fit scene
P                        Perspective / parallel
W                        Wireframe
S                        Surface
Esc                      Close
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


## Win32 GUI embedding demo

A complete native Win32 application with toolbar buttons outside the FEAViz 3D
viewport is included in `examples/31_Win32GuiDemo`. Build target:
`FVizExampleWin32GuiDemo` (output `FEAVizWin32GuiDemo.exe`).
