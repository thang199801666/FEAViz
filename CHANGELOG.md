# Changelog

## Unreleased

- Reworked `FVizExecutor` scheduling from a linear priority/dependency scan to a
  readiness-aware binary max-heap keyed on (priority, sequence). Dependency-blocked
  continuations are parked in a per-dependency waiter chain and promoted only when
  their antecedent completes, so worker pop cost is O(log n) instead of O(n) and no
  longer re-scans the queue for a blocked task. The park/push decision is made under
  the executor lock with the dependency lock nested (matching executor destruction),
  the waiter chain is detached inside `fviz_future_complete` under the same lock that
  publishes ready, and `worker->current` is cleared before ready is published, so a
  waiter thread that destroys a completed future cannot race with the completing
  worker or with executor teardown. Cross-executor continuations are promoted by the
  completing worker and by the destroy path (which collects detached waiter chains
  after releasing the executor lock), so futures cancelled during executor teardown
  are completed on their own executors instead of remaining parked forever.
- Added `FVizBenchmarkExecutor` covering independent-task throughput, deep dependent
  chains (the former worst case for the pop scan), and cancellation/drain. On the
  Windows/MSVC v145 development workstation a 200k independent no-op task batch
  drains in about 84 ms single-threaded (~2.4M tasks/s); the same batch is
  lock-contended at high worker counts because every queue mutation serializes on
  one executor mutex. These are regression references, not performance guarantees.
- Added `FVizTestExecutorStress`: strict priority-then-sequence ordering across a
  filled ready heap, a 4,000-link dependent chain, executor destroy with queued and
  parked continuations, queue-capacity rejection, cancellation without execution,
  and cross-executor continuation promotion.
- Added `fviz_algorithm_update_async_chain()`: runs an ordered array of algorithm
  updates as a dependent continuation chain on one executor, so multi-stage
  pipelines drain through the shared worker pool without one-thread-per-request or
  caller-side waits. Each stage runs only after its predecessor completes; a failed
  or cancelled stage short-circuits the remaining stages; intermediate links are
  owned by the chain and released by the terminal stage's destroy callback.
- Made `fviz_algorithm_update_async()` report live monotonic pipeline progress: the
  async task now runs as a context task and forwards `fviz_algorithm_report_progress`
  to the returned future (clamped non-decreasing), so `fviz_future_progress()` tracks
  a running update instead of remaining zero until completion.
- Reordered executor completion so a future's dependency is released before its
  user-data destroy callback runs. This unblocks chain/continuation destroy callbacks
  that free antecedent futures (a cancelled continuation previously kept the
  antecedent `dependent_count` at one and could deadlock `fviz_future_destroy`).
- Added `FVizTestAsyncPipelineChain` covering ordered chain execution, failure
  short-circuit, chain cancellation, and monotonic live progress through the future.
- Added `FVizBenchmarkPipelineAsyncChain` measuring full re-execution drain of a
  32-stage continuation chain through the executor pool.
- Fixed a `FVizTemporalPrefetchQueue` cancellation deadlock. The queue now owns a
  dedicated cancellation token and `fviz_temporal_prefetch_queue_cancel()` /
  `request_window()` direction reversal cancel that token instead of the drain
  future's. A cancelled future is completed by the executor without running the
  drain task, which previously left the queue's active flag set and blocked
  `fviz_temporal_prefetch_queue_wait_idle()` / `destroy()` forever. The drain
  task always runs, observes the token, and clears the active flag; a fresh
  kick resets the token so a cancelled queue can still prefetch later.
- Added the header-only C++17 binding under `bindings/cpp`. It wraps the C ABI
  with RAII ownership (`fviz::Object<T>` retaining/releasing through
  `fviz_retain`/`fviz_release`), typed math value types (Vec2/Vec3/Vec4, Mat4,
  Quat, Bounds, Plane, Ray), and ergonomic wrappers for data (DataArray,
  AttributeSet, UnstructuredGrid, PolyData, Points, CellArray), rendering
  (Camera, LookupTable, Mapper, Actor, Scene, Renderer, ScalarLegend,
  RendererWidget) and IO (`readVtu`, `readVtkLegacy`, `readObj`, `readStl`).
  Include `<FVizCpp/FVizCpp.hpp>` and link `FEAViz::Core`; failures throw
  `fviz::Error`. The C ABI remains the source of truth.
- Added `FVizTestCppBinding` (`FViz.Cpp.Binding`) covering math operators,
  RAII refcounting, grid construction, data arrays, readers, and headless
  rendering-object assembly. The binding builds in both the full and
  `FVIZ_BUILD_FEA=OFF` configurations.
- Extended the C++ binding to the full upper API layer: additional data
  objects (ImageData, StructuredGrid, RectilinearGrid, Transform), rendering
  (RenderWindow, Light, TextActor2D, BillboardTextActor3D, LabelSet3D),
  interaction (InteractorStyle, RenderWindowInteractor), pipeline filters
  (Threshold/Warp/CellDataToPoint/Surface/Slice/Transform), the parallel
  runtime (Executor, Future, CancellationToken, `parallel::forEach`), IO
  writers (VTP/PLY/PVD + VTU writers) and the full FEA module wrappers
  (HistorySeries/Region, Frame, Field, Step, ResultDatabase, PrimaryVariable,
  DeformedShape, ScalarBarActor, `fea::*` helpers). Added `FVizTestCppFEABinding`
  (`FViz.Cpp.FEABinding`), built only when `FEAViz::FEA` is enabled.
- Added FEA result-rendering helpers that close the highest-priority gaps vs
  VTK for post-processing display (`docs/architecture/FEA_RENDERING_VTK_GAP_PLAN.md`):
  - `fviz_fea_build_contour_surface()` — continuous (smooth) contour surface
    mapping each vertex through the Abaqus rainbow, preserving provenance.
  - `fviz_fea_build_contour_lines()` — iso-value contour-line overlay using the
    same interval conventions as the banded surface, with per-vertex level
    scalars and provenance. The core `FVizContourFilter` now tags every output
    vertex with its contour level (`contour_level` point array).
  - `fviz_fea_find_extrema()` — surface scalar extrema with original
    cell/face provenance for min/max markers.
- Added `FVizTestFEAVisualizationContours` (`FViz.FEA.VisualizationContours`)
  covering the smooth contour surface, contour lines, and extrema, plus
  matching C++ wrappers in the FEA binding.
- Completed the FEA result-rendering feature set called out in
  `docs/architecture/FEA_RENDERING_VTK_GAP_PLAN.md`:
  - Result-driven contour: `fviz_fea_build_contour_surface_from_result()`
    maps a `FVizFEAPrimaryVariableResult` display field onto the grid point
    data and builds a banded or smooth contour surface (G4).
  - Banded out-of-range colors + reversed spectrum:
    `fviz_fea_banded_surface_options_initialize()` and
    `fviz_fea_build_abaqus_banded_surface_ex()` (G6).
  - Section cut with result coloring: `fviz_fea_slice_contour()` slices a grid
    with a plane and colors the cut by a point scalar (G5).
  - C++ wrappers `fea::buildContourFromResult`, `fea::buildBandedSurfaceEx`
    and `fea::sliceContour`.
- Added `FVizTestCppFeatures` (`FViz.Cpp.Features`) exercising the filter
  chain (threshold/warp/surface/slice/cell-to-point), the trackball
  interaction driving the camera through `processEvent`, and a headless
  `fea::FramePlayer` animation controller that iterates ResultDatabase frames
  and rebuilds a contour surface per frame.
- Closed the final two gaps in `docs/architecture/FEA_RENDERING_VTK_GAP_PLAN.md`
  (G1–G8 all done):
  - Element (facet) contour without nodal averaging:
    `fviz_fea_build_element_facet_surface()` colors every triangle flat by its
    source cell's scalar from the grid cell data, with per-triangle provenance
    (G8); C++ wrapper `fea::buildElementFacetSurface`.
  - Deformed/undeformed overlay: `fea::SuperimposedDisplay::build()` builds the
    deformed solid actor plus a translucent undeformed wireframe/ghost actor
    from a `FVizFEADeformedShapeResult` into one scene (G7).
  - Extended `FViz.FEA.VisualizationContours` (facet case) and
    `FViz.Cpp.Features` (facet + superimposed cases).
- Completed the VTK cell-type catalog in the mesh layer. `FVizCellType` now
  carries every VTK cell type ID: PIXEL (8), VOXEL (11), PENTAGONAL/HEXAGONAL
  prism (15/16), TRIQUADRATIC_HEXAHEDRON (29), QUADRATIC_LINEAR_QUAD (30),
  QUADRATIC_LINEAR_WEDGE (31), BIQUADRATIC_QUADRATIC_WEDGE (32),
  BIQUADRATIC_QUADRATIC_HEXAHEDRON (33), BIQUADRATIC_TRIANGLE (34), CUBIC_LINE
  (36), QUADRATIC_POLYGON (37), CONVEX_POINT_SET (41), POLYHEDRON (42), and the
  higher-order/Lagrange/Bézier IDs (60-66, 68-81). Topology tables
  (dimension/points/edges/faces), `accepts_point_count`, and interpolation
  weights are implemented for the fixed-order types (partition-of-unity
  verified); VTU and legacy VTK readers now map the new IDs. Higher-order
  wedge shape functions remain explicit NOT_SUPPORTED.
