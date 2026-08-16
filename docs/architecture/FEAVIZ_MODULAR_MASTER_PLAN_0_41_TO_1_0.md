# FEAViz Modular Master Plan — 0.41 → 1.0

## 1. Product architecture

FEAViz 1.0 is a **modular visualization platform**, not an Abaqus-specific library.
The project is split into a solver/domain-neutral visualization Core and optional domain
modules. The first domain module is the FEA post-processing layer, whose behavioral target
is an Abaqus/Viewer-class workflow without introducing an Abaqus runtime dependency into
Core.

```text
                       applications
                           │
             ┌─────────────┼─────────────┐
             │             │             │
          Win32/Qt      scripting      headless
             │             │             │
             └─────────────┴─────────────┘
                           │
              ┌────────────┴────────────┐
              │                         │
       FEAViz::FEA                future modules
   FEA result semantics        CAD / CFD / medical / ...
              │                         │
              └────────────┬────────────┘
                           │
                    FEAViz::Core
        data + pipeline + filters + renderer + interaction
```

Dependency rule:

```text
FEAViz::Core  <-  FEAViz::FEA

Core MUST NOT include, link, or call FEA.
```

`FEAViz::FEAViz` remains a compatibility aggregate target during the transition. New
applications should link explicitly to `FEAViz::Core` or `FEAViz::FEA`.

## 2. Core responsibility

Core owns functionality that is meaningful outside finite-element analysis:

- object/refcount/MTime/event/command runtime;
- arrays, attributes, points, PolyData, ImageData, StructuredGrid,
  RectilinearGrid, UnstructuredGrid, MultiBlock, PartitionedDataSet and temporal data;
- generic pipeline/executive, piece/extent/time requests and caching;
- SMP runtime, arena/scratch allocation and memory accounting;
- generic geometry, topology, spatial acceleration and field statistics;
- generic filters: contour, threshold, slice/clip, append, clean, normals, connectivity,
  probe/resample, transform, calculator and interpolation primitives;
- renderer/window/camera/actor/mapper/material/light/text/legend;
- interaction, generic picking, selection and widget framework;
- Win32/Qt host integration;
- generic IO retained for workflows that require it.

Core must be usable with `FVIZ_BUILD_FEA=OFF` and must pass its own independent test,
sanitizer and package-consumer gates.

## 3. FEA module responsibility

`FEAViz::FEA` owns solver-neutral FEA post-processing semantics:

- ResultDatabase / Step / Frame / FieldOutput / HistoryOutput;
- result positions (nodal, element nodal, integration point, centroid, face, region);
- entity labels, local integration-point ids and section points;
- components and invariants;
- integration-point extrapolation and nodal averaging rules;
- deformed-shape state and deformation scaling;
- contour-result preparation and discontinuity handling;
- shell plies/section points and beam/connector semantics;
- sets/surfaces/display groups as FEA concepts;
- history/XY/path extraction;
- modal/frequency/complex result semantics;
- contact, status, damage and result visibility rules;
- FEA view/session controller;
- solver bridges, including a future Abaqus ODB bridge, as optional adapters outside Core.

The FEA API should remain solver-neutral. “Abaqus-like” describes expected post-processing
behavior, not a dependency or naming requirement in Core.

## 4. Release gates shared by both tracks

Every phase must satisfy, where applicable:

1. Release build with warnings-as-errors.
2. Core-only build with `FVIZ_BUILD_FEA=OFF`.
3. Full Core+FEA build.
4. ASan + UBSan + leak-detection gate on portable code.
5. Public C17 and C++17 header compilation.
6. Install-tree consumer using explicit package components.
7. Deterministic numerical regression vectors for result math.
8. Benchmark comparison for changed hot paths; regressions require justification.
9. No Core source may include public/private FEA headers.
10. No FEA-specific enum/name/solver concept may be added to Core solely for convenience.

---

## 5. Dual-track development discipline from 0.41 onward

Every release may advance both tracks, but a feature is assigned to exactly one owner before implementation:

- **Core owns mechanism** when the capability is solver/domain neutral: geometry deformation, typed array math, topology interpolation, dirty-range tracking, cache/scheduler policy, rendering, picking, selection/provenance and temporal data transport.
- **FEA owns policy and semantics** when the capability depends on structural-FEA concepts: Step/Frame, displacement field selection, result position, section point, primary variable, nodal averaging rules, contour display policy, display groups, history output and solver adapters.
- When an FEA feature exposes a reusable primitive, the primitive is promoted to Core only after its API can be described without FEA/solver terminology. FEA then consumes the Core primitive through the public component boundary.
- Core must never gain a dependency on `FEAViz::FEA`; Core-only build/test/package gates remain mandatory for every release.

