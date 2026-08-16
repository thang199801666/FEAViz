# FEAViz Abaqus-Like FEA Visualization Master Plan — 0.39 → 1.0


> **Module scope from 0.40 onward:** this document is the sub-plan for the optional
> `FEAViz::FEA` post-processing module. The domain-neutral visualization runtime is
> `FEAViz::Core` and must not depend on this module. The cross-project architecture and
> interleaved Core/FEA schedule are defined in
> `FEAVIZ_MODULAR_MASTER_PLAN_0_40_TO_1_0.md`. “Abaqus-like” describes target FEA
> post-processing behavior, not the identity or dependency model of FEAViz Core.

## 1. Mission

FEAViz will prioritize complete, production-grade finite-element post-processing over
feature-by-feature compatibility with VTK file formats or the full breadth of VTK.
The target is an Abaqus/Viewer-like visualization workflow for structural and
multiphysics FEA while retaining FEAViz's lightweight C17 core, embeddable Win32/Qt
renderer, high-performance data model, and solver-neutral result ingestion layer.

The project will continue to maintain existing VTK/VTP/VTU/PVD/PVTU code that already
works, but new work on VTK format parity is **not a release blocker** unless required by
a real FEAViz application workflow. VTK compatibility is now a maintenance concern,
not the architectural north star.

## 2. Definition of "Abaqus-like"

The 1.0 target is not a clone of Abaqus/CAE. It means the post-processing stack can
support the workflows an FEA engineer expects from the Abaqus Visualization module:

- open or ingest an analysis-result database;
- browse Step → Frame → Field Output → Component/Invariant;
- choose result position and section point;
- show undeformed, deformed, contour-on-undeformed, contour-on-deformed, and
  superimposed shapes;
- control deformation scale;
- contour stresses, strains, displacement, reaction force, contact variables,
  damage/status, user variables, and arbitrary solver fields;
- calculate stress invariants and principal values/directions;
- extrapolate/average integration-point results with user-controlled averaging rules;
- visualize shell section points, plies, top/bottom surfaces, and envelopes;
- visualize beam/connector/shell/solid-specific geometry and outputs;
- isolate sets, surfaces, instances, element types, materials, sections, and result
  predicates through display groups;
- probe node, element, integration point, face, section-point, and world-coordinate data;
- create paths and XY data from field/history outputs;
- perform XY arithmetic and reporting;
- animate time, increment, mode shape, harmonic phase, and result sweep;
- create section cuts/view cuts/isosurfaces and free-body resultants;
- show vectors, symbols, tensors, principal directions, material orientations, and loads;
- preserve camera, viewport, annotation, legend, display-group, and result-selection state;
- remain responsive on models with millions of elements and hundreds/thousands of frames;
- expose the same result/session functionality to C/C++ GUI code and scripting bindings.

## 3. Architecture principles

### 3.1 Result semantics are first-class

A result is not merely a `FVizDataArray`. The core must preserve:

- step and frame identity;
- analysis domain (time, frequency, modal, arc length);
- field name and description;
- scalar/vector/tensor semantic type;
- component labels;
- valid invariants;
- result position;
- instance identity;
- node/element labels;
- local integration-point/face/element-nodal identifiers;
- section point / ply location;
- frame value, increment number, mode, frequency, and phase where applicable.

### 3.2 Topology and results stay separable

The FEA model topology should remain resident while frame-varying data streams through
small, replaceable result buffers. A new frame must not rebuild connectivity or GPU
resources unless topology actually changes.

### 3.3 Field evaluation is demand-driven

Components, invariants, nodal extrapolations, averaging, envelopes, and derived fields
must be generated only when requested and cached by source MTime + evaluation options.

### 3.4 Solver-neutral FEA module, solver-specific bridges

The FEA result model must not depend on Abaqus libraries. Abaqus ODB, CalculiX,
Ansys, Nastran, custom solvers, and application bridges convert their native data into
the same FEAViz result-domain objects.

### 3.5 GUI is a consumer, not the owner of result semantics

Win32, Qt Widgets, QtGui, scripting, and headless report generation must use the same
primary-variable, frame, contour, display-group, probe, and XY engines.

### 3.6 Correctness before smoothing

