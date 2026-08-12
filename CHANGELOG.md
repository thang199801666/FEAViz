# Changelog

## 0.15.0 - Typed VTU round trips and interchange writers

- Added VTU ASCII and appended-raw writers with 32/64-bit headers, typed
  point/cell/field arrays, XML-safe names, active roles, and 64-bit connectivity
  serialization.
- Extended VTU reading to appended raw data and preserved all ten numeric array
  types instead of converting attributes to `Float32`.
- Added caller-configurable file, point, cell, connectivity, and array limits;
  malformed tuple counts and unsupported point-ID narrowing now fail explicitly.
- Added ASCII and binary little-endian PLY triangle writers.
- Added round-trip regressions for large unsigned IDs, NaN/infinity, tensor
  components, field metadata, association roles, header widths, and limits.
- Compression remains an explicit optional capability and returns
  `FVIZ_ERROR_NOT_SUPPORTED` when no approved compression backend is configured.

## 0.14.0 - Deterministic interaction and FEA inspection

- Added host-driven one-shot/repeating timers with stable IDs, reset/destroy,
  deterministic catch-up, and native Win32 event-loop polling.
- Added enter/leave, expose, focus, and timer events plus viewport capture during
  drag sequences and safe retained-style dispatch under nested observer changes.
- Added a trackball-actor style alongside the camera and rubber-band styles.
- Expanded selection records with rendered and original IDs, output MTime,
  persistent re-resolution, invalidation state, world position, and scalar/vector
  probe tuples.
- Added depth-aware actor/point/cell click selection with GPU and CPU paths.
- Added source-preserving selection highlight geometry and a multi-viewport
  orientation-axes overlay widget.
- Added synthetic regressions for timers, capture, actor manipulation, persistent
  FEA selections, probes, highlighting, axes, and point/cell picking.

## 0.13.0 - Render passes, offscreen lifecycle, and hardware selection

- Added an ordered, backend-neutral render-pass pipeline for clear, opaque,
  translucent, edge, selection, and overlay stages, including retained custom
  passes with explicit user-state destruction.
- Split modern OpenGL rendering into opaque/translucent/edge stages and added
  actor opacity, edge styling, RGBA/direct colors, point/cell/field selection,
  opacity arrays, automatic ranges, and six clipping planes.
- Added explicit render-window lifecycle, hidden offscreen contexts, resize,
  color/depth readback, PPM output, child-window attachment, context recreation,
  and capability diagnostics.
- Added viewport-aware world/view/NDC/display conversion and display-ray APIs.
- Added a depth-tested GPU ID pass resolving rendered triangles through
  `FVizOriginalCellIds` and `FVizOriginalFaceIds` provenance.
- Added deterministic offscreen, clipping, RGBA, edge, lifecycle, occlusion,
  provenance, and repeat-render image regressions.

## 0.12.0 - Isolated parallel runtimes and deterministic primitives

- Replaced sole reliance on the global dispatch lock with explicitly owned,
  independently executable parallel contexts and persistent worker pools.
- Added cancellable result-returning ranges, reusable task groups, deterministic
  floating-point reduction, checked scans, stable key/index sorting, worker
  affinity hints on Windows, callback-local scratch, and runtime statistics.
- Propagated the shared cancellation token through demand-driven pipeline
  requests with `FVIZ_ERROR_CANCELLED` and deterministic worker-error capture.
- Parallelized unstructured-grid transforms/warps and BVH primitive setup while
  retaining the legacy `fviz_parallel_for` compatibility entry point.
- Added independent-context, nested, cancellation, error, scratch, and repeated
  create/shutdown regressions plus a CSV HEX8 scaling benchmark matrix.

## 0.11.0 - Data associations and FEA provenance

- Completed point, cell, and field attribute associations on polygonal data,
  including active scalar, vector, normal, tensor, and global-ID roles.
- Added unified mapper array selection by association, name, component mode,
  and component index.
- Surface extraction now emits 64-bit original point, cell, and face IDs so a
  rendered triangle can resolve to its source FEA entity.
- Added checked `FVizId` topology entry points that reject unsupported wide IDs
  rather than silently narrowing them.
- Added shallow, deep, and structure-only PolyData copy contracts, aliasing
  regressions, and memory-size estimation.