Release notation used by this roadmap:

```text
0.xx release
├─ C0.xx  Core deliverables
└─ F0.xx  FEA deliverables
```

A release does not need equal amounts of work in both tracks, but both dependency direction and independent buildability are always validated.

# Track C — Generic FEAViz Core

## C0.40 — Physical module boundary and package components

Status: **implemented in 0.40 development**.

Deliverables:

- real `FEAViz::Core` target (`libFEAViz`);
- optional `FEAViz::FEA` target (`libFEAVizFEA`) linking Core;
- `FVIZ_BUILD_FEA` option;
- component-aware package config: `find_package(FEAViz COMPONENTS Core FEA)`;
- compatibility `FEAViz::FEAViz` target;
- internal module ABI for object allocation/local MTime/error propagation, exported only
  for sibling FEAViz modules and not installed as public API;
- generic `UnstructuredGrid` and `FieldStatistics` public headers under `FViz/Data`;
- compatibility wrappers under old `FViz/FEA` paths.

Acceptance:

- Core-only tests pass with FEA disabled;
- full tests pass with FEA enabled;
- installed Core package works without `libFEAVizFEA`;
- requesting required FEA component fails cleanly when FEA was not built;
- Core target dependency graph contains no FEA target.

## C0.41 — Generic deformation and reusable geometry-update primitives

Status: **implemented in 0.41 development**.

Core additions discovered by the FEA deformed-shape requirement but intentionally solver-neutral:

- typed three-component vector-field metrics: finite tuple count, maximum magnitude and RMS magnitude;
- generic auto-scale helper based on model bounds and vector magnitude, with configurable target fraction and scale clamps;
- apply deformation arrays to `FVizPoints`, `FVizPolyData` and `FVizUnstructuredGrid`;
- direct-array `UnstructuredGrid` warp API while preserving the legacy name-based warp contract;
- in-place point/mesh deformation updates that reuse allocated geometry across frames;
- typed raw-vector hot paths to avoid repeated component-accessor overhead;
- permanent deformation benchmark protecting create/update/measurement performance.

Acceptance:

- Core deformation APIs compile and run with `FVIZ_BUILD_FEA=OFF`;
- APIs contain no `Step`, `Frame`, `U`, section-point or solver terminology;
- output topology and non-coordinate attributes remain unchanged;
- in-place update and fresh-output paths are numerically identical;
- old `fviz_unstructured_grid_warp_by_vector()` error behavior remains source/behavior compatible;
- changed point coordinates propagate geometry revision without forcing topology replacement.

## C0.42 — Generic field-evaluation and topology-interpolation primitives

Status: **implemented in 0.42 development**.

Continue extracting only reusable mechanisms required by contour/result presentation:

- component extraction and finite masks;
- vector magnitude and symmetric/full tensor eigensystem helpers;
- gather/scatter by stable ids;
- indexed reduction and weighted averaging;
- cell-local interpolation/extrapolation matrices;
- element-local to global gather;
- point/cell adjacency cached by topology revision;
- deterministic parallel reductions;
- generic discontinuity-mask arrays and cache-key utilities.

FEA owns the rules deciding when/how these primitives are applied. No concept of an Abaqus-style averaging threshold belongs in Core.

## C0.43 — Render-data update contracts

Status: **implemented in 0.43 development**.

Formalize resource revisions already used internally:

- topology revision;
- geometry revision;
- attribute revision;
- mapper/color-state revision;
- dirty ranges for points/arrays;
- partial CPU render-data rebuild;
- GPU subrange update contract;
- mapper-shared geometry residency.

Gate: FEA frame update changing one scalar array must not rebuild topology buffers.

## C0.44 — Frame scheduler and interactive quality policy

Status: **implemented in 0.44 development**.

Generic GUI/render scheduling:

- coalesced frame requests;
- frame budget and target FPS;
- interactive vs still quality states;
- temporary AA/LOD degradation while interacting;
- one high-quality frame at `EndInteractionEvent`;
- render request reason/statistics.

## C0.45 — Generic expression engine

Status: **implemented in 0.45 development**.

Build a typed array expression engine that is not FEA-specific:

- scalar/vector/tensor inputs;
- arithmetic and common math functions;
- component addressing;
- conditional expressions;
- output type/association validation;
- compiled expression cache;
- parallel evaluation.

