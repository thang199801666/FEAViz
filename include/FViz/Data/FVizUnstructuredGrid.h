#ifndef FVIZ_DATA_UNSTRUCTURED_GRID_H
#define FVIZ_DATA_UNSTRUCTURED_GRID_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizPlane.h>
#include <FViz/Math/FVizTransform.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/Mesh/FVizPoints.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizUnstructuredGrid FVizUnstructuredGrid;
#define FVIZ_TYPE_UNSTRUCTURED_GRID UINT64_C(0xD27A6F1B90C4E835)

FVIZ_API FVizResult fviz_unstructured_grid_create(FVizUnstructuredGrid** out_grid);
FVIZ_API FVizResult fviz_unstructured_grid_shallow_copy(const FVizUnstructuredGrid* source,
                                                        FVizUnstructuredGrid** out_copy);
FVIZ_API void fviz_unstructured_grid_clear(FVizUnstructuredGrid* grid);
FVIZ_API FVizPoints* fviz_unstructured_grid_points(FVizUnstructuredGrid* grid);
FVIZ_API FVizCellArray* fviz_unstructured_grid_cells(FVizUnstructuredGrid* grid);
FVIZ_API FVizResult fviz_unstructured_grid_reserve(FVizUnstructuredGrid* grid, FVizSize point_capacity,
                                                   FVizSize cell_capacity, FVizSize connectivity_capacity);
FVIZ_API FVizResult fviz_unstructured_grid_add_point(FVizUnstructuredGrid* grid, FVizVec3 point, uint32_t* out_id);
FVIZ_API FVizResult fviz_unstructured_grid_add_points(FVizUnstructuredGrid* grid, const FVizVec3* points,
                                                      FVizSize point_count, uint32_t* out_first_id);
FVIZ_API FVizResult fviz_unstructured_grid_add_points_ids(FVizUnstructuredGrid* grid, const FVizVec3* points,
                                                          FVizSize point_count, FVizId* out_first_id);
FVIZ_API FVizResult fviz_unstructured_grid_add_cell(FVizUnstructuredGrid* grid, FVizCellType type, FVizSize point_count,
                                                    const uint32_t* point_ids);
FVIZ_API FVizResult fviz_unstructured_grid_add_cell_ids(FVizUnstructuredGrid* grid, FVizCellType type,
                                                        FVizSize point_count, const FVizId* point_ids);
FVIZ_API FVizResult fviz_unstructured_grid_add_cells_fixed_ids(FVizUnstructuredGrid* grid, FVizCellType type,
                                                               FVizSize points_per_cell, FVizSize cell_count,
                                                               const FVizId* point_ids);
FVIZ_API FVizResult fviz_unstructured_grid_add_cells_fixed(FVizUnstructuredGrid* grid, FVizCellType type,
                                                           FVizSize points_per_cell, FVizSize cell_count,
                                                           const uint32_t* point_ids);
FVIZ_API FVizAttributeSet* fviz_unstructured_grid_point_data(FVizUnstructuredGrid* grid);
FVIZ_API FVizAttributeSet* fviz_unstructured_grid_cell_data(FVizUnstructuredGrid* grid);
FVIZ_API FVizAttributeSet* fviz_unstructured_grid_field_data(FVizUnstructuredGrid* grid);
FVIZ_API FVizSize fviz_unstructured_grid_point_count(const FVizUnstructuredGrid* grid);
FVIZ_API FVizSize fviz_unstructured_grid_cell_count(const FVizUnstructuredGrid* grid);
FVIZ_API FVizBounds fviz_unstructured_grid_bounds(const FVizUnstructuredGrid* grid);
FVIZ_API FVizResult fviz_unstructured_grid_validate(const FVizUnstructuredGrid* grid);
FVIZ_API FVizResult fviz_unstructured_grid_extract_surface(const FVizUnstructuredGrid* grid,
                                                           FVizPolyData** out_surface);
FVIZ_API FVizResult fviz_unstructured_grid_extract_surface_scalars(const FVizUnstructuredGrid* grid,
                                                                   FVizPolyData** out_surface);
/* Extracts renderable geometry from mixed-dimensional FEA meshes.
 * 0D cells become vertices, 1D cells line segments, 2D cells triangles, and
 * 3D cells contribute only exterior faces. Quadratic cells are tessellated
 * through their midside nodes while preserving source-cell provenance. */