## 0.10.0 - Demand-driven executive and custom algorithms

- Moved graph traversal into `FVizExecutive` and dispatch real information,
  data-object, update-extent, and data request stages.
- Added a versioned public request descriptor carrying output port, piece,
  ghost level, extent, time, release/exact flags, and transaction ID.
- Added public custom algorithm callbacks, user state lifecycle, typed port
  configuration, resolved input access, output publication, and progress APIs.
- Added request-aware output caches, stable algorithm diagnostic IDs, and DOT
  graph export with execution/cache statistics and typed connection labels.
- Added public-header regressions for diamond graphs, shared-upstream caching,
  piece/time invalidation, repeatable inputs, and selective multi-output work.

## 0.9.1 - Integrity and automated quality gates

- Added Windows and Linux CI jobs covering warnings-as-errors builds, CTest,
  shared/static installation, and clean `find_package(FEAViz)` consumers.
- Added a Linux Clang ASan/UBSan job and a bounded libFuzzer smoke target for
  the VTU and legacy VTK readers.
- Defined public ownership, failure-atomicity, MTime, and thread-safety rules,
  plus architecture decisions for pipeline requests, 64-bit IDs, render passes,
  and cooperative cancellation.
- Reconciled stale roadmap entries and made the VTK convergence plan the active
  implementation roadmap.

## 0.9.0 - General pipeline and architecture preview

- Added `FVizDataObject`, `FVizAlgorithm`, and borrowed `FVizAlgorithmOutput` proxies with indexed, type-checked input/output ports, direct data, repeatable connection storage, ownership retention, and generalized cycle detection.
- Migrated filters and mappers onto algorithm ports while retaining the 0.8 filter/mapper connection functions as compatibility wrappers.
- Added `FVizExecutive` request state, execution/cache-hit statistics, progress callbacks, and atomic cooperative abort; renderer pulls now enter the pipeline through the executive.
- Replaced per-call thread creation in `fviz_parallel_for()` with a lazily initialized persistent worker pool, serialized dispatch, nested-call serial fallback, thread limits, and dispatch statistics.
- Added multiple renderers per render window, normalized viewports, ordered layers, viewport-aware picking and mouse routing, plus non-blocking render-window/widget event processing.
- Added interactor enable/disable/done/render lifecycle state, update-rate hints, poked-renderer tracking, and a synthetic-event-testable rubber-band style.
- Added `FVizSelection` actor/point/cell records and viewport-aware CPU rectangle selection of projected triangle centroids as a backend-neutral fallback for future hardware selection.
- Added `FVizTransform`, actor user transforms with composite MTime, and special NaN/below-range/above-range lookup-table colors.
- Added a transform pipeline filter for unstructured grids; editing its retained transform invalidates the executive cache through composite filter MTime.
- Extended the connected HEX8 regression to use generic algorithm ports and kept the complete MSVC warnings-as-errors suite green.

## 0.8.0 - Modification time and correct cache invalidation

- Added global monotonic 64-bit `FVizMTime` to every `FVizObject`, with public `fviz_object_modified()` and `fviz_object_mtime()` APIs.
- Added automatic Modified tracking to core arrays, buffers, bit arrays, hash maps, strings, numeric data arrays, attribute sets, datasets, points, cells, unstructured grids, polygonal data, and filter parameter/input setters.
- Added composite MTime propagation: attribute sets include their arrays; datasets include all attribute associations; grids include points, cells, and dataset attributes; polygonal data includes topology, normals, scalars, and point attributes.
- Migrated connected filters and contour filters from local generation counters to composite input MTime.
- Migrated OpenGL geometry/scalar uploads and picking BVH cache identity to composite PolyData MTime, removing the legacy mesh/grid generation counters.
- Added regressions proving that changing a displacement tuple recomputes the complete grid-to-surface renderer pipeline, and changing scalar tuples invalidates dataset, PolyData, contour, and GPU-facing cache identity.
- Made concurrent `Modified()` calls preserve monotonically increasing per-object time using 64-bit atomic compare/exchange.

## 0.7.0 - Interaction observers

