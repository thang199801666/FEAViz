> **Superseded for active development:** FEAViz now prioritizes Abaqus-like FEA post-processing. See `ABAQUS_FEA_VISUALIZATION_MASTER_PLAN_0_39_TO_1_0.md`. VTK compatibility is maintenance-only unless required by an FEA workflow.

> **Historical/reference document:** from FEAViz 0.40 onward the active roadmap is `FEAVIZ_MODULAR_MASTER_PLAN_0_40_TO_1_0.md`. VTK format/class parity is not a release target; this file is retained only for architectural comparison and regression context.


# FEAViz VTK-parity master plan — 0.38 to 1.0

Status: active execution plan  
Baseline entering plan: FEAViz 0.37.0  
Target: FEAViz 1.0 production FEA visualization toolkit  
Planning date: 2026-08-14

## 1. Target definition

FEAViz does **not** need to duplicate the full VTK class catalog. VTK is a broad
scientific-visualization platform spanning imaging, volume rendering, AMR,
graphs, molecules, geospatial data, charts, distributed rendering, and many
specialized scientific domains. FEAViz is intentionally narrower: a C17-native,
embeddable visualization/data-processing core optimized for finite-element
post-processing and large engineering models.

The 1.0 target is therefore defined in two dimensions:

- **FEA visualization capability:** 85–90% of the VTK capabilities that a
  serious desktop/HPC FEA post-processor actually needs.
- **General VTK breadth:** approximately 55–65%. Missing breadth is acceptable
  when it is outside FEA/geometry/scientific-mesh workflows.

A 1.0 release is considered a credible VTK replacement for FEAViz-based FEA
applications when the application no longer needs VTK for its normal model,
result, rendering, picking, GUI-hosting, or temporal/partitioned-data paths.

## 2. Non-negotiable architecture rules

1. Public core remains C17 and opaque-handle based.
2. Ownership remains explicit retain/release; getters are borrowed unless named
   create/copy/retain.
3. MTime answers cache validity; observers/events answer notification/scheduling.
4. Pipeline execution is demand driven; cache hits must not replay work.
5. Cancellation never commits a partial output as current.
6. Parallel and serial execution must be semantically equivalent.
7. Stable output ordering is preferred over maximum raw throughput unless an API
   explicitly opts into unordered output.
8. Provenance (original node/element/face IDs) is a first-class FEA contract.
9. Large-model work must be measured with benchmark gates, not assumed faster.
10. GUI frameworks own their event loops and, when requested, their GL context.
11. 1.0 ABI is not frozen until the explicit ABI audit phase.
12. New modules must have install-tree consumer coverage before being considered
    complete.

## 3. Definition of done for every phase

Every phase must pass the following gates before its version is closed:

- Release build with warnings-as-errors.
- Full portable unit/regression test suite.
- ASan + UBSan + leak detection on supported portable targets.
- Install-tree consumer compile/link/run.
- Benchmark comparison against the immediately previous released version for
  every touched hot path.
- Deterministic serial-vs-parallel comparison when SMP behavior changes.
- Patch reconstruction check: previous full tree + patch == new full tree.
- Documentation and feature-matrix update.
- Windows/MSVC/WGL runtime validation whenever native Windows rendering code is
  changed; if unavailable in the build environment, the gap must be stated.

## 4. Macro roadmap

| Stage | Versions | Main objective | Exit condition |
|---|---|---|---|
| A | 0.38–0.42 | Executive/composite/async streaming | Large composite temporal graph is demand-driven and cancellable |
| B | 0.43–0.47 | Data model + IO + out-of-core | Large datasets stream without full-memory materialization |
| C | 0.48–0.53 | GPU resource engine + rendering | Million-element playback avoids full CPU/GPU rebuilds |
| D | 0.54–0.59 | FEA algorithms + interaction maturity | Core FEA workflows no longer require VTK fallback |
| E | 0.60–0.69 | Parallel/distributed + huge-model hardening | Strong SMP and optional distributed partition workflows |
| F | 0.70–0.79 | Cross-platform/host/product integration | Win32/Qt and headless paths production-ready |
| G | 0.80–0.89 | Interoperability/quality/ABI preparation | VTK interchange and public contracts stable |
| H | 0.90–0.99 | Release-candidate hardening | No known architectural blocker to VTK replacement |
| 1.0 | 1.0 | Stable production contract | ABI/API/documentation/performance gates frozen |

