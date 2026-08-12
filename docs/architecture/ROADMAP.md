# FEAViz Roadmap

## Phase 0 — Foundation repository — COMPLETE

- [x] Repository architecture
- [x] CMake/C17 baseline
- [x] CMake-first Windows build using NMake + MSVC v145
- [x] Compiler warning policy
- [x] Platform/compiler abstraction macros
- [x] Public/private include boundary
- [x] DLL/shared-library visibility macros
- [x] Generated version/config headers
- [x] Static/shared build option
- [x] Install/export package support
- [x] CTest infrastructure
- [x] Optional ASan/UBSan and LTO hooks

## Phase 1 — Core runtime — COMPLETE in 0.0.6

- [x] Allocator interface and default allocator
- [x] Allocation/reallocation/free wrappers
- [x] Opaque object runtime and type identity
- [x] Atomic reference counting
- [x] Result/error system
- [x] Thread-local last-error storage
- [x] Logging interface
- [x] Unit/leak/stress/thread tests

## Phase 2 — Core containers — COMPLETE in 0.2.0

- [x] `FVizBuffer`
- [x] Ownership-aware zero-copy external buffer wrapping
- [x] Generic dynamic `FVizArray`
- [x] `FVizString`
- [x] Bit array
- [x] Hash map

## Phase 3 — Math — COMPLETE in 0.2.0

- [x] `FVizVec3`
- [x] `FVizMat4`
- [x] Perspective projection
- [x] Look-at/view matrix
- [x] Bounds
- [x] Vec2 / Vec4
- [x] Mat3
- [x] Quaternion
- [x] Ray / Plane / AABB
- [x] General transform object (0.9.0)

## Phase 4 — Data foundation — COMPLETE

- [x] Numeric data type enumeration
- [x] Typed/component-aware `FVizDataArray`
- [x] Attribute sets
- [x] Point data / cell data / field data
- [x] Data object / dataset base types

## Phase 5 — Surface mesh — FIRST USABLE IMPLEMENTATION COMPLETE

- [x] `FVizPolyData`
- [x] Point storage
- [x] 32-bit indexed triangles
- [x] Bounds
- [x] Mesh validation
- [x] Smooth vertex-normal generation
- [x] Dedicated Points / CellArray objects
- [ ] Lines / vertices / general polygons
- [ ] 64-bit connectivity path

## Phase 6 — Mesh IO — FIRST USABLE IMPLEMENTATION COMPLETE

- [x] OBJ vertex/face reader
- [x] OBJ polygon fan triangulation
- [x] Positive and negative OBJ vertex indices
- [x] ASCII STL reader
- [x] Binary STL reader
- [x] Extension-dispatching mesh reader
- [ ] PLY
- [x] Legacy VTK (ASCII in 0.4.5, binary in 0.4.6)
- [x] VTU unstructured-grid reader (ASCII 0.4.2, binary/appended 0.4.4)

## Phase 7 — Scene and camera — FIRST USABLE IMPLEMENTATION COMPLETE

- [x] Actor
- [x] Multi-actor Scene
- [x] Renderer frontend
- [x] Perspective Camera
- [x] Scene bounds
- [x] Fit-camera
- [x] Orbit / pan / dolly primitives

## Phase 8 — First visible 3D scene — MILESTONE COMPLETE in 0.1.0

- [x] RenderWindow public abstraction
- [x] Native Windows Win32 window
- [x] WGL OpenGL context
- [x] Depth testing and back-face culling
- [x] Smooth normal lighting
- [x] Indexed triangle rendering
- [x] Actor color
- [x] Wireframe mode
- [x] Mouse orbit
- [x] Mouse pan
- [x] Wheel zoom
- [x] Fit view hotkey
- [x] OBJ/STL command-line viewer
- [x] Built-in cube fallback

### 0.1.0 definition of done

```text
OBJ/STL file
    -> FVizPolyData
    -> FVizActor
    -> FVizScene
    -> FVizRenderer + FVizCamera
    -> FVizRenderWindow
    -> visible interactive 3D scene on Windows
```

## Phase 9 — Modern GPU renderer — MILESTONE COMPLETE in 0.1.4

- [x] Internal OpenGL 3.3 function loader with no third-party dependency
- [x] Probe-context WGL extension discovery (`wglChoosePixelFormatARB`/`wglCreateContextAttribsARB`)
- [x] OpenGL 3.3 core-profile context with legacy 1.1 fallback
- [x] GLSL 330 per-pixel lighting program (Lambert + ambient)
- [x] Flat-shading fallback for meshes without computed normals
- [x] Per-actor GPU cache (VAO + position/normal VBO + index EBO)
- [x] Cache invalidation via composite `FVizPolyData` MTime (generation counter superseded in 0.8.0)
- [x] Wireframe mode preserved through `glPolygonMode`
- [x] Public scene API unchanged
- [x] Per-actor model transform (position/orientation/scale) with normal matrix (0.2.1)