- Added `FVizTestCellTypesVTK` (`FViz.Mesh.CellTypesVTK`) covering topology
  tables, partition-of-unity and delta interpolation, cell-array append/validate,
  and VTU round-trip, plus a matching C++ binding case in `FViz.Cpp.Binding`.
- Added the point-gradient filter (VTK `vtkGradientFilter` compatible):
  `fviz_unstructured_grid_gradient()` computes per-point gradients of a point
  scalar or vector field via the Green-Gauss method (per-cell least-squares fit
  over the cell's own points, then averaging incident cell gradients at each
  point). Scalar input yields 3 components (dx,dy,dz); vector input yields the
  full 3xN Jacobian. Exact for linear fields including at boundary points.
  C++ wrapper `UnstructuredGrid::gradient()`.
- Added `FVizTestUnstructuredGridGradient` (`FViz.Data.UnstructuredGridGradient`)
  covering exact gradients of linear scalar and vector fields plus error cases,
  and a matching case in `FViz.Cpp.Binding`.

## 0.41.0 - Dual-track deformation Core and FEA Deformed Shape controller

- Added solver-neutral Core deformation APIs under `FViz/Algorithms/FVizDeformation.h`: typed three-component vector metrics, bounds-based automatic scale calculation, fresh-output deformation for Points/PolyData/UnstructuredGrid, and in-place update paths that reuse allocated geometry for animation.
- Added `fviz_unstructured_grid_warp_by_array()` and refactored the legacy name-based warp implementation through the direct-array path while preserving its established missing-vector error behavior.
- Optimized Core deformation hot loops to read typed raw vector tuples once instead of performing three component-accessor calls per point. On the current 500k-point benchmark, vector measurement moved from about 4.4 ms to a five-run clean-build median of 0.75 ms, fresh deformation from about 10.3 ms to 7.33 ms, and in-place update from about 4.5 ms to 1.11 ms; these are environment-specific regression references rather than cross-platform guarantees.
- Added `FVizFEADeformedShapeController` in the optional FEA module. It resolves nodal displacement fields from a Frame, performs instance-aware block selection, maps result entity labels to mesh point GlobalIds, supports strict or partial nodal coverage, and exposes mapped/missing counts plus a coverage mask.
- Added true, user-uniform and automatic deformation scale modes plus undeformed, deformed and superimposed states. FEA results retain both base and deformed grids so presentation code can render overlays without duplicating result-domain policy.
- Added Deformed Shape evaluation caching keyed by Frame/Grid/options revisions; source displacement edits invalidate the cache through existing Modified/MTime propagation. All numerical coordinate updates are delegated to public Core deformation primitives, preserving the Core <- FEA dependency direction.
- Added Core deformation regression/benchmark coverage and an `examples/34_FEADeformedShape` example. The Core API remains buildable with `FVIZ_BUILD_FEA=OFF`, while the FEA example/test links only through `FEAViz::FEA`.
- Updated the active modular roadmap to 0.41 -> 1.0 and formalized a dual-track release discipline: domain-neutral mechanisms are implemented in Core; FEA/Abaqus-like result policy remains in the optional FEA module.

## 0.40.0 - Modular Core/FEA split and Primary Variable engine

- Split the project into two real library/package components. `FEAViz::Core` (`libFEAViz`) is the domain-neutral data/pipeline/render/interaction runtime; optional `FEAViz::FEA` (`libFEAVizFEA`) links Core and owns FEA result semantics. `FVIZ_BUILD_FEA=OFF` produces a complete Core-only build, while `FEAViz::FEAViz` remains a compatibility aggregate target.
- Added component-aware installed-package behavior for `find_package(FEAViz COMPONENTS Core FEA)`. Qt/Win32 integration and generic tests/examples link Core directly; FEA result/integration-point examples link the FEA module. Installed consumers were validated against `FEAViz::Core`, `FEAViz::FEA`, and the legacy aggregate target.
- Added a controlled internal sibling-module ABI for Core object allocation, local MTime access and TLS error propagation. These symbols let optional FEAViz modules create normal `FVizObject` subclasses without creating a Core->module dependency; the declarations remain under `internal/` and are not installed as public API.
- Reclassified generic `FVizUnstructuredGrid` and `FVizFieldStatistics` public headers under `FViz/Data`. Their former `FViz/FEA/...` paths are compatibility wrappers, allowing CFD/scientific/CAD consumers to use generic unstructured data/statistics without including FEA-domain headers.
- Added `FVizFEAPrimaryVariableEvaluator`, `FVizFEAPrimaryVariable`, and `FVizFEAPrimaryVariableResult`. A request selects component/invariant, instance, source/target result position, section point/entity subset and averaging policy; the result preserves raw values/ids and exposes display values, display association, discontinuity mask and raw/display ranges.
- Added integration-point -> element-nodal extrapolation as an explicit unaveraged intermediate. Primary-variable evaluation can then average element-nodal contributions to unique nodal values while preserving raw element-local data, reject cross-block averaging, and reject averaging when relative spread exceeds the configured threshold.
- Added evaluator caching keyed by field/grid/filter revisions plus selection/averaging options. Repeated identical requests hit the cache; changes to source field values or mesh/result-selection inputs invalidate it.
- Hardened label resolution: active point/cell GlobalIds used by primary-variable evaluation must be one-component integer arrays of the correct size and must be unique. Duplicate labels now return `FVIZ_ERROR_INVALID_ARGUMENT` instead of silently overwriting hash-map entries.
- Added `FViz.Examples.FEAPrimaryVariable` and regression coverage for native nodal fields, centroid display association, element-nodal block boundaries, averaging threshold discontinuities, integration-point Mises extrapolation, entity-label filtering, cache invalidation and duplicate GlobalId rejection.
- Added `docs/architecture/CORE_FEA_MODULE_BOUNDARY.md` and `FEAViz_MODULAR_MASTER_PLAN_0_40_TO_1_0.md`. The project roadmap is now explicitly two-track: reusable visualization Core and optional solver-neutral FEA/Abaqus-like post-processing behavior.

## 0.39.0 - Abaqus-like FEA result-domain foundation

- Reoriented the active roadmap from broad VTK parity to an Abaqus/Viewer-like FEA post-processing target. Existing VTK-compatible IO remains maintained, but new VTK format/class parity is no longer a release blocker unless required by an FEA workflow. The new active plan is `docs/architecture/ABAQUS_FEA_VISUALIZATION_MASTER_PLAN_0_39_TO_1_0.md`.
- Added `FVizFEAResultDatabase -> FVizFEAStep -> FVizFEAFrame -> FVizFEAField -> field block` as a solver-neutral ODB-like result hierarchy. Fields retain semantic type, component labels, valid invariants, instance identity, result position, section point metadata, entity labels/local ids, and typed numeric values.
- Added Abaqus-style result positions including nodal, element-nodal, integration-point, centroid, element-face, whole-element, and whole-region. Child `ModifiedEvent` propagation makes changes in field arrays invalidate Frame -> Step -> ResultDatabase without deep MTime traversal.
- Added field component evaluation and derived scalar invariants: vector magnitude, von Mises, Tresca, pressure, and maximum/middle/minimum principal values. Symmetric 3D, planar symmetric, and full 3x3 tensor storage are supported with documented component order.
- Added `FVizFEAHistoryRegion` / `FVizFEAHistorySeries` with ordered samples and interpolation, matching the field-output + history-output split expected by FEA post-processing and providing the data foundation for future XY plotting.
- Added regression coverage for field metadata, section points, component lookup, vector magnitude, analytic tensor invariant states, frame interpolation, database observer propagation, and history interpolation.

## 0.38.0 - Parallel composite scheduling and bounded leaf geometry cache