# Stage A — Executive, composite scheduling, asynchronous streaming

## Phase 0.38 — Parallel composite execution and bounded geometry cache

Status in this development session: **implementation started**.

### Deliverables

- Split composite geometry execution into:
  1. serial hierarchy construction/cache probe,
  2. parallel conversion of only dirty/new leaves,
  3. deterministic serial cache/output commit.
- Keep cache hits O(1) by input identity + MTime.
- Add configurable dirty-leaf parallel threshold.
- Add enable/disable control for deterministic benchmarking/debugging.
- Add converted-leaf cache byte budget.
- LRU eviction when budget is exceeded.
- Oversize leaf bypass: output remains valid but is not retained by cache.
- Extend cache statistics with bytes, evictions, oversize bypasses, parallel
  batches, and parallel leaf-conversion counts.
- Add benchmark comparing serial and parallel cold conversion.
- Add regressions for cache eviction, oversize bypass, and serial/parallel paths.

### Performance gate

For a heterogeneous assembly with >= 128 independently convertible leaves:
parallel cold conversion should show a repeatable improvement on a multi-core
machine, while warm cache-hit hierarchy updates remain within noise of 0.37.

## Phase 0.39 — General asynchronous task/future runtime

### Objective

Introduce a small backend-neutral async layer rather than adding ad-hoc threads
inside every reader.

### Public concepts

- `FVizFuture` / `FVizAsyncTask` opaque handle.
- Pending / Running / Completed / Cancelled / Failed states.
- Cooperative cancellation token integration.
- `wait`, timed wait, poll, result, error, progress.
- Completion callback/event without requiring a GUI event loop.
- Explicit ownership of result payload.

### Runtime design

- Reuse persistent worker infrastructure where safe.
- Separate compute saturation from latency-sensitive IO scheduling.
- Bounded queue; no unbounded thread-per-request behavior.
- Avoid deadlock on nested parallel algorithms.
- Completion callback executes on documented worker/completion context, never
  implicitly on the GUI thread.

### First users

- PVD/PVTU prefetch.
- Large reader decompression.
- CPU render-data preparation.

## Phase 0.40 — Asynchronous temporal/piece streaming

### Deliverables

- `prefetch_time_async` and `prefetch_piece_time_async`.
- Multiple look-ahead requests with cancellation of stale scrub targets.
- Generation IDs so old completions cannot overwrite newer requests.
- Byte-budget aware completion admission.
- Read-ahead policy: next/previous frame, direction-aware playback, bounded N.
- Streaming statistics: queued, active, completed, cancelled, cache-admitted,
  cache-rejected, bytes read.

### FEA playback target

Dragging a frame slider must not synchronously parse/decompress frames that are
not immediately needed for display.

## Phase 0.41 — Information/executive metadata layer

VTK gains substantial flexibility from request metadata. FEAViz should add a
smaller typed equivalent without copying VTK's C++ `vtkInformation` API.

### Deliverables

- Typed pipeline metadata keys for:
  - whole extent,
  - piece availability,
  - time steps/range,
  - ghost levels,
  - data type,
  - memory estimate,
  - capability flags.
- Immutable request snapshot passed downstream/upstream.
- Output information independent from output payload lifetime.
- Information MTime/revision independent from data MTime.
- Query-only information pass that does not allocate heavy outputs.

## Phase 0.42 — General composite executive scheduling

### Objective

Move composite-awareness from individual filters toward the executive.

### Deliverables