FEA contour smoothing is dangerous if result positions, section points, discontinuities,
or averaging regions are ignored. FEAViz must preserve raw values and make every
averaging/extrapolation transformation explicit.

---

# Stage A — Result-domain foundation

## 0.39 — FEA Result Database / ODB-like object model

Status: **completed in 0.39**.

Deliverables:

- `FVizFEAResultDatabase`;
- named `FVizFEAStep` objects;
- ordered `FVizFEAFrame` objects;
- named `FVizFEAField` objects;
- field blocks split by instance/result position/section point;
- entity labels and local ids;
- component labels;
- valid-invariant masks;
- vector magnitude;
- Mises;
- Tresca;
- pressure;
- maximum/middle/minimum principal values;
- HistoryRegion/HistorySeries model;
- interpolation of history and frame positions;
- child `ModifiedEvent` propagation all the way to the database;
- no dependency on an ODB runtime or VTK data formats.

Acceptance gates:

- known stress-state invariant regression vectors;
- direct component extraction;
- multi-block result metadata regression;
- section-point metadata preserved;
- HistorySeries interpolation tests;
- modifying raw field values invalidates Frame → Step → Database in O(1) notification;
- Release + warnings-as-errors;
- ASan/UBSan/leak gate;
- install-tree consumer compiles all new public headers.

## 0.40 — Primary-variable evaluation and result-position engine

Deliverables:

- `FVizFEAPrimaryVariable` / result-selection descriptor;
- component-vs-invariant selection;
- block filtering by instance, position, section point, element set, and surface;
- standardized mapping of NODAL, ELEMENT_NODAL, INTEGRATION_POINT, CENTROID,
  ELEMENT_FACE, WHOLE_ELEMENT, WHOLE_REGION;
- interpolation-point → element-nodal extrapolation policies;
- element-nodal → unique-nodal averaging;
- averaging thresholds based on relative discontinuity;
- averaging-region boundaries;
- preserve raw/unaveraged values beside display values;
- cached evaluated arrays keyed by field MTime + selection options;
- scalar-range cache that reports raw and displayed min/max separately.

Acceptance gates:

- nodal contour from native nodal values;
- element centroid contour without accidental nodal smoothing;
- integration-point stress → element-nodal → nodal display chain;
- discontinuity test across two materials/sections;
- no averaging across disabled region boundary;
- cache hit must not rescan full field.

## 0.41 — Deformed-shape engine

Deliverables:

- select deformation field independently of primary contour field;
- vector component mapping by component labels rather than hard-coded U1/U2/U3 indices;
- uniform deformation scale;
- automatic deformation scale based on model diagonal and max displacement;
- user-specified scale;
- true-scale option;
- undeformed/deformed/superimposed display states;
- independent style/color for undeformed overlay;
- deformation cache keyed by geometry revision + displacement field revision + scale;
- deformation of solid, shell, beam and mixed-dimensional render geometry;
- missing-displacement handling by instance/region;
- complex-mode deformation phase hook reserved for 0.48.

Performance gate:

- topology unchanged: no connectivity rebuild;
- displacement-only frame update: position-buffer update only;
- target interactive playback on million-node meshes when result streaming is available.

---

# Stage B — Abaqus-class contour behavior

## 0.42 — Contour engine and averaging controls

Deliverables:

- continuous contours;
- discrete/banded contours;
- line contours;
- contour-on-undeformed;
- contour-on-deformed;
- contour on both/superimposed shape;
- auto/manual range;
- interval count;
- user-defined intervals;
- above/below-limit colors;
- reversed spectrum;
- logarithmic color mapping where meaningful;
- min/max markers with original entity labels;
- show/hide extrema;
- contour averaging threshold;
- nodal averaging on/off;
- element-boundary discontinuity visualization;
- element result facet coloring without forced point interpolation;
- missing/invalid/nonfinite result color;
- hidden/deactivated element policy.

Correctness gates:

- neighboring elements with discontinuous stress must show exact unaveraged values when
  averaging is disabled;
- averaging threshold changes only display values, never raw result storage;
- min/max markers resolve back to instance + entity label + section point.

## 0.43 — Shell, composite, section-point and envelope results

Deliverables:

