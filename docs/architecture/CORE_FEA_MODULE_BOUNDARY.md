# FEAViz Core / FEA Module Boundary

## Dependency rule

`FEAViz::Core` is the domain-neutral visualization runtime. `FEAViz::FEA` is an optional
post-processing module and depends on Core. The dependency is one-way.

```text
FEAViz::FEA  --->  FEAViz::Core
FEAViz::Core -X->  FEAViz::FEA
```

Core builds and packages successfully with `FVIZ_BUILD_FEA=OFF`.

## Core owns

- object/runtime, errors, events, commands and MTime;
- generic data objects and arrays;
- pipeline/executive/SMP/memory utilities;
- generic field statistics/interpolation/filter operations and solver-neutral deformation/geometry-update primitives;
- renderer/interaction/selection/widgets;
- Win32/Qt integration;
- generic IO retained by existing applications.

`FVizUnstructuredGrid` and `FVizFieldStatistics` are Core APIs under `FViz/Data`. The old
`FViz/FEA/...` header paths remain source-compatibility wrappers only.

## FEA owns

- ResultDatabase/Step/Frame/Field/History;
- result positions, entity/local ids and section points;
- invariants and FEA result metadata;
- PrimaryVariable/averaging/result-position policy;
- DeformedShape field-selection/mapping/scale/display-state policy;
- future contour/session/display-group/XY/contact/shell semantics.

## Internal sibling-module ABI

Optional FEAViz modules need to create `FVizObject` subclasses while preserving the same
refcount/MTime/error runtime. Three Core helpers are therefore exported for sibling-module
use: object allocation, local MTime, and TLS error propagation. Their declarations remain
under `internal/` and are not installed as public user API. This is a controlled internal
module ABI, not a general plugin ABI commitment.

## Package use

Generic application:

```cmake
find_package(FEAViz 0.41 REQUIRED COMPONENTS Core)
target_link_libraries(app PRIVATE FEAViz::Core)
```

FEA postprocessor:

```cmake
find_package(FEAViz 0.41 REQUIRED COMPONENTS Core FEA)
target_link_libraries(app PRIVATE FEAViz::FEA)
```

`FEAViz::FEAViz` remains a compatibility aggregate target and should not be used by new
code when the dependency boundary matters.

## Review rule

Before adding an API, ask: “Would this concept make sense in a CFD/scientific/CAD
visualizer with no finite elements?” If yes, it may belong in Core. If the concept assumes
steps/frames, integration points, section points, element labels, FEA averaging or solver
post-processing semantics, it belongs in FEA.