- Added the active `docs/architecture/VTK_PARITY_MASTER_PLAN_0_38_TO_1_0.md`, a staged 0.38→1.0 convergence plan covering executive/composite scheduling, async streaming, zero-copy arrays, VTKHDF/out-of-core IO, GPU resource budgets/partial updates, FEA high-order/result completion, SMP/distributed hardening, platform integration, interoperability, ABI audit, fuzzing, and release-candidate gates.
- Reworked `FVizCompositeGeometryFilter` execution into a deterministic three-phase path: serial hierarchy construction/cache probing, parallel conversion of only dirty/new leaves, and serial cache/output commit. Unchanged leaves remain O(1) cache hits while independent misses can use the persistent parallel runtime.
- Added `fviz_composite_geometry_filter_set_parallel_enabled()` and `set_parallel_threshold()` controls. Parallel scheduling defaults on with a four-leaf threshold and remains serial automatically on single-thread contexts or small dirty sets.
- Added byte-budgeted LRU caching for converted composite leaves. `set_cache_byte_capacity()` limits retained converted geometry, evicts least-recently-used entries under pressure, and bypasses caching for a single oversize leaf without invalidating the current output tree.
- Expanded `FVizCompositeGeometryCacheStatistics` with resident bytes, byte capacity, evictions, oversize skips, parallel batch count, and parallel leaf-conversion count.
- Added regression coverage for parallel composite scheduling, cache byte eviction, oversize bypass, and controls. Extended `FVizBenchmarkCompositeGeometryCache` to compare serial-vs-parallel cold conversion in the same build. On the development Linux Release runner, 256 ImageData leaves improved from a five-run median of roughly 67.5 ms serial to 16.0 ms parallel (~4.2× faster) while warm hierarchy-only updates remained around 0.06–0.07 ms. These timings are regression references, not cross-platform guarantees.

## 0.37.0 - Demand-driven memory budgets, temporal piece streaming, and composite leaf caching

- Added `FVizDataObjectMemoryInfo` / `fviz_data_object_memory_size()` for logical resident-memory estimates split into object, geometry, topology, attribute, and composite payload categories. Shared children in composite trees are counted once, and allocation failures in the visited set now propagate instead of silently undercounting.
- Added byte-budgeted LRU policies to `FVizPVDReader` and `FVizPVTUReader` in addition to entry-count limits. Cache statistics report resident bytes and oversize bypasses; objects larger than a non-zero budget are returned normally without being retained.
- Added true PVD time+piece pulls with `fviz_pvd_reader_update_piece_time()`, `piece_count_at_time()`, and piece prefetch. A PVD→PVTU timestep can now load only one VTU piece, while multi-entry PVD timesteps select one stable part without materializing the complete frame. Whole-timestep APIs remain source compatible.
- Reused one stateful PVTU manifest reader for repeated piece requests at the current PVD timestep, avoiding repeated manifest parsing while preserving independent whole-frame and piece-cache behavior.
- Added leaf-MTime caching to `FVizCompositeGeometryFilter`. Unchanged heterogeneous leaves reuse their converted PolyData across hierarchy updates, modified leaves alone reconvert, and removed leaves are pruned/released after a successful execution. Cache hit/miss/prune statistics and explicit cache clearing are public.
- Made `FVIZ_PIPELINE_REQUEST_FLAG_RELEASE_DATA` release retained payloads on non-requested output ports instead of only marking them stale. Added explicit `fviz_algorithm_release_output_data()` / `release_all_output_data()` controls for memory-pressure and streaming workflows.
- Added `resident_mesh_gpu_bytes` to render statistics, estimating resident mapper/glyph position, normal, topology, color, adjacency, point-index, and instance buffers independently of per-frame upload traffic.
- Added `FVizBenchmarkCompositeGeometryCache`. On the development Linux/LTO runner, a 256-leaf ImageData assembly remained near cold-start parity with 0.36 (~63 ms), while hierarchy-only warm updates fell from ~56 ms in pristine 0.36 to ~0.065 ms and a single modified leaf updated in roughly 0.3 ms. These timings are regression references, not cross-platform guarantees.

## 0.36.0 - Topology-aware ghost partitions, lazy PVTU streaming, and shared render resources

- Added `FVizCellLinks` point-to-cell CSR incidence and facet-hash `FVizCellAdjacency` for exact same-dimensional cell neighbors across shared facets. The adjacency builder supports non-manifold relationships without recursive traversal and is reused by partition/ghost workflows.
- Upgraded `FVizUnstructuredGridPieceFilter` from provenance-only cell slicing to topology-aware multi-layer ghost generation. Owned cells are emitted first, ghost layers retain original cell IDs, and point/cell `FVizGhostType` plus cell `FVizGhostLevel` metadata are generated deterministically.
- Added deterministic global point ownership across pieces. A source point is owned by the lowest partition containing an incident owned cell; copies in all other pieces are marked duplicate even when they also belong to locally owned cells. This makes point-data reductions over a `FVizPartitionedDataSet` agree with the original whole mesh instead of double-counting partition-boundary nodes.
- Added `FVizUnstructuredGridPartitionFilter` for materializing a complete `FVizPartitionedDataSet` from one mixed unstructured grid. It reuses a persistent piece filter so topology adjacency, point links, and point-owner tables survive deformation-only frame updates.
- Made surface/mixed-geometry extraction ghost aware: ghost cells still participate in face ownership to suppress partition seams, but ghost-only exterior faces and direct duplicate ghost primitives are not emitted. Threshold preserves ghost/provenance arrays; slice skips duplicate/hidden ghost cells.
- Expanded `FVizFieldStatistics` with deterministic parallel `FVizFieldMoments` (mean, RMS, population variance, standard deviation), composite traversal, and default duplicate/hidden ghost exclusion. The original extrema-only API retains its lower-cost fast path.
- Hardened cell-data-to-point-data interpolation so topology metadata (`FVizGhostType`, `FVizGhostLevel`, original cell IDs/global IDs) is never averaged into point data, while real field values from ghost cells can still contribute at partition boundaries.
- Added `.pvtu` parallel unstructured-grid IO. The writer emits per-piece VTU files plus a schema-bearing PUnstructuredGrid manifest and VTK-standard ghost metadata; the reader resolves relative piece paths and preserves partition names/attributes. `.pvd` temporal collections can now reference `.pvtu` frames.
- Added a stateful `FVizPVTUReader` for large datasets: parse the manifest once, inspect piece count/source/GhostLevel, lazily load or prefetch individual pieces, and retain a configurable LRU with hit/miss/eviction statistics. Existing `fviz_pvtu_read()` remains a materialize-all convenience wrapper.
- Improved VTK XML interoperability. VTU Points arrays no longer require a nonstandard `Name="Points"` attribute. VTK-standard `vtkGhostType` arrays are preserved and normalized into FEAViz ghost flags; VTU/PVTU output exports FEAViz ghost arrays back as `vtkGhostType` with association-correct point/cell hidden bits.
- Shared OpenGL mesh resources across actors that reference the same mapper/glyph mapper, while keeping transform/material state per actor. Topology, geometry, color, and render-state revisions continue to select full rebuild, buffer sub-update, color-only update, or uniform-only paths.
- Added ghost/partition, shared-mapper, PVTU cache/round-trip, VTK ghost normalization, topology-adjacency, and partition-deformation benchmark coverage.

## 0.35.0 - Streaming structured data, composite geometry, and incremental render-data caching

- Added per-input pipeline request remapping so downstream algorithms can transform piece, extent, ghost-level, and time requests before each upstream producer executes. Added request helper setters plus `fviz_executive_update_piece()`, `fviz_executive_update_extent()`, and `fviz_executive_update_time()` convenience pulls.
- Added output whole-extent metadata independently of data MTime, bringing structured streaming metadata closer to VTK demand-driven pipeline semantics while preserving the existing request/cache transaction model.
- Added `FVizStructuredGrid` with explicit points, implicit structured LINE/QUAD/HEX connectivity, non-zero-based extents, point/cell/field attributes, O(1) aggregate MTime, bounds, validation, and geometry conversion to renderable PolyData.
- Added `FVizRectilinearGrid` with implicit structured topology backed by three monotonic numeric coordinate arrays, cached bounds, attributes, and geometry conversion. Rectilinear grids avoid storing explicit point triples/connectivity for orthogonal nonuniform meshes.
- Added `FVizStructuredGridExtractFilter` and `FVizRectilinearGridExtractFilter`. Extent requests materialize exact sub-grids, gather point/cell arrays by global structured indices, preserve field data/active roles, expand upstream requests for ghost levels, and honor exact-extent requests.
- Added `FVizUnstructuredGridPieceFilter` for balanced cell-piece extraction of mixed FEA meshes. It compacts only referenced points, copies point/cell/field arrays, publishes `FVizOriginalPointIds` / `FVizOriginalCellIds`, maps connected upstream requests to a whole input before local partitioning, and explicitly rejects unsupported ghost-cell generation rather than returning incorrect topology.
- Added iterative `FVizMultiBlockDataSet` traversal/leaf visitors and `FVizCompositeGeometryFilter`. Named assembly trees containing PolyData, UnstructuredGrid, StructuredGrid, RectilinearGrid, ImageData, nested MultiBlock, and PartitionedDataSet leaves can be converted to renderable composite geometry without recursive C traversal.
- Changed DataSet, ImageData, UnstructuredGrid, PolyData, Actor, Scene, and Mapper MTime reads to O(1) where retained dependencies already synchronously propagate `ModifiedEvent`; this removes repeated deep dependency walks on read-heavy render/pipeline paths.
- Split mapper/render-resource invalidation into geometry, topology, attribute/color, and render-state revisions. The Windows OpenGL backend now keeps full rebuilds for topology/count changes, uses buffer sub-updates for geometry-only changes, refreshes only color data for scalar/LUT changes, and keeps clipping-plane-only edits uniform-only. The portable revision/cache policy is covered by core tests; native WGL execution still requires Windows validation.
- Added permanent contour and Scene-MTime performance smoke benchmarks. The latter protects O(1) scene validity reads against regressions back to actor-count-dependent scans. A trial parallel final-compaction implementation was deliberately reverted after controlled comparison showed it slower than the existing serial publish path, keeping the measured faster implementation instead of retaining unproductive parallelism.
- Expanded streaming/composite/structured regressions, including remapped upstream requests, non-zero extents, exact extent behavior, attribute/provenance preservation, iterative composite traversal, and unstructured piece extraction.

