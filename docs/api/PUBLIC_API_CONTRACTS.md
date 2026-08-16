# FEAViz public API contracts

This document defines default behavior for the installed C17 API. A function's
specific documentation may strengthen these guarantees but may not silently
contradict them.

## Ownership vocabulary

- `create`, `copy`, and explicit `retain` operations return an owned reference.
- Successful object setters retain their input unless documented otherwise.
- Ordinary getters return borrowed pointers. They remain valid only while the
  documented owner and the relevant retained child remain alive.
- Successful replacement releases the previously retained child after the new
  child has been retained; self-assignment is valid.
- Output-port proxies are borrowed from their producer.
- Releasing `NULL` is valid.

## Result and output parameters

- Functions returning `FVizResult` return `FVIZ_OK` only after completing their
  documented state change.
- Required null inputs return `FVIZ_ERROR_INVALID_ARGUMENT`.
- Creation and read operations clear owned output pointers to `NULL` before
  work begins and leave them `NULL` on failure.
- Failed mutating operations leave the target valid and, unless explicitly
  documented as incremental, observably unchanged.
- The calling thread can query detailed failure state through the last-error
  API. A worker thread's error state does not replace another thread's state.

## Counts, IDs, and arithmetic

- Public counts use `FVizSize`; public persistent identities use `FVizId`.
- Every allocation derived from external or public counts checks addition and
  multiplication overflow before allocating.
- Conversion to a narrower file, platform, or GPU representation is checked.
- An unsupported representable range returns an explicit error; it is never
  silently truncated.

## Modification time

- An observable mutation advances the object's MTime.
- Composite objects report an MTime at least as new as retained children that
  affect their observable output.
- Raw mutable pointers cannot be tracked automatically; callers must call
  `fviz_object_modified()` on the documented owner after writing.
- Failed and cancelled pipeline execution does not mark an output current.

## Thread safety

- Reference counting, last-error state, and MTime operations are thread-safe.
- Immutable concurrent reads are allowed unless a type says otherwise.
- Container, dataset, pipeline-graph, renderer, and interaction mutation
  requires external synchronization.
- A render window and its OpenGL resources are used only from the owning render
  thread unless a platform API explicitly documents another mode.
- Cancellation request flags may be set from another thread.

## Compatibility

- Public structures are opaque or explicitly versioned before ABI 1.0.
- Compatibility wrappers use the same generalized core implementation.
- Source/API removals require a documented deprecation cycle after 1.0.

## Incremental data and rendering

- DataArray and PolyData dirty-range queries return the union of changes newer
  than the supplied MTime. `full` means the bounded history cannot prove a safe
  subrange update; consumers must rebuild or upload the complete resource.
- Geometry, topology, attributes, mapper color state, and render state have
  independent revisions. A scalar-only update does not invalidate topology.
- GPU byte budgets are advisory for the visible working set. Visible or pinned
  mapper/glyph resources are never evicted to meet a budget; pressure is exposed
  through statistics. Manual purge overrides pins and rebuilds lazily.

## Expressions and field operations

- Compiled expressions are immutable and safe for concurrent evaluation when
  bindings and output ownership are independent. Expression-cache mutation
  requires external synchronization.
- Specified expression associations must agree and all bound arrays must have
  equal tuple counts. Scalar operands broadcast; incompatible vectors fail.
- Tuple matrices are row-major. Least-squares operator construction rejects
  underdetermined or singular systems instead of silently inventing a policy.
- Discontinuity helpers expose domain-neutral relative spread only. FEA decides
  thresholds, grouping, averaging, and result-position policy.

## Spatial, selection, and provenance

- BVH batch results preserve input order; individual misses are reported in the
  parallel flag array. Cancellation may stop the batch between chunks.
- BVH memory size excludes retained source datasets. Refit requires unchanged
  topology and updates leaves in parallel before deterministic internal reduction.
- Selection masks are UInt8. Selection extraction currently targets PolyData
  points or render triangles and gathers compatible attributes.
- Named selection collections retain their selections. Provenance IDs use exact
  integer conversion and never pass through floating-point representation.

## Temporal requests

- Synchronous cache `get` and background future requests return owned frame
  references. Futures retain their cache; destroying a future cancels and joins.

## Executable render graph

- `FVizRenderGraph` owns retained pass references, resource descriptions,
  dependencies, compiled execution order, and logical transient target objects.
- Pass and resource IDs are graph-local stable IDs until `fviz_render_graph_clear()`.
- Compilation is deterministic for the same insertion/dependency order. It rejects
  cycles, invalid references, and reads of internal resources before their first
  write. External resources are considered initialized by the backend.
- Resource hazards add conservative ordering edges. Compatible transient targets
  with non-overlapping compiled live ranges share one physical slot.
