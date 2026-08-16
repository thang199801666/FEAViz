# FEAViz / VTK-style feature matrix

> **Historical/reference document:** from FEAViz 0.40 onward the active roadmap is `FEAVIZ_MODULAR_MASTER_PLAN_0_40_TO_1_0.md`. VTK format/class parity is not a release target; this file is retained only for architectural comparison and regression context.


Status baseline: FEAViz 0.38.0 (2026-08-14)

FEAViz is intentionally a C17-native toolkit rather than a source-compatible
VTK clone. The matrix tracks the parts of VTK's architecture that are most
useful for finite-element visualization and general geometry processing.

| Area | FEAViz 0.38 status | Next convergence target |
|---|---|---|
| Object runtime | Strong: opaque objects, refcounting, type IDs, MTime, errors, tagged priority/abort/mutation-safe observers, Modified/Delete/User events, and reusable ref-counted `FVizCommand` callbacks | Finish ABI/thread-safety audit; broaden object-specific event coverage |
| Data arrays | Strong numeric typed arrays; component/range/deep-copy and bulk-append helpers | Tuple iterators, mapped/external arrays, zero-copy views |
| Attributes | Point/cell/field associations and active roles | Copy/pass/interpolate policies matching filter needs |
| PolyData | General `Verts` / `Lines` / `Polys` / `Strips`, legacy triangle/line fast paths, normals, attributes | 64-bit native PolyData connectivity and richer cell-traits/iteration API |
| Unstructured grids | FEA-oriented native-ID connectivity, mixed vertex/beam/shell/solid geometry, provenance, optimized exterior-face extraction, and quadratic TET10/HEX20 surface/probe support | Complete WEDGE15/PYRAMID13 interpolation, broader high-order families, section-point conventions |
| Structured data | `FVizImageData`, explicit-point `FVizStructuredGrid`, coordinate-array `FVizRectilinearGrid`, plus `FVizMultiBlockDataSet`, `FVizPartitionedDataSet`, and `FVizTemporalDataSet`; structured geometry/extract filters preserve attributes and support non-zero extents | Ghost-array conventions, blanking/visibility, and broader structured filter families |
| Pipeline | Strong typed ports/repeatable inputs, iterative frame-stack executive, O(1) dependency-driven cache-hit gate, per-input piece/extent/time request remapping, whole-extent metadata, structured extent extraction, topology-aware unstructured pieces with multi-layer ghosts and deterministic point ownership, explicit output-payload release, input/producer ModifiedEvent propagation, observable custom-algorithm state, and time-specific updates, plus parallel dirty-leaf composite geometry scheduling with byte-budgeted leaf caching | Solver-native ownership maps, distributed exchange, and general composite executive scheduling beyond the geometry filter |
| Geometry sources | Plane, cube, sphere, unit arrow | Line, disk, cone/cylinder and richer source families |
| PolyData filters | Transform, elevation, append, generalized clean, triangle, normals, feature edges, connectivity, clip, smooth, decimate, contour | Windowed-sinc/quality smoothing, higher-quality decimation, boolean/cutter families |
| FEA filters | Threshold, slice, contour/probe/resample, surface-first warp, mixed-dimensional geometry, mesh quality, integration-point extrapolation, scalar/vector/tensor cell-to-point averaging, von-Mises/principal/Tresca/deviatoric results, beam tubes, shell thickness extrusion, ghost-aware unstructured pieces/partitions, original-ID provenance, and ghost-safe statistics/moments | Gradient/derivative/result-expression family, shell section-point reduction, solver-native partition ownership |
| Spatial | Median-split/pruned triangle BVH and cached AABB cell locator | Neighborhood/closest-point queries and reusable static-locator variants |
| Mapping/rendering | Render passes, backend-neutral render-target descriptors, mapper-shared persistent GPU mesh resources with resident mesh-byte statistics, perspective/parallel camera, material/multi-light shading, gradient background, sorted alpha + weighted OIT, sRGB, MSAA + FXAA, adaptive interaction quality, scalar mapping, integer Actor/Point/Cell/Edge/GlyphInstance selection, shader-expanded AA lines with cap/dash and adjacency-aware miter/round refinement, point impostors, true GPU-instanced glyphs/vector arrows, DPI-aware text actors, custom Unicode coverage atlases, batched 3D label sets, and labeled scalar legends | Dual depth peeling, richer line-join tessellation, text shaping/font rasterizer backend, annotation culling |
| Interaction/widgets | Stateful trackball interaction, SelectionModel with Replace/Add/Subtract/Toggle, integer Actor/Point/Cell/Edge/GlyphInstance picking, rectangle/lasso/frustum selection, throttled hover, reusable Widget/Representation/Manipulator abstractions, generic object-event bridging, abortable priority observers, X1/X2/horizontal-wheel input, and engineering widgets | Touch/pen gestures, richer composite widget representations, selection-aware widget handles |
| GUI hosting | Native Win32 top-level/offscreen/child hosting, host reparenting, lifecycle/DPI/focus events, reusable `FVizWin32RenderControl`, Qt 5/6 native-child adapters, plus toolkit-owned external OpenGL context/FBO adapters for `QOpenGLWindow` and `QOpenGLWidget` with context-recreation recovery and coalesced redraw scheduling | QML/QRhi integration, Linux/macOS native backends, broader Windows Qt runtime/visual integration tests |
| IO | OBJ/STL/legacy VTK/VTU, PVTU parallel manifests with VTK-standard ghost export plus lazy per-piece byte-budgeted LRU loading, ASCII VTP, and PVD temporal collections with direct PVD→PVTU time+piece selection; VTP ASCII interop checked against VTK 9.6.2 | Appended/compressed VTP/VTU, VTKHDF, asynchronous IO/prefetch, and distributed file sets |
| Parallel | Persistent runtime, independent contexts, concurrently dispatched task groups, deterministic parallel scan/stable-sort, and SMP field-statistics / multi-field cell-to-point paths | Parallelize contour/surface/derived-result families under benchmark and semantic-equivalence gates |

## Priority order after 0.38

1. **Executive/composite continuation.** Generalize the new parallel dirty-leaf scheduling pattern from `FVizCompositeGeometryFilter` into an executive-level composite work planner, then add async futures/prefetch with cancellation.
2. **FEA high-order/result completion.** WEDGE15/PYRAMID13 interpolation, shell section points/through-thickness reduction, beam local-axis/section profiles, and more solver integration schemes.
3. **Streaming/composite breadth.** Solver-native partition ownership, distributed ghost exchange, VTKHDF, asynchronous temporal-piece prefetch, general composite executive scheduling beyond the geometry filter, and broader structured-filter request propagation.
4. **GPU/visual hardening.** Windows visual baselines for OIT/AA/text/widgets/highlight, large-topology GPU chunking, alternate-geometry LOD, and annotation decluttering.

## Compatibility principle

Feature convergence does not mean adopting VTK's C++ ABI or exposing public
struct layouts. FEAViz keeps a stable C ABI, explicit ownership, deterministic
outputs, and FEA provenance as first-class constraints. The target is to make
VTK users recognize the pipeline/data-processing model while keeping the
library smaller and easier to embed.