FEA derived fields later expose domain-friendly names on top of this engine.

## C0.46 — Generic spatial query 2.0

Status: **implemented in 0.46 development**.

- reusable cell locator API;
- closest point/cell;
- ray and frustum queries;
- refit vs rebuild by fine-grained revisions;
- batch queries;
- parallel BVH build/refit;
- large-model memory accounting.

## C0.47 — Selection/data provenance foundation

Status: **implemented in 0.47 development**.

- stable provenance ids through filters;
- association conversion helpers;
- selection extraction as generic data operation;
- selection masks and inverse masks;
- selection propagation through composite datasets;
- generic named selection collection API.

FEA display groups later map node/element sets/surfaces onto these primitives.

## C0.48 — Temporal streaming core

Status: **implemented in 0.48 development**.

- asynchronous generic temporal request interface;
- cancellation tokens/futures;
- bounded prefetch queue;
- byte-budgeted cache;
- priority for current/next/previous frame;
- cancellation on scrub direction change;
- no solver-format dependency.

## C0.49 — GPU memory manager

Status: **implemented in 0.49 development**.

- resident resource registry;
- byte budgets;
- LRU/priority eviction;
- explicit pin/unpin;
- shared geometry accounting;
- statistics by geometry/attribute/render-target class;
- graceful OOM fallback.

## C0.50 — Large-data/SMP hardening

Status: **implemented in 0.50 development**.

- parallel contour/surface only where benchmark-profitable;
- parallel topology links/adjacency;
- range/statistics improvements;
- transient arena adoption in high-churn filters;
- NUMA-neutral chunking where practical;
- deterministic reductions.

## C0.51–C0.60 — Production renderer and interaction breadth

Detailed execution plan: `CORE_DEVELOPMENT_PLAN_0_51_TO_1_0.md`.

- C0.51: executable render graph and backend state contracts;
- C0.52: transparency correctness and fallback policy;
- C0.53: line, point, edge and glyph production quality;
- C0.54: generic clipping and section-geometry foundation;
- C0.55: retained overlay, text and legend layout;
- C0.56: multi-viewport and shared-device residency;
- C0.57: scalable hardware picking and region selection;
- C0.58: interaction, widget, DPI and context robustness;
- C0.59: visual regression and renderer observability infrastructure;
- C0.60: renderer production gate.

## C0.61–C0.79 — General-purpose maturity

The detailed plan divides this range into pipeline/data/out-of-core maturity
(C0.61–C0.70) followed by extensibility and bindings (C0.71–C0.79). Volume
rendering and additional algorithms remain product-demand-gated rather than release
checkboxes.

## C0.80–C0.99 — Core stabilization

The detailed plan assigns C0.80–C0.89 to reliability/failure hardening and
C0.90–C0.99 to ABI, packaging, documentation and product freeze.

## C1.0 — Core production gate

Core can be released and used independently for non-FEA visualization applications.
No FEA module is required to create datasets, pipelines, renderers, GUI viewports,
selections, animations or generic scientific field visualizations.

---

# Track F — FEAViz FEA / Abaqus-like Post-Processing

## F0.39 — Result-domain model

Status: **done**.

- ResultDatabase → Step → Frame → Field → FieldBlock;
- HistoryRegion/HistorySeries;
- positions, labels, local ids, section-point metadata;
- components and base invariants;
- Modified propagation.

## F0.40 — Primary Variable / Position / Averaging Engine

Status: **implemented in 0.40 development**.

Primary selection describes:

- field;
- instance;
- component or invariant;
- source result position;
- target display position;
- section point;
- optional entity-label filter;
- averaging on/off;
- cross-block averaging policy;
- relative averaging threshold;
- local-id convention.

Result object preserves both raw and display data:

```text
raw values + entity/local ids
          │
          ├─ extrapolation / association conversion
          ├─ averaging policy
          └─ discontinuity detection
                  ↓
         display values + mask + range
```

Acceptance:

- native nodal result unchanged;
- centroid result remains cell-associated unless conversion requested;
- integration-point result can extrapolate to element-nodal and average to nodal;
- averaging can preserve discontinuities across blocks;
- threshold can reject averaging;
- raw arrays are never overwritten;
- field/grid/filter MTime changes invalidate evaluation cache;
- duplicate GlobalIds are rejected rather than silently mapped.

## F0.41 — Deformed Shape Controller

Status: **implemented in 0.41 development**.

FEA additions:

- select a nodal displacement-like field from an `FVizFEAFrame` (default field name `U`);
- instance-aware result-block selection;
- map result entity labels to mesh point `GlobalIds`, independent of tuple ordering;
- reject duplicate mesh labels and duplicate result contributions;
- support complete-coverage enforcement or zero-filled missing-node displacement with a coverage mask;
- true, user-uniform and automatic deformation scale modes;
- undeformed, deformed and superimposed display states;
- retain both base and deformed grids in the result for overlay rendering;
- expose mapped/missing counts, displacement array, coverage mask, scale factor and displacement metrics;
- cache by Frame/Grid/options MTime and invalidate when the source displacement field changes;
- use only public generic Core deformation APIs for numerical coordinate updates.

Deferred rather than forced into Core/FEA prematurely:

- rigid-body-motion removal and modal normalization policy;
- coordinate-system transform of displacement vectors;
- assembly-level multi-instance deformation controller;
- renderer-owned double-buffered deformation geometry for animation.

## F0.42 — Contour Display Engine

- contour on undeformed/deformed shape;
- nodal vs element result presentation;
- averaged/unaveraged contour modes;
- discontinuity boundaries;
- automatic/manual limits;
- interval count and banded colors;
- outside-limit colors;
- legend metadata;
- no mutation of raw FieldBlock values.

## F0.43 — Shell / Composite / Section Points

- explicit section-point inventory;
- top/bottom and numbered points;
- ply names/indices;
- section-point selection;
- max/min/absolute envelopes across plies;
- shell thickness-position metadata;
- shell result orientation handling;
- section-point-aware probe and contour.

## F0.44 — Beam / Connector Results

- beam section force/moment components;
- connector components;
- beam orientation and section axes;
- line/symbol result rendering;
- connector relative motion/force visualization;
- specialized probe formatting.

## F0.45 — Sets / Surfaces / Display Groups

- FEA model entities: instance, node set, element set, surface;
- boolean display-group operations;
- result predicate groups;
- isolate/hide/show/reverse;
- display group applies consistently to contour, probe, section cut and statistics;
- implemented on top of Core generic selection/mask/provenance primitives.

## F0.46 — Section Cut / View Cut / Free Body

- clipping and true cut geometry;
- multiple active cuts;
- cap surfaces where appropriate;
- cut result interpolation;
- free-body forces/moments from selected cut or region;
- moving/interactive section plane.

## F0.47 — Probe / Query

- node/element label lookup;
- integration-point/element-nodal/section-point query;
- nearest-world-coordinate probe;
- current primary variable and raw field query;
- min/max location query;
- formatted value + provenance record;
- persistent probe annotations.

## F0.48 — XY / History

- history series extraction;
- field value vs frame/time;
- path creation and sampling;
- multiple curves;
- XY arithmetic;
- envelope/combination;
- CSV/report export through a domain-neutral text/report layer;
- headless API identical to GUI behavior.

## F0.49 — Animation

- time/increment animation;
- deformed+contour update;
- mode-shape animation;
- scale-factor sweep;
- result-range policy per-frame vs all-frames;
- prefetch/cancellation using Core temporal streaming;
- frame pacing and drop policy.

## F0.50 — Complex / Frequency / Phase

- real/imaginary result storage;
- magnitude/phase;
- harmonic phase sweep;
- complex invariant policy;
- frequency/mode metadata;
- animation and XY integration.

## F0.51 — Vector / Tensor / Symbol Plots

- displacement/reaction/load vectors;
- principal direction glyphs;
- tensor glyphs;
- orientation triads;
- symbol scaling/coloring;
- sparse/decimated symbol placement;
- selection/display-group aware symbols.

## F0.52 — Contact / Cohesive

- contact pressure/opening/slip/traction fields;
- master/slave or pair metadata where supplied by bridge;
- cohesive traction/separation/damage;
- contact-surface probing and XY;
- display filters for open/closed/sliding states.

## F0.53 — Damage / Status / Element Deletion

- status/active flags;
- element deletion visibility;
- damage scalar families;
- frame-dependent topology visibility without rebuilding connectivity;
- contour/statistics ignore inactive elements by policy.

## F0.54 — Specialized multiphysics result semantics

Only FEA-post workflows required by products:

- thermal fields;
- pore pressure;
- acoustic/other scalar/vector fields;
- volume fraction/material state where available;
- generic user field variables.

These use Core data/render primitives; no solver-specific class should leak into Core.

## F0.55 — Coordinate-system Result Transforms

- global/local/material coordinate systems;
- vector transformation;
- symmetric/full tensor transformation;
- cylindrical/spherical evaluation hooks;
- principal results after/before transformation with documented order;
- orientation-field support.

