# FEAViz VTK convergence plan

> **Historical/reference document:** from FEAViz 0.40 onward the active roadmap is `FEAVIZ_MODULAR_MASTER_PLAN_0_40_TO_1_0.md`. VTK format/class parity is not a release target; this file is retained only for architectural comparison and regression context.


Status: historical comparison baseline

Baseline: FEAViz 0.21.0

> The active forward plan from FEAViz 0.38 to 1.0 is `VTK_PARITY_MASTER_PLAN_0_38_TO_1_0.md`. This document is retained as the historical convergence record.

Last updated: 2026-08-13

## 1. Objective

FEAViz will become a VTK-style, C17-native visualization and data-processing
toolkit focused on finite-element post-processing. The goal is architectural
convergence with the useful core of VTK, not feature-for-feature duplication of
the complete VTK ecosystem.

The 1.0 target is reached when an application can:

1. Load a large unstructured FEA result without application-side conversion.
2. Connect custom and built-in sources, filters, and sinks through typed ports.
3. Execute the graph through a real demand-driven executive with deterministic
   caching, progress, cancellation, and output-specific requests.
4. Transform, threshold, deform, slice, contour, surface-extract, and color the
   data while preserving original node, face, and element identities.
5. Render opaque, translucent, edge, overlay, and selection passes in one or
   more viewports, onscreen or offscreen.
6. Select a visible result entity and recover its original FEA identity and
   associated scalar/vector/tensor result.
7. Run expensive independent algorithms through a reusable parallel runtime
   without changing deterministic output.
8. Export the processed result through interoperable VTK formats.
9. Install and consume FEAViz as a shared or static package using only its
   documented public API.

## 2. Scope and non-goals

### 2.1 Required for 1.0

- Opaque C17 ABI and explicit retain/release ownership.
- `FVizDataObject`, datasets, polygonal data, and unstructured FEA grids.
- Typed attributes with point, cell, and field associations.
- General source/filter/sink pipeline with a demand-driven executive.
- 64-bit-safe topology and provenance.
- Surface, slice, contour, threshold, warp, transform, and interpolation
  algorithms.
- Multi-renderer OpenGL rendering with explicit render passes.
- Scalar/vector result mapping, transparency, edges, clipping, overlays, and
  hardware-backed selection.
- Trackball-camera, trackball-actor, rubber-band selection, highlighting,
  probing, and orientation widgets.
- Persistent shared-memory parallel execution.
- Legacy VTK and XML VTU interoperability, including compressed VTU writing.
- Windows renderer, portable headless processing, and offscreen rendering.
- Automated build, sanitizer, package, performance, and visual regression
  gates.

### 2.2 Deferred until after 1.0

- Full VTK class or source compatibility.
- Medical image pipelines and volume ray casting.
- AMR, hypertree grids, molecule/graph/table ecosystems, and charting.
- MPI/distributed execution and distributed rendering.
- Python, Java, or .NET wrapping.
- A stable third-party binary plugin ABI.
- Vulkan, Metal, WebGPU, OSPRay, or ANARI backends.
- PBR materials, shadows, SSAO, and cinematic post-processing.

These deferred areas must not shape the 1.0 ABI unless a small backend-neutral
contract is already necessary for the FEA scope.

## 3. Architectural target

```text
Application / host UI
        |
        v
Interaction + widgets ---- Selection / picking
        |                         |
        v                         v
RenderWindow -> Renderer -> ordered RenderPass graph
        |                         |
        +-------------> Mapper / render resources
                               |
                               v
Pipeline Executive -> Algorithm graph -> DataObject hierarchy
        |                    |               |
        |                    v               v
        +------------ cancellation     attributes/topology/provenance
                             |
                             v
                    Parallel runtime

Readers/Writers <------ DataObject hierarchy ------> application data
```

The public boundary remains backend-neutral. Win32, WGL/OpenGL, scheduler
threads, and file-compression implementation details remain private.

## 4. Engineering rules

### 4.1 Ownership

- `*_create()` and explicit copy functions return an owned reference.
- A successful setter retaining an object must document that retention.
- Ordinary getters return borrowed pointers unless their name explicitly says
  `retain`, `copy`, or `create`.
- Output-port proxies remain borrowed and valid only while their producer is
  alive.
- Weak back-references are used for cycles such as executive-to-algorithm and
  interactor-to-window.
- Every public function documents null behavior and whether output parameters
  are cleared on failure.

### 4.2 Modification time and caching

- Every cacheable object has a composite MTime that includes retained children
  affecting its observable output.
- Writing through a raw mutable pointer requires an explicit `Modified()` call.
- Cache keys include request parameters, requested output port, algorithm
  MTime, resolved input MTime, and relevant execution context.
- A failed or cancelled execution never marks output as current.

### 4.3 Determinism

- Thread-limit 1 and thread-limit N produce equivalent topology, attributes,
  bounds, provenance, and selection IDs.
- Parallel compute may be unordered internally; public topology assembly is
  stable unless an API explicitly opts into unordered output.
- Tests compare semantic output rather than allocation addresses.

### 4.4 Extensibility

