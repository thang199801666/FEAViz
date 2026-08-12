# FEAViz 0.1.0 — First 3D Scene Milestone Verification

## Scope

This milestone extends FEAViz from the Phase 1 runtime to a first end-to-end visualization path:

```text
mesh file -> PolyData -> Actor -> Scene -> Renderer/Camera -> RenderWindow
```

## Implemented subsystems

### Core / Data

- Buffer with owned and external zero-copy memory.
- Generic dynamic Array.
- Dynamic String.
- Typed numeric DataArray with arbitrary component count.

### Math

- Vec3 operations.
- Bounds accumulation/center/radius.
- Mat4 identity, perspective, orthographic, look-at and multiply.

### Mesh

- Triangle-only PolyData for the first rendering milestone.
- Bounds tracking.
- Index validation.
- Smooth vertex-normal generation.

### IO

- OBJ vertices and polygon faces.
- OBJ negative indices.
- Polygon fan triangulation.
- ASCII STL.
- Binary STL.
- Automatic `.obj` / `.stl` dispatch.

### Scene / Rendering

- Actor ownership of PolyData.
- Scene with multiple retained actors.
- Renderer with scene, camera and background.
- Camera fit/orbit/pan/dolly.
- RenderWindow public abstraction.
- Windows Win32 + WGL OpenGL backend.
- Depth test, culling, fixed-function lighting, indexed triangles.
- Interactive example viewer.

## Verification performed in the development container

- GCC 14.2 Debug, shared, warnings-as-errors: PASS.
- GCC 14.2 Release, static + LTO, warnings-as-errors: PASS.
- Clang 17 Debug, shared, warnings-as-errors: PASS.
- ASan + UBSan test suite: PASS.
- CTest: 14/14 PASS.
- OBJ cube loading: PASS (8 points / 12 triangles).
- ASCII STL loading: PASS.
- Every public source header compiled in isolation: PASS.
- `cmake --install`: PASS.
- External C17 consumer using `find_package(FEAViz CONFIG REQUIRED)`: PASS.
- Public shared-library `fviz_*` exports detected: PASS.
- Exported `fviz_internal_*` symbols: 0.
- Project `.bat` / `.cmd` files: 0.

## Windows runtime verification boundary

The development container is Linux, so the Win32/WGL source cannot be executed here. The Windows backend is isolated behind the RenderWindow platform interface and is selected only when `WIN32` is true. The user should perform the final native runtime check with Visual Studio 2026's x64 Developer Command Prompt:

```text
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
out\build\windows-msvc-release\bin\FEAVizViewer.exe assets\testdata\cube.obj
```

Expected interaction:

- left drag: orbit
- middle drag: pan
- wheel: zoom
- F: fit
- W: wireframe
- Esc: close

## Known intentional limitations of 0.1.0

- The first Windows renderer uses the OpenGL compatibility API/client arrays. It is a backend implementation detail, not the intended final high-performance renderer.
- Rendering connectivity is currently 32-bit.
- PolyData currently renders indexed triangles only.
- OBJ material/texture/normal-index semantics are not yet preserved; FEAViz computes smooth geometry normals.
- The scene has no actor transform matrix yet.
- No picking, BVH, scalar coloring, unstructured volume mesh, or FEA result mapping yet.
- Native render window support is Windows-only at this milestone; non-Windows builds provide a clean `NOT_SUPPORTED` stub while all non-window core/mesh/scene functionality remains portable.