- Composite leaf work-plan generation.
- Per-leaf request mapping.
- Parallel independent leaf execution.
- Deterministic composite reassembly.
- Per-leaf cache status and cancellation.
- Partial failure policy: fail-fast by default; optional collect-errors mode for
  diagnostic/batch processing.
- Support MultiBlock and PartitionedDataSet first; Temporal remains explicit time
  selection rather than automatic cross-time fan-out.

### Exit gate for Stage A

A large MultiBlock/Partitioned timestep can execute a connected pipeline while
only dirty/requested leaves run; cancelled scrub requests do not commit stale
results; cached leaves remain O(1) hits.

# Stage B — Data model, zero-copy arrays, IO, out-of-core

## Phase 0.43 — Data-array view/mapped storage model

### Deliverables

- Read-only and mutable array views.
- External-memory arrays with owner/deleter callbacks.
- Strided tuple/component views.
- Numeric conversion view when a filter can consume source type directly.
- Explicit contiguous requirement query.
- Copy-on-request fallback for algorithms requiring packed data.

### Goal

Reader and solver bridges should be able to expose large result arrays without
mandatory deep copies.

## Phase 0.44 — 64-bit topology completion

- Native 64-bit PolyData connectivity end-to-end.
- 64-bit render chunking when GL index limits/platform constraints require it.
- No implicit narrowing in filters, provenance, selection, IO, or spatial trees.
- Stress tests beyond 2^32 logical IDs using synthetic/sparse data where full
  allocation would be impractical.

## Phase 0.45 — VTK XML high-performance IO completion

- Appended/binary/compressed VTP writer and reader symmetry.
- Compressor abstraction.
- Large-array chunked decode to avoid giant temporary base64 buffers.
- Strict schema/overflow validation.
- Round-trip tests against VTK-generated datasets.

## Phase 0.46 — VTKHDF / HDF5

### Priority

High. VTKHDF is a practical path for large partitioned/temporal scientific data.

### Deliverables

- Optional HDF5 dependency, isolated behind IO module configuration.
- Unstructured grid first.
- Partitioned and temporal layout next.
- Selective array loading.
- Piece/time hyperslab selection where format allows.
- Metadata-only open.
- Writer after reader stabilizes.

## Phase 0.47 — Out-of-core data-object cache

- Global or application-owned cache manager.
- Byte budgets and categories (CPU geometry, attributes, temporal, IO decode).
- Pin/unpin semantics for currently rendered frame.
- LRU/2Q-style policy exploration under benchmarks.
- Explicit cache-pressure events.
- Optional memory-mapped backing for immutable arrays.

### Exit gate for Stage B

A multi-GB temporal partitioned model can be inspected with a configured memory
budget significantly below total dataset size, without requiring whole-file
materialization.

# Stage C — GPU resource engine and rendering scalability

## Phase 0.48 — Backend-neutral GPU resource manager

- Resource identity independent from Actor.
- Shared geometry key and mapper/color-state key.
- Explicit CPU/GPU byte accounting.
- Reference-counted resource residency.
- LRU eviction by GPU byte budget.
- Pin resources referenced by active frame until present completes.
- Context-generation tracking for Qt context recreation.

## Phase 0.49 — Persistent/partial buffer updates

FEA topology usually remains static across frames.

- Persistent topology/index buffers.
- Position-only updates for deformation.
- Scalar-only updates for result changes.
- Dirty-range updates when only a subset changes.
- Prefer orphaning/persistent mapping/subdata based on measured driver behavior.
- Upload-byte and stall metrics.

## Phase 0.50 — Upload scheduling and double buffering

- Separate CPU render-data preparation from GL upload.
- Bounded staging buffers.
- Optional double/triple buffer for dynamic result/position streams.
- Avoid writing a buffer still in GPU use.
- Frame serials/fences where supported.

## Phase 0.51 — Draw-packet/batching model

- Build visible draw packets once per frame.
- Group compatible mapper/shader/material state.
- Reuse shared geometry across instances.
- Optional instancing for repeated parts.
- Reduce per-actor GL state transitions.