- Added multiple observers to `FVizRenderWindowInteractor`, with stable IDs, event-type filtering or `FVIZ_INTERACTION_EVENT_ANY`, and deterministic high-to-low priority ordering.
- Observer callbacks can consume an event before the active interactor style; the existing single callback API remains compatible and executes first.
- Observer removal during dispatch takes effect immediately, while observers added during a callback become active on the next outermost dispatch.
- Added remove-one, remove-all, and observer-count APIs with safe compaction after nested/reentrant dispatch.
- Added `FVizRendererWidget` convenience APIs for observer registration and removal.
- Extended native widget tests to cover filtering, ordering, consumption, duplicate removal errors, removal during dispatch, and deferred activation during dispatch.

## 0.6.0 - Demand-driven connected pipeline

- Added retained filter-to-filter input connections with recursive update propagation, cycle rejection, output caching, and runtime cycle protection.
- Added mutable parameters for threshold, warp, surface, and slice filters; parameter changes invalidate their cached output.
- Added `FVizSurfaceFilter` and `FVizSliceFilter` with explicit polygonal output typing, while volumetric filters continue to produce unstructured grids.
- Added mapper input connections for polygonal producers and renderer-wide pipeline updates before rendering, camera fitting, and picking.
- Corrected the OpenGL actor cache identity to include the polygonal-data object as well as its generation, preventing stale GPU geometry when a producer replaces its cached output.
- Added an end-to-end HEX8 pipeline test covering cell-to-point interpolation, deformation, surface extraction, scalar transfer, normals, mapper/renderer pull updates, caching, parameter invalidation, slicing, type validation, and cycle detection.

## 0.5.0 - Interaction, renderer widget and parallel runtime

- Added platform-neutral `FVizInteractionEvent` mouse/key/resize events and moved camera manipulation out of the Win32 backend.
- Added `FVizRenderWindowInteractor` with a replaceable `FVizInteractorStyleTrackballCamera`: left-drag orbit, middle-drag pan, right-drag dolly, wheel zoom, F/R fit, W wireframe, S surface, and Escape close.
- Every render window now owns a default interactor while the interactor keeps a detachable weak window reference, avoiding ownership cycles.
- Added a consumable interactor event callback so applications can layer selection, measurements, menus, and custom hotkeys ahead of the active style.
- Added `FVizRendererWidget`, a high-level facade owning the render window and exposing its renderer/interactor with add-actor, style, show, render, and start APIs.
- Added portable `fviz_parallel_for()`, hardware-thread detection, configurable thread limits, grain-size range partitioning, synchronous fallback when thread creation fails, and a 64-thread safety cap.
- Parallelized the point-deformation kernel in `fviz_unstructured_grid_warp_by_vector()` while keeping topology mutation sequential and deterministic.
- Migrated `FEAVizBentBeam` to `FVizRendererWidget`; added interaction, parallel-range, and hidden native widget/context tests.

## 0.4.7 - Rainbow bent-beam FEA viewer

- Added `FVIZ_COLOR_MAP_RAINBOW` and `fviz_lookup_table_build_preset()`; the Rainbow preset maps blue → cyan → green → yellow → red and can be shared by mappers and scalar legends.
- Added `FEAVizBentBeam`: a cantilever beam built from a 32×4×4 mesh (512 HEX8 elements), deformed using an Euler–Bernoulli displacement field and colored by 0–250 MPa Von Mises stress.
- The deformed surface includes 1,088 visible hexahedral boundary edges and a matching Rainbow scalar legend.
- Added `FEAVizBentBeam --validate` and the `FViz.Examples.BentBeam` CTest for non-GUI verification of mesh counts, surface topology, tip deflection and stress extrema.
- Fixed surface scalar transfer to convert numeric values to float32 correctly instead of copying bytes from a temporary double; added a regression value check.
- Fixed mixed triangle/line GPU rendering by binding the correct element buffer before each draw; filled surfaces now render correctly with polygon-offset mesh edges overlaid.

## 0.4.6 - Binary and typed legacy VTK compatibility

- Extended `fviz_vtk_legacy_read()` with big-endian `BINARY` legacy VTK support for points, cell connectivity/types, and result arrays.
- Preserves original attribute names, numeric types, tuple counts, and component counts instead of renaming and coercing every result to float32.
- Supports multiple `SCALARS`, `COLOR_SCALARS`, `VECTORS`, `NORMALS`, `TENSORS`, `TEXTURE_COORDINATES`, and `FIELD` arrays in point/cell/field data sections.
- Added strict topology/count validation, endian conversion, truncated-input diagnostics, and a generated binary fixture covering float32, float64, int32, vectors, and fields.
- Verified the MSVC v145 warnings-as-errors build and all 29 CTest tests.

