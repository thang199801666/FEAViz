# FEAViz performance engineering

Current documented baseline: FEAViz 0.41 development

FEAViz treats performance as a data-structure and algorithm property first, and
as a compiler-tuning problem second. Benchmarks in `benchmarks/` use wall-clock
time and are also registered as smoke tests when both benchmarks and tests are
enabled.

## 0.19 performance work

### Mesh construction and bulk mutation

The dynamic array/cell-array growth path now uses geometric capacity growth
instead of reserving exactly `count + 1` for every appended primitive. Public
bulk APIs were added for arrays, data arrays, points, PolyData triangles/lines,
and fixed-size UnstructuredGrid cells.

During development on the same Linux runner, construction of a 6,561-point /
12,800-triangle grid through the scalar API fell from roughly 1.74-1.78 seconds
to roughly 6.5-6.8 milliseconds after the growth-policy fix. A 320,000-triangle
case that did not finish inside a 120-second development gate completed in about
0.17 seconds afterward. These numbers are development observations rather than
portable performance guarantees; use `FVizBenchmarkMeshBuild` on the target
machine for a reproducible local comparison.

### Static cell-location hierarchy

`FVizPointLocator` builds a centroid-split AABB hierarchy when a grid is bound.
Queries traverse only candidate nodes/cells while preserving a safe brute-force
fallback when the source grid's geometry MTime makes the hierarchy stale.
`FVizBenchmarkPointLocator` reports accelerated and fallback query costs.

### Triangle BVH

`FVizBVH` now performs true median selection by primitive centroid on the largest
node axis. Ray traversal visits the nearer child first and prunes nodes whose
entry distance exceeds the current closest hit. This increases build work but
substantially improves repeated picking/ray-query behavior on large meshes.
`FVizBenchmarkBVH` measures build and repeated ray-query cost.

### Filter data movement

High-fan-out paths use bulk copies where semantics permit them. This includes
Plane/Sphere sources, AppendPolyData point assembly, PolyData normal output,
UnstructuredGrid surface point extraction, and VTU point ingestion. Smooth uses
a CSR adjacency graph built once per execution and parallel point iterations;
Clip caches edge intersections; Probe reuses one locator per update.

### Parallel benchmark timing

The HEX8 parallel benchmark uses wall-clock time (`timespec_get`) rather than
process CPU time, so reported multi-thread scaling reflects elapsed latency.

## 0.20 rendering and interaction performance work

High-frequency visual changes are separated from geometry/scalar-buffer changes.
Camera motion, actor transforms, material/light uniforms, and mapper clipping
planes do not by themselves invalidate resident VBO/VAO/EBO resources. The GL
actor cache uses data MTime plus a mapper render-data revision, and stale actor
resources are reclaimed after they are no longer observed in a frame.

Scalar legends and other simple 2D overlay geometry reuse persistent buffers and
update them with `glBufferSubData` instead of allocating CPU/GPU objects every
frame. Translucent actors are sorted back-to-front before drawing.

The Win32 interactor now uses its desired update rate to frame-pace continuous
mouse manipulation. A final mouse-up render is forced so adaptive AA can restore
the full MSAA + FXAA quality path. Render statistics expose frame time, present
time, draw counts, primitive counts, GPU upload count/bytes, resident actor
resources, sample count, and whether FXAA was actually applied.

Renderer performance depends strongly on GPU/driver/window-system behavior. The
portable CI gates validate common logic and backend syntax, while actual visual
and GPU-timing baselines should be recorded on the target Windows/MSVC/OpenGL
machine.

## 0.22 primitive/glyph performance work

Glyph rendering keeps one source mesh plus a compact per-instance matrix/color buffer. Instance-only changes have their own MTime/revision and use a dynamic VBO; when the existing allocation is large enough, updates use `glBufferSubData` instead of reallocating the source geometry. Source PolyLine indices, lazily-created triangle-edge indices, point indices, and surface geometry are cached independently so edge/wireframe/point visibility changes do not force full mesh uploads.

`FVizBenchmarkGlyphInstances` exercises 100,000 retained glyphs, bulk instance ingestion, transformed bounds, and data-driven vector-to-glyph construction. During 0.22 development on the Linux runner, bulk ingest was about 3.7-3.8 million instances/s. Replacing eight-corner-per-instance bounds expansion with the mathematically equivalent center/half-extent transform reduced the same 100k bounds pass from roughly 128 ms to roughly 48 ms. These are development observations, not cross-platform guarantees; use the benchmark on the target workstation.