## Phase 10 — VTK-style mapper pipeline — MILESTONE COMPLETE in 0.3.0

- [x] `FVizLookupTable` with range, divergent colormap, and interpolated scalar mapping
- [x] `FVizMapper` data source with lookup table and scalar coloring configuration
- [x] Actor owns a default mapper; `set_poly_data` API preserved
- [x] Per-point scalars on `FVizPolyData`
- [x] Per-vertex scalar coloring through the shader color attribute
- [ ] Pipeline/filter execution (clip/slice/threshold/warp) Threshold cell filter **COMPLETE**, slice filter **COMPLETE in 0.3.1**

## Phase 11 — FEA result visualization — MILESTONE COMPLETE in 0.3.2

- [x] Surface extraction with point scalar transfer (0.3.1)
- [x] Interior slice/cut-plane filter with scalar interpolation (0.3.1)
- [x] Warp-by-vector deformation filter (0.3.2)
- [x] Cell-data to point-data interpolation for smooth contours (0.3.2)
- [x] `FVizFilter` pipeline framework with cached, composite-MTime-tracked outputs (generation tracking superseded in 0.8.0)

## Phase 12 — Spatial index and picking — MILESTONE COMPLETE in 0.4.0

- [x] `FVizBVH` triangle-mesh spatial index with ray casting (0.4.0)
- [x] `FVizPointLocator` point-in-cell location + scalar/vector interpolation (0.4.0)
- [x] `fviz_camera_pick_ray` screen-to-world ray (0.4.0)
- [x] Render-window pick API + click callback (0.4.0)

## Phase 13 — Scalar legend overlay — MILESTONE COMPLETE in 0.4.1

- [x] `FVizScalarLegend` metadata object (0.4.1)
- [x] Renderer hook + orthographic 2D overlay color-bar pass (0.4.1)

## Phase 14 — VTU result-file IO — MILESTONE COMPLETE in 0.4.2

- [x] `fviz_vtu_read` parses VTK XML UnstructuredGrid (ascii) into `FVizUnstructuredGrid` (0.4.2)
- [x] PointData/CellData result arrays preserved with original names (0.4.2)
- [x] Base64/binary VTU format support (0.4.4)
- [x] Legacy VTK `.vtk` reader

## Phase 15 — Contour lines — MILESTONE COMPLETE in 0.4.3

- [x] `FVizPolyData` line topology (0.4.3)
- [x] GPU line rendering (0.4.3)
- [x] `FVizContourFilter` isoline extraction (0.4.3)

## Phase 16 — Binary VTU — MILESTONE COMPLETE in 0.4.4

- [x] Base64 decoder + `binary`/`appended` VTU array parsing (0.4.4)

## Phase 17 — Legacy VTK reader — MILESTONE COMPLETE in 0.4.5

- [x] `fviz_vtk_legacy_read` ASCII unstructured grid + point/cell scalars/vectors (0.4.5)

## Phase 18 — Binary and typed legacy VTK — MILESTONE COMPLETE in 0.4.6

- [x] Big-endian binary points, cells, cell types, and data arrays
- [x] Original attribute names and numeric types preserved
- [x] Multiple scalar/color/vector/tensor/texture-coordinate arrays
- [x] FIELD arrays and strict topology/count validation

## Phase 19 — Rainbow bent-beam FEA example — MILESTONE COMPLETE in 0.4.7

- [x] Reusable Rainbow lookup-table preset
- [x] Structured 32×4×4 HEX8 cantilever mesh
- [x] Analytical bending displacement and Von Mises stress fields
- [x] Deformed colored surface with visible hexahedral grid edges
- [x] Interactive viewer and headless CTest validation

## Phase 20 — Interaction/widget/parallel foundation — MILESTONE COMPLETE in 0.5.0

- [x] Platform-neutral interaction event model
- [x] Render-window interactor with replaceable style ownership
- [x] Trackball-camera style: orbit, pan, dolly, fit and representation keys
- [x] Win32 backend reduced to native-event translation
- [x] Renderer widget facade for window/renderer/interactor lifecycle
- [x] Portable parallel-for with hardware detection, limits and grain size
- [x] Parallel warp-by-vector point kernel

## Phase 21 — Demand-driven connected pipeline — MILESTONE COMPLETE in 0.6.0

- [x] Retained filter-to-filter input connections and recursive updates
- [x] Cycle detection and cached execution
- [x] Mutable threshold, warp, surface and slice parameters
- [x] Explicit unstructured-grid and polygonal filter output types
- [x] Surface and slice producer filters
- [x] Mapper producer connections
- [x] Automatic renderer pull before render, fit and picking
- [x] End-to-end connected HEX8 FEA pipeline test

## Phase 22 — Interaction observers — MILESTONE COMPLETE in 0.7.0