- shell top/bottom/mid section points;
- arbitrary through-thickness section points;
- composite ply names and ply section points;
- active section-point selection;
- top-and-bottom simultaneous display;
- absolute-maximum envelope through section points;
- maximum envelope;
- minimum envelope;
- section-point identity preserved in probe and extrema;
- shell normal/side convention metadata;
- shell extrusion display compatible with selected section point;
- optional through-thickness result chart for a probed shell element;
- composite ply failure/damage result selection.

## 0.44 — Beam, connector, truss and 1D result visualization

Deliverables:

- beam profile rendering;
- profile scale control;
- beam section forces/moments;
- beam section stress locations;
- beam orientation visualization;
- connector force/moment/vector symbol support;
- connector relative motion outputs;
- truss axial force/stress display;
- line-element result coloring;
- local coordinate system indicator per selected element;
- section-point result lookup for 1D sections.

---

# Stage C — Model visibility and sectioning

## 0.45 — Display groups / sets / surfaces

Deliverables:

- named node sets;
- element sets;
- surfaces;
- instances;
- parts;
- sections/material categories;
- element type categories;
- display-group boolean operations: replace/add/remove/intersect;
- transient selection-derived display groups;
- result-predicate groups, e.g. `S.Mises > limit`;
- display group applies to rendering, picking, legend range, statistics, probe, and XY path;
- hide/show/isolate actions;
- display-group state serializable into session/view state.

Performance gate:

- million-element group masks must use compact bitsets/ranges, not one heap object per entity;
- visibility changes must not rebuild topology.

## 0.46 — Section cut, clipping, view cut and free-body foundation

Deliverables:

- one or multiple clipping planes;
- interactive plane widget;
- section-cut cap surface;
- result interpolation onto cut surface;
- cut contour uses selected primary variable;
- cut-plane position/orientation annotations;
- view cuts based on plane/cylinder/sphere;
- isosurface view cuts for scalar fields;
- clip by display group/result predicate;
- free-body face collection from section cut or selected surface;
- resultant force/moment accumulation hook.

Correctness gate:

- no duplicate cut geometry at partition ghosts;
- cut provenance must resolve to source cell/entity labels.

## 0.47 — Probe, query and entity inspection

Deliverables:

- query by node label;
- element label;
- picked vertex/edge/face/cell;
- nearest node/world point;
- integration point;
- element-nodal position;
- section point;
- multiple selected fields at one entity;
- raw vs averaged display value;
- coordinates in global/local systems;
- element connectivity/type/material/section metadata;
- probe annotations pinned in world space;
- probe table suitable for GUI property panel;
- copy/export selected values.

---

# Stage D — XY and animation

## 0.48 — XY/history engine

Deliverables:

- HistoryRegion/HistorySeries plotting backend;
- field-output XY from selected node/element/IP;
- XY along path;
- path by nodes;
- path by points;
- path by edges;
- deformed or undeformed path coordinate option;
- XY combine/add/subtract/multiply/divide;
- absolute, envelope, derivative, integral, smoothing, resampling;
- curve naming/style metadata;
- multiple Y axes support hook;
- report/table export independent of VTK formats;
- CSV/plain text native export;
- persistent XY session objects.

## 0.49 — Animation controller

Deliverables:

- step frame animation;
- all-steps animation;
- selected frame range;
- time-based playback;
- fixed FPS playback;
- ping-pong/loop/once;
- mode-shape animation;
- scale-factor animation;
- contour range fixed-across-animation option;
- dynamic range option;
- prefetch next/previous result frame;
- frame cache budget;
- cancel stale prefetch after scrub;
- playback quality mode vs paused full-quality mode;
- event API: animation start/frame/end.

Performance gate:

- frame switch does not reconstruct scene/actors;
- topology-static results only update relevant position/scalar GPU buffers.

## 0.50 — Complex results, frequency and phase

Deliverables:

- complex scalar/vector/tensor field storage;
- real/imaginary;
- magnitude/phase;
- phase-angle projection;
- harmonic animation;
- frequency-step frame metadata;
- mode number/frequency display;
- mode-shape normalization options;
- complex history XY.

---

# Stage E — Symbols and specialized FEA outputs

## 0.51 — Vector, tensor and symbol plots

Deliverables:

- vector arrow plots;
- displacement vectors;
- reaction-force vectors;
- velocity/acceleration vectors;
- principal stress direction glyphs;
- tensor ellipsoid/cross glyph hook;
- material orientation glyphs;
- local coordinate triads;
- nodal normal symbols;
- load/BC symbols for model-overlay use cases;
- glyph density/scale/color controls;
- glyph picking/probe metadata.

## 0.52 — Contact/cohesive visualization

Deliverables:

- contact pressure;
- contact opening/clearance;
- contact slip vector/magnitude;
- contact status states;
- main/secondary surface identity;
- contact traction vectors;
- cohesive traction/separation/damage variables;
- surface-based result mapping;
- hide open-contact regions option;
- contact-only display group convenience.

## 0.53 — Damage, deletion, status and failure visualization

Deliverables:

- element active/deleted state;
- progressive damage variables;
- failed element hiding;
- failed element alternate color;
- erosion animation without topology rebuild where possible;
- status-variable-driven visibility;
- composite failure indices;
- user-defined threshold presets.

## 0.54 — Eulerian / volume-fraction / special-domain results

Deliverables:

- volume fraction / occupancy field semantics;
- isosurface extraction driven by result variable;
- material-boundary visualization;
- void filtering;
- adaptive topology change tolerance;
- reserved hooks for ALE/remeshing frame-to-frame topology changes.

---

# Stage F — Derived result mathematics

## 0.55 — Coordinate-system transformations

Deliverables:

- global/local field transforms;
- cylindrical/spherical systems;
- user-defined coordinate systems;
- vector transform;
- symmetric/full tensor transform;
- material orientation transform;
- transformed component labels;
- transformed principal directions;
- probe reports both native and transformed value on request.

## 0.56 — Derived fields and expression engine

Deliverables:

- arithmetic across fields/components/invariants;
- scalar/vector expressions;
- tensor component expressions;
- conditional expressions;
- min/max/clamp/abs/sqrt/log/exp;
- user result aliases;
- cached derived field graph;
- units metadata hook;
- provenance from derived array back to source fields;
- expression result available to contour, probe, XY, display groups, cuts, and animation.

## 0.57 — Envelope and frame-combination engine

Deliverables:

- min/max/absolute-max across frames;
- entity-wise envelope;
- frame index/time that controls each envelope result;
- section-point envelope;
- load-case combination API;
- modal combination hooks;
- derived frame creation without copying topology;
- envelope extrema report.

---

# Stage G — View state and professional visualization UX

## 0.58 — Result-view controller

Deliverables:

One controller object must own the complete viewport result state:

- active step/frame;
- primary field;
- component/invariant;
- result position;
- section point/ply;
- averaging settings;
- deformation field/scale;
- contour style/range;
- display group;
- section cuts;
- symbol plots;
- animation state;
- active probe/selection;
- legend configuration.

Changing one property must generate a minimal dirty set and a coalesced redraw.

## 0.59 — Annotations, legends and viewport overlays

Deliverables:

- Abaqus-style scalar legend customization;
- title/state block;
- current step/frame/time/frequency annotation;
- deformation scale annotation;
- min/max annotations;
- user text annotations;
- arrows/callouts;
- coordinate triad;
- view compass hook;
- multiple overlay layers;
- DPI-safe Qt/Win32 rendering;
- export-ready layout state.

## 0.60 — Multi-viewport result comparison

Deliverables:

- multiple independent renderers/views;
- linked cameras;
- linked or independent frame/result state;
- side-by-side undeformed/deformed;
- different field outputs in separate viewports;
- synchronized animation;
- viewport copy/duplicate;
- per-viewport display groups and legends.

---

# Stage H — Large-result performance and ingestion

## 0.61 — Frame/result streaming architecture

Deliverables:

- lazy frame metadata catalog;
- lazy field block loading;
- result cache budget by bytes;
- deformation and primary-field separate caches;
- async prefetch;
- cancellation after user scrub;
- pinned current/neighbor frames;
- mapped-file/zero-copy ingestion hook;
- topology resident independently of results;
- statistics and contour ranges computed asynchronously where safe.

Targets:

- thousands of frames without storing every frame in RAM;
- first interactive viewport before entire result database is loaded.

## 0.62 — Abaqus ODB bridge contract