## 0.23 text/annotation performance work

Text rendering keeps one persistent atlas texture and one persistent dynamic text VBO. CPU glyph vertices use a device-owned geometrically grown staging allocation rather than allocating/freeing a vertex buffer for every label. `FVizLabelSet3D` provides a batch fast path: labels sharing one text property can be projected and appended to one vertex stream and submitted in a single draw when background/shadow styling does not require separate per-label geometry.

Label-set MTime lookup is O(1): string changes are only exposed through LabelSet mutation APIs, which mark the owning set modified, so render invalidation does not scan every label string. `FVizBenchmarkTextLabels` exercises 20,000 retained labels, repeated MTime queries, and text measurement. Development measurements are machine-specific and must not be treated as performance guarantees.

## 0.25 selection and large-scene performance work

Actor world bounds are cached against geometry MTime, actor transform revisions, and user-transform MTime. Camera frustum planes are cached against camera MTime plus aspect ratio, so steady-state camera culling avoids repeated world-AABB and view-projection reconstruction. Modern surface, edge, point, and integer-selection paths reject out-of-frustum actors before `ensure_actor_resource()`, preventing unnecessary GPU uploads for invisible props.

Optional screen-space small-object culling uses a conservative world-bounds sphere projected through perspective or parallel camera parameters. It is disabled by default. `FVizBenchmarkLargeScene` exercises 20,000 shared-geometry actors and reports steady-state cull throughput. During 0.25 development on the Linux runner, cached frustum testing improved from roughly 1.84 million to roughly 3.46 million actor tests/s in that benchmark; this is a development observation, not a cross-platform guarantee.

Modern OpenGL can also use two rotating `GL_TIME_ELAPSED` queries. Results are polled asynchronously in subsequent frames and exposed as `gpu_frame_nanoseconds` only when available, avoiding a deliberate CPU/GPU synchronization point. The current metric covers the main GL render-pass interval bracketed by device begin/end-frame; post-process FXAA and buffer presentation are intentionally outside that query.


## 0.33 core/data-path performance work

FEAViz 0.33 treats high-frequency FEA updates as logical mutations rather than
sequences of private-container mutations. Internal array helpers can resize,
append, and clear without publishing MTime changes; the public owning object
publishes one `ModifiedEvent` when the logical operation succeeds. New bounded
batch update APIs for DataArray tuples and Points support frame-by-frame result
and deformation playback without raw-pointer bookkeeping.

`FVizDataArray` has specialized contiguous float32/float64 range loops and a
last-query cache keyed by component, non-finite policy, and local MTime.
`FVizAttributeSet` builds an adaptive lazy hash index only once its array count
passes the small-set threshold. The index remains an accelerator rather than a
correctness dependency: exact names are validated and allocation/collision
failures fall back to the linear path. This keeps tiny attribute sets cheap while
removing O(N) name scans from field-rich result files.

Spatial acceleration now distinguishes topology/geometry changes from field-data
changes. `FVizPointLocator` remains valid when only result arrays change and can
`refit` after nodal deformation when cell topology is unchanged. `FVizBVH` can
likewise refit triangle bounds without repartitioning the hierarchy. These APIs
are intended for FEA animation and repeated probe/pick workloads where geometry
moves every frame but connectivity remains stable.

The 0.33 benchmark additions split cold range scans from repeated cached queries
and measure large AttributeSet name lookup. On the development Linux runner,
controlled pristine-0.32/current comparisons measured approximately 8.71
ns/value versus 0.75 ns/value for a forced-cold 2M-float range scan and about
1.71 us versus 25.4 ns for lookup of the last array in a 256-field set. A 180k
triangle BVH refit measured about 14.5 ms versus about 90 ms for rebuild; an 8k
HEX PointLocator refit measured about 1.17 ms versus roughly 4.2 ms build.
Large-scene construction improved from roughly 0.467 s to 0.408 s for 20k actors
and the 125k-HEX surface benchmark from roughly 71.6 ms to 59.3 ms in median-of-five
controlled runs. These figures are machine/compiler-specific development references, not
portable performance guarantees.

Renderer-side changes avoid repeated no-op MTime propagation, create default
mapper lookup tables only on demand, and reuse culling state at render-pass scope
so rejected actors do not pay transform/material work. The portable benchmark
suite cannot measure Win32/WGL driver time; GPU/backend performance must still be
validated on the target Windows workstation.