- Applications can create custom algorithms without including private headers.
- Data, pipeline, rendering, and interaction contracts do not expose OpenGL or
  operating-system handles except through explicit native-integration APIs.
- New behavior is added through callbacks or opaque interfaces, not public
  structure layout.

### 4.5 Compatibility

- Existing 0.x filter/mapper functions remain compatibility wrappers until a
  later major version.
- Public removals require a deprecation cycle.
- ABI stability is not promised until the 1.0 audit is complete.

## 5. Current baseline and principal gaps

| Area | 0.9.0 baseline | Required convergence |
|---|---|---|
| Object runtime | Refcount, type hierarchy, error state, MTime | Contract audit, weak references, thread-safety matrix |
| Pipeline | Typed indexed ports and output proxies | Real requests, custom algorithms, graph transaction, output-specific cache |
| Executive | Request-state labels and update statistics | Actual per-stage dispatch, information propagation, pieces/extents/time |
| Data | Generalized PolyData (`Verts/Lines/Polys/Strips`) and UnstructuredGrid | 64-bit-native PolyData IDs, reusable cell traits, structured datasets |
| Parallel | Persistent global `parallel_for` pool | Context, task groups, cancellation, TLS scratch, deterministic algorithms |
| Rendering | OpenGL scene, viewport, layers, scalar legend | Pass graph, transparency, offscreen FBO, clipping, hardware selection |
| Interaction | Trackball camera, observers, rubber band | Timers, focus/capture, actor style, highlight, widgets, persistent selection |
| IO | OBJ/STL/VTU/legacy readers | Writers, compression, round-trip, streaming and fuzz coverage |
| Delivery | Local MSVC tests and install consumers | CI matrix, sanitizers, visual/performance regression, ABI documentation |

### 5.1 0.19 geometry and performance convergence update

The common mesh-processing layer now includes connected Clip, Smooth, Decimate,
and Probe algorithms in addition to the 0.18 generalized PolyData filters. Clip
interpolates point attributes and preserves provenance; Smooth reuses CSR
adjacency with parallel iterations; Probe uses a cached AABB cell hierarchy.
Mesh construction and common fan-out paths use geometric growth and bulk writes.
The triangle BVH uses true centroid median splits and distance-pruned traversal.
Remaining high-value gaps are native 64-bit PolyData connectivity, `FVizImageData`,
VTP/PVD interchange, higher-quality smoothing/decimation, and broader resampling.

### 5.2 0.20 renderer, interaction, and antialiasing convergence update

The Windows rendering path now requests multisampled pixel formats, exposes the
actual sample count, and can resolve the frame through a tunable FXAA post-pass.
Adaptive quality keeps MSAA during drag while allowing the post-pass to be
skipped until the final still frame. Camera state supports perspective and
parallel projection, materials and up to four lights are renderer-controlled,
and high-frequency camera/actor/clipping changes remain uniform-only rather than
invalidating resident geometry buffers. Interaction has explicit manipulation
states, DPI/content-scale metadata, pixel-derived pan math, multi-viewport
routing, capture-loss cancellation, and desired-update-rate frame pacing.
Remaining renderer gaps with the highest VTK value are depth peeling/OIT, richer
line/point primitives, scalable text/annotation rendering, and native Windows
visual-regression coverage for MSAA/FXAA combinations.

### 5.3 0.21 render-target and transparency convergence update

0.21 introduces backend-neutral render-target descriptors, sRGB-aware WGL/FXAA
handling, and a real weighted-blended OIT backend. The OIT path seeds its depth
attachment from the opaque scene, renders translucent geometry into separate
accumulation and revealage passes using MSAA-matched renderbuffers, resolves
those buffers, then composites over opaque color. Unsupported OIT requests fall
back to sorted alpha and report the applied mode. The first 0.22 rendering work
is also present: modern edge rendering no longer depends on driver line-width
limits and instead expands logical edges into pixel-width screen-space quads with
analytical coverage AA. 0.22 completes segment cap/dash control, direct general-PolyData
Line/PolyLine expansion, point square/circle/sphere-impostor rendering, and a true
GPU-instanced `FVizGlyphMapper` with a reusable `FVizArrowSource`. Glyph instances
can also be generated from active/named point vectors with magnitude scaling/color.
Source-line, triangle-edge, point, source-geometry, and dynamic instance buffers are
kept as separate cache concerns so visual toggles and vector animation avoid full
mesh re-upload. 0.23 adds adjacency buffers with miter-limit and endpoint-aware
round-join refinement while retaining ordinary line buffers for fallback.

### 5.4 0.23 text and annotation convergence update

0.23 adds backend-neutral font-atlas, font, text-property, 2D text actor, billboard
text actor, and batched 3D label-set contracts. A dependency-free built-in coverage
atlas keeps core deployments self-contained, while custom Unicode coverage atlases
allow FreeType, DirectWrite, or other rasterizers to feed FEAViz without becoming
required core dependencies. Modern OpenGL keeps one atlas texture plus persistent
dynamic text buffers, reuses CPU staging storage, and can submit a shared-style 3D
label set as one draw. Render-window content scale is applied to logical text sizes,
pixel offsets, and scalar-legend layout while normalized viewport anchors remain
resolution-independent. Scalar legends now share the text stack for title, units,
format-controlled numeric ticks, and label/title properties.

