# FEAViz Roadmap

## Phase 0 — Foundation repository — COMPLETE

- [x] Repository architecture
- [x] CMake/C17 baseline
- [x] CMake-first Windows build using NMake + MSVC v145
- [x] Compiler warning policy
- [x] Platform/compiler abstraction macros
- [x] Public/private include boundary
- [x] DLL/shared-library visibility macros
- [x] Generated version/config headers
- [x] Static/shared build option
- [x] Install/export package support
- [x] CTest infrastructure
- [x] Optional ASan/UBSan and LTO hooks

## Phase 1 — Core runtime — COMPLETE in 0.0.6

- [x] Allocator interface and default allocator
- [x] Allocation/reallocation/free wrappers
- [x] Opaque object runtime and type identity
- [x] Atomic reference counting
- [x] Result/error system
- [x] Thread-local last-error storage
- [x] Logging interface
- [x] Unit/leak/stress/thread tests

## Phase 2 — Core containers — COMPLETE in 0.2.0

- [x] `FVizBuffer`
- [x] Ownership-aware zero-copy external buffer wrapping
- [x] Generic dynamic `FVizArray`
- [x] `FVizString`
- [x] Bit array
- [x] Hash map

## Phase 3 — Math — COMPLETE in 0.2.0

- [x] `FVizVec3`
- [x] `FVizMat4`
- [x] Perspective projection
- [x] Look-at/view matrix
- [x] Bounds
- [x] Vec2 / Vec4
- [x] Mat3
- [x] Quaternion
- [x] Ray / Plane / AABB
- [ ] General transform object

## Phase 4 — Data foundation — COMPLETE

- [x] Numeric data type enumeration
- [x] Typed/component-aware `FVizDataArray`
- [x] Attribute sets
- [x] Point data / cell data / field data
- [x] Data object / dataset base types

## Phase 5 — Surface mesh — FIRST USABLE IMPLEMENTATION COMPLETE

- [x] `FVizPolyData`
- [x] Point storage
- [x] 32-bit indexed triangles
- [x] Bounds
- [x] Mesh validation
- [x] Smooth vertex-normal generation
- [x] Dedicated Points / CellArray objects
- [ ] Lines / vertices / general polygons
- [ ] 64-bit connectivity path

## Phase 6 — Mesh IO — FIRST USABLE IMPLEMENTATION COMPLETE

- [x] OBJ vertex/face reader
- [x] OBJ polygon fan triangulation
- [x] Positive and negative OBJ vertex indices
- [x] ASCII STL reader
- [x] Binary STL reader
- [x] Extension-dispatching mesh reader
- [ ] PLY
- [ ] Legacy VTK
- [ ] VTU

## Phase 7 — Scene and camera — FIRST USABLE IMPLEMENTATION COMPLETE

- [x] Actor
- [x] Multi-actor Scene
- [x] Renderer frontend
- [x] Perspective Camera
- [x] Scene bounds
- [x] Fit-camera
- [x] Orbit / pan / dolly primitives

## Phase 8 — First visible 3D scene — MILESTONE COMPLETE in 0.1.0

- [x] RenderWindow public abstraction
- [x] Native Windows Win32 window
- [x] WGL OpenGL context
- [x] Depth testing and back-face culling
- [x] Smooth normal lighting
- [x] Indexed triangle rendering
- [x] Actor color
- [x] Wireframe mode
- [x] Mouse orbit
- [x] Mouse pan
- [x] Wheel zoom
- [x] Fit view hotkey
- [x] OBJ/STL command-line viewer
- [x] Built-in cube fallback

### 0.1.0 definition of done

```text
OBJ/STL file
    -> FVizPolyData
    -> FVizActor
    -> FVizScene
    -> FVizRenderer + FVizCamera
    -> FVizRenderWindow
    -> visible interactive 3D scene on Windows
```

## Phase 9 — Modern GPU renderer — MILESTONE COMPLETE in 0.1.4

- [x] Internal OpenGL 3.3 function loader with no third-party dependency
- [x] Probe-context WGL extension discovery (`wglChoosePixelFormatARB`/`wglCreateContextAttribsARB`)
- [x] OpenGL 3.3 core-profile context with legacy 1.1 fallback
- [x] GLSL 330 per-pixel lighting program (Lambert + ambient)
- [x] Flat-shading fallback for meshes without computed normals
- [x] Per-actor GPU cache (VAO + position/normal VBO + index EBO)
- [x] Cache invalidation via `FVizPolyData` generation counter
- [x] Wireframe mode preserved through `glPolygonMode`
- [x] Public scene API unchanged
- [x] Per-actor model transform (position/orientation/scale) with normal matrix (0.2.1)

## Phase 10 — VTK-style mapper pipeline — MILESTONE COMPLETE in 0.3.0

- [x] `FVizLookupTable` with range, divergent colormap, and interpolated scalar mapping
- [x] `FVizMapper` data source with lookup table and scalar coloring configuration
- [x] Actor owns a default mapper; `set_poly_data` API preserved
- [x] Per-point scalars on `FVizPolyData`
- [x] Per-vertex scalar coloring through the shader color attribute
- [ ] Pipeline/filter execution (clip/slice/threshold/warp) Threshold cell filter **COMPLETE**, slice filter **COMPLETE in 0.3.1**

## Phase 11 — FEA result visualization — MILESTONE PARTIAL in 0.3.1

- [x] Surface extraction with point scalar transfer (0.3.1)
- [x] Interior slice/cut-plane filter with scalar interpolation (0.3.1)
- [ ] Warp-by-vector deformation filter
- [ ] Cell-data to point-data interpolation for smooth stress contours
- [ ] General pipeline/filter framework

## Immediate work after 0.3.1

1. Warp-by-vector deformation filter and cell-data to point-data interpolation (Batch 6).
2. General `FVizAlgorithm`/filter framework + a complete FEA viewer example (Batch 7).
3. Add spatial acceleration (BVH, point locator, cell locator) and picking.
4. Add `FVizPoints`, `FVizCellArray`, topology-aware attributes and dataset subclasses.
5. Add `FVizUnstructuredGrid` and linear FEA cell types (TRI/QUAD/TET/HEX/WEDGE/PYRAMID). **COMPLETE**
6. Implement surface extraction from volumetric FE meshes. **COMPLETE**