## F0.56 — Derived Field / FEA Expression Layer

- names such as `S.Mises`, `U.Magnitude`, components and user fields;
- use Core generic expression engine underneath;
- frame/position/section-point compatibility validation;
- derived-field cache;
- metadata and units propagation.

## F0.57 — Envelopes / Frame Combination

- max/min/abs-max over frames;
- controlling frame index per entity;
- envelope over modes/load cases/section points;
- linear combination of compatible frames;
- result provenance retained for probe/report.

## F0.58 — FEA Result View Controller

A single state object drives GUI/headless postprocessing:

```text
Step / Frame
Primary Variable
Position / Averaging
Section Point
Deformation
Contour
Display Group
Section Cuts
Symbols
Animation
```

Changing state creates the minimal dirty set; GUI does not reimplement result logic.

## F0.59 — Legend / Annotation / Overlay State

- FEA-aware legend title/component/invariant/units;
- min/max labels and locations;
- state/frame text;
- deformation scale annotation;
- user annotations;
- consistent export/screenshot metadata.

## F0.60 — Multi-Viewport Comparison

- synchronized cameras optionally;
- different frame/field per viewport;
- shared topology/GPU resources;
- linked/unlinked legends;
- compare undeformed/deformed, fields, increments or cases.

## F0.61 — Result Streaming

- topology resident, result buffers streamed;
- frame cache by memory budget;
- asynchronous next-frame prefetch;
- cancellation during scrubbing;
- low-copy bridge ingestion;
- partial instance/field loading.

## F0.62 — Abaqus ODB Bridge Adapter

Optional adapter, not part of Core and not required by FEA module itself:

```text
Abaqus ODB API / bridge process
          ↓
solver adapter
          ↓
FEAViz::FEA ResultDatabase
```

The adapter may be a separate DLL/process to isolate Abaqus runtime/version constraints.
Other solvers implement the same neutral ingestion contract.

## F0.63–F0.79 — FEA breadth and huge-model hardening

- high-order element result-position validation;
- solver-native partitions;
- large assembly sets/surfaces;
- contact-heavy models;
- shell/composite production cases;
- result streaming from remote/solver bridge;
- GPU partial result uploads;
- large-selection performance;
- scripting bindings for result/session APIs;
- report/image/movie pipelines.

## F0.80–F0.89 — Numerical/visual validation

Build reference suites against trusted solver postprocessors:

- analytic stress invariant cases;
- IP extrapolation per supported element type;
- nodal averaging/discontinuity cases;
- shell section-point cases;
- deformation scale;
- contour ranges/bands;
- modal/complex phase;
- probe values;
- free-body resultants;
- history/XY curves.

Exact behavior may intentionally differ from Abaqus where documented, but differences
must be deliberate and testable.

## F0.90–F0.99 — Production hardening

- malformed/incomplete result data;
- huge frame counts;
- cancellation/error recovery;
- out-of-memory behavior;
- bridge disconnect/reconnect;
- GUI context recreation while result state is active;
- long animation soak tests;
- API/ABI review;
- documentation/examples.

## F1.0 — FEA production gate

`FEAViz::FEA` can support a complete structural-FEA post-processing application with an
Abaqus/Viewer-class workflow while remaining solver-neutral and optional. Removing or
disabling FEA must leave a fully usable generic `FEAViz::Core` visualization library.

---

# 6. Immediate order after 0.41

The default sequence remains deliberately interleaved:

1. **C0.42 generic field/topology primitives** needed by display-field preparation, without result-position or averaging policy.
2. **F0.42 Contour Display Engine** over `PrimaryVariableResult`, supporting deformed/undeformed presentation without mutating raw result data.
3. **C0.43 render-data update contracts** so scalar-only and coordinate-only frame changes publish dirty ranges instead of rebuilding topology.
4. **F0.43 Shell/Composite/Section Points**, promoting only generic tensor/interpolation primitives to Core.
5. **F0.45 Display Groups** together with **C0.47 selection/provenance**.
6. **F0.46/F0.47 section cut + probe**, reusing Core clipping/spatial query mechanisms.
7. **F0.48/F0.49 XY/history + animation**, using **C0.48 temporal streaming** and the C0.41 in-place deformation update path.
8. Continue alternating Core mechanism and FEA policy, with Core-only validation on every release.

This sequence prevents either track from absorbing the other: FEA requirements may reveal reusable mechanisms, but only solver-neutral mechanisms are promoted into Core.
