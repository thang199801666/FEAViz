# FEAViz development plan: 0.9 to 1.0

## Direction

The next development cycle turns the useful 0.8 pipeline into a general,
VTK-style visualization architecture without copying VTK's C++ API. FEAViz
keeps its opaque C17 objects, explicit ownership, small public surface, and
headless-testable behavior.

The dependency order is intentional:

```text
DataObject + Algorithm ports
            |
            v
Demand-driven executive + cancellation
            |
            +------------------+
            v                  v
Persistent scheduler     Renderer/pass model
            |                  |
            +---------+--------+
                      v
          Selection + interaction widgets
                      |
                      v
               1.0 stabilization
```

## 0.9.0 - General algorithm and port model

### Scope

- Add `FVizDataObject` as the common pipeline data base and make dataset,
  unstructured-grid, and polygonal outputs usable through it.
- Add `FVizAlgorithm` for sources, filters, and sinks.
- Add lightweight `FVizAlgorithmOutput` proxies containing producer and output
  port index.
- Describe input/output port count, accepted data types, optional/repeatable
  input connections, and output type.
- Support direct input data as well as `set`, `add`, `remove`, and query input
  connections by `(port, connection)`.
- Preserve the current `FVizFilter` and mapper connection functions as
  compatibility wrappers.
- Retain graph ownership and cycle detection guarantees.

### Validation

- Unit tests for zero-, one-, and multi-input algorithms and multi-output
  producers.
- Type-contract, invalid-port, replacement, ownership, and cycle tests.
- Run the existing HEX8 FEA chain entirely through output ports and compare its
  geometry/scalars with the 0.8 pipeline.

### Definition of done

The mapper accepts an `FVizAlgorithmOutput`; downstream code no longer needs to
know whether its producer is a filter or source, and all 0.8 connection APIs
continue to pass their tests.

## 0.10.0 - Demand-driven executive

### Scope

- Add `FVizExecutive` to own graph traversal and execution state instead of
  keeping recursive update policy inside `FVizFilter`.
- Implement a compact request sequence: output information, output allocation,
  update extent/piece, and data execution.
- Propagate composite MTime, requested output port, errors, progress, and abort
  state through the graph.
- Add algorithm progress observers and cooperative cancellation.
- Add source and sink examples plus a two-input append/merge algorithm to prove
  the generalized contract.
- Keep execution synchronous in this milestone; requests must be deterministic
  and re-entrant failures must return an error rather than deadlock.

### Validation

- Verify upstream execution order, cache hits, branch fan-out, and that a shared
  producer executes only once per unchanged request.
- Test cancellation, failure propagation, re-entrant update rejection, and
  output replacement after MTime changes.
- Add graph-level diagnostics that print algorithms, ports, connections, and
  last execution state.

### Definition of done

Rendering, camera fitting, and picking pull data through the executive, with no
filter-specific recursion in consumers.

## 0.11.0 - Persistent parallel runtime and parallel algorithms

### Scope

- Replace per-call worker creation with a persistent bounded thread pool.
- Add task groups, `parallel_for`, thread-local scratch storage, cooperative
  cancellation, nested-call handling, and a serial fallback.
- Keep chunk ownership explicit and results deterministic for a fixed input.
- Parallelize the expensive independent phases of surface extraction,
  cell-to-point accumulation, contour/slice generation, and BVH construction.
- Separate parallel computation from ordered topology assembly where stable
  output ordering matters.
- Expose runtime statistics useful for tests and profiling without exposing
  scheduler internals.

### Validation

- Serial/parallel output equivalence tests for geometry, attributes, bounds,
  and picking.
- Stress tests for nested ranges, cancellation, small workloads, thread limits,
  allocation failure, and repeated runtime startup/shutdown.
- Benchmarks using medium and large HEX8 meshes; record time, peak memory, and
  scaling instead of enforcing machine-specific speed thresholds in CTest.

### Definition of done

The scheduler reuses workers, algorithms remain correct at thread limits 1 and
N, and the large-mesh benchmark demonstrates useful scaling without changing
public data results.

## 0.12.0 - Renderer, render-window, and widget architecture

### Scope

- Allow a render window to own multiple renderers.
- Add normalized viewports, renderer layers, active/interactive flags, and
  deterministic renderer ordering.
- Split device rendering into opaque geometry, translucent geometry, edge/line,
  selection, and overlay passes.
- Add world/view/display coordinate conversion and viewport-aware camera fit,
  picking, and event routing.
- Complete render-window lifecycle APIs: initialize, resize, render, finalize,
  offscreen mode, and native-handle/child-window attachment where Win32 permits.
