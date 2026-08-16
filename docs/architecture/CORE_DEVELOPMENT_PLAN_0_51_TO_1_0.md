# FViz Core development plan — 0.51 to 1.0

## 1. Goal

Take the domain-neutral FViz Core from the completed 0.42–0.50 foundation to a
production-ready visualization SDK. The plan keeps FEA policy out of Core and
advances in independently releasable increments with measurable rendering,
interaction, data, pipeline, portability, and ABI gates.

This document is the executable continuation of the Core track in
`FEAVIZ_MODULAR_MASTER_PLAN_0_41_TO_1_0.md`.

## 2. Non-negotiable constraints

- `FEAViz::Core` never includes, links, or calls `FEAViz::FEA`.
- Public ABI remains C17 and headers compile as C++17.
- New optional behavior is discoverable through capabilities; unsupported paths
  return an explicit error and have a tested fallback.
- Renderer work preserves headless/offscreen use and never makes a GUI toolkit a
  Core dependency.
- CPU ownership, GPU ownership, cancellation, threading, and callback lifetime
  are documented before a public API is accepted.
- Every milestone leaves the tree releasable; incomplete experimental features
  remain private or build-time gated.
- Linux native presentation, volume rendering, and new graphics backends are
  demand-gated and do not block the Windows/OpenGL Core 1.0 promise.

## 3. Release gates for every milestone

1. Core-only and full Core+FEA Release builds with warnings-as-errors.
2. Core test suite, public C17/C++17 headers, and install-tree Core consumer.
3. No Core-to-FEA dependency or FEA vocabulary introduced into public Core APIs.
4. Deterministic unit/visual regression for changed behavior.
5. Benchmark and memory delta for every changed hot path.
6. ASan/UBSan/leak gate on portable CI; Windows smoke and context recreation gate
   for rendering/host work.
7. API contract, support matrix, migration note, and example updated together.

## 4. Execution order

```text
C0.51 render-pass contracts
  -> C0.52 transparency
  -> C0.53 primitive quality
  -> C0.54 clipping geometry
  -> C0.55 overlay layout
  -> C0.56 multi-viewport sharing
  -> C0.57 scalable picking
  -> C0.58 interaction/host robustness
  -> C0.59 visual regression infrastructure
  -> C0.60 renderer production gate

C0.60
  -> C0.61-C0.70 pipeline and out-of-core maturity
  -> C0.71-C0.79 extensibility and bindings
  -> C0.80-C0.89 reliability and failure testing
  -> C0.90-C0.99 ABI/documentation freeze
  -> C1.0 production release gate
```

## 5. Wave R — Production renderer and interaction (C0.51–C0.60)

### C0.51 — Executable render graph and backend state contracts

Scope:

- turn standard pass ordering into an explicit executable frame graph;
- declare pass inputs/outputs, attachment formats, sample count, load/store and
  clear behavior;
- pool transient render targets under the GPU memory manager;
- isolate GL state around custom passes and report unsupported dependencies;
- expose per-pass CPU/GPU timing and resource-byte statistics;
- preserve the current simple pass API through a compatibility adapter.

Acceptance:

- the same graph produces deterministic pass order and attachment lifetime;
- resize/recreate cycles do not leak targets or retain stale framebuffer handles;
- a custom pass cannot corrupt the following standard pass state;
- graph compilation rejects cycles, missing producers, and incompatible formats;
- a single-pass opaque scene has no material performance regression.

### C0.52 — Transparency correctness and fallback policy

Scope:

- complete weighted-blended OIT with accumulation/revealage targets;
- deterministic back-to-front sorted fallback for unsupported hardware;
- capability-gated depth peeling with configurable layer/error limits;
- correct translucent surfaces, edges, glyphs, scalar opacity, and overlays;
- document linear/sRGB blending and premultiplied-alpha contracts.

Acceptance:

- overlapping translucent geometry is order-independent in weighted mode within
  the documented numerical tolerance;
- opaque output is unchanged when transparency is disabled;
- every requested mode selects a tested implementation or explicit fallback;
- no target churn occurs frame-to-frame at stable viewport size;
- visual cases cover intersecting surfaces, coincident edges, glyphs, and text.

### C0.53 — Line, point, edge, and glyph production quality

Scope:

- screen-space line and point rendering independent of driver width limits;
- consistent joins, caps, depth bias, and coincident-topology policy;
- MSAA-aware antialiasing plus deterministic non-MSAA fallback;
- large instanced glyph batches with partial instance-buffer updates;
- world-space and screen-space sizing with DPI-aware semantics;
- robust normals and two-sided lighting for degenerate/shell-like geometry.

Acceptance:

- mesh edges remain visible without z-fighting across camera distance and scale;
- line/point widths are stable across supported DPI and projection modes;
- updating a glyph range performs a subrange upload, not a full rebuild;
- one-million-instance benchmark has a fixed time and memory baseline;
- degenerate primitives are rejected or rendered without NaN propagation.

### C0.54 — Generic clipping and section-geometry foundation

Scope:

- renderer/mapper clip collections supporting plane, box, and implicit-function
  descriptions without domain terminology;