## 0.4.5 - Legacy VTK reader

- Added `fviz_vtk_legacy_read()`: parses ASCII legacy `.vtk` files (`DATASET UNSTRUCTURED_GRID`) into `FVizUnstructuredGrid` — POINTS, CELLS + CELL_TYPES, and POINT_DATA/CELL_DATA SCALARS and VECTORS.
- Handles the legacy format's split cell storage (point ids in CELLS, types in CELL_TYPES) and the `LOOKUP_TABLE` line preceding scalar data.
- Added `assets/testdata/hex_legacy.vtk` and `FViz.IO.VTKLegacyReader` tests.

## 0.4.4 - Binary VTU support

- Added a base64 decoder and binary data parsing to `fviz_vtu_read`: the `binary` and `appended` formats (with 8-byte block header) are now decoded and read as typed arrays (Float32/64, Int8..64, UInt8..64) for points, connectivity/offsets/types, and result fields.
- Result fields are emitted as float32; integer topology arrays support Int64 offsets/connectivity as written by common solvers.
- Added `assets/testdata/hex_binary.vtu` (binary-encoded points, Int64 connectivity/offsets, UInt8 types, float32 temperature) and extended `FViz.IO.VTUReader` tests to cover it.

## 0.4.3 - Contour lines

- Added line topology to `FVizPolyData` (`add_line`, `line_count`, `line_indices`) so polydata can carry segment primitives alongside triangles.
- Extended the GPU renderer to draw line primitives (dark, unlit) on top of triangle geometry from the same resource.
- Added `FVizContourFilter`: extracts isolines from a scalar field over a triangle mesh at arbitrary levels (marching-edges per triangle, with double-line handling for saddle cases), with a cached generation-tracked `update`.
- Added `FEAVizContourLines` example (wave scalar field with 9 contour levels over a colored surface) and `FViz.Algorithms.ContourFilter` tests.

## 0.4.2 - VTU reader

- Added `fviz_vtu_read()`: parses VTK XML UnstructuredGrid (`.vtu`) files into `FVizUnstructuredGrid` — points, connectivity/offsets/types cells, and arbitrary `PointData`/`CellData` result arrays (ascii format, scalar and vector components).
- Maps VTK cell types to FEAViz cells (triangle, quad, tetra, hexahedron, wedge, pyramid) and preserves result fields with their original names for scalar coloring and probing.
- Added `assets/testdata/hex.vtu` and `FViz.IO.VTUReader` tests (counts, bounds, point positions, point/cell scalars, surface extraction).
- Added `FEAVizVTUViewer`: loads a `.vtu`, extracts the colored surface with a scalar legend.

## 0.4.1 - Scalar legend overlay

- Added `FVizScalarLegend`: color-bar overlay metadata with a `FVizLookupTable`, value range, corner position, visibility, and title.
- Added `fviz_renderer_set_scalar_legend()` / `fviz_renderer_scalar_legend()` to attach a legend to a renderer.
- Added a second GLSL 2D overlay program to the GPU renderer that draws the legend as an orthographic color-bar (gradient strips from the lookup table plus frame and min/max ticks) on top of the 3D scene.
- Wired the legend pass into the modern Windows render path after the scene draw.
- Updated `FEAVizFEAViewer` to display a "Stress" legend; added `FViz.Rendering.ScalarLegend` tests.

## 0.4.0 - Spatial index, point locator and picking

- Added `FVizBVH`: bounding-volume hierarchy over triangle meshes with ray-box and ray-triangle tests, `ray_cast` (closest hit with position/normal/distance/triangle), `ray_cast_any`, and `intersects_bounds`. Built from any `FVizPolyData`; triangle count > 0 required.
- Added `FVizPointLocator`: point-in-cell location over `FVizUnstructuredGrid` (tetrahedral barycentric + hexahedral Newton shape-function inversion) with `locate_point`, `interpolate_scalar`, and `interpolate_vector` for probing FEA results at arbitrary world points.
- Added `fviz_camera_pick_ray()`: unprojection-free world ray from screen coordinates.
- Added `fviz_render_window_pick()` and a `FVizPickCallbackFn` click callback on the render window (click = pick, drag = orbit); picks are BVH-cached per mesh generation.
- Added `FEAVizPicking` example: click the model to probe and print the interpolated temperature field.
- Added `FViz.Spatial.BVH` and `FViz.Spatial.PointLocator` tests.