## Phase 0.52 — Transparency and render-quality convergence

- Dual-depth-peeling implementation and fallback policy.
- OIT quality/performance comparison suite.
- Stable MSAA/FXAA/sRGB combinations.
- Large-line/point rendering hardening.
- Clipping/section passes with consistent picking behavior.

## Phase 0.53 — Text/annotation production backend

- Proper font rasterizer/shaping backend option.
- Unicode shaping and fallback.
- Annotation decluttering/culling.
- Large label-set batching.
- Deterministic text visual baselines.

### Exit gate for Stage C

Playback of a topology-static million-element model changes only the buffers
required by displacement/result updates; GPU memory is budgeted and measurable;
repeated instances share mesh resources.

# Stage D — FEA algorithm and interaction maturity

## Phase 0.54 — High-order element completion

Priority families:

- WEDGE15.
- PYRAMID13.
- TRI6 / QUAD8 mapping completeness.
- Higher-order surface extraction/probe interpolation.
- Consistent integration-point and extrapolation contracts.

## Phase 0.55 — FEA result-expression engine

- Scalar/vector/tensor expressions.
- Invariants and principal values.
- Coordinate-system transforms.
- Derived component caching.
- Expression dependency graph.
- Parallel evaluation.

## Phase 0.56 — Shell/beam specialization

- Shell section points.
- Top/bottom/envelope/average reduction.
- Through-thickness result selection.
- Beam local axes and section profiles.
- Beam force/moment visualization.
- Composite layup metadata hooks where feasible.

## Phase 0.57 — Advanced geometry/filter family

- Robust cutter/plane/implicit-function family.
- Iso-surface on volume cells where meaningful.
- Connectivity/region extraction.
- Gradient/derivative filter.
- Cell/point interpolation policies.
- Better smoothing/decimation.
- Feature edges and boundary extraction hardening.

## Phase 0.58 — Selection/picking maturity

- Composite-path selection identity.
- Stable original FEA identity through partition/composite filters.
- Hardware selection for very large ID domains through chunking/indirection.
- Pick lists and selection masks.
- Selection-aware filter outputs.

## Phase 0.59 — Widget ecosystem

- Plane/section widget.
- Box clip widget.
- Transform gizmo.
- Ruler/distance/angle.
- Probe/result cursor.
- Seed/annotation handles where useful for FEA applications.
- Touch/pen input translation.

### Exit gate for Stage D

The common FEAViewer workflows—deformation, contour, threshold, section,
measurement, selection, probing, animation, high-order geometry, shell/beam
results—have no mandatory VTK fallback.

# Stage E — Parallel, huge model, optional distributed workflows

## Phase 0.60 — SMP algorithm audit

Classify every O(N) / O(N log N) algorithm as:

- already parallel and benchmark-positive;
- serial because overhead wins below threshold;
- parallel candidate;
- fundamentally sequential assembly phase.

Do not parallelize by policy; require benchmark evidence.

## Phase 0.61 — Parallel surface/contour at million-element scale

- Better partitioning/grain selection.
- Per-thread arenas.
- Deterministic merge.
- Avoid extra full-size classification arrays when memory dominates.

## Phase 0.62 — Spatial acceleration scalability

- Parallel BVH tree construction beyond primitive setup.
- Static locator variants.
- Closest-point and neighborhood queries.
- Batched ray/pick queries.
- Refit/rebuild cost model.

## Phase 0.63 — Solver-native ownership/partition maps

- Import partition owner IDs when available.
- Avoid recomputing ownership from arbitrary contiguous cell ranges.
- Preserve solver domain IDs through filters/IO.
- Ghost generation based on native partitions.

## Phase 0.64 — Optional distributed ghost exchange contract

- Backend-neutral exchange interface first.
- Optional MPI implementation later.
- Point/cell owner/global-ID exchange.
- Ghost synchronization for result arrays.
- No MPI types in public core ABI.