## 0.34 parallel/composite/pipeline performance work

The executive now uses an explicit frame stack and dependency-driven fast cache gate.
When a node's request key and algorithm MTime match its published output, the update
returns without walking upstream. In a controlled development comparison using a
4,096-algorithm pass-through chain and 1,000 repeated cache hits, pristine 0.33
measured roughly 2.05-2.15 ms per repeated update, while the 0.34 path measured
about 0.083-0.09 microseconds per update after the initial execution. This is an
algorithmic O(N)-to-O(1) cache-hit change, not a general cross-platform speedup claim.

Composite containers now receive child `ModifiedEvent` notifications and therefore
return aggregate MTime in O(1). A development microbenchmark with 5,000 temporal
steps and 20,000 MTime queries measured about 209 microseconds/query in pristine
0.33 versus roughly 3.4-3.7 ns/query in 0.34. Mutation cost moves to the point where
the child actually changes, which better matches demand-driven pipeline usage.

Field-statistics kernels read typed contiguous storage directly and reduce deterministic
per-chunk extrema/counts. For 2 million three-component float32 tuples with magnitude
statistics, pristine 0.33 measured a median around 79 ms while the 0.34 benchmark was
about 4.0-4.7 ms on the same development runner. Multi-field cell-to-point conversion is
adaptive: the one-field case keeps the serial fast path, while four fields on a
64k-HEX grid measured roughly 10.9-11.1 ms versus about 22.6-23.6 ms for pristine
0.33 in controlled runs.

`FVizArena` provides resettable transient storage for update-scoped scratch, and SMP
primitives now include block-parallel scans plus stable parallel merge passes. Spatial
`update()` APIs use fine-grained geometry/topology revisions to select no-op, refit, or
rebuild automatically. All values above are machine/compiler-specific regression
references; target Windows/MSVC and production models must establish their own baselines.

## Running benchmarks

```sh
cmake -S . -B build-perf \
  -DCMAKE_BUILD_TYPE=Release \
  -DFVIZ_BUILD_BENCHMARKS=ON \
  -DFVIZ_BUILD_TESTS=ON
cmake --build build-perf

build-perf/bin/FVizBenchmarkMeshBuild
build-perf/bin/FVizBenchmarkDataRange
build-perf/bin/FVizBenchmarkAttributeLookup
build-perf/bin/FVizBenchmarkPointLocator
build-perf/bin/FVizBenchmarkBVH
build-perf/bin/FVizBenchmarkParallelHex
build-perf/bin/FVizBenchmarkGlyphInstances
build-perf/bin/FVizBenchmarkTextLabels
build-perf/bin/FVizBenchmarkLargeScene
build-perf/bin/FVizBenchmarkCellToPoint
```

Benchmark output is CSV-friendly. Compare the same executable, compiler,
build type, hardware, and thread-limit configuration when tracking regressions.
Performance numbers are not API contracts.

## 0.42-0.50 core roadmap baseline

The field layer now includes reusable tuple-matrix application, least-squares
operator construction, indexed averaging, discontinuity masks, deterministic
cache keys, and an immutable compiled expression engine with an LRU compile
cache. `FVizBenchmarkExpression` is the regression target for evaluation.

Spatial querying adds closest-point and ordered batch ray/closest-point APIs.
BVH leaf refit and batch queries use the shared parallel runtime, while internal
node reduction remains dependency ordered. `fviz_bvh_memory_size()` exposes CPU
acceleration-structure storage separately from retained mesh memory.

Temporal streaming provides retained futures, cancellation, a bounded
priority-prefetch worker, direction-change cancellation, and count/byte-budgeted
LRU frames. `FVizBenchmarkTemporalFrameCache` protects the synchronous hot-cache
path; asynchronous loader throughput remains provider dependent.

The OpenGL working set distinguishes geometry, attribute, instance, and render
target estimates, supports mapper/glyph pinning, and evicts only unseen unpinned
resources. Active-set pressure is reported rather than turning a memory budget
into a rendering correctness failure.

On the Windows/MSVC v145 development workstation after the 0.50 gate, measured
baselines were 20.476 ns/tuple for a one-million-tuple expression, 51.095
ns/request for one million temporal-cache requests (999,968 hits), and 2.181 ms
to refit a 180,000-triangle BVH versus 36.883 ms to rebuild it (16.91x). Re-run
the benchmark binaries on the same hardware before treating these as thresholds.