The next renderer/interaction dependency is not more ad-hoc widgets: 0.24 should
introduce reusable widget representation/manipulator/state-machine contracts, after
which Selection 2.0 can move picking to integer ID render targets and large-scene
interaction can add hover throttling, culling, and LOD.

### 5.5 0.24 widget-framework convergence update

0.24 introduces a reusable `FVizWidget` / `FVizWidgetRepresentation` /
`FVizWidgetManipulator` split. Widgets consume interactor observers at higher
priority than camera styles, representations retain renderer props with a
separate local-visibility mask, and manipulators constrain display-space drags
to view planes, explicit planes, or world axes. The first concrete family is
Handle, Plane, Box, Line, Distance, Angle, SectionCut, and Probe. Stable mapper
clipping-plane IDs let SectionCut own one plane per target actor without
clearing unrelated clipping state.

The remaining interaction dependency shifts to Selection 2.0: integer ID
attachments, point/cell/edge/glyph IDs, region/lasso selection, and throttled
hover should become the shared picking substrate for richer widget handles.

### 5.6 0.25 selection/large-scene convergence update

0.25 replaces color-packed picking with a dedicated integer selection target and makes Actor, Point, Cell, Edge, and GlyphInstance first-class associations. Point/edge raster picking is depth-qualified by an opaque surface prepass; headless rectangle/lasso/frustum selection shares the same association contracts. `FVizSelectionModel` centralizes Replace/Add/Subtract/Toggle and throttled hover behavior so applications no longer need to merge ad-hoc selection lists.

Large-scene rendering now rejects props before GPU resource creation using cached actor world bounds and cached camera frustum planes. Optional projected-size culling handles sub-pixel props, while world-frustum engineering selection deliberately remains independent of render LOD. Optional asynchronous GPU timer queries extend the existing CPU/draw/upload statistics without forcing synchronization. Full alternate-mesh LOD and occlusion-driven submission remain later renderer refinements rather than blockers for the 0.26 data-model work.


## 6. Release dependency order

Version numbers represent dependency milestones, not calendar commitments.

```text
0.9.1 integrity and automated gates
          |
          v
0.10 real executive and public algorithm extensibility
          |
          v
0.11 data contracts, provenance, and 64-bit topology
          |
          +----------------------+
          v                      v
0.12 parallel runtime       0.13 renderer/pass architecture
          |                      |
          +-----------+----------+
                      v
             0.14 interaction/selection
                      |
                      v
             0.15 IO and portability
                      |
                      v
             0.16 API stabilization
                      |
                      v
                     1.0
```

Data provenance is intentionally moved before hardware selection. Selection is
not considered correct until a rendered primitive can be mapped back to the
original FEA entity.

## 7. Milestone 0.9.1 - Integrity and automated gates

### 7.1 Documentation and roadmap reconciliation

- Remove stale or duplicate checkboxes from the existing roadmap.
- Mark already delivered transform and binary-VTU work consistently.
- Replace the stale “Immediate work after 0.8.0” section.
- Give every milestone a scope, validation list, and definition of done.
- Create architecture decision records for ownership, request processing,
  topology ID width, renderer passes, and parallel cancellation.

Deliverables:

- Updated roadmap and changelog.
- `docs/architecture/decisions/` with numbered ADRs.
- A generated or reviewed public API inventory.

### 7.2 Public contract audit

Audit all exported functions for:

- Null input and null output behavior.
- Borrowed versus retained return values.
- Failure atomicity and output clearing.
- Integer multiplication/addition overflow.
- Object lifetime and back-reference cycles.
- MTime propagation.
- Thread-safe, read-only-thread-safe, or externally synchronized status.
- Consistent `FVizResult` and error-message behavior.

Required regressions:

- Setter self-assignment.
- Replacing a retained child with an already-owned child.
- Allocation failure at every owned allocation boundary.
- Release order permutations for connected pipelines and window/interactor
  pairs.
- Invalid object type passed through an opaque pointer.

### 7.3 CI and package gates

Add CI configurations for:

| Configuration | Build | Tests | Package consumer |
|---|---:|---:|---:|
| Windows MSVC shared Debug | yes | yes | yes |
| Windows MSVC static Debug | yes | yes | yes |
| Windows MSVC shared Release | yes | smoke | yes |
| Linux Clang shared Debug | headless | yes | yes |
| Linux Clang static Debug | headless | yes | yes |
| Linux Clang ASan+UBSan | headless | yes | no |

The install consumer must configure from a clean directory and may not use a
source-tree include or library path.

### 7.4 Reader fuzzing and malformed input

- Isolate byte-buffer entry points for VTU, legacy VTK, STL, and OBJ parsing.
- Add a deterministic malformed-input corpus.
- Add libFuzzer or an equivalent Clang fuzz target outside ordinary CTest.
- Enforce size, tuple-count, offset, connectivity, and decompression limits.
- Ensure every parser failure frees partial objects.