## 0.3.3 - Filter pipeline and FEA viewer

- Added the `FVizFilter` pipeline framework: `set_input` / `update` / `output` execution model with cached outputs that only re-run when the input grid mutates (generation-tracked).
- Added concrete grid filters: `fviz_threshold_filter_create`, `fviz_warp_filter_create`, and `fviz_cell_data_to_point_filter_create` (each with a dedicated object type id).
- Added a mutation generation counter to `FVizUnstructuredGrid` for filter cache invalidation.
- Added `FEAVizFEAViewer`: a complete FEA visualization pipeline example — cell stress is smoothed to points, the grid is deformed by displacement, then both the deformed surface and an interior slice are rendered colored by stress.
- Added `FViz.Pipeline.Filter` tests covering the three filters and the update/caching behavior.

## 0.3.2 - FEA deformation and field interpolation

- Added `fviz_unstructured_grid_warp_by_vector()`: deforms a grid by displacing every point along a named three-component vector field scaled by a factor, preserving topology and all point/cell/field data.
- Added `fviz_unstructured_grid_cell_data_to_point_data()`: averages one-component cell scalars onto points using incident-cell weights for smooth stress/displacement contours, preserving the original grid.
- Added `FViz.FEA.Filters` tests covering warp correctness, warp validation, and cell-to-point averaging.

## 0.3.1 - FEA result visualization: slicing and surface scalars

- Added `FVizPolyData` point attribute storage (`FVizAttributeSet* point_data`) with `point_data`/`const_point_data` accessors so named per-point fields (stress, displacement, ...) can live on rendered surfaces.
- Added `fviz_unstructured_grid_slice()`: cut-plane filter through volume cells (tet/hex/wedge/pyramid) that emits a triangle mesh of the interior cross-section, with per-point scalar fields interpolated along cut edges and an active scalar set for coloring.
- Added `fviz_unstructured_grid_extract_surface_scalars()`: surface extraction that also transfers all one-component point scalar arrays onto the surface for scalar coloring.
- Robust handling of planes passing exactly through grid vertices (on-plane vertices become part of the intersection polygon).
- Added `FViz.FEA.Slice` tests (mid-plane, offset plane, miss, surface scalars) and the `FEAVizFEASlice` example showing surface + interior slice colored by stress.

## 0.3.0 - VTK-style mapper pipeline and scalar coloring

- Added `FVizLookupTable`: scalar-to-color mapping with configurable range, a default divergent color map, per-entry colors, and interpolated `map_scalar`.
- Added `FVizMapper`: VTK-style data source bridge holding the `FVizPolyData`, an optional `FVizLookupTable`, and scalar coloring configuration (visibility + scalar range with auto-range from data).
- Reworked `FVizActor` to own a `FVizMapper` (created by default); the existing `set_poly_data`/`poly_data` API remains source compatible.
- Added `FVizPolyData` per-point scalar support (`set_scalars`/`const_scalars`, float32 single-component, count must match points, bumps the mutation generation).
- Extended the GLSL 330 renderer with an optional per-vertex `aColor` attribute and `uScalarColorEnabled` uniform; scalar colors are baked into a per-mesh color VBO through the lookup table, otherwise the actor color is used.
- Added `FViz.Rendering.Mapper` tests for the lookup table, mapper wiring, and actor/mapper integration.

## 0.2.1 - Per-actor transforms

- Added `FVizActor` transform state: position, orientation (`FVizQuat`), and scale with public setters/getters.
- Added `fviz_actor_transform_matrix()` composing the model matrix as T * R * S.
- Extended the GLSL 330 renderer with per-actor `uModel` matrix and a `uNormalMatrix` (transpose-inverse of the model 3x3, including non-uniform scale) for correct lighting under transforms.
- Added a matrix-uniform path (`glUniformMatrix3fv`) to the internal GL function loader.
- Extended `FViz.Rendering.Scene` tests with transform matrix validation.