## 0.34.0 - Parallel/composite core, iterative executive, and temporal acceleration

- Added `FVizArena`, a resettable transient allocator with retained blocks, alignment-aware allocation, statistics, trimming, and an allocator adapter for per-update/filter scratch memory. The pipeline executive now reuses an arena per update instead of repeatedly allocating short-lived visited tables.
- Upgraded `FVizParallel`: task groups dispatch independent queued tasks concurrently, large exclusive/inclusive `uint64` scans use deterministic block-parallel passes, and stable sort parallelizes merge pairs while preserving equal-key order. Cancellation and independent-context behavior remain covered by regression tests.
- Reworked the pipeline executive around an explicit frame stack rather than recursive upstream execution. A 4,096-stage connected pipeline executes without one native C stack frame per algorithm, and cycle/DOT traversal remain iterative.
- Added an O(1) pipeline cache-hit gate keyed by request + algorithm MTime. Because direct inputs, producer connections, and observable state propagate `ModifiedEvent`, an unchanged downstream node can return without rewalking its entire upstream graph; an upstream modification still invalidates and re-executes the affected path.
- Added `FVizMultiBlockDataSet` for named hierarchical composite trees such as Assembly -> Instance/Part -> Partition/DataSet. Child modifications propagate to parents in O(1), retained ownership is explicit, and direct/indirect composite cycles are rejected.
- Hardened `FVizPartitionedDataSet` and `FVizTemporalDataSet` child subscriptions so aggregate MTime no longer scans every partition/frame. Added reserve helpers, temporal batch append, and interpolation-bracket lookup.
- Added `fviz_pvd_reader_prefetch_time()` to synchronously warm the existing PVD LRU without changing the reader's selected time/output, enabling next-frame prefetch workflows.
- Added fine-grained `FVizPolyData` geometry/topology/attribute revisions. Actor bounds now depend on geometry revision rather than scalar/field changes, reducing cache invalidation during result-only FEA playback.
- Added `fviz_bvh_update()` / `fviz_bvh_refit_required()` and `fviz_point_locator_update()` / `fviz_point_locator_refit_required()`. Geometry-only changes select refit, topology changes rebuild, and attribute-only changes keep spatial acceleration current.
- Parallelized field statistics over raw typed numeric storage with deterministic extrema reduction. Added a dedicated benchmark covering 2 million float32 vector tuples.
- Made cell-data-to-point-data adaptive: the single-field workload keeps the lower-overhead serial scatter path, while multi-field FEA frames build point-to-cell adjacency once and gather fields in parallel without atomics. Added a multi-field benchmark to guard this dispatch policy.
- Contour filtering now accepts all numeric one-component array types and bulk-compacts final points/lines instead of publishing one mutation per segment. Surface extraction likewise bulk-publishes boundary triangles and provenance arrays.
- Mapper setters avoid more no-op invalidations, and clipping-plane mutations correctly invalidate mapper render data.
- Added performance smoke benchmarks for field statistics, deep pipeline cache hits, temporal MTime, and multi-field cell-to-point conversion.

## 0.33.0 - Core performance, refit acceleration, and event-driven data propagation

- Reworked hot container mutation paths so one public logical mutation generally produces one parent `ModifiedEvent` instead of recursively modifying private storage and the wrapper. `FVizArray` now exposes internal untracked mutation helpers used by `FVizDataArray`, `FVizPoints`, `FVizCellArray`, `FVizPolyData`, Scene storage, and AttributeSet internals while public MTime/event semantics remain intact.
- Added `fviz_data_array_set_tuples()` and `fviz_points_set_many()` for bounded in-place FEA frame updates. Both detect byte-identical no-op writes and emit a single modification for a changed batch; point bounds are recomputed lazily only when coordinate edits invalidate them.
- Specialized `FVizDataArray` scalar-range scanning for contiguous float32/float64 storage and added a one-entry range cache keyed by component, non-finite policy, and array MTime. Repeated scalar-range queries are O(1), while raw writable-pointer users continue to invalidate the cache through the documented `fviz_object_modified()` contract.
- Added adaptive lazy name indexing to `FVizAttributeSet`. Small sets keep allocation-free linear lookup; larger sets build a hash index while retaining exact-string validation and collision fallback. Child DataArray `ModifiedEvent`s now propagate through AttributeSet -> dataset/mesh -> mapper/render graph instead of relying only on later MTime polling.
- Fixed `FVizHashMap` tombstone reuse so erase/insert churn does not force unnecessary growth, made same-value updates no-op, hardened growth overflow, and made `clear()` remove tombstones even when the live count is already zero.
- Added geometry-only invalidation and `fviz_point_locator_refit()`. Field/result array updates no longer stale a locator whose points/topology are unchanged; deformed coordinates with unchanged cells can refit node/cell AABBs without rebuilding the hierarchy. HEX8 point-location also fuses shape/Jacobian accumulation into one node pass and avoids an unnecessary final inverse.
- Added `fviz_bvh_refit()` for deformation sequences with unchanged triangle topology. Leaf triangle bounds and interior hierarchy bounds are refreshed without repartitioning primitives. BVH rebuild of an empty mesh now resets prior state, and bounds intersection uses direct iterative AABB hierarchy traversal instead of a center ray that could miss coplanar geometry.
- Replaced recursive pipeline cycle detection and its former fixed depth cutoff with iterative graph traversal plus a visited hash set. Deep valid graphs remain supported and cycles beyond 1,024 connections are rejected reliably without recursion-stack growth.
- Optimized first-order UnstructuredGrid surface ownership hashing/sorting and reduced private-storage modification traffic in points/cells/PolyData. The hot HEX surface path remains width-aware and preserves existing provenance output.
- Made default mapper lookup tables lazy, eliminating a 256-color allocation/build for every mapper that never enables scalar coloring. Scalar ranges set before first LUT access are preserved, and explicit `set_lookup_table(NULL)` remains an intentional disable.
- Added `fviz_scene_reserve()` and `fviz_scene_add_actors()` for large retained scenes. Bulk actor attachment is rollback-safe, reserves once, installs dependency observers, and emits one Scene modification for the batch. The common one-observer object allocation now reserves one observer record initially rather than four.
- Extended no-op modification suppression across Camera, Actor, Renderer, LookupTable, and selected Mapper state so repeated assignment of an already-effective value does not increment MTime or schedule a redundant coalesced GUI render.
- Reduced Win32/OpenGL scene-pass CPU work by fetching the cached frustum once per pass, rejecting actors once before transform/material work, and reusing the same culling state in edge/point passes rather than repeatedly entering renderer-level culling helpers.
- Added permanent `FVizBenchmarkDataRange` and `FVizBenchmarkAttributeLookup` smoke benchmarks and expanded BVH/PointLocator benchmarks with refit timing. On the development Linux runner, representative controlled comparisons against pristine 0.32 measured roughly 0.467 s -> 0.408 s for constructing a 20k-actor scene, ~71.6 ms -> ~59.3 ms for a 125k-HEX surface extraction, ~1.71 us -> ~25.4 ns for lookup of the last field in a 256-field AttributeSet, and ~8.71 ns/value -> ~0.75 ns/value for a forced-cold 2M-float scalar range scan. BVH refit measured ~14.5 ms versus ~90 ms rebuild for 180k triangles. These measurements are machine/compiler-specific regression references, not API performance guarantees.

## 0.32.0 - External OpenGL GUI integration and dependency observers