## Phase 0.65 — Distributed reductions and statistics

- Global min/max/moments.
- Global histogram/range.
- Explicit local vs global result contracts.

## Phase 0.66–0.69 — Huge-model hardening

- Multi-million cell stress suites.
- Memory-failure injection.
- Cancellation latency targets.
- Allocation-count profiling.
- NUMA/affinity experiments where justified.

# Stage F — Platform, GUI hosting, deployment

## Phase 0.70 — Windows/MSVC production gate

- Visual Studio 2026 / v145 first-class build.
- Native WGL regression application.
- DPI 100/125/150/200%.
- Multi-monitor movement.
- docking/reparent/context-recreation soak tests.
- GPU vendor matrix where hardware is available.

## Phase 0.71 — Qt Widgets production integration

- `FVizQtWidget` native-child path.
- `FVizQtOpenGLWidget` external-context path.
- Shortcut/focus/event propagation.
- Docking, splitters, tabs, hidden/show lifecycle.
- Qt 5/6 build matrix.

## Phase 0.72 — QtGui/QWindow and optional QML investigation

- Harden `FVizQtWindow` / `FVizQtOpenGLWindow`.
- Evaluate QRhi/QQuick integration without contaminating core renderer ABI.
- QML support only if product needs justify it.

## Phase 0.73 — Linux native/offscreen path

- EGL/GLX decision based on target deployment.
- Headless EGL preferred where available.
- CI smoke rendering.

## Phase 0.74 — macOS feasibility checkpoint

OpenGL is deprecated on macOS. Decide whether FEAViz 1.x needs macOS before
committing to a Metal/backend abstraction. Do not distort 1.0 Windows/Linux FEA
scope unless required.

## Phase 0.75–0.79 — Packaging/plugin/deployment

- Componentized CMake packages.
- Optional IO/render backends.
- Runtime plugin contract evaluation.
- Symbol visibility/export audit.
- Static/shared linkage matrix.

# Stage G — Interoperability and public-contract maturity

## Phase 0.80 — VTK interchange oracle suite

Use VTK only as an external validation oracle in tests, not as a runtime
FEAViz dependency.

- Generate reference VTU/VTP/PVTU/PVD/VTKHDF datasets with VTK.
- Read in FEAViz and compare topology/attributes/ghosts.
- Write from FEAViz and validate with VTK/ParaView.
- Cover endian/types/large IDs/compression/empty arrays/NaN/Inf.

## Phase 0.81 — Filter semantic comparison suite

For representative datasets, compare FEAViz and VTK semantics for:

- geometry extraction,
- threshold,
- contour,
- clip/cut,
- normals,
- interpolation,
- ghost handling,
- field associations.

Exact topology ordering need not match if documented; geometric/data semantics
must.

## Phase 0.82 — Public API consistency audit

- Naming.
- Null behavior.
- Output clearing on failure.
- ownership annotations.
- const correctness.
- versioned option structs.
- enum stability.
- thread-safety classification.

## Phase 0.83 — Error and logging model completion

- Per-thread error state behavior.
- Structured error callbacks.
- Context-rich IO/pipeline diagnostics.
- No worker-thread error races.

## Phase 0.84 — Serialization of configuration/state

Optional lightweight serialization for camera, mapper, LUT, renderer, and
pipeline configuration—not raw dataset serialization.

## Phase 0.85–0.89 — Compatibility cleanup

- Deprecate redundant 0.x APIs.
- Keep compatibility wrappers where cheap.
- Prepare migration guide to 1.0.

# Stage H — Release-candidate hardening

## Phase 0.90 — Fuzzing expansion

- VTK XML readers.
- Legacy VTK.
- PVD/PVTU manifests.
- topology inputs.
- pipeline request structures.
- malformed composite trees.

## Phase 0.91 — Failure-injection testing

- Allocation failures.
- Reader partial failures.
- cancelled parallel jobs.
- context loss.
- cache eviction during memory pressure.