- Extend `FVizRendererWidget` with explicit initialize/show/process-events/start
  separation, resize notification, ownership setters, and non-blocking event
  processing for host applications.
- Keep OpenGL details private so another graphics backend can implement the same
  renderer contract later.

### Validation

- Headless tests for viewport math, layer ordering, renderer routing, resizing,
  and lifecycle idempotency.
- Image tests for two viewports, a 3D scene plus overlay layer, translucent and
  wireframe passes, and the Rainbow bent-beam example.
- Win32 smoke test for standalone and embedded/non-blocking widget operation.

### Definition of done

One render window can reliably display multiple independent FEA views and
overlays, while `FVizRendererWidget` works both as a standalone viewer and as a
host-controlled component.

## 0.13.0 - Interaction, selection, and widgets

### Scope

- Complete interactor state with initialize/enable/disable/done, event
  processing, one-shot/repeating timers, render enable, and update-rate hints.
- Route events to the renderer under the pointer in multi-viewport windows.
- Add picker and selection abstractions for actor, point, cell, and area results.
- Add trackball-actor and rubber-band selection styles while retaining the
  trackball-camera style.
- Add selection highlighting and an orientation-axes widget implemented as an
  observer plus overlay renderer.
- Define focus, event capture, style switching, abort, and nested-dispatch
  behavior explicitly.

### Validation

- Drive all styles using synthetic events in headless tests.
- Test timers, enable/disable, renderer routing, observer priority, style
  switching during dispatch, and selection persistence after pipeline updates.
- Add an interactive FEA example with rectangle cell selection, highlighted
  cells, scalar readout, and orientation axes.

### Definition of done

Applications can select and inspect FEA entities without implementing native
event handling, and the same interaction logic works in standalone and embedded
widgets.

## 0.14.0 - Data, rendering, and IO completeness

### Scope

- Add a general transform object and transform algorithm.
- Complete polygonal topology with vertices, lines, triangles, and general
  polygons; define a 64-bit connectivity path.
- Complete mapper array selection by name/association/component and point/cell
  scalar modes.
- Add NaN, below-range, and above-range lookup-table colors, opacity mapping,
  edge properties, and clipping planes.
- Add PLY and VTU writing; add compressed VTU reading/writing behind the bundled
  compression option.
- Define deep-copy, shallow-copy, and structure-copy contracts for data objects.

### Validation

- Round-trip IO tests, malformed-input tests, 32/64-bit topology parity, and
  scalar-association rendering tests.
- Visual regression scenes for point data, cell data, NaN values, opacity,
  clipping, edges, and transformed datasets.

### Definition of done

Common FEA datasets can be loaded, transformed, colored, selected, and exported
without application-side topology or scalar conversion.

## 1.0.0 - Stable public release

### Scope

- Freeze the ABI-1 public API and document ownership, thread-safety, MTime, and
  error behavior for every exported function.
- Audit null handling, overflow, large allocations, object lifetime cycles, and
  public/private header boundaries.
- Add install-tree consumer tests for shared and static builds.
- Establish deprecation policy, semantic versioning rules, and supported compiler
  and platform matrix.
- Publish API documentation, migration guide, architecture guide, and a focused
  gallery of FEA workflows.
- Record baseline performance and visual-regression artifacts for future releases.

### Release gates

- Warnings-as-errors build and all unit/integration tests pass on supported
  configurations.
- Sanitizer jobs pass where supported; no known high-severity lifetime or data
  race issue remains.
- Fresh consumers can install, find, link, and run FEAViz without source-tree
  paths.
- Existing bent-beam, connected-pipeline, picking, and interaction examples pass
  headless validation and visual review.

## Cross-cutting rules

- Each milestone updates the changelog, roadmap, design notes, public umbrella
  header, install/export tests, and at least one end-to-end example.
- New cacheable objects use composite MTime; raw mutable memory continues to
  require an explicit `Modified()` call.
- Public APIs remain backend-neutral and opaque. Platform, OpenGL, and scheduler
  types stay internal.
- Parallel code must have a deterministic serial path and be tested at thread
  limits 1 and N.
- Visual features require both state-level headless tests and a small reference
  image or interactive smoke test.
- Compatibility wrappers are removed only in a later major version, not during
  the 0.x-to-1.0 cycle.

## Immediate implementation order for 0.9.0

1. Introduce `FVizDataObject` without changing concrete object ownership.
2. Add `FVizAlgorithmOutput` and immutable output-port queries.
3. Add input-port connection storage and data-type validation.
4. Move existing filter update callbacks behind `FVizAlgorithm`.
5. Adapt mapper connections and keep the old filter APIs as wrappers.
6. Add multi-input and multi-output test algorithms.
7. Convert the connected bent-beam pipeline and update documentation.