- Added toolkit-neutral `FVizExternalOpenGLSurface` and external-context render-window / renderer-widget creation APIs. Hosts can provide make-current, physical framebuffer sizing, default-FBO, present, and render-request callbacks while retaining ownership of the context and surface.
- Updated the Win32 modern OpenGL renderer so the host-provided default framebuffer is respected by normal rendering, weighted OIT, FXAA, color/depth readback, and integer hardware picking instead of assuming framebuffer zero.
- Added external-context lifecycle APIs that release only FEAViz GPU resources before a host context is destroyed and reinitialize those resources on a replacement context while preserving renderer, scene, camera, actor, and interactor objects.
- Added `FVizQtOpenGLWindow` (`QOpenGLWindow`, QtGui) and `FVizQtOpenGLWidget` (`QOpenGLWidget`, Qt Widgets). They route Qt input directly into the FEAViz interactor, scale pointer/resize coordinates to physical pixels, schedule redraw through Qt `update()`, render during `paintGL()`, and recover from Qt OpenGL context recreation.
- Retained the existing `FVizQtWindow` / `FVizQtWidget` native-child path, giving applications an explicit choice between FEAViz-owned WGL child windows and Qt-owned OpenGL composition. Added Qt external-context examples for both QtGui and Qt Widgets.
- Extended VTK-style dependency observation through pipeline inputs/producers -> mapper/transform -> actor -> scene -> renderer and camera -> renderer. Mapper dependencies now include producer algorithms, output PolyData, and lookup tables; renderer modifications propagate into coalesced render-window requests, while replaced/detached dependencies remove their observer tags to prevent ghost redraws.
- Added optional observable state objects to `FVizAlgorithmCallbacks`. Built-in custom source/filter wrappers bridge their state `ModifiedEvent` into the underlying algorithm, so parameter edits such as CubeSource dimensions invalidate downstream mappers and schedule GUI redraws without manual render calls. The trailing callback field is gated by `struct_size`, preserving the pre-0.32 callback prefix contract.
- Algorithm input ports now observe both direct input data and upstream producer algorithms. Dirty events therefore propagate across multi-stage filter graphs instead of relying only on MTime polling at the next explicit update.
- Added child-aware Scene MTime and scene actor observer tracking so actor/property/mapper mutations are visible to upstream rendering state without manual redraw glue.
- Extended renderer child observation to lights, scalar legends, render passes, 2D/3D text actors, and label overlays. Text actors now bridge their text-property ModifiedEvent, while scalar legends bridge lookup-table plus title/label text-property changes; detach/removal paths remove tags before release to prevent ghost redraws.
- Added regressions for the external-surface contract and observer dependency graph, including detached-scene/light/legend/text/pass isolation, producer/direct-data propagation, lookup-table/PolyData propagation, nested text-property propagation, observable custom-algorithm state, state deletion safety, and callback-prefix compatibility.

## 0.31.0 - Reusable commands, pick/progress events, and coalesced GUI rendering

- Added ref-counted `FVizCommand`, a reusable VTK-style command object that can be registered on multiple `FVizObject` instances/events, carries its own client data, supports an explicit abort flag, and is retained by observer registrations until their tags are removed.
- Added `fviz_object_add_command_observer()` while preserving the lightweight function-callback observer API; command observers participate in the same float-priority, stable-order, mutation-safe dispatch path.
- Added `StartPickEvent`, `PickEvent`, and `EndPickEvent` with typed `FVizPickEventData` for CPU and hardware picks. Successful picks additionally publish `PickEvent` on the participating renderer and actor where available.
- Added pipeline `StartEvent`, `ProgressEvent`, `AbortCheckEvent`, and `EndEvent`. Progress observers can request cancellation by aborting propagation; actual algorithm executions emit lifecycle events while pure executive cache hits do not.
- Added coalesced render requests with `fviz_render_window_request_render()`, pending/serial introspection, and `render_if_requested()`. Repeated requests before a frame collapse into one pending frame while explicit `render()` remains available for synchronous rendering.
- Extended `RenderStartEvent` / `RenderEndEvent` to each renderer participating in the Win32 frame, in addition to the existing render-window lifecycle, so per-viewport overlays/profiling can observe renderer work independently.
- Prevented recursive rendering on one native GL context: a render requested from `RenderStart`/`RenderEnd` or another render callback is deferred to the next host frame rather than recursively re-entering WGL.
- Changed dense Win32 interaction/widget redraws to request frames instead of rendering recursively from input handlers. The Win32 backend posts a private render-request message and invalidates later, so a request raised during `WM_PAINT` survives `EndPaint()` validation.
- Extended `FVizWin32RenderControl` and `FVizQtWidget` with render-request/pending helpers so application state changes can schedule rather than force immediate drawing.
- Added `FVizQtWindow`, a QtGui-only `QWindow` adapter for Qt 5/6 on Windows. It owns the native host lifecycle, handles resize/expose/focus/reparent recovery, pumps FEAViz timers without taking Qt's event loop, and can also be embedded into Widgets through a window container.
- Added a QtGui embedding example and expanded algorithm/command/Win32 embedding regressions for lifecycle events, command retention/abort behavior, render-request coalescing, and host-control convenience APIs.

## 0.30.0 - GUI lifecycle and VTK-style object observers

- Promoted observation to every `FVizObject` with stable `FVizObserverTag` handles, float priorities, `AnyEvent`, `ModifiedEvent`, `DeleteEvent`, application `UserEvent` IDs, event-name lookup, abort propagation, and mutation-safe/nested dispatch semantics.
- Unified interaction with the generic object event namespace while preserving the existing interactor callback/observer API for compatibility; `InteractionAny` observes concrete mouse, keyboard, focus, resize, timer, double-click, and character events, while `StartInteraction` / `Interaction` / `EndInteraction` plus Enable/Disable events mirror VTK-style semantic interactor lifecycle notifications.
- Added render-window lifecycle notifications for render start/end, resize, close, focus, DPI change, and native-host reparenting, allowing application code to observe GUI/render state without backend-specific callbacks.
- Added attached-window reparenting so renderer widgets can move to a replacement native host without rebuilding renderer/interactor state; Win32 updates child styles, parent HWND, effective DPI, and native client size, and can recreate only the native/WGL surface if a toolkit destroyed the old host first.
- Added reusable `FVizWin32RenderControl`, a native Win32 GUI control that owns an attached renderer widget and automatically handles layout, focus, visibility/enabled propagation, and host-owned interactor timer pumping.
- Hardened Win32 input routing with X1/X2 mouse buttons, horizontal wheel input, system key/character dispatch, dialog keyboard codes, capture-before-dispatch behavior, and embedded Escape semantics that cancel interaction instead of destroying the GUI-owned viewport.
- Hardened `FVizQtWidget` for docking/reparent workflows by detecting native-host changes, reparenting the FEAViz child, disabling unnecessary Qt paint-engine/backing-store work for the viewport, and suspending the interactor timer pump while hidden.
- Added observer mutation/priority/abort/lifecycle regressions and expanded Windows embedding tests for reparenting, render-control ownership, timer configuration, and native parent/child contracts.

## 0.29.0 - Native host embedding for Win32 and Qt Widgets

- Promoted renderer hosting to a first-class API with `fviz_render_window_create_attached_with_options()` and matching `FVizRendererWidget` constructors, while retaining the existing convenience attach call.
- Added explicit embedded-window introspection (`is_attached`, host native handle, native render handle), widget resize helpers, and `sync_host_size()` so GUI adapters can match the real native client area instead of assuming logical pixels.
- Hardened the Win32 child-window path with exact child-client sizing plus `WS_CLIPSIBLINGS`/`WS_CLIPCHILDREN`, improving layout behavior and reducing redraw interference inside complex native GUIs.
- Embedded windows now reject `run()`/`start()` because the host application owns the event loop. Non-blocking Win32 event processing filters to the FEAViz child HWND when attached instead of peeking the entire thread queue.
- Added an optional Qt 5/Qt 6 Widgets adapter (`FVizQtWidget`) that keeps Qt outside the C17 core, embeds the FEAViz WGL child HWND, synchronizes native-pixel geometry for high-DPI displays, forwards focus, and services FEAViz interactor timers without stealing Qt messages.
- Added standalone Win32 and Qt embedding examples plus a Win32 regression test covering parent/child ownership, native handles, host-size synchronization, and embedded event-loop contracts.

## 0.28.0 - Production FEA results, high-order elements, and VTK-class surface performance