## 0.26 data-model and cell-to-point performance work

`FVizCellArray` now uses 32/64-bit storage policies with geometric capacity growth and bulk native-ID append. Width conversion is performed as one pre-sized pass rather than repeated pushes. Unstructured-grid clone/warp/threshold and VTU ingestion use native bulk point/cell paths.

Cell-data-to-point-data was changed from a point-major implementation that scanned every cell for every point to a cell-major accumulation pass followed by one normalization pass. The complexity changes from roughly `O(points * cells * points-per-cell)` to `O(connectivity + points)`, and scratch sum/count arrays are reused across eligible cell-data arrays. On the development Linux runner, the same 2,197-cell / 2,744-point HEX workload measured a median of about 0.164 s in FEAViz 0.25 versus about 0.00145 s after the 0.26 rewrite (roughly 113x for that controlled workload). This is not a cross-platform performance guarantee.

`FVizImageDataGeometryFilter` computes exact structured boundary-triangle reserve sizes instead of reserving proportional to all volume cells, preventing large peak over-allocation for dense 3D images. Structured point materialization also caches extent/origin/spacing/direction once per execution. `FVizBenchmarkCellToPoint` is included as a release smoke benchmark.


## 0.27 temporal/resampling performance work

`FVizResampleWithDataSet` computes ImageData continuous-index coordinates and one compact corner-ID/weight stencil per destination point, then reuses that stencil for every eligible point-data array and every component. Source array storage/type/stride are cached for the request, avoiding repeated high-level component access inside the interpolation loop. UnstructuredGrid sources continue through the accelerated Probe/PointLocator path.

`FVizBenchmarkResampleImage` samples 100,000 PolyData points from an ImageData scalar plus three-component vector field. On the development Linux runner, four independent component sampler calls took about 0.156 s while the shared-stencil filter completed in about 0.058 s (roughly 2.7x for that controlled workload while also producing copied geometry and a validity mask). This number is machine-specific and is not an API or cross-platform performance guarantee.

## 0.28 FEA surface-extraction gate

The 0.28 UnstructuredGrid surface path replaces the historical repeated face scan with a data-oriented canonical-face table. The first-order HEX/TET fast path uses compact 32-bit face records whenever the dataset permits it and keeps a native-ID fallback for wider connectivity.

Representative controlled runs on the development runner, including output triangulation/provenance work, placed FEAViz in the same practical performance class as installed VTK 9.6.2 for large structured HEX meshes. Typical measurements were approximately 0.21-0.24 s versus 0.16-0.20 s at 125k HEX cells and approximately 0.40 s versus 0.315 s around 262k cells. These numbers are regression references for this machine, not cross-platform guarantees.

The release requirement is therefore not a fixed absolute time but preservation of linear/near-linear scaling, no return to O(F^2) face ownership, and no regression of the existing mesh-build, locator, BVH, glyph, resampling, cell-to-point, and large-scene smoke benchmarks.

## 0.35 streaming/invalidation performance work

Objects whose mutable retained dependencies synchronously propagate `ModifiedEvent` no longer rescan every child when their MTime is queried. This changes common read-heavy validity checks on DataSet/ImageData/UnstructuredGrid/PolyData/Actor/Scene/Mapper from repeated dependency walks to O(1) local MTime reads while preserving child-to-parent invalidation. `FVizBenchmarkSceneMTime` guards this property with 5,000 retained actors and repeated scene MTime queries. On the development Linux runner, a controlled pristine-0.34 comparison measured roughly 103-133 microseconds/query versus about 3.3-3.5 nanoseconds/query in 0.35. These are local complexity-regression observations, not cross-platform timing guarantees.

The mapper/GL cache now distinguishes topology, geometry, attribute/color, and render-state revisions. On the Windows OpenGL path, topology/count changes keep the full VAO/VBO rebuild, geometry-only edits use buffer sub-updates, scalar/LUT-only changes refresh the color buffer, and clipping-only changes remain uniform state. The portable revision logic is covered by core tests; native WGL/GPU timing still requires a Windows runner.

Structured/rectilinear sub-extent extraction and unstructured cell-piece extraction compact only requested data and preserve indexed attributes/provenance. A parallel final-compaction experiment in ContourFilter was rejected after benchmark comparison showed it slower than the existing serial publication stage; `FVizBenchmarkContour` remains in the suite to protect the faster path.

### Partition/ghost topology (0.36)