### 7.5 Definition of done

- All current tests pass in the CI matrix.
- Shared/static install consumers run automatically.
- ASan/UBSan report no failures in core, pipeline, and IO tests.
- Roadmap and changelog contain no known contradictory completion state.
- No new feature work is merged without its required gate.

## 8. Milestone 0.10 - Real executive and extensible algorithms

This milestone is the highest architectural priority.

### 8.1 Request contract

Introduce an opaque or versioned request descriptor conceptually containing:

```c
typedef struct FVizPipelineRequest
{
    FVizPipelineRequestType type;
    uint32_t requested_output_port;
    uint32_t piece;
    uint32_t number_of_pieces;
    uint32_t ghost_levels;
    FVizBool has_extent;
    int64_t extent[6];
    FVizBool has_time;
    double time;
    FVizPipelineFlags flags;
} FVizPipelineRequest;
```

The final API may keep this structure opaque, but it must support:

- `REQUEST_INFORMATION`.
- `REQUEST_DATA_OBJECT`.
- `REQUEST_UPDATE_EXTENT`.
- `REQUEST_DATA`.
- Requested output port.
- Whole extent or piece requests.
- Optional time value.
- Exact-extent and release-data flags.
- Cancellation context.

### 8.2 Public custom algorithm API

Add a stable C callback interface for source, filter, and sink algorithms:

```c
typedef struct FVizAlgorithmCallbacks
{
    FVizAlgorithmProcessRequestFn process_request;
    FVizAlgorithmDestroyStateFn destroy_state;
    FVizAlgorithmGetMTimeFn get_state_mtime;
} FVizAlgorithmCallbacks;
```

Required APIs:

- Create a custom algorithm with N input and M output ports.
- Configure port type, optional, and repeatable metadata.
- Access resolved input `(port, connection)` inside a callback.
- Allocate or replace a requested output.
- Report progress and query cancellation.
- Associate user state with an explicit destructor.

The callback must not depend on private object layouts.

### 8.3 Execution transaction

Add an update-scoped graph context containing:

- Monotonic transaction ID.
- Algorithm/output visit state.
- Request key and requested output.
- Shared-upstream completion table.
- Cancellation state.
- Root error and failing algorithm.
- Progress aggregation state.

Rules:

- A shared upstream output executes at most once for an equivalent request in
  one transaction.
- Re-entrant update of an active algorithm returns a deterministic error.
- A failure stops dependent downstream execution but preserves unrelated cached
  outputs.
- Cancellation travels upstream and downstream without publishing incomplete
  output.
- Only requested output ports must be produced unless an algorithm declares
  outputs coupled.

### 8.4 Information propagation and cache keys

Port information should support at minimum:

- Declared data-object type.
- Whole extent or number of pieces.
- Available time steps/range.
- Attribute availability by name and association.
- Estimated memory size when known.

Cache validity must include:

- Algorithm composite MTime.
- Each resolved input output-MTime.
- Requested output port.
- Piece/extent/time request.
- Algorithm-specific execution key.

### 8.5 Graph diagnostics

Provide text and DOT output including:

- Algorithm type/name and stable diagnostic ID.
- Input/output ports and declared types.
- Connections.
- Last request and result.
- Execution and cache-hit counts.
- Current output MTimes.
- Last failure and cancellation location.

Diagnostics must not retain the graph after the call returns.

### 8.6 Required test graph

Add reusable test algorithms:

- Constant source with zero inputs.
- Counting source.
- Pass-through filter.
- Two-input append/merge filter.
- Repeatable-input append filter.
- Two-output split filter.
- Sink with zero outputs, if the output-count contract is expanded to support
  sinks.
- Failing, cancelling, and re-entrant algorithms.

Test scenarios:

1. Linear chain execution and cache hit.
2. Diamond graph with one shared producer.
3. Two independent branches where only one output is requested.
4. Multi-output producer with one output invalidated.
5. Direct data mixed with connected data.
6. Optional and repeatable inputs.
7. Failure and cancellation propagation.
8. Parameter MTime change during an inactive interval.
9. Request changes for piece, extent, and time.
10. Graph destruction in every legal release order.

### 8.7 Migration

- Implement built-in filters through the same public-semantic callback path.
- Keep `FVizFilter` factory functions as typed convenience wrappers.
- Keep mapper filter-connection compatibility functions.
- Convert BentBeam and one small source/sink example to the generalized API.

### 8.8 Definition of done

- Graph traversal is owned by the executive, not recursive filter policy.
- Request stages invoke real algorithm callbacks.
- Output-port requests are observable and tested.
- The diamond graph executes its shared source once per equivalent transaction.
- Custom application algorithms compile using only installed public headers.
- BentBeam output remains numerically and topologically equivalent.

## 9. Milestone 0.11 - Data contracts, provenance, and 64-bit topology

### 9.1 ID-width strategy

Adopt one public topology ID type, preferably:

```c
typedef uint64_t FVizId;
```

Requirements:

- No silent narrowing from file connectivity to memory topology.
- Checked conversion at OpenGL index-buffer boundaries.
- GPU upload may select 16-, 32-, or partitioned 32-bit buffers internally.
- APIs accepting counts and IDs validate overflow independently.
- Existing 32-bit helper APIs may remain as deprecated convenience wrappers.

### 9.2 DataObject information and copy semantics

Implement and document:

- `shallow_copy`: share retained arrays/topology where allowed.
- `deep_copy`: recursively copy observable data.
- `copy_structure`: copy geometry/topology without result arrays.
- `initialize/clear`: return an object to a valid empty state.
- Optional metadata/information map with typed keys.
- Memory-size estimation for diagnostics and caching policy.

Copy tests must mutate source and destination after each copy kind and verify
the documented sharing behavior.

### 9.3 Attribute associations and active roles

Every dataset exposes:

- Point data.
- Cell data.
- Field data.
- Active scalars.
- Active vectors.
- Active normals.
- Active tensors.
- Optional global IDs and pedigree/original IDs.

Mapper and algorithm APIs select arrays through one descriptor:

```text
association + array name + component mode + component index
```

Component modes include direct component, magnitude, and direct RGB/RGBA when
the array contract permits it.

### 9.4 PolyData completeness

Support separate topology collections for:

- Vertices/poly-vertices.
- Lines/polylines.
- Triangles and general polygons.
- Optional strips only if needed by a real use case.

Rendering may triangulate polygons internally but must preserve source cell IDs.

### 9.5 FEA provenance

All topology-changing algorithms propagate explicit arrays:

- `FVizOriginalPointIds`.
- `FVizOriginalCellIds`.
- `FVizOriginalFaceIds` when surface faces are generated.
- Optional part/material/block identifiers.

Rules are specified separately for surface, slice, contour, threshold,
cell-to-point, warp, and transform operations.

For generated geometry with no one-to-one source cell, provenance may contain
multiple IDs or an interpolation record, but it may not silently invent the
rendered primitive ID as the FEA cell ID.

### 9.6 Cell model

Before 1.0, support and test at least:

- Vertex, line, triangle, quad, polygon.
- Tetra, hexahedron, wedge, pyramid.
- Quadratic tetra and quadratic hexahedron if required by target solver files.

Each supported volumetric cell defines:

- Point count and face topology.
- Parametric interpolation where probing supports it.
- Surface extraction behavior.
- Slice behavior or a documented unsupported result.

### 9.7 Definition of done

- Models exceeding 32-bit file connectivity fail safely or load through the
  64-bit path without truncation.
- PolyData has point/cell/field attributes and general primitive topology.
- Copy contracts pass aliasing tests.
- A selected surface triangle returns its original FEA cell and face IDs.
- Mapper array selection uses the unified descriptor.

## 10. Milestone 0.12 - Parallel runtime and deterministic algorithms

### 10.1 Runtime context

Replace sole reliance on the global dispatch state with an opaque runtime:

- Explicit create/default/shutdown operations.
- Configurable thread count and affinity policy where portable.
- Multiple independent task groups.
- Runtime statistics and worker utilization.
- Safe process-exit cleanup.

The default runtime remains convenient, but tests can create isolated runtimes.

### 10.2 Task and cancellation model

Add:

- Task group begin/run/wait.
- Cancellation token shared with pipeline requests.
- Worker callback returning `FVizResult`.
- First-error capture with deterministic reporting.
- Thread-local scratch storage with task-lifetime reset.
- Nested scheduling without deadlock.
- Serial fallback with identical error/cancellation semantics.

### 10.3 Parallel primitives

Provide only primitives required by algorithms:

- `parallel_for`.
- Deterministic reduction.
- Exclusive/inclusive scan for topology offsets.
- Stable key/index sort or a documented internal equivalent.

### 10.4 Algorithm conversion order

Convert algorithms in this risk order:

1. Point transforms and warps.
2. Cell-to-point accumulation using deterministic reduction.
3. Bounds and attribute range calculation.
4. BVH primitive bounds and tree construction.
5. Surface face classification, followed by ordered assembly.
6. Slice and contour classification, followed by ordered assembly.

Each conversion keeps a serial reference implementation until equivalence is
proven.

### 10.5 Benchmark matrix

Use generated HEX8 cantilever grids:

| Size | Approximate cells | Purpose |
|---|---:|---|
| Small | 512 | correctness and CTest |
| Medium | 32,768 | routine benchmark |
| Large | 262,144 or larger | scaling and peak-memory study |

Measure:

- Wall time by algorithm.
- Worker utilization.
- Dispatch/task count.
- Peak transient memory.
- Thread-limit 1, 2, 4, 8, and hardware maximum.
- Output equivalence hash plus semantic validation.

CTest does not enforce machine-specific speed ratios. A separate performance
job records baselines and flags statistically significant regressions.

### 10.6 Definition of done

- No global dispatch lock prevents independent runtime contexts from executing.
- Cancellation and worker error propagate to the root pipeline request.
- Serial and parallel results are equivalent for every converted algorithm.
- Medium/large benchmarks show useful scaling on supported multicore hardware.
- Repeated runtime create/shutdown and nested tasks pass stress tests.