## Phase 0.92 — Thread/race hardening

- ThreadSanitizer where toolchain allows.
- Document object-level thread-safety.
- Ensure caches do not silently become shared mutable races.

## Phase 0.93 — Performance baseline freeze

Define permanent benchmark gates for:

- DataArray range/statistics.
- surface/contour.
- partition/ghost.
- deep pipeline cache hit.
- composite cold/warm update.
- temporal piece streaming.
- large scene culling.
- GPU upload bytes/frame on Windows.
- frame playback for representative FEA models.

## Phase 0.94 — Visual regression baseline freeze

- camera/view presets.
- scalar maps.
- OIT/transparency.
- edge/point AA.
- text/scalar legend.
- selection/highlight.
- clipping/section.

## Phase 0.95 — Documentation/reference examples

- Minimal pipeline.
- FEA result playback.
- partitioned temporal model.
- Win32 GUI.
- Qt Widgets GUI.
- external GL host.
- custom algorithm.
- custom observer/command.
- memory-budget streaming.

## Phase 0.96–0.99 — Release candidates

No large new architecture. Only:

- blockers,
- correctness,
- performance regressions,
- missing contracts/docs,
- portability/build fixes.

# FEAViz 1.0 acceptance criteria

## Core/object

- Refcount/object/observer/command contracts stable.
- No known retain cycles in supported object graphs.
- Public thread-safety matrix complete.

## Pipeline

- Typed multi-port algorithms.
- demand-driven piece/extent/time requests.
- information metadata.
- composite leaf scheduling.
- cancellation/progress.
- deterministic cache behavior.
- explicit memory-release policy.

## Data

- PolyData, UnstructuredGrid, ImageData, StructuredGrid, RectilinearGrid,
  MultiBlock, Partitioned, Temporal.
- 64-bit IDs throughout.
- ghost/provenance conventions.
- zero-copy/external arrays.

## Algorithms

- robust geometry/surface/contour/threshold/clip/cut/warp/probe/resample.
- FEA statistics/derived fields.
- common high-order elements.
- shell/beam result paths.

## IO

- legacy VTK + VTU/VTP/PVTU/PVD production quality.
- VTKHDF reader at minimum.
- temporal/piece lazy loading.
- asynchronous prefetch.

## Rendering

- multi-renderer/pass system.
- opaque/transparency/selection/overlay.
- shared GPU resources.
- partial topology/position/color updates.
- memory budget and resident-byte visibility.
- stable Win32 + Qt hosting.

## Interaction

- trackball styles.
- selection model.
- hardware picking.
- engineering widgets and measurements.

## Quality

- Release, sanitizer, install, fuzz, benchmark, visual gates.
- no known high-severity memory/correctness issue.
- migration/API docs complete.

# Priority rules when schedule pressure occurs

If implementation capacity is limited, use this priority order:

1. Correctness and ownership.
2. FEA model/result fidelity and provenance.
3. Demand-driven execution and memory scalability.
4. Measured CPU/GPU performance.
5. Rendering/interaction behavior required by FEAViewer.
6. VTK interoperability.
7. General-purpose breadth.
8. Nice-to-have rendering effects.

# Explicit non-goals for FEAViz 1.0 unless product requirements change

- Source compatibility with VTK C++.
- Every VTK filter.
- Every VTK reader/writer.
- Medical-imaging pipeline parity.
- AMR/HyperTreeGrid parity.
- Graph/molecule/chart ecosystems.
- Full volume-rendering ecosystem.
- ANARI/OSPRay parity.
- Mandatory MPI dependency.
- Mandatory Python/.NET bindings in the core 1.0 gate.

# Current execution checkpoint

The first implementation work under this master plan is Phase 0.38:
parallel dirty-leaf composite geometry execution plus cache byte budgeting. The
next planned implementation after 0.38 is the general async task/future runtime
and asynchronous PVD/PVTU prefetch with cancellation.