## 0.2.0 - Complete core containers and math primitives

- Added `FVizBitArray`: compact 64-bit-word backed bit storage with set/test, resize, clear, set-all, and hardware pop-count.
- Added `FVizHashMap`: open-addressing hash map with linear probing and tombstone erase, keyed by `FVizId` with `void*` values, automatic growth at 70% load, and iteration support.
- Expanded `FViz.Core.Containers` test coverage for both containers including growth and erase patterns.

## 0.2.0 - Complete math primitives

- Added `FVizVec2` and `FVizVec4` value types with add/sub/scale/dot/length/normalize operations.
- Added `FVizMat3` (column-major): identity, multiply, transpose, adjugate-based inverse, `transform_vec3`, and `from_quaternion`.
- Added `FVizQuat`: identity, `from_axis_angle`, Hamilton multiply, normalize, `rotate_vec3`, dot.
- Added `FVizRay` and `FVizPlane` for picking/intersection foundations: ray point/distance-to-point, ray-sphere intersection, plane from point+normal, signed point distance, and point projection.
- Extended the `FVizMath.h` umbrella header to include the new primitives.
- Expanded `FViz.Math.Core` test coverage for every new primitive.
- Fixed `FVizMat3` inverse index mapping (column-major result layout).

## 0.1.4 - Modern OpenGL renderer

- Added an internal OpenGL function loader (`FVizGL`) for the OpenGL 3.3 core subset, resolving entry points through `wglGetProcAddress` with an `opengl32.dll` fallback and no third-party dependency.
- Added `FVizGLDevice`, a shader-based render device with a built-in GLSL 330 program and per-actor GPU resource cache (VAO + position/normal VBO + index EBO).
- GPU geometry is uploaded once per mesh and reused across frames; the cache rebuilds only when the `FVizPolyData` generation counter changes.
- Reworked the Win32/WGL context creation to request an OpenGL 3.3 core-profile context via `wglChoosePixelFormatARB`/`wglCreateContextAttribsARB` (probe-context based), with a seamless fallback to the legacy 1.1 compatibility path.
- Replaced fixed-function lighting with per-pixel shader lighting (Lambert diffuse + ambient) and automatic flat-shading fallback for meshes without computed normals.
- Kept the public scene/actor/render-window API unchanged; all renderer changes are internal.
- Added a mesh mutation generation counter to `FVizPolyData` used for GPU-cache invalidation.
- Verified: OpenGL 3.3 core-profile path active on the Windows build, all 19 CTest tests pass.

## 0.1.3

- Fixed MSVC 19.50 / C17 build failure caused by direct use of `max_align_t`.
- Added an internal portable maximum-fundamental-alignment abstraction used by the allocator and object runtime.
- Fixed misleading VS 2026 toolset reporting under CMake 3.30/NMake by deriving v145 from `MSVC_VERSION=1950..1959`.
- Added explicit 32-bit/64-bit architecture reporting at configure time and a warning for 32-bit Windows builds.
- Kept the CMake/NMake workflow IDE-independent and added no `.bat`/`.cmd` files.

## 0.1.2

- Bind runtime/library/archive output directories directly to FEAViz and every example target.
- Keep FEAViz.dll and example/viewer executables together under the build `bin/` directory.
- Add configure-time output path diagnostics to make stale build trees easier to identify.

## 0.1.1 - Simple model example

- Added `FEAVizSimpleModel`, a minimal interactive cube example built entirely through the public C API.
- Added `examples/06_Tutorial/simple_model.c` and tutorial build/run notes.
- No public ABI changes.

## 0.1.0 - First interactive 3D scene

- Added `FVizBuffer`, zero-copy external-memory wrapping, `FVizArray`, and `FVizString`.
- Added typed/component-aware `FVizDataArray`.
- Added Vec3, Mat4, bounds, view/projection math and camera navigation primitives.
- Added triangle `FVizPolyData`, bounds, validation and smooth normal generation.
- Added OBJ and ASCII/binary STL mesh readers plus extension dispatch.
- Added `FVizActor`, multi-actor `FVizScene`, `FVizRenderer`, and `FVizCamera`.
- Added the public `FVizRenderWindow` abstraction.
- Added a native Windows Win32/WGL OpenGL backend with depth testing, lighting, indexed triangles, wireframe mode, orbit, pan, zoom and fit-view interaction.
- Added `FEAVizViewer`, which loads OBJ/STL from the command line or displays a built-in cube.
- Standardized build-tree runtime output so `FEAViz.dll` and `FEAVizViewer.exe` are colocated under `bin/`.
- Expanded the suite to 14 CTest tests and public-header isolation checks.
- Retained a CMake-first, no-project-`.bat`/`.cmd` build workflow.