- Reworked UnstructuredGrid surface-face ownership from the former quadratic-style face scan into a cache-friendly canonical-face table with compact 32-bit records and a native-64 fallback. Controlled large HEX benchmarks on the development runner moved surface extraction into the same performance class as VTK 9.6.2 (roughly 1.1-1.3x on representative 125k/262k-cell cases rather than orders of magnitude slower).
- Added quadratic VTK-compatible cell types for Edge3, TRI6, QUAD8, TET10, HEX20, WEDGE15, PYRAMID13, and QUAD9; extended shared cell traits with high-order face/edge topology and added quadratic shape weights for Edge3/TRI6/QUAD8/QUAD9/TET10/HEX20.
- Added high-order boundary tessellation while preserving the optimized first-order HEX8/TET4 path. TET10 and HEX20 now flow through VTU -> UnstructuredGrid -> renderable surface geometry with provenance retained.
- Extended PointLocator/Probe with isoparametric Newton inversion and quadratic interpolation for TET10 and HEX20; off-center linear-field regressions verify high-order locate/interpolate correctness.
- Added `FVizIntegrationPointData` for solver integration/Gauss-point results. Concatenated per-cell integration tuples can be extrapolated to nodes from standard or caller-provided parametric coordinates, with least-squares extrapolation and explicit underdetermined fallback policy. HEX8 2x2x2 and HEX20 3x3x3 regressions recover linear nodal fields.
- Expanded FEA tensor-derived results with mean/hydrostatic stress, principal values/directions, max shear, Tresca, and deviatoric tensors in addition to von Mises.
- Added `FVizMeshQualityFilter` with measure, edge ratio, scaled Jacobian, min/max corner angle, and warpage. Quality evaluation is high-order-aware and uses corner topology rather than treating midside nodes as corners; output shallow-shares the source mesh.
- Generalized elemental-to-nodal averaging to arbitrary numeric component counts, including vector/tensor arrays and active-role transfer, while reusing one point-valence pass and shallow-sharing geometry/topology.
- Added `FVizUnstructuredGridGeometryFilter` / mixed-dimensional geometry extraction so vertex, beam/truss, shell/membrane, and solid cells can coexist in one FEA grid. Linear/quadratic beam and shell cells are tessellated alongside exterior solid faces with original cell/face/point provenance.
- Added `FVizWarpVectorFilter` for surface-first deformation. PolyData topology and attributes are shared while only point coordinates are rebuilt, enabling extract-surface-once / warp-boundary-per-frame animation pipelines.
- Added `FVizTubeFilter` for beam/truss display geometry and `FVizShellExtrusionFilter` for finite shell thickness. Both preserve mapped point/cell result arrays and produce render-ready triangle surfaces.
- Added recursive global field statistics/extrema across PolyData, UnstructuredGrid, ImageData, partitioned data, and temporal data, including leaf/partition/time/tuple/world-position provenance for contour legends and Min/Max annotations.
- Upgraded `FVizPVDReader` from a last-frame cache to a configurable LRU timestep working set with hit/miss/eviction statistics for interactive temporal scrubbing.
- Added `25_FEAProduction`, high-order/mixed-geometry/integration-point/quality regressions, and surface-extraction/cell-to-point performance smoke coverage.

## 0.27.0 - Temporal/composite data, VTP/PVD, resampling, and derived results

- Added `FVizPartitionedDataSet` as a retained composite/partition foundation for FEA parts/instances, with named partitions, validation, and child-aware MTime.
- Added `FVizTemporalDataSet` with strictly sorted finite time steps, time-range queries, nearest-step binary lookup, and retained per-step data objects.
- Extended algorithm output metadata with time steps/time range and added request-time helpers. Single-upstream algorithms inherit temporal metadata automatically through the executive, while the existing request hash keeps output caches time-specific.
- Added VTK XML PolyData (`.vtp`) ASCII reader/writer with native 64-bit Verts/Lines/Polys/Strips connectivity, point/cell/field arrays, active attribute roles, XML entity handling, checked limits, and explicit rejection of unsupported non-ASCII encodings.
- Added PVD collection read/write plus `FVizPVDReader` as a temporal pipeline source. Equal-time entries are grouped into `FVizPartitionedDataSet`; single-entry time steps remain direct PolyData/UnstructuredGrid outputs. Nearest-time selection uses binary lookup and caches the selected time group.
- Validated bidirectional ASCII VTP interoperability against installed VTK 9.6.2. This exposed and fixed tag-unbounded XML attribute lookup, VTK `FieldData` placement/`NumberOfTuples`, and nested `InformationKey` handling.
- Hardened the VTP contract so unsupported binary/appended/compressed encodings and Multi-Piece VTP fail explicitly instead of partially decoding or silently dropping pieces.
- Added `FVizResampleWithDataSet`: PolyData sampling geometry can sample all valid ImageData point arrays through one shared structured interpolation stencil or sample UnstructuredGrid through the accelerated Probe/PointLocator path. Active roles and `FVizValidPointMask` are preserved.
- Optimized ImageData resampling by caching raw array storage/type/stride and reusing each point stencil across every array/component. On the development runner the 100k-point scalar+vector benchmark improved from roughly 0.156 s with independent component sampling to roughly 0.058 s through the shared-stencil path (~2.7x for that controlled workload).
- Added `FVizArrayCalculator` operations for component extraction, vector magnitude, 6/9-component von Mises stress, and scale/offset, with parallel execution above a configurable threshold. Added `FVizArrayCalculatorFilter` so derived point/cell arrays participate directly in PolyData pipelines and can become active scalars.
- Continued native-ID cleanup in PolyData filters: TriangleFilter and AppendPolyData now consume `FVizCellView`/`FVizId` topology rather than assuming uint32 logical connectivity.
- Added temporal/composite, VTP/PVD, resampling/calculator regressions, escaped-XML round-trip coverage, `24_TemporalResample`, and a structured-resample benchmark smoke gate.

## 0.26.0 - ImageData, native 64-bit connectivity, and data-path optimization

- Added `FVizImageData` with VTK-style inclusive extents, origin/spacing/direction, point/cell/field attributes, structured point/cell IDs, physical/index transforms, bounds, scalar allocation, and linear physical-space sampling.
- Added `FVizImageDataGeometryFilter` for structured 0D/1D/2D/3D geometry extraction with point/field attributes, transitive `FVizOriginalCellIds`, cell-data replication, normals, and exact boundary-surface reserve sizing for 3D volumes.
- Generalized `FVizCellArray` to `UINT32` or `UINT64` connectivity storage with auto-promotion, explicit conversion, deep-copy preservation, width-agnostic `FVizCellView`, checked down-conversion, and native `FVizId` fixed-cell/bulk append APIs.
- Added native-ID point/cell ingestion to `FVizPoints`, `FVizPolyData`, and `FVizUnstructuredGrid`. Legacy uint32 APIs remain compatible and keep render-ready triangle/line fast paths for ordinary meshes.
- Added shared `FVizCellTypeTraits` for dimension, arity, edges, faces, and linear shape weights; surface extraction and probe/locator interpolation now reuse the shared topology/interpolation contracts.
- Upgraded VTU connectivity read/write to the native `FVizId` path and removed the previous UINT32 connectivity rejection for decoded non-negative 64-bit IDs.
- Migrated UnstructuredGrid clone/warp/threshold, surface/slice, PointLocator, and Probe hot paths to width-agnostic cell views so forced UINT64 storage with ordinary IDs remains functional.
- Reworked cell-data-to-point-data from a point-by-cell scan to a single cell-connectivity accumulation pass, reducing complexity from O(P*C*k) to O(C*k+P), reusing scratch buffers across arrays, and bulk-writing output tuples.
- Added ImageData/native-ID regressions, a structured-data example, and a `CellToPoint` benchmark smoke gate.

## 0.25.0 - Selection 2.0 and large-scene rendering

- Reworked `FVizSelection` as a deduplicated identity set over `(Actor, association, rendered ID)` with `Replace`, `Add`, `Subtract`, and `Toggle` merge semantics, self-safe application, and new `EDGE` / `GLYPH_INSTANCE` associations.
- Added `FVizSelectionModel` as the shared current/hover selection controller, including modifier mapping, configurable/throttled hover updates, click/rectangle/polygon helpers, and world-frustum selection application.
- Replaced RGB-packed modern hardware picking with a dedicated single-sample `RGBA32UI + depth` framebuffer. Integer records carry actor, association, primitive/point/edge/glyph-instance IDs without the previous 8-bit actor/24-bit primitive color packing limits.
- Added direct integer point and edge picking. A depth-only surface prepass prevents occluded points/edges from selecting through visible surfaces; cell-based/CPU refinement remains a portable fallback when exact GPU raster picking is unavailable.
- Added headless region-selection utilities for Actor/Point/Cell/Edge/GlyphInstance associations across rectangle, arbitrary polygon/lasso, and 3D frustum queries while honoring actor visibility/pickability and renderer culling policy where screen-space semantics apply.
- Added `Actor.pickable`, world-transformed actor-bounds caching keyed by geometry/transform revisions, renderer frustum-plane caching, and pre-upload frustum rejection in surface/edge/point/selection GPU paths.
- Added optional small-object screen-space culling with a projected-diameter threshold. It is disabled by default and only affects render/screen-selection policy; world-frustum engineering selection remains data-complete.
- Extended render statistics with considered/frustum-culled/small-object-culled actor counts plus optional asynchronous GPU elapsed-time query results; timer queries degrade cleanly to unavailable when driver entry points are missing.
- Hardened the integer selection pass by preserving framebuffer/program/VAO/viewport/depth/cull/polygon/color-mask/line/point state, restoring Win32 viewport/scissor state around platform picks, and supporting Actor picking for surface, line-only, point-only, and instanced glyph props.
- Marked widget-representation and selection-highlight helper actors non-pickable so engineering helpers do not mask model selection. Expanded selection highlights to Actor/Point/Cell/Edge/GlyphInstance records.
- Added cached `FVizFrustum` math, a large-scene benchmark, Selection 2.0/region/highlight/model regressions, and `22_SelectionLargeScene` demonstrating pickability, small-object render culling, screen selection, world-frustum selection, and modifier composition.