`FVizCellAdjacency` uses canonical facet hashing rather than candidate-neighbor rescans.
`FVizUnstructuredGridPartitionFilter` keeps its piece filter alive across updates, so
adjacency, point links, and deterministic point-owner tables are reused when only node
coordinates/results change. `FVizBenchmarkPartitionGhost` exercises first materialization
and deformation-only rematerialization separately.


## 0.37 demand-driven cache and memory-budget work

PVD/PVTU cache policy can now enforce both an entry-count ceiling and a logical
resident-byte ceiling based on `FVizDataObjectMemoryInfo`. Oversized pieces are
served without retention instead of forcing the cache above its memory budget.
`FVIZ_PIPELINE_REQUEST_FLAG_RELEASE_DATA` also drops non-requested retained output
payloads, and applications can explicitly release algorithm outputs under memory
pressure.

`FVizCompositeGeometryFilter` caches converted geometry by leaf identity and MTime.
A controlled LTO comparison on the development Linux runner used 256 ImageData
leaves (9x9x9 points each). Pristine 0.36 measured about 55.9 ms for each hierarchy-
only update because every leaf reconverted. The 0.37 cache measured about 0.063-
0.068 ms for the same warm hierarchy update and roughly 0.29-0.39 ms when exactly
one leaf changed, while cold conversion remained around 61-66 ms versus roughly
63 ms for the pristine baseline. These values demonstrate the intended complexity
change and are not Windows/GPU or cross-platform guarantees.

`FVizRenderStatistics::resident_mesh_gpu_bytes` estimates resident mapper/glyph mesh
buffer storage separately from per-frame upload bytes. It excludes render targets,
textures, programs and driver overhead, so it is a resource-pressure signal rather
than a total VRAM measurement. Native WGL validation remains required on Windows.

## 0.51-0.60 renderer-core baselines

The executable render graph benchmark compiles a deterministic 64-pass/16-resource
graph 200 times. On the Windows/MSVC v145 development workstation it measured about
20 microseconds per compile. The fixture intentionally overlaps all resource live
ranges, so it reports 16 physical slots and 265,420,800 logical/peak bytes; separate
unit fixtures verify aliasing for non-overlapping compatible targets.

Glyph instance edits retain fixed-size buffers and upload only the dirty range. The
regression fixture changes one instance and observes one 80-byte upload rather than
a full instance-buffer rebuild. The existing million-instance benchmark remains the
throughput/memory baseline target.

Four offscreen viewports rendering four actors backed by one shared mapper retain one
GPU mesh resource. A stable second frame and a camera-only change in one viewport
perform zero geometry uploads. This is a complexity/residency assertion; absolute GPU
frame timing remains adapter- and driver-specific.

Overlay layout is linear in item count plus deterministic prior-item collision checks
(quadratic in the worst case for one dense collision group). Region selection checks
cancellation inside candidate loops and stops scanning as soon as the configured
result limit would be exceeded, bounding published result memory.

## Async executor scheduling

`FVizExecutor` schedules ready futures through a binary max-heap keyed on
(priority descending, sequence ascending). Dependency-blocked continuations are
parked in a waiter chain on their antecedent and promoted when it completes, so a
worker pop is O(log n) in the ready heap and never re-scans the queue for a
blocked task. `FVizBenchmarkExecutor` reports three regression signals:

- independent-task batch throughput (submit N + wait all);
- deep dependent-chain throughput (each link waits for its predecessor, the
  former worst case for the linear scan);
- cancellation/drain throughput with every other future cancelled before wait.

On the Windows/MSVC v145 development workstation a 200,000 independent no-op batch
drained in about 84 ms single-threaded (~2.4M tasks/s). A 50,000-link chain
drained in about 23 ms single-threaded. Because every queue mutation serializes on
one executor mutex, near-no-op tasks contend at high worker counts and throughput
drops; the numbers are machine/compiler-specific regression references, not API
performance guarantees. Re-run the benchmark on the target workstation before
treating them as thresholds.

`FVizBenchmarkPipelineAsyncChain` drains a 32-stage continuation chain of
independent algorithm updates (each stage re-executed) through the executor pool.
On the same development workstation a fresh 100-iteration run completed in about
16 ms single-threaded (~200k stages/s) and the same wall time at four threads;
high worker counts add lock contention because the chain is intentionally serial.
This is a scheduling-overhead regression reference: it covers submit, park,
promote, continuation execution, and terminal-link teardown, not GPU time.