## 0.0.6 - Phase 1 Core Runtime

- Added the public `FVizAllocator` callback interface and portable aligned default allocator.
- Added default allocation/reallocation/free APIs and checked size arithmetic.
- Added opaque `FVizObject`, stable 64-bit type IDs, internal class-parent metadata, and generic retain/release operations.
- Added atomic reference counting with overflow/underflow protection.
- Added extended `FVizResult` diagnostics, result strings, and fixed-capacity thread-local last-error storage.
- Added log levels, filtering, callback sinks, and a default stderr logger.
- Added custom-allocator leak tracking, aligned-memory stress tests, threaded retain/release tests, and TLS isolation tests.
- Added a Core Runtime example and design/verification documentation.
- Kept the CMake-first, no-project-batch-script Windows build baseline unchanged.

## 0.0.5 - Phase 0 script-free Windows build

- Removed all project-local `.bat` and `.cmd` files.
- Windows baseline remains CMake CLI + `NMake Makefiles` + MSVC v145.
- MSVC environment setup is now obtained by opening the x64 Developer Command Prompt for Visual Studio 2026; FEAViz no longer wraps Visual Studio environment scripts.
- Updated build and troubleshooting documentation to use only direct CMake/CTest commands.
- Clean rebuild examples now use `cmake -E remove_directory` rather than shell-specific scripts.

## 0.0.4 - Phase 0 CMake/MSVC/NMake Windows baseline

- Replaced the Windows Ninja/Visual Studio-generator presets with `NMake Makefiles` presets.
- Windows builds now use CMake CLI plus the MSVC v145 `cl.exe`, `link.exe`, and `nmake.exe` supplied by Visual Studio 2026.
- Removed any Windows requirement for Ninja, GCC, MinGW, `.sln` generation, CMake 4.2+, or the `Visual Studio 18 2026` generator.
- Added separate `windows-msvc-debug` and `windows-msvc-release` single-config build directories.
- Added `setup_msvc_env.bat`, environment validation, and Debug/Release convenience scripts.
- Updated the v145 compiler guard to be generator-independent and NMake-friendly.
- Retained portable Ninja presets only for non-Windows development and CI verification.

## 0.0.3 - Phase 0 VS2026 compatibility fix

- Changed the default `vs2026-x64` preset to `Ninja Multi-Config` so VS2026/v145 builds do not require the CMake 4.2 Visual Studio generator.
- Added compiler-family validation using `MSVC_VERSION` so v145 is enforced with both Ninja and Visual Studio generators.
- Added optional `vs2026-msbuild-x64` presets for CMake 4.2+ users who explicitly want native Visual Studio/MSBuild generation.
- Added Windows environment check/configure scripts.
- Expanded troubleshooting for machines where `cmake --help` does not list `Visual Studio 18 2026`.

## 0.0.2 - Phase 0 VS2026 toolchain refresh

- Replaced the Visual Studio 2022 preset with Visual Studio 2026 x64.
- Pinned the Windows platform toolset to `v145,host=x64`.
- Added a VS2026/v145 validation guard and generator/toolset diagnostics.
- Added dedicated Visual Studio 2026 build documentation.
- Documented the CMake 4.2+ requirement for the Visual Studio 18 2026 generator.
- Kept the portable C17/Ninja build path intact.

## 0.0.1 - Phase 0

- Established long-term FEAViz repository layout.
- Added C17 CMake build and presets.
- Added shared/static build support and symbol visibility macros.
- Added generated version/configuration headers.
- Added public platform/compiler detection macros.
- Added install/export package configuration.
- Added compiler warning policy, optional sanitizers, and optional LTO.
- Added CTest-based smoke tests and a first example.
- Reserved module boundaries for Data, Mesh, Geometry, Spatial, Pipeline, Algorithms, Rendering, Interaction, IO, FEA, Parallel, and Plugins.
