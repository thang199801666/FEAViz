# FEAViz 1.0 support matrix

## Supported baseline

- Language/ABI: C17 public ABI 1; headers also compile as C++17.
- Windows: MSVC 19.50+ / v145, Win32 windowing, OpenGL 3.3 modern path with
  compatibility fallback, onscreen, hidden offscreen, and child-window hosting.
- GUI integration: raw Win32 native-host API plus optional Qt 5/Qt 6 QtGui (`QWindow`) and Widgets (`QWidget`) adapters on Windows; host applications retain their own event loop and may use coalesced render requests.
- Build: CMake 3.24+, shared and static libraries, install-tree package config.
- Data/IO: ImageData, StructuredGrid, RectilinearGrid, UnstructuredGrid, PolyData,
  named MultiBlock/Partitioned/Temporal composites; typed VTU ASCII/raw appended,
  PVTU parallel manifests with lazy per-piece LRU loading, PVD temporal frames,
  legacy VTK, OBJ/STL input, PLY output, 64-bit file/provenance IDs with checked
  in-memory topology boundaries.
- Streaming: per-input piece/extent/time request remapping, whole-extent metadata,
  structured/rectilinear extent extraction, topology-aware unstructured pieces with
  multi-layer ghost cells, deterministic point ownership, and whole-mesh partition
  materialization with original-ID provenance.

## Explicit limitations

- Native Linux onscreen/offscreen rendering is not in the 1.0 support promise;
  portable data, pipeline, IO, math, and parallel modules remain backend-neutral.
- Compressed VTU requires a future approved compression backend; current builds
  return `FVIZ_ERROR_NOT_SUPPORTED`.
- The renderer is OpenGL-backed internally; no Vulkan/Metal/D3D backend is
  promised by ABI 1.
- Very large point IDs beyond the current checked 32-bit cell connectivity
  representation are rejected rather than truncated.
- Ghost generation currently partitions by balanced contiguous source-cell ranges
  and expands through exact shared-facet adjacency. Solver-native partition IDs,
  distributed MPI exchange, and arbitrary user-supplied ownership maps remain future work.
- Weighted blended OIT is supported when advertised by the OpenGL capabilities.
  Dual depth peeling is not yet advertised and requests fall back to deterministic
  sorted alpha while reporting requested/applied modes separately.
- Integer-ID single-pixel hardware picking is capability-gated. Rectangle/lasso
  selection currently uses the bounded cancellable CPU through-selection path;
  visible-only asynchronous GPU region readback remains unsupported.
- Multiple renderer viewports in one render window share mapper/glyph GPU residency
  through one device cache. Explicit resource share groups across separate render
  windows are not yet part of the public support promise.
- CPU cap generation supports closed manifold triangle-surface cut loops. Cap loops
  with holes, open cuts, non-manifold branches, or numerical self-intersections are
  rejected rather than triangulated heuristically.
