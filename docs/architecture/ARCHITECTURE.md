# FEAViz Architecture

## Public ABI

FEAViz is written in C17. `FViz` is represented by naming conventions rather than a C++ namespace:

- `FVizObject`, `FVizPolyData`, ... for types.
- `fviz_object_*`, `fviz_poly_data_*`, ... for functions.
- `FVIZ_*` for macros and constants.

Only headers below `include/FViz` are public. Headers below `internal/FViz` are private implementation details and may change without ABI guarantees.

## Long-term layers

```text
Core / System
      |
     Math
      |
     Data
      |
     Mesh
   /      \
Geometry  FEA
   |       |
Spatial   |
   \      /
 Algorithms
      |
   Pipeline
      |
 Rendering
      |
Interaction

IO adapts external formats into Data/Mesh/FEA.
Parallel is an internal service used by compute-heavy layers.
Plugins extend optional formats/backends without contaminating the core ABI.
```

## Hard dependency rules

1. Core cannot depend on Mesh, Rendering, FEA, OpenGL, or IO.
2. Math must remain allocation-free for primitive operations and independent of Rendering.
3. Data must never depend on Rendering.
4. Mesh owns topology/data representation, not rendering state.
5. Geometry and Algorithms must not contain GPU backend calls.
6. Rendering frontend must not leak OpenGL/Vulkan/D3D types into the public API.
7. Toolkit-specific event handling (Qt/Win32/etc.) stays outside FEAViz Interaction.
8. Third-party dependencies must be isolated behind optional modules or adapters.

## Binary strategy

The default Phase 0 target is one `FEAViz` library. Internal module boundaries are kept in the source tree so the project can later split into multiple binary targets without reorganizing source files.