## 11. Milestone 0.13 - Renderer and render-pass architecture

### 11.1 Backend-neutral pass contract

Introduce an opaque render-pass interface with ordered standard stages:

1. Clear/background.
2. Opaque geometry.
3. Translucent geometry.
4. Edge/line geometry.
5. Selection/ID rendering.
6. Overlay/annotation.

The renderer owns pass ordering; the OpenGL device implements the pass
operations. Public actor/mapper APIs do not expose shader or GL object IDs.

### 11.2 Mapper and property completion

Add:

- Array selection by association/name/component mode.
- Automatic or explicit scalar ranges.
- Point/cell scalar interpolation policy.
- Direct RGB/RGBA arrays.
- Actor opacity and optional opacity arrays.
- Edge visibility, edge color, and line width.
- One or more clipping planes.
- NaN/below/above-range colors in both modern and fallback paths.

### 11.3 Coordinate system

Provide tested conversions:

- World to view.
- View to normalized device coordinates.
- Normalized device to viewport display.
- Display to world ray.

All functions take an explicit renderer so multi-viewport behavior is
unambiguous.

### 11.4 Offscreen and host integration

Add render-window lifecycle states:

- Created.
- Initialized/context available.
- Visible or offscreen.
- Finalized.

Required operations:

- Resize before and after initialization.
- Render to an offscreen color/depth framebuffer.
- Read color/depth pixels.
- Save a reference image through an optional image writer or test utility.
- Attach to a supported host/native child window without owning its event loop.
- Process events non-blockingly.

### 11.5 Hardware selection

Implement an ID-buffer pass encoding:

- Renderer/layer identity.
- Actor identity.
- Primitive/cell identity.
- Optional point identity.

Selection resolves the rendered primitive through provenance arrays to original
FEA IDs. Depth testing ensures hidden primitives are not selected in visible
selection mode. An optional through-selection mode may retain the CPU path.

### 11.6 GPU resource lifecycle

- Cache resources by data/mapper/property MTime.
- Separate geometry, scalar-color, edge, and selection resources.
- Release resources explicitly when a context is finalized.
- Recover predictably from context recreation.
- Report OpenGL capability and fallback state through diagnostics.

### 11.7 Visual regression scenes

Add deterministic scenes for:

- BentBeam Rainbow result.
- Two independent viewports.
- Overlay renderer and scalar legend.
- Point versus cell scalar coloring.
- NaN/below/above colors.
- Opacity and intersecting translucent shells.
- Clipping plane.
- Surface plus edges.
- Occluded hardware selection.
- Actor transform.

Use tolerance-based image comparison and preserve reference images with backend
and driver metadata.

### 11.8 Definition of done

- Standard rendering is expressed as ordered passes.
- Onscreen and offscreen paths render the same scene semantics.
- Transparency, edges, clipping, and overlays have image tests.
- Hardware selection respects depth and returns original FEA IDs.
- Multi-viewport coordinate conversion and picking pass headless tests.

## 12. Milestone 0.14 - Interaction, selection, and FEA widgets

### 12.1 Timer and event lifecycle

Add:

- One-shot and repeating timers with stable IDs.
- Timer reset/destroy.
- Focus and event capture during drag sequences.
- Explicit enter/leave, resize, expose, and timer events.
- Safe style changes during nested observer dispatch.
- A generic host-driven interactor that does not own the application loop.

### 12.2 Styles

Complete and test:

- Trackball camera.
- Trackball actor.
- Rubber-band visible selection.
- Rubber-band through selection.
- Point/cell picking style.
- Measurement mode only after selection and coordinate contracts are stable.

### 12.3 Selection model

Selection records contain:

- Actor.
- Association.
- Rendered primitive ID.
- Original point/cell/face ID when available.
- Dataset/output MTime used to create the record.
- Optional world position and scalar tuple.

Define behavior when pipeline output changes:

- Persistent selections re-resolve by original IDs.
- Ephemeral selections become invalid and report that state.
- Highlight actors never mutate the source dataset.

### 12.4 Widgets

Implement observer-based widgets in this order:

1. Selection highlight overlay.
2. Orientation axes.
3. Scalar/vector probe readout.
4. Optional clipping-plane manipulator.

Widgets use renderer overlays and interaction observers; they do not add
platform-native UI controls to the core library.

### 12.5 End-to-end example

Create an interactive FEA inspector that can:

- Load VTU.
- Select displacement/stress array and component/magnitude.
- Apply deformation scale.
- Toggle surface, edges, and clipping.
- Select visible cells by click or rectangle.
- Highlight selected cells.
- Display original element ID and result tuple.
- Export processed or selected data after milestone 0.15.

### 12.6 Definition of done

- Synthetic tests cover every style without native input.
- Timers, focus, capture, observer priority, and style changes are deterministic.
- Selection survives compatible pipeline recomputation through original IDs.
- Highlight, axes, and probe widgets work in multi-viewport windows.

## 13. Milestone 0.15 - IO, interoperability, and portability

### 13.1 Writer architecture

