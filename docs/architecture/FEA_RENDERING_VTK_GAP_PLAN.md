# FEAViz / VTK gap analysis — FEA result rendering

Status: active gap-closing plan (2026-08-16)
Scope: the FEA result **rendering** path only (deformation, contour, edges,
scalar bars, section, display options). General VTK breadth outside FEA
post-processing is out of scope here.

## How to read this document

VTK is used as the *capability reference* for FEA post-processing. The gap is
the set of VTK workflows a desktop/HPC FEA post-processor uses that FEAViz
cannot already express. Columns: **VTK workflow** → **FEAViz today** →
**gap** → **priority**.

"Shipped" means the capability exists and is covered by a test.
"Partial" means usable but with a known limitation or missing convenience.

## Current FEA rendering pipeline (what TestVisualization does today)

```
load(.vtu/.vtk) ─→ UnstructuredGrid
  └─ cell_data_to_point_data (smooth cell results)
  └─ warp_by_vector(scale)              ← deformation
  └─ extract_geometry                   ← surface extraction
  └─ fviz_fea_build_abaqus_banded_surface  ← 12-band contour surface
  └─ fviz_fea_extract_element_edges     ← FE perimeter lines
  └─ actor + mapper(contour colors) + scalar bar (legend)
```

This covers: deformation, banded contour, element edges, scalar legend, and
parallel/perspective camera with orbit/pan/zoom. It does **not** yet cover the
VTK workflows in the table below.

## Gap table

| # | VTK workflow | FEAViz today | Gap | Priority |
|---|---|---|---|---|
| G1 | **Line contour overlay** (`vtkContourFilter` on surface, colored by value) | Core `FVizContourFilter` exists (line output on PolyData) but not exposed through the FEA module; no convenience to build line contours matching the current contour LUT | Add `fviz_fea_build_contour_lines()` returning a line PolyData colored by the contour LUT, with the same range/interval conventions as the banded surface | **High** |
| G2 | **Continuous (non-banded) contour** (`vtkLookupTable` smooth scalar map) | Mapper supports continuous scalar coloring via a lookup table (tested in `test_mapper`); the FEA module only builds *banded* surfaces | Add `fviz_fea_build_contour_surface()` (smooth, per-vertex RGB) as the analog of banded; document mapper path | High |
| G3 | **Min/max markers with original entity labels** (`vtkScalarBarActor` range + ParaView annotations) | Scalar legend has a range/title; no extrema reporting from the FEA module | Add `fviz_fea_find_extrema()` returning value + original cell/face provenance for min/max labels | High |
| G4 | **Averaging on/off visible in contour** (VTK `vtkCellDataToPointData` + `vtkDataSetMapper` ScalarMode) | `FVizFEAPrimaryVariable` computes raw/display values and discontinuity mask; **not wired to rendering** | Add `fviz_fea_build_contour_surface_from_result()` taking a `FVizFEAPrimaryVariableResult` so averaged vs raw contours come from one code path | Medium |
| G5 | **Section cut with result interpolation** (`vtkSlice`/`vtkCutter` + `vtkProbeFilter`) | `fviz_unstructured_grid_slice` exists; scalar interpolation onto the cut surface is not verified in FEA tests | Verify/complete scalar transfer on slice; add FEA helper `fviz_fea_slice_contour()` | Medium |
| G6 | **Above/below-range colors** (`vtkLookupTable` SetAboveRangeColor/SetBelowRangeColor) | `fviz_lookup_table_set_above/below_range_color` exists; banded surface clamps to endpoints instead | Banded surface should honor above/below colors (or expose explicit clamp vs extend) | Medium |
| G7 | **Deformed vs undeformed overlay color** (`vtkWarpVector` + separate actor) | **Closed** — `fea::SuperimposedDisplay::build()` in the C++ binding builds the deformed solid actor plus a translucent undeformed wireframe/ghost actor from a `FVizFEADeformedShapeResult` and adds both to a scene | Low → **done** |
| G8 | **Element (facet) contour without point averaging** | **Closed** — `fviz_fea_build_element_facet_surface()` colors every triangle flat by its source cell's scalar from grid cell data (bypassing nodal averaging), with per-triangle provenance | Low → **done** |

## Plan to close the gap

The ordering below is intentional: G1–G3 give the three most visible
Abaqus/VTK behaviors with the least new architecture, and G4 builds on the
same surface-building helper.

### WP-1 (High): FEA contour lines — `fviz_fea_build_contour_lines`

- API (in `include/FViz/FEA/FVizVisualization.h`):
  ```c
  FVIZ_FEA_API FVizResult fviz_fea_build_contour_lines(
      const FVizPolyData* input,
      const char* scalar_array_name,
      uint32_t components,
      float range_minimum,
      float range_maximum,
      uint32_t interval_count,
      const char* output_scalar_array_name, /* e.g. "contour_level" */
      FVizPolyData** out_lines);
  ```