Neither `FEAViz::Core` nor `FEAViz::FEA` will **require** Abaqus ODB libraries.

Deliverables:

- solver-neutral ingestion callbacks/interfaces;
- metadata discovery API;
- model/assembly/instance ingestion;
- step/frame catalog ingestion;
- lazy field block callback;
- history region callback;
- set/surface metadata callback;
- section point metadata callback;
- bridge can live in a separate DLL/process linked against the Abaqus ODB API;
- bridge failures cannot corrupt FEAViz core state;
- stable versioned ABI for bridge messages.

This phase is the correct place to integrate an Abaqus-specific bridge, rather than
embedding proprietary solver APIs into FEAViz itself.

## 0.63 — Large model GPU/resource budget

Deliverables:

- resident mesh budget;
- result buffer budget;
- LRU mapper resource eviction;
- shared topology across instances;
- persistent/static connectivity buffers;
- partial position updates;
- partial scalar/color updates;
- result buffer orphan/ring strategy benchmark;
- async upload where supported;
- CPU/GPU memory-pressure metrics exposed to GUI.

## 0.64 — Parallel FEA result evaluation

Deliverables:

- parallel invariants;
- parallel extrapolation;
- parallel averaging;
- parallel envelope;
- parallel probe batches;
- parallel section-cut interpolation;
- deterministic results where engineering reproducibility requires it;
- cancellation token propagation.

---

# Stage I — Selection, sets and reporting at production depth

## 0.65 — Selection semantics aligned with FEA entities

Deliverables:

- node;
- element;
- element face;
- edge;
- integration point;
- section point;
- instance;
- set/surface;
- path;
- result extrema.

Every selection carries instance + native label + local sub-entity information instead
of only render primitive indices.

## 0.66 — Reports and data extraction

Deliverables:

- field output table;
- history output table;
- selected set/surface report;
- min/max report;
- path report;
- free-body report;
- configurable numeric precision;
- sorting/grouping;
- CSV/text export;
- callback-based streaming writer for very large reports;
- report APIs usable headlessly.

## 0.67 — Free-body force/moment completion

Deliverables:

- resultants from selected cut/surface;
- force vectors;
- moments about selectable origin;
- component decomposition;
- coordinate-system transform;
- summed nodal force and element traction pathways;
- visual arrows + textual annotation;
- XY history of free-body resultant.

---

# Stage J — GUI integration and scripting

## 0.68 — GUI-neutral post-processing controller API

Deliverables:

- query available steps/frames/fields/components/invariants;
- change active result with one API call;
- list positions and section points;
- list sets/surfaces/instances;
- events suitable for combo boxes/sliders/property panels;
- no Qt/Win32 types in core API;
- stable borrowed/owned lifetime rules.

## 0.69 — Qt and Win32 reference panels

Deliverables:

Reference UI demonstrating:

- Step combo;
- Frame slider;
- Primary Variable combo;
- Component/Invariant combo;
- Deformation controls;
- contour options;
- section points;
- display groups;
- animation buttons;
- probe panel;
- XY panel.

These are examples/adapters, not a mandatory GUI framework dependency.

## 0.70 — Scripting parity layer

Deliverables:

- C ABI sufficient for Python/C# wrappers;
- ODB-like navigable object hierarchy where practical;
- result selection from script;
- probe/report/XY extraction;
- view/camera manipulation;
- animation export callbacks;
- bindings kept in lockstep with native result APIs.

---

# Stage K — Verification and production hardening

## 0.80 — FEA reference corpus

Build a solver-independent reference corpus containing:

- solid HEX/TET/WEDGE/PYRAMID;
- quadratic elements;
- shell section points;
- composite plies;
- beams/trusses/connectors;
- contact;
- element deletion;
- mixed instances;
- discontinuous material/section stress;
- modal/frequency results;
- complex values;
- large deformation;
- topology-changing frame sample.

Each dataset stores expected component/invariant/averaging/probe values independent of
render screenshots.

## 0.81 — Numerical validation

For every invariant and transformation:

- analytic known states;
- randomized tensor cross-check against high-precision reference implementation;
- NaN/Inf behavior;
- degenerate/equal principal stress cases;
- sign conventions documented;
- pressure convention explicitly tested;
- shell/beam coordinate conventions tested.

