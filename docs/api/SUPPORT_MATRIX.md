# FEAViz 1.0 support matrix

## Supported baseline

- Language/ABI: C17 public ABI 1; headers also compile as C++17.
- Windows: MSVC 19.50+ / v145, Win32 windowing, OpenGL 3.3 modern path with
  compatibility fallback, onscreen, hidden offscreen, and child-window hosting.
- Build: CMake 3.24+, shared and static libraries, install-tree package config.
- Data/IO: typed VTU ASCII and raw appended, legacy VTK, OBJ/STL input, PLY
  output, 64-bit file/provenance IDs with checked in-memory topology boundaries.

## Explicit limitations

- Native Linux onscreen/offscreen rendering is not in the 1.0 support promise;
  portable data, pipeline, IO, math, and parallel modules remain backend-neutral.
- Compressed VTU requires a future approved compression backend; current builds
  return `FVIZ_ERROR_NOT_SUPPORTED`.
- The renderer is OpenGL-backed internally; no Vulkan/Metal/D3D backend is
  promised by ABI 1.
- Very large point IDs beyond the current checked 32-bit cell connectivity
  representation are rejected rather than truncated.