## 0.24.0 - Widget/representation framework and engineering manipulators

- Added a VTK-style `FVizWidget` foundation with explicit enabled/start/hover/active states, high-priority interactor event routing, focus-aware begin/end/cancel semantics, headless event processing, runtime interactor attach/detach, and rollback-safe observer reconfiguration.
- Added `FVizWidgetRepresentation` with retained renderer ownership for geometry actors, 2D text actors, billboard labels, and 3D label sets. Per-child local visibility is masked by representation visibility so disable/enable does not corrupt internal child state.
- Added `FVizWidgetManipulator` with view-plane, explicit-plane, and axis constraints based on display-to-world rays; manipulators capture their active constraint at interaction start for deterministic drags.
- Added `FVizHandleWidget` with screen-space hit testing, pixel-stable sphere-impostor presentation, view/plane/axis constrained dragging, and Escape rollback.
- Added `FVizPlaneWidget` with translated/normal-constrained manipulation, translucent plane representation, normal indicator, and change propagation hooks used by section cutting.
- Added `FVizBoxWidget` with wireframe/corner representation, six screen-space face handles, axis-constrained face resizing, whole-box translation, minimum-extent guards, and rollback on cancel.
- Added `FVizLineWidget` with endpoint handles, segment translation, pixel-space hit testing, round AA line/point presentation, and cancel/restore semantics.
- Added `FVizDistanceWidget` and `FVizAngleWidget` with geometry plus billboard measurement labels, programmatic point assignment, reset/completion state, and interaction-ready world picking.
- Added `FVizSectionCutWidget`, which binds one plane widget to multiple actors using stable mapper clipping-plane identifiers. Removing the widget removes only the planes it owns and preserves unrelated application clipping planes.
- Added `FVizProbeWidget` with selection/result label ownership, optional result-array probing, and representation-managed annotation lifecycle.
- Added stable `FVizClipPlaneId` mapper APIs for add/update/remove without clearing or reindexing unrelated clipping planes, and added `fviz_renderer_world_to_display()` for screen-space widget hit testing.
- Hardened widget lifetime behavior: interactor callbacks retain the widget during dispatch; interactor/priority observer changes roll back if reattachment fails.
- Hardened the Win32 OpenGL procedure loader by replacing object-pointer/function-pointer casts with size-checked `PROC` bit copies, allowing the loader and GL device to pass strict C17 pedantic warnings-as-errors gates.
- Added `21_Widgets` plus headless widget regression coverage for manipulator geometry, handle/line/box interaction, representation child visibility, measurements, probe lifecycle, and section-cut clipping-plane ownership.

## 0.23.0 - Text, annotations, DPI scaling, and polyline join refinement

- Added a dependency-free text subsystem: `FVizFontAtlas`, `FVizFont`, `FVizTextProperty`, `FVizTextActor2D`, and `FVizBillboardTextActor3D`, with UTF-8 decoding, multiline measurement/layout, alignment, backgrounds, and shadows.
- Added an owned custom coverage-atlas API with Unicode codepoint glyph tables, sorting/binary lookup, fallback glyphs, and `FVizFont` construction from externally rasterized atlases; FreeType/DirectWrite can therefore be integrated without becoming FEAViz core dependencies.
- Added a persistent modern-OpenGL text atlas texture and dynamic VBO path; CPU glyph staging is device-owned and geometrically grown to avoid per-label allocation churn.
- Added `FVizLabelSet3D` for scalable node/element-style annotations. Label sets share one text property and batch visible glyphs into one draw when background/shadow styling does not require per-label passes.
- Added renderer-owned 2D text, billboard text, and 3D label-set collections with retained ownership and duplicate-add protection.
- Upgraded `FVizScalarLegend` with units, configurable tick counts, validated floating-point label formats, separate title/label text properties, and rendered numeric/title labels.
- Propagated render-window DPI content scale into text glyph geometry, pixel offsets, display-pixel anchors, and scalar-legend layout while preserving normalized-viewport anchors.
- Refined shader polyline joins using adjacency buffers, miter-limit handling, and endpoint-specific round coverage while retaining ordinary `GL_LINES` index buffers for capability fallback.
- Added `20_TextAnnotations`, text/annotation contract tests (including custom Unicode atlas fallback), and a 20k-label benchmark with O(1) label-set MTime queries.
- Hardened string-backed rendering properties after sanitizer testing: `FVizString` set/append are now self-alias safe across reallocations/overlap, and `FVizScalarLegend` preserves child ownership while aggregating child MTimes.
- Native Win32/OpenGL visual validation remains a target-machine/Windows-CI responsibility; portable gates validate text/layout logic and strict backend compilation.

## 0.22.0 - Antialiased primitives and GPU-instanced glyphs

- Expanded modern line rendering with pixel-stable shader widths, analytical AA, butt/square/round cap control, dash/gap/phase patterns, and optional scalar/instance coloring.
- Generalized the GPU line path to render native `Line` and multi-point `PolyLine` cells directly instead of relying only on legacy two-point line indices.
- Added point rendering from `Verts`/`PolyVertex` topology with square, analytically antialiased circle, and lightweight sphere-impostor styles, pixel sizing, color/scalar control, and opaque point depth writes.
- Added `FVizGlyphMapper` with bulk retained instances (`position + quaternion + scale + RGBA`) and true OpenGL instanced triangle/line/point draws; one source mesh remains resident regardless of instance count.
- Added data-driven vector-glyph construction from named or active three-component point arrays, including +X-to-vector orientation, optional magnitude scaling, magnitude color interpolation, non-finite/zero-vector rejection, and opacity control.
- Added `FVizArrowSource`, a demand-driven unit-arrow PolyData source aligned to +X with configurable shaft/tip dimensions and radial resolution for FEA loads, boundary-condition symbols, and vector fields.
- Split glyph-instance GPU updates from source-geometry revisions and use dynamic instance-buffer capacity with `glBufferSubData` reuse when possible, avoiding source VBO rebuilds during vector animation.
- Separated source-line and lazily-created triangle-edge GPU index buffers so toggling surface edges/wireframe no longer invalidates/re-uploads surface geometry; point topology is likewise independent of point style visibility.
- Corrected scene bounds to include actor transforms and glyph-instance transforms, and optimized 100k-glyph bounds evaluation using transformed center/half-extents rather than eight corner transforms per instance.
- Added `19_PrimitivesGlyphs`, dedicated ArrowSource/primitive/glyph regression tests, and a 100k-instance glyph benchmark.
- Glyph-instance hardware selection remains intentionally deferred to Selection 2.0 integer-ID targets; current hardware selection skips glyph actors rather than returning ambiguous IDs.
- Full polyline adjacency-aware miter/bevel/round joins remain a follow-up item; 0.22 cap/dash semantics are complete for segment-based lines without claiming unsupported join behavior.

## 0.21.0 - Render targets, order-independent transparency, and shader lines

- Added backend-neutral `FVizRenderTarget` descriptors with typed color/depth/integer formats, multisample metadata, validation, resize semantics, and memory-footprint estimation.
- Added per-renderer transparency strategies (`SORTED`, `WEIGHTED_BLENDED`, `DEPTH_PEELING` request) plus tunable weighted-OIT parameters and explicit capability/statistics fallback reporting.
- Implemented an OpenGL 3.3 weighted-blended OIT path using MSAA-matched accumulation/depth renderbuffers, opaque-depth seeding, separate accumulation/revealage passes, single-sample resolves, and fullscreen compositing.
- Kept sorted back-to-front alpha as the portable fallback when weighted OIT resources/shaders are unavailable; depth-peeling requests currently fall back explicitly rather than silently claiming support.
- Added WGL sRGB-capable pixel-format negotiation with graceful non-sRGB fallback and runtime sRGB capability reporting.
- Made the FXAA resolve texture sRGB-aware so post-processing samples linearized color when the native framebuffer is sRGB-capable.
- Added a modern shader-expanded edge path: logical GL lines are expanded by a geometry shader into screen-space quads with pixel-stable width and analytical one-pixel coverage AA; legacy `glLineWidth` remains fallback.
- Added renderer capability reporting for weighted OIT and shader-expanded lines.
- Added `18_Transparency` and render-target/transparency regression coverage.

## 0.20.0 - Renderer, interaction, and antialiasing