Add algorithm-compatible writers or sinks for:

- XML VTU ASCII.
- XML VTU appended raw binary.
- XML VTU compressed binary behind the compression option.
- Legacy VTK where useful for debugging/interchange.
- PLY only if polygonal interchange remains a real user requirement.

Writer configuration includes byte order, header width, compressor, output
mode, and selected arrays.

### 13.2 Round-trip contract

Round-trip tests compare:

- Point coordinates.
- Cell types and connectivity.
- Point/cell/field arrays.
- Array names, scalar types, tuple counts, and components.
- Active roles where the format can preserve them.
- Original IDs and metadata.
- NaN and infinity behavior.

### 13.3 Streaming and large files

- Avoid unnecessary full-file copies for appended binary data.
- Validate compressed and uncompressed block sizes before allocation.
- Support 64-bit headers and connectivity.
- Define maximum allocation policy or caller-configurable limits.
- Preserve detailed byte/section context in parser errors.

### 13.4 Portability

- Keep all data, pipeline, parallel, IO, and headless tests working on Windows
  and Linux.
- Implement or explicitly defer native Linux onscreen windows; the 1.0 minimum
  is portable processing plus tested offscreen rendering on supported CI.
- Separate platform window ownership from graphics context ownership.
- Document compiler, C runtime, OpenGL, and threading requirements.

### 13.5 Definition of done

- VTU ASCII/binary/compressed round-trips preserve required FEA semantics.
- Large 64-bit connectivity is not truncated.
- Reader fuzzing and malformed compressed blocks pass sanitizers.
- Supported platforms can install and run a clean consumer.

## 14. Milestone 0.16 - Public API stabilization

### 14.1 API organization

- Review naming consistency and module ownership.
- Remove accidental public dependencies between modules.
- Ensure every public header compiles alone as C17 and under C++ `extern "C"`.
- Minimize umbrella-header cost without removing convenience.
- Mark compatibility wrappers and planned deprecations.

### 14.2 ABI audit

- Verify all public structs intended to be stable are opaque or versioned.
- Fix enum width/extension policy.
- Define callback calling conventions and lifetime rules.
- Verify shared-library export lists.
- Record an ABI dump for the release candidate.
- Test consumer binaries across patch-version library updates where practical.

### 14.3 Documentation

Publish:

- API reference for every exported function.
- Ownership guide.
- Thread-safety matrix.
- Pipeline and request guide.
- Custom algorithm tutorial.
- Data association and provenance guide.
- Renderer/host integration guide.
- IO format support matrix.
- 0.x-to-1.0 migration guide.
- FEA gallery centered on BentBeam and the interactive inspector.

### 14.4 Definition of done

- No unresolved P0 API-contract issue.
- Documentation covers every exported ownership boundary.
- ABI and installed-header checks are automated.
- Release-candidate applications use no private headers.

## 15. Milestone 1.0 - Stable FEA visualization release

### 15.1 Mandatory release gates

- Warnings-as-errors shared/static builds pass on supported compilers.
- All unit, integration, package, and end-to-end tests pass.
- ASan/UBSan and reader fuzz corpus pass.
- No known high-severity leak, use-after-free, overflow, or data race remains.
- Visual regression scenes pass within documented tolerance.
- Performance baselines show no unexplained major regression.
- Fresh install consumers find, link, and run without source-tree paths.
- BentBeam and the interactive FEA inspector pass headless validation and visual
  review.
- ABI report, support matrix, changelog, migration guide, and release notes are
  complete.

### 15.2 1.0 support promise

- Semantic versioning begins at 1.0.
- Patch releases preserve source and binary compatibility.
- Minor releases preserve compatibility while adding APIs.
- Public removals wait for a major release and a documented deprecation cycle.
- Unsupported thread-safety or platform behavior is stated explicitly rather
  than implied.

## 16. Cross-milestone test strategy

### 16.1 Test pyramid

1. Unit tests for containers, math, types, and state machines.
2. Contract tests for ownership, errors, MTime, copy, and port semantics.
3. Graph tests for executive traversal, caching, failure, and cancellation.
4. Algorithm equivalence tests for topology and attributes.
5. Rendering state tests without a native window.
6. Offscreen image regression tests.
7. Native window smoke tests.
8. Install-tree consumer tests.
9. Performance and fuzz jobs outside the fast default CTest path.

### 16.2 Required invariant checks

- Counts and connectivity are internally valid.
- Attribute tuple counts match association sizes.
- Bounds contain all finite points.
- No output references destroyed input storage unless shallow sharing is
  documented and retained.
- MTime increases for every observable mutation.
- Failed operations leave objects valid and reusable.
- Original IDs remain correct through topology-changing filters.

### 16.3 Reference workloads

- Single tetra and single HEX8 for exact topology tests.
- Current 512-cell BentBeam for end-to-end correctness.
- Medium and large generated HEX8 beams for scaling.
- Mixed tetra/wedge/pyramid model.
- Dataset containing point and cell arrays of multiple numeric types.
- NaN/Inf scalar dataset.
- Malformed and truncated IO corpus.

## 17. Metrics

Track these metrics per milestone:

### Correctness

- Test count and pass rate.
- Sanitizer and fuzz failures.
- Known P0/P1 defects.
- Public APIs without ownership documentation.

### Pipeline

- Executions and cache hits by algorithm.
- Shared-upstream duplicate execution count.
- Cancellation latency by algorithm.
- Peak retained pipeline memory.

### Parallel

- Speedup and efficiency by thread count.
- Worker utilization.
- Peak scratch memory.
- Serial/parallel equivalence failures.

### Rendering

- Frame time and GPU upload time.
- Geometry/scalar cache rebuild count.
- Selection latency and correctness.
- Visual-regression difference score.

### IO

- Read/write throughput.
- Peak transient memory.
- Compression ratio.
- Round-trip fidelity failures.

Metrics inform decisions; only stable, machine-independent correctness limits
belong in ordinary CTest.

## 18. Risk register

| Risk | Impact | Mitigation |
|---|---|---|
| Freezing ABI before contracts mature | Long-term incompatible design | Freeze only after 0.16 audit |
| Executive becomes a second filter-specific recursion layer | Streaming and fan-out remain incorrect | Move all traversal into a request transaction and test diamond graphs |
| 64-bit migration doubles GPU/index cost | Large memory and rendering regression | Keep 64-bit CPU IDs; select/partition GPU index width internally |
| Parallel topology becomes nondeterministic | Unstable tests and selection IDs | Parallel classify/count, deterministic scan/assembly |
| Selection returns surface triangle instead of FEA cell | Incorrect engineering interpretation | Require provenance arrays before hardware selector acceptance |
| Render API leaks OpenGL concepts | Blocks offscreen/alternate backend work | Public pass/device contracts remain opaque and backend-neutral |
| Scope expands toward all VTK modules | 1.0 never stabilizes | Enforce FEA-focused non-goals and real-use-case requirement |
| Legacy compatibility wrappers dominate design | Hard-to-maintain duplicate paths | Implement wrappers over one generalized core |
| IO decompression trusts file sizes | Memory exhaustion/security defect | Checked limits, fuzzing, and allocation policy |
| Documentation drifts from implementation | Misleading release status | Documentation changes are part of every definition of done |

## 19. Work-package completion template

Every work package is complete only when it contains:

1. Public or internal contract and ownership decision.
2. Implementation with checked failure paths.
3. Unit/contract tests.
4. Integration into at least one end-to-end path when applicable.
5. MTime/cache invalidation test when cacheable state changes.
6. Serial/parallel equivalence test when parallelized.
7. Headless state test plus image test for a visual feature.
8. Public-header and install-consumer validation for exported APIs.
9. Changelog, roadmap, and design documentation update.

## 20. Immediate next implementation session

The next session should begin milestone 0.10 and stay narrowly focused on the
executive. Recommended commit-sized sequence:

1. Add graph tests for a constant source, pass-through, two-input append,
   two-output split, diamond fan-out, failure, cancellation, and re-entry.
2. Define the request descriptor and request-key comparison without changing
   existing filter behavior.
3. Add public custom-algorithm creation and callback state ownership.
4. Add an execution transaction and move upstream visitation into the
   executive.
5. Dispatch real information, data-object, update-extent, and data requests.
6. Make cache state output-port/request-specific.
7. Aggregate progress and propagate cancellation through the transaction.
8. Add text/DOT graph diagnostics.
9. Port all built-in filters to the generalized execution path.
10. Run the full warnings-as-errors build, CTest, BentBeam validation, and clean
    shared/static install consumers.

Do not begin parallel algorithm conversion or hardware selection until the
0.10 executive definition of done is satisfied.


### 5.7 0.26 data-model convergence update

0.26 adds the first structured dataset (`FVizImageData`) with VTK-style extent/origin/spacing/direction semantics and introduces native 32/64-bit connectivity storage through `FVizCellArray`/`FVizCellView`. `FVizPoints`, `FVizPolyData`, `FVizUnstructuredGrid`, VTU IO, PointLocator, Probe, and core FEA transformations now have native-ID paths while ordinary renderable meshes retain compact uint32 fast paths. Shared `FVizCellTypeTraits` centralizes topology and linear interpolation weights, reducing duplicated cell definitions across surface extraction and probing. The next convergence step is time/composite-aware IO and resampling rather than further renderer breadth.


### 5.8 0.27 temporal/composite and interoperability update

0.27 adds `FVizPartitionedDataSet` and `FVizTemporalDataSet`, time metadata/requests in the executive, PVD temporal-source semantics, ASCII VTP interchange, structured/unstructured resampling, and a pipeline array calculator for derived engineering results. Equal-time PVD entries become one partitioned dataset, while one-file time steps remain direct datasets. VTP ASCII was checked bidirectionally against VTK 9.6.2; that external gate exposed tag-bounded attribute parsing, FieldData tuple metadata, and nested InformationKey issues that self-roundtrip tests did not catch. Remaining convergence work shifts to stabilization, composite-aware algorithm breadth, streaming requests, and optional compressed XML/VTKHDF rather than adding more renderer surface area.