- Mutating the graph invalidates execution order and physical targets. Borrowed
  execution passes and physical targets remain valid only until the next graph
  mutation, compilation, clear, or graph destruction.
- The renderer compatibility pass list is compiled lazily into an executable graph;
  existing pass-list APIs remain source compatible.
- Custom backend passes execute inside a backend state guard. FViz restores the
  framebuffer, program, vertex array, viewport/scissor, depth/color masks, raster
  modes, and common enable state before continuing standard passes.
- Render-window statistics expose graph generation/pass count and custom-pass state
  restorations. Per-pass statistics are replaced on every frame and contain borrowed
  renderer pointers valid while the render window retains its renderer collection.

## Transparency

- `FVIZ_TRANSPARENCY_WEIGHTED_BLENDED` uses accumulation and revealage targets and
  composites a straight-alpha result over the opaque target. Surface input colors
  are straight alpha; the accumulation shader applies alpha and weight explicitly.
- Weighted OIT is capability-gated. Unsupported weighted-OIT requests and all
  depth-peeling requests currently fall back to deterministic back-to-front sorted
  alpha; `transparency_mode_requested` and `transparency_mode_applied` report both.
- `depth_peeling_supported` is false until the backend provides and validates the
  full dual-depth-peeling path. Requesting it never silently claims peeling was used.
- The temporal cache serializes loads and cache mutation performed by requests.
  Loader callbacks must poll their cancellation token for prompt cancellation.
- Prefetch queues are bounded and priority ordered. A scrub-direction reversal
  cancels active work and removes stale pending requests.

## Clipping and section geometry

- Mapper clipping planes are interactive GPU discard state and do not mutate or
  rebuild source topology. CPU `FVizClipPolyDataFilter` extraction is explicit.
- CPU clipping accepts render-ready triangle PolyData, interpolates compatible
  point attributes at edge intersections, and preserves original-cell provenance.
- Cap generation is opt-in and requires closed manifold cut loops. Cap triangles
  share cut-boundary vertices, use the outward orientation of the retained half,
  carry `FVizClipCap=1`, and use `UINT64_MAX` for `FVizOriginalCellIds`.
- Open/non-manifold or numerically non-simple cap loops fail explicitly; the
  filter never publishes a partially capped output.

## Overlay layout and image comparison

- Overlay layout output uses bottom-left-origin physical display pixels. Anchor
  inputs may be display pixels, viewport pixels, normalized viewport/window, or
  world coordinates through a caller-owned synchronous projection callback.
- Padding and stack gaps are logical pixels multiplied by `content_scale`; safe
  area insets are already physical pixels. Input order is the deterministic
  collision priority. Overflow and projection failure are reported in result flags.
- Scalar legends resolve their complete bar/text footprint against their owning
  viewport, including on the first render and after size/DPI changes.
- RGBA8 image comparison never rewrites a baseline. Exact, per-channel tolerance,
  byte-domain RMSE, and linear-light luma RMSE modes report metrics independently
  of the selected pass/fail threshold. Optional diff storage is caller-owned.

## Region selection

- Legacy rectangle/lasso selection remains synchronous, CPU, through-selection,
  and unlimited. The options API adds a maximum result count and cooperative
  cancellation without changing legacy behavior.
- Reaching `maximum_results` returns `FVIZ_OK` with a valid deterministic prefix,
  `overflow=true`, and the returned count. Cancellation returns
  `FVIZ_ERROR_CANCELLED`, sets `cancelled=true`, and leaves the output selection NULL.
- Visible-only region selection is reserved for the asynchronous integer-ID
  backend and currently returns `FVIZ_ERROR_NOT_SUPPORTED`; it never silently
  substitutes through-selection.

## Interaction transactions

- Trackball camera and actor pointer drags are transactions. Button release commits;
  explicit cancel, focus loss, or pointer-capture loss restores the pre-drag camera
  or actor transform before leaving interactive scheduler quality.
- Actor rotation composes normalized quaternions. Interaction acceleration/inertia
  is not applied by default.

## FEA scalar-bar actor

- `FVizFEAScalarBarActor` is an FEA-owned preset object that owns a generic
  `FVizScalarLegend`; Core remains independent of FEA presentation policy.
- The default preset is Abaqus-style: 12 discrete contour intervals, 13 ticks,
  top-left placement, 2% horizontal and 5% vertical viewport padding, `S, Mises`
  title, `MPa` units, and a compact numeric label format.
- Options are copied/applied immediately; strings and an optional custom lookup
  table are borrowed for the call, while the resulting legend/table ownership is
  retained by the actor. Renderer attachment is non-owning; the renderer retains
  its own legend reference.
- Range, interval/tick count, title/units/format, position/padding, text sizes and
  colors, shadow flags, visibility, and custom lookup-table selection are all
  configurable through the versioned options structure.