- Added perspective/parallel camera projection with parallel scale, projection-preserving `P` toggle, pixel-correct pan math, camera MTime propagation, and raster pixel-center pick rays.
- Expanded trackball interaction into explicit rotate/pan/dolly states with Shift/Ctrl left-button mappings, wheel magnitude, double-click fit, key-up/character events, mouse deltas, drag-threshold accumulation, capture/focus cancellation, and correct multi-viewport event routing.
- Added DPI/content-scale reporting and propagated content scale through interaction events; Win32 handles `WM_DPICHANGED` without imposing process-global DPI policy.
- Added render-window quality options for requested MSAA, FXAA, adaptive AA, swap interval, runtime FXAA tuning, actual capability reporting, and per-frame render/GPU-upload statistics.
- Added WGL multisample pixel-format selection with graceful sample-count fallback, OpenGL 3.3 core context creation, compatibility fallback without a second `SetPixelFormat`, and swap-control capability detection.
- Added an MSAA-safe FXAA resolve path using framebuffer blit to a single-sample texture followed by a fullscreen FXAA pass; FXAA is optional so shader/FBO failure does not disable the modern renderer.
- Added adaptive interaction AA: MSAA remains active during manipulation while the FXAA post-pass may be skipped for lower latency, followed by a forced full-quality frame when interaction ends.
- Added actor material controls (ambient/diffuse/specular/specular power), smooth/flat shading, front/back/no culling, inverse-transpose normal transforms, and matching modern/legacy renderer behavior.
- Added `FVizLight` and per-renderer retained lighting with one default headlight plus configurable scene/head lights, color, intensity, enable state, and up to four active lights.
- Added gradient backgrounds, back-to-front translucent actor sorting, persistent overlay/legend GPU buffers, frame-aware stale actor-resource collection, and stricter GL state restoration around post-processing and hardware selection.
- Split mapper render-data revision from uniform-only clipping state so camera motion, actor transforms, and clipping-plane drags do not invalidate/re-upload VBO data; common high-frequency interaction paths now stay GPU-resident.
- Added interaction frame pacing on Win32 using the interactor desired update rate, while mouse-up/wheel/key/double-click operations force a final rendered frame.
- Hardened hardware ID picking by temporarily disabling MSAA/dither, honoring per-actor culling, and restoring blend/depth/cull/dither/depth-write state after selection.
- Added `FVizRendererWidget_create_with_options` and the `17_RenderQuality` example covering gradient background, material lighting, requested 8x MSAA, tuned FXAA, and backend capability reporting.
- Restored the missing OBJ cube regression asset and updated camera-pick regression expectations for pixel-center raster coordinates.
- Updated the install-tree consumer gate to validate runtime semantic/ABI versions against the installed generated version macros instead of a stale hard-coded 0.16 minor version.

## 0.19.0 - VTK mesh processing and performance

- Reworked dynamic mesh construction around geometric capacity growth and added bulk append/mutation APIs for generic arrays, data arrays, Points, PolyData triangles/lines, and fixed-size UnstructuredGrid cells.
- Converted Plane/Sphere sources, AppendPolyData point assembly, PolyData normal output, UnstructuredGrid surface extraction, and VTU point ingestion to bulk data paths to reduce allocation, MTime, and per-element call overhead.
- Added demand-driven `FVizClipPolyDataFilter` with shared edge-intersection caching, point-attribute interpolation, cell-data remapping, `insideOut`, and transitive original-cell provenance.
- Added parallel CSR-based `FVizSmoothPolyDataFilter` and deterministic vertex-clustering `FVizDecimatePolyDataFilter`, including point/cell attribute transfer and regenerated normals.
- Added `FVizProbeFilter` with separate sampling/source ports, interpolation of source point arrays onto PolyData, active-role preservation, and `FVizValidPointMask`.
- Rebuilt `FVizPointLocator` as a cached centroid-split AABB hierarchy with geometry-MTime invalidation, explicit rebuild, deterministic candidate resolution, and safe stale fallback.
- Corrected HEX8 parametric shape-function node signs to match VTK/FEA node ordering, including off-center scalar/vector interpolation regression coverage.
- Improved triangle `FVizBVH` construction with true centroid median selection and ray traversal with near-child ordering plus closest-hit distance pruning.
- Corrected the parallel HEX benchmark to use wall-clock elapsed time and added MeshBuild, PointLocator, and BVH benchmarks as CTest smoke gates.
- Added `16_MeshProcessing` showing a connected `Sphere -> Clip -> Smooth -> Decimate` VTK-style pipeline and added dedicated performance-engineering documentation.
- Hardened new hot paths with checked size arithmetic, 32-bit topology limits, stale-cache behavior, strict warnings, and focused regression coverage.
- Fixed pre-existing signed-shift undefined behavior in VTU Base64 binary decoding and hardened decoded-buffer size arithmetic, caught by the 0.19 UBSan gate.

## 0.18.0 - General PolyData topology and geometry filters

- Generalized `FVizCellArray` and `FVizPolyData` to model VTK-style `Verts`, `Lines`, `Polys`, and `Strips` while retaining the existing render-ready triangle/line fast paths for compatibility.
- Added PolyVertex, PolyLine, arbitrary Polygon/Quad, and TriangleStrip construction, logical cell counts/accessors, generalized validation, deep-copy support, and polygon/strip normal generation.
- Added demand-driven `TriangleFilter`, `PolyDataNormalsFilter`, `FeatureEdgesFilter`, and `PolyDataConnectivityFilter` with typed pipeline ports and deterministic output.
- Added triangle/strip conversion with cell-data remapping and transitive `FVizOriginalCellIds`, so original FEA provenance survives multi-filter pipelines rather than being replaced by intermediate cell indices.
- Extended `AppendPolyDataFilter` to preserve generalized topology and concatenate common point/cell arrays across inputs.
- Extended `CleanPolyDataFilter` to merge points and remap Verts/PolyVertex, Line/PolyLine, Triangle/Quad/Polygon, and TriangleStrip cells while preserving generalized cell attributes and the legacy triangle-cell attribute contract.
- Hardened generalized geometry paths with checked allocation/edge-capacity arithmetic and recomputation of normals for polygon/strip geometry after transforms.
- Added `15_GeneralPolyData` as a headless end-to-end example plus regression coverage for generalized topology, cell-array deep copy, triangulation, normals, feature edges, connectivity, generalized cleaning, append cell attributes, and provenance chaining.

## 0.17.0 - VTK-style sources and PolyData algorithms

- Added demand-driven `PlaneSource`, `CubeSource`, and `SphereSource` algorithms with typed output ports, MTime-aware caching, and direct output/update compatibility APIs.
- Added `TransformPolyDataFilter`, `ElevationFilter`, `AppendPolyDataFilter`, and deterministic tolerance-based `CleanPolyDataFilter` for connected PolyData processing.
- Extended `FVizDataArray` with deep copy, numeric component conversion, component mutation, component ranges, and vector-magnitude ranges.
- Added public shallow/deep copy operations for `FVizAttributeSet` and mutable point get/set operations with lazy PolyData bounds invalidation.
- Added a public pipeline input-clear operation for repeatable-input algorithms.
- Hardened PolyData validation for both triangle and line topology and preserved point, triangle-cell, field, active-role, and legacy-scalar metadata through cleaning.
- Added a VTK-style source/filter example plus strict C17/C++17 header, sanitizer, and end-to-end pipeline regression coverage.
- Replaced optimizer-sensitive fixed-buffer `strncpy` use in legacy threshold/warp/contour filter names so strict optimized Release builds remain warning-clean.
- Added an update-scoped executive visit table so equivalent shared-upstream
  outputs execute once per transaction in diamond/fan-out graphs.
- Extended pipeline DOT diagnostics with request/result state, output MTimes,
  execution counts, and cache-hit counts.
- Parallelized scalar cell-to-point interpolation with deterministic per-point
  reduction and added thread-limit equivalence coverage.
- Parallelized surface-face construction with a deterministic merge phase and
  added serial/parallel topology equivalence coverage.
- Parallelized contour segment construction with deterministic commit ordering
  and added multi-level serial/parallel equivalence coverage.
- Added BVH serial/parallel build equivalence coverage for primitive bounds,
  centroids, and ray-hit stability; recursive tree construction remains serial.
- Parallelized slice cell-plane classification while retaining deterministic
  polygon interpolation/merge and added serial/parallel topology coverage.
- Registered the persistent parallel HEX8 benchmark as a CTest smoke gate for
  release validation.
- Added public API contract regressions covering NULL release, cleared output
  contracts, retain/release lifetime, MTime stability, and attribute replacement.

## 0.16.0 - API and ABI release-candidate stabilization

- Added per-header standalone compilation gates for both strict C17 and C++17.
- Fixed ABI 1 as the shared-library SOVERSION and made the install consumer
  verify both semantic and ABI versions.
- Published ownership, thread-safety, support, IO, migration, and host-integration
  contracts for the 1.0 release candidate.
- Reconciled the active roadmap with the implemented 0.13-0.15 subsystems and
  recorded optional/deferred capabilities explicitly.

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