FVIZ_API FVizResult fviz_unstructured_grid_extract_geometry(const FVizUnstructuredGrid* grid,
                                                            FVizPolyData** out_geometry);
FVIZ_API FVizResult fviz_unstructured_grid_slice(const FVizUnstructuredGrid* grid, FVizPlane plane,
                                                 FVizPolyData** out_slice);
FVIZ_API FVizResult fviz_unstructured_grid_threshold_cells(const FVizUnstructuredGrid* grid, const char* scalar_name,
                                                           double minimum, double maximum,
                                                           FVizUnstructuredGrid** out_grid);
FVIZ_API FVizResult fviz_unstructured_grid_warp_by_vector(const FVizUnstructuredGrid* grid, const char* vector_name,
                                                          double scale, FVizUnstructuredGrid** out_grid);
FVIZ_API FVizResult fviz_unstructured_grid_warp_by_array(const FVizUnstructuredGrid* grid, const FVizDataArray* vectors,
                                                         double scale, FVizUnstructuredGrid** out_grid);
FVIZ_API FVizResult fviz_unstructured_grid_cell_data_to_point_data(const FVizUnstructuredGrid* grid,
                                                                   FVizUnstructuredGrid** out_grid);
FVIZ_API FVizResult fviz_unstructured_grid_transform(const FVizUnstructuredGrid* grid, const FVizTransform* transform,
                                                     FVizUnstructuredGrid** out_grid);

/* Computes per-point gradients of a point scalar or vector field using a
 * least-squares fit over the points' incident cells (VTK vtkGradientFilter
 * compatible). For a scalar field the output has 3 components (dx,dy,dz); for
 * a vector field with components N the output has 3*N components (the full
 * Jacobian). The gradient array is added to the output grid's point data under
 * output_name; the grid topology is shared. */
FVIZ_API FVizResult fviz_unstructured_grid_gradient(const FVizUnstructuredGrid* grid, const char* scalar_array_name,
                                                    const char* output_name, FVizUnstructuredGrid** out_grid);

/* Per-cell derivatives of a point scalar or vector field (vtkCellDerivatives
 * compatible). Fits a linear field over each cell's own points and stores the
 * gradient (3*N components for an N-component input) as a cell-data array under
 * output_name. Exact for linear fields on affine cells. */
FVIZ_API FVizResult fviz_unstructured_grid_cell_derivatives(const FVizUnstructuredGrid* grid,
                                                            const char* scalar_array_name, const char* output_name,
                                                            FVizUnstructuredGrid** out_grid);

/* Warps points by a scalar field (vtkWarpScalar compatible). Points are
 * displaced along the +Z axis by scalar * scale (the classic VTK default), or
 * along point normals when the optional normal array is supplied. */
FVIZ_API FVizResult fviz_unstructured_grid_warp_scalar(const FVizUnstructuredGrid* grid, const char* scalar_array_name,
                                                       double scale, const char* normal_array_name,
                                                       FVizUnstructuredGrid** out_grid);

/* Integrates streamlines through a 3-component point vector field from the
 * given seed points (vtkStreamTracer compatible, Euler + RK4 steps). Returns a
 * PolyData of polylines; the interpolated vector magnitude is stored per
 * output point under "speed". */
FVIZ_API FVizResult fviz_unstructured_grid_stream_tracer(const FVizUnstructuredGrid* grid,
                                                         const char* vector_array_name, const FVizVec3* seed_points,
                                                         FVizSize seed_count, double step_length, FVizSize max_steps,
                                                         FVizPolyData** out_lines);

/* Cuts a grid with multiple planes and merges the result into one PolyData
 * (vtkCutter compatible). Each plane produces a slice with interpolated point
 * scalars; the merged output shares point data arrays. */
FVIZ_API FVizResult fviz_unstructured_grid_cutter(const FVizUnstructuredGrid* grid, const FVizPlane* planes,
                                                  FVizSize plane_count, FVizPolyData** out_cut);

/* Extracts a 3-D iso-surface from a volumetric unstructured grid
 * (vtkContourFilter compatible, marching tetra). Produces a triangle PolyData
 * whose point data carries the scalar value. Tetrahedral cells and arbitrary
 * simplex decompositions are supported. */
FVIZ_API FVizResult fviz_unstructured_grid_iso_surface(const FVizUnstructuredGrid* grid, const char* scalar_array_name,
                                                       double iso_value, FVizPolyData** out_surface);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_UNSTRUCTURED_GRID_H */