- [x] Multiple observers with stable handles
- [x] Event-type filtering and wildcard observation
- [x] Stable priority ordering and consumable propagation
- [x] Safe add/remove during nested event dispatch
- [x] Renderer-widget observer facade
- [x] Observer lifecycle and reentrancy tests

## Phase 23 — Object MTime and composite invalidation — MILESTONE COMPLETE in 0.8.0

- [x] Global monotonic 64-bit object modification time
- [x] Automatic Modified calls from mutable core/data APIs
- [x] Composite MTime for attributes, datasets, grids and PolyData
- [x] MTime-based connected filter and contour caching
- [x] MTime-based OpenGL and picking BVH cache invalidation
- [x] Displacement/scalar mutation regression coverage

## Phase 24 - General algorithm and port model - MILESTONE COMPLETE in 0.9.0

- [x] `FVizDataObject` pipeline base
- [x] `FVizAlgorithm` source/filter/sink abstraction
- [x] `FVizAlgorithmOutput` producer/port proxy
- [x] Typed, optional, and repeatable input-port contracts
- [x] Indexed multiple input and output port infrastructure
- [x] Compatibility wrappers for current filter and mapper connections
- [x] Port-based connected HEX8 FEA regression test

## Integrity gate - COMPLETE in 0.9.1

- [x] Reconciled roadmap and VTK convergence plan
- [x] Public ownership, failure, MTime, and thread-safety contract
- [x] Architecture decisions for requests, IDs, render passes, and cancellation
- [x] Windows/Linux shared/static CI and clean install consumers
- [x] Clang ASan/UBSan job and bounded reader fuzz smoke target

## Phase 25 - Demand-driven executive - FOUNDATION in 0.9.0, planned completion in 0.10.0

- [x] Central graph traversal and execution state
- [x] Information, allocation, extent/piece, and data request states
- [x] Progress reporting and cooperative cancellation entry points
- [ ] Shared-upstream cache and branch fan-out correctness
- [ ] Pipeline graph diagnostics

## Phase 26 - Persistent parallel runtime - COMPLETED in 0.12.0

- [x] Bounded persistent worker pool with reusable dispatch
- [ ] Task groups, cooperative cancellation, and thread-local scratch storage
- [x] Nested `parallel_for` serial fallback and thread-limit tests
- [ ] Parallel surface, cell-to-point, contour/slice, and BVH phases
- [ ] Serial/parallel equivalence tests and large HEX8 benchmarks

## Phase 27 - Renderer/window/widget architecture - COMPLETED in 0.13.0

- [x] Multiple renderers, normalized viewports, and layers
- [x] Opaque, translucent, line, selection, and overlay passes
- [x] World/view/display coordinate conversion
- [x] Offscreen and host-controlled render-window lifecycle
- [x] Offscreen and embedded/host-native `FVizRendererWidget` operation
- [x] Non-blocking Win32/widget event processing

## Phase 28 - Interaction, selection, and widgets - COMPLETED in 0.14.0

- [x] Interactor initialize/enable/disable/done lifecycle and update-rate control
- [x] One-shot and repeating interactor timers
- [x] Multi-viewport event routing and drag capture
- [x] Multi-viewport mouse routing and poked-renderer tracking
- [x] Actor/point/cell selection result and CPU rectangle-selection fallback
- [x] Trackball-actor style
- [x] Rubber-band rectangle style and synthetic-event tests
- [x] Persistent original-ID selection records and scalar/vector probes
- [x] Selection highlight and orientation-axes widget

## Phase 29 - Data/rendering/IO completeness - FOUNDATION in 0.9.0, planned completion in 0.14.0

- [x] Transform pipeline algorithm
- [x] General transform object and actor user-transform integration
- [ ] Complete polygonal topology and 64-bit connectivity path
- [ ] Mapper point/cell array selection by name, association, and component
- [x] NaN, below-range, and above-range lookup-table colors
- [ ] Opacity, edges, and clipping planes
- [ ] PLY and compressed VTU read/write paths

## Phase 30 - Stable public release - PLANNED for 1.0.0

- [ ] ABI-1 API, ownership, thread-safety, and error-contract audit
- [x] Shared/static install-tree consumer tests
- [ ] Sanitizer, performance, and visual-regression release gates
- [ ] API, migration, architecture, and FEA workflow documentation

The detailed milestone scope, validation plan, dependencies, work packages,
release gates, and risk register are in
[`VTK_CONVERGENCE_PLAN.md`](VTK_CONVERGENCE_PLAN.md). The original compact
0.9-to-1.0 proposal remains in
[`DEVELOPMENT_PLAN_0_9_TO_1_0.md`](DEVELOPMENT_PLAN_0_9_TO_1_0.md).

## Immediate work after 0.9.1

1. Add executable request descriptors and output-specific request keys.
2. Expose public custom source/filter/sink callbacks.
3. Move all graph traversal into an executive transaction.
4. Prove shared-upstream fan-out, multi-input, and multi-output behavior.
5. Add progress/cancellation propagation and graph diagnostics.