- Behavior: generate interval mid-levels, run the core `FVizContourFilter`,
  attach the level scalar to each output vertex, and copy provenance
  (`FVizOriginalCellIds` / `FVizOriginalFaceIds`) through so lines can be
  picked and mapped back to elements.
- Test: build a small quad surface with a linear scalar; assert line count > 0,
  every output vertex carries the level scalar, and provenance arrays survive.
- Also expose a C++ `fea::buildContourLines(...)` wrapper in `FVizCppFEA.hpp`.

### WP-2 (High): Smooth contour surface — `fviz_fea_build_contour_surface`

- API: same signature shape as `fviz_fea_build_abaqus_banded_surface` but maps
  each vertex through the Abaqus rainbow (continuous) instead of splitting
  triangles into bands.
- Behavior: copy input points/triangles; per-vertex RGB = rainbow of
  normalized scalar; NaN/non-finite → grey; keep provenance.
- Test: a 4-point quad → output has same topology, per-vertex RGB arrays.

### WP-3 (High): FEA extrema with provenance — `fviz_fea_find_extrema`

- API:
  ```c
  typedef struct FVizFEAExtrema
  {
      uint32_t struct_size;
      FVizSize min_point_id;
      FVizSize max_point_id;
      double min_value;
      double max_value;
      uint64_t min_cell_id;  /* FVIZ_INVALID_ID if unavailable */
      uint64_t max_cell_id;
      uint64_t min_face_id;
      uint64_t max_face_id;
  } FVizFEAExtrema;
  FVIZ_FEA_API FVizResult fviz_fea_find_extrema(
      const FVizPolyData* surface,
      const char* scalar_array_name,
      uint32_t components,
      FVizFEAExtrema* out_extrema);
  ```
- Behavior: scan the surface scalar (magnitude for vectors), track min/max
  point ids, and resolve the original cell/face via provenance.
- Test: known linear ramp; assert values and point ids.

### WP-4 (Medium): Result-driven contour — `fviz_fea_build_contour_surface_from_result`

- Takes a `FVizFEAPrimaryVariableResult` (raw/display values + ids) plus a
  target `FVizUnstructuredGrid`, extracts the grid surface, maps display values
  by entity id, and builds either banded or smooth RGB. This is the single path
  that makes averaged-vs-raw contours observable.
- Test: two-material discontinuous stress → display (averaged) vs raw differ;
  contour reflects the selected mode.

### WP-5 (Medium): Banded out-of-range colors

- Extend the banded builder (new `_ex` variant or option struct) to honor the
  lookup table's above/below range colors instead of clamping to the last band.
- Test: value below min / above max → above/below color appears.

## Definition of done

For each WP:

- Public C API added to `include/FViz/FEA/FVizVisualization.h` (Core-neutral,
  no Abaqus dependency).
- C regression test added under `tests/FEA/`.
- C++ wrapper added to `bindings/cpp/include/FVizCpp/FVizCppFEA.hpp` and
  exercised in `tests/Cpp/test_cpp_fea.cpp`.
- Release build with warnings-as-errors + full ctest + ASan gate.
- `CHANGELOG.md` updated.

## Non-goals (keep VTK breadth out)

- VTK format/source parity for its own sake.
- Volume rendering, graph/chart/molecule ecosystems.
- GPU-specific rendering breadth already tracked by the renderer roadmap.

## Execution checkpoint

**All gaps G1–G8 are closed (2026-08-16).**

| Gap | API | Status |
|---|---|---|
| G1 contour lines | `fviz_fea_build_contour_lines` + `fea::buildContourLines` | done |
| G2 smooth contour | `fviz_fea_build_contour_surface` + `fea::buildContourSurface` | done |
| G3 extrema | `fviz_fea_find_extrema` + `fea::Extrema` | done |
| G4 result contour | `fviz_fea_build_contour_surface_from_result` + `fea::buildContourFromResult` | done |
| G5 section cut | `fviz_fea_slice_contour` + `fea::sliceContour` | done |
| G6 out-of-range colors | `fviz_fea_build_abaqus_banded_surface_ex` + `fea::buildBandedSurfaceEx` | done |
| G7 superimposed display | `fea::SuperimposedDisplay::build` (C++) | done |
| G8 element facet contour | `fviz_fea_build_element_facet_surface` + `fea::buildElementFacetSurface` | done |

C coverage: `FViz.FEA.VisualizationContours`. C++ coverage:
`FViz.Cpp.FEABinding` + `FViz.Cpp.Features`. Gates: Release warnings-as-errors,
full ctest, ASan, Core-only build.

Next candidates (beyond this plan's scope): a full `FVizFEAResultView`/session
controller, section-point/envelope engine, and display-group visibility —
tracked in the Abaqus FEA visualization master plan (0.42–0.49).