## 0.82 — Visual regression

Golden-image cases for:

- contour undeformed/deformed;
- averaging on/off;
- shell top/bottom;
- beam profile;
- section cut;
- display group;
- vector symbols;
- contact field;
- min/max labels;
- multi-viewport comparison.

Golden images are backend/platform tolerant with defined pixel/error thresholds.

## 0.83 — Memory and long-session stability

Stress tests:

- open/close result databases repeatedly;
- scrub 10,000 frames;
- switch primary field repeatedly;
- toggle section points/display groups/cuts;
- create/delete probes and XY curves;
- GPU resource eviction/recreation;
- context loss/recreation;
- Qt docking/reparent;
- no monotonically increasing retained objects after cache reaches budget.

## 0.84 — Failure injection

Inject:

- allocation failure;
- cancelled async result load;
- incomplete field block;
- missing instance;
- inconsistent labels;
- invalid section point;
- topology mismatch between frames;
- renderer/context loss;
- bridge process disconnect.

The FEA module must return errors without corrupting the active valid frame/view.

---

# Release-candidate track

## 0.90 — Feature freeze

No new major post-processing feature. Fix correctness, performance and API consistency.

## 0.91 — Public API naming/lifetime audit

- every borrowed pointer documented;
- every retained result documented;
- no ambiguous `get` ownership;
- struct-size/versioning for extensible options;
- C++ compilation of every public header;
- bindings API reviewed.

## 0.92 — Performance freeze

Track fixed benchmark corpus:

- 1M / 5M / 20M elements;
- 10 / 100 / 1000 frames;
- Mises evaluation;
- integration-point extrapolation;
- averaging;
- contour render update;
- deformation update;
- section cut;
- probe batch;
- display-group changes;
- animation frame switch.

Regressions above agreed thresholds block release.

## 0.93 — Windows/Qt production gate

Mandatory native gate on the intended production toolchain:

- Visual Studio 2026 / v145;
- Win32 GUI embedding;
- Qt 5.12 compatibility where required by application;
- supported Qt 6 path;
- multi-DPI monitors;
- context recreation;
- long interaction/animation session;
- GPU vendors used in deployment.

## 0.94 — Binding gate

Python and C# bindings expose all stable FEA result-domain functionality needed by the
application. Binding tests consume the same FEA reference corpus.

## 0.95 — Documentation freeze

- result model guide;
- contour/averaging semantics;
- section point conventions;
- bridge author guide;
- GUI integration guide;
- scripting examples;
- performance recommendations;
- migration guide.

## 0.99 — Release candidate

Only release blockers accepted.

## 1.0 — FEAViz FEA Visualization Module

1.0 is ready when a production FEA viewer can use FEAViz without VTK for its normal
post-processing workflow and users can perform the core Abaqus/Viewer-style tasks listed
in Section 2 with stable numerical semantics, interactive performance, and tested GUI
embedding.

---

# De-scoped / maintenance-only work

The following work is explicitly **not required for 1.0** unless an application needs it:

- full VTK XML encoding parity;
- every legacy VTK reader/writer option;
- VTKHDF parity solely for compatibility;
- matching VTK class names or executive APIs;
- VTK filter breadth unrelated to FEA;
- medical imaging pipelines;
- graph/network visualization;
- geospatial rendering;
- generic AMR parity unrelated to supported FEA workflows;
- WebGPU/OpenXR parity for its own sake.

Existing VTK-compatible I/O remains supported and bug-fixed, but new development effort
must justify itself through an FEA workflow.

---

# Immediate implementation order after 0.39

1. Finish ResultDatabase/Step/Frame/Field/History ownership and numerical tests.
2. Implement `FVizFEAPrimaryVariable` and display-value cache.
3. Implement integration-point extrapolation + element-nodal averaging controller.
4. Bind primary variable to mapper/legend with raw/display extrema provenance.
5. Implement deformation state independent of contour field.
6. Implement section-point selection/envelope.
7. Implement display groups and FEA-native selections.
8. Implement probe/query and XY extraction.
9. Implement frame animation/prefetch.
10. Only then expand specialized contact/beam/composite visualization.

This order is intentional: it creates one coherent post-processing state model instead
of accumulating disconnected filters that a GUI must manually coordinate.