- GPU discard path for interactive clipping;
- CPU geometry extraction path with provenance and interpolated attributes;
- optional cap generation with explicit orientation and source provenance;
- dirty/revision contracts so moving a plane does not rebuild source topology;
- clipping widget hooks remain generic; FEA free-body policy stays in FEA.

Acceptance:

- GPU and CPU clip classifications agree on deterministic fixtures;
- generated cut edges/caps are watertight for supported manifold inputs;
- interpolated fields and original entity provenance survive extraction;
- moving an interactive plane stays inside the frame budget on the reference mesh;
- composite inputs preserve block identity and empty-block behavior.

### C0.55 — Retained overlay, text, and legend layout

Scope:

- normalized, pixel, viewport, and world anchor spaces;
- constraint-based padding, alignment, stacking, collision avoidance, and safe area;
- DPI-aware font metrics and deterministic atlas invalidation;
- scalar legend tick formatting, title/unit slots, discrete/continuous modes, and
  stable top-left/right/bottom placement;
- retained overlay scene rendered once per requested frame without interaction
  flicker or resize-only initialization.

Acceptance:

- overlay placement is correct before first interaction and after resize/DPI change;
- scalar legend anchors are expressed relative to the owning viewport, not window;
- repeated renders produce identical layout and image hashes;
- missing glyphs and atlas growth cannot invalidate unrelated geometry resources;
- layout stress test covers multiple legends, labels, and small viewports.

### C0.56 — Multi-viewport and shared-device residency

Scope:

- multiple renderers/viewports/layers in one render window;
- explicit device/share-group object beneath render windows;
- shared immutable geometry, shader, font-atlas, and lookup-table resources;
- per-viewport camera, clipping, selection, overlay, and scheduler state;
- fair frame scheduling and memory accounting across viewports;
- safe resource teardown in any window/renderer destruction order.

Acceptance:

- four viewports can share one mesh with one geometry residency allocation;
- viewport-local changes do not invalidate other viewport caches;
- hardware picking and display/world transforms respect viewport origin and DPI;
- deletion permutations pass leak and stale-handle tests;
- shared versus duplicated residency is exposed in statistics and benchmarked.

### C0.57 — Scalable hardware picking and region selection

Scope:

- integer-ID selection targets for actor, instance, point, cell, and primitive IDs;
- rectangle, lasso, brush, and visible-only/through selection policies;
- tiled/asynchronous readback with cancellation and bounded staging memory;
- selection result carries provenance and association explicitly;
- CPU BVH fallback and capability-driven strategy selection;
- configurable result limits and overflow reporting for very large selections.

Acceptance:

- single-pixel and region picks agree with deterministic CPU fixtures;
- overlapping actors, translucent geometry, glyph instances, and clipped geometry
  return the documented visible/through result;
- a large drag selection does not block the UI thread for an unbounded duration;
- cancellation releases staging resources and never publishes partial stale data;
- selection of at least ten million rendered primitives has a benchmark baseline.

### C0.58 — Interaction, widget, DPI, and context robustness

Scope:

- transactional widget begin/update/commit/cancel lifecycle;
- quaternion rotation and constraint manipulators with optional inertia disabled by
  default, consistent with current camera interaction policy;
- pointer capture, focus loss, modifier transitions, high-resolution wheel and
  touch/pen event normalization;
- per-monitor DPI changes and logical/physical coordinate conversion;
- Win32 and optional Qt context recreation with GPU resource rehydration;
- no host event-loop ownership imposed by the SDK.

Acceptance:

- cancelled interactions restore the exact pre-interaction state;
- focus/capture loss cannot leave the scheduler in interactive quality mode;
- picking and overlays remain aligned after runtime DPI and viewport changes;
- context destruction/recreation restores a rendered scene without reloading data;
- Win32 embedding and optional Qt adapters pass repeated create/destroy soak tests.

### C0.59 — Visual regression and renderer observability infrastructure

Scope:

- golden-image manifest with backend/vendor/feature metadata;
- exact, per-channel tolerance, RMSE, and perceptual comparison modes;
- deterministic camera, light, font, color-space, and random-seed fixtures;
- diff image and machine-readable result artifacts;
- per-frame pass timings, draw/triangle/upload counts, cache hits, and memory pressure;
- CI sharding for headless and hardware rendering lanes.

Acceptance:

- a deliberate one-pixel/color regression is detected with an actionable diff;
- baselines cannot be silently rewritten by the test runner;
- unsupported capability cases are skipped explicitly, never reported as pass;
- observability counters have documented units and reset/lifetime semantics;
- the standard visual suite has a bounded runtime and artifact-size budget.

### C0.60 — Renderer production gate

Scope:

- close defects found by C0.51–C0.59 without adding new feature families;
- lock the renderer capability/fallback matrix for the 1.0 support promise;
- stress resize, minimize/restore, DPI, context loss, memory pressure, and host teardown;
- establish small, medium, large, and multi-viewport benchmark baselines;
- review renderer/interaction API naming, ownership, threading, and error contracts;
- publish renderer and embedding examples for non-FEA consumers.

Acceptance:

- 8-hour render/interaction soak has no unbounded CPU/GPU memory growth;
- recoverable allocation/context failures return errors and restore rendering;
- visual suite passes on the supported Windows/OpenGL capability matrix;
- Core-only package can build and run all renderer examples;
- no unresolved P0/P1 renderer correctness or lifetime issue remains.

## 6. Wave D — Pipeline, data, and out-of-core maturity (C0.61–C0.70)

### C0.61–C0.63 — General asynchronous execution

- promote temporal worker/future mechanisms into a reusable Core task runtime;
- add executor ownership, priorities, cancellation, continuations, progress, and
  deterministic shutdown;
- make pipeline execution optionally asynchronous without changing synchronous API
  behavior;
- prevent callbacks into destroyed consumers and eliminate one-thread-per-request.

### C0.64–C0.66 — Composite executive and streaming providers

- multi-input/multi-output port information and request negotiation;
- composite dirty-leaf work planning and bounded parallel execution;
- provider interface for metadata-first, range, piece, extent, and temporal fetch;
- byte budgets, backpressure, cancellation, retry policy, and cache observability;
- retain local-file providers as the reference implementation.

### C0.67–C0.68 — Scalable data representation

- evaluate 64-bit in-memory connectivity behind an explicit ABI-safe type/config;
- zero-copy/external buffer views with lifetime callbacks and immutability flags;
- mapped/chunked arrays and checked conversions to renderable chunks;
- dataset validation levels and corruption diagnostics.

### C0.69–C0.70 — Data/pipeline maturity gate

- property-based and fuzz tests for arrays, topology, filters, and request graphs;
- failure/cancellation injection across algorithms and providers;
- million-cell temporal/composite/out-of-core benchmark matrix;
- reconcile legacy single-input filter APIs through compatibility adapters;
- freeze the data/pipeline contracts needed by plugins and bindings.

## 7. Wave E — Extensibility and language bindings (C0.71–C0.79)

- versioned plugin descriptor and host-services table with size/version negotiation;
- module discovery, explicit load/unload policy, dependency diagnostics, and isolation;
- stable C ABI handles for bindings; no public exposure of backend-native objects;
- callback trampolines, error retrieval, ownership annotations, and UTF-8 policy;
- generated binding metadata plus reference Python and C# consumers;
- parser/reader plugin contracts and fuzz corpus management;
- optional volume-rendering research remains feature-gated until a product requires it;
- C0.79 gate: two out-of-tree plugins and both language consumers pass install-tree CI.

## 8. Wave S1 — Reliability and failure hardening (C0.80–C0.89)

- full object/thread-safety matrix and race-detector lane;
- allocator, I/O, thread, GPU allocation, and context failure injection;
- renderer/device loss recovery and resource rehydration audit;
- malformed/cyclic/deep composite and pipeline graph tests;
- fuzz all supported readers, expression parser, topology builders, and plugin loader;
- deterministic replay of interaction, temporal requests, and scheduler decisions;
- long-duration memory-pressure, cancellation, and create/destroy soak suites;
- eliminate undefined ownership and unchecked arithmetic found by the audits.

## 9. Wave S2 — ABI and product freeze (C0.90–C0.99)

- inventory every exported symbol, struct layout, enum, ownership, and error result;
- reserve struct fields or use size-versioned extension records where evolution is needed;
- finalize deprecation/migration policy and compatibility aggregate behavior;
- freeze supported compiler, platform, OpenGL, CMake, and package-component matrix;
- deterministic benchmark baselines with explicit regression budgets;
- complete non-FEA tutorials for data, pipeline, rendering, picking, temporal data,
  plugins, headless use, Win32 embedding, and bindings;
- release-candidate install/package validation from clean machines;
- C0.99 gate: no undocumented public symbol or unresolved ABI freeze issue.

## 10. C1.0 production gate

Core 1.0 is ready only when:

- non-FEA applications can create, stream, process, render, select, and animate data
  using the installed Core component alone;
- supported rendering and host paths pass correctness, recovery, visual, soak, and
  memory-pressure suites;
- portable modules pass sanitizer, race, fuzz, and malformed-input gates;
- public ABI, package config, support matrix, migration guide, and examples agree;
- benchmark regressions are within recorded budgets or explicitly approved;
- all P0/P1 defects are closed and remaining limitations are documented.

## 11. Immediate implementation backlog

The first development session after this planning milestone should execute C0.51 in
the following order:

1. Record current opaque/translucent/edge/selection/overlay execution and GL state.
2. Introduce private frame-graph and attachment-lifetime structures without public
   API changes.
3. Route existing six standard passes through the compiled graph.
4. Add transient render-target pooling and GPU memory-manager accounting.
5. Add custom-pass state save/restore and graph validation diagnostics.
6. Expose size-versioned pass timing/resource statistics.
7. Add unit, offscreen visual, resize/recreate, memory-pressure, and benchmark gates.
8. Update public contracts only after the private model proves stable.

Exit criterion: C0.51 is merged only when the legacy rendering path is fully covered
by the graph, public behavior remains compatible, and C0.52 can add OIT without
another pass-system redesign.
