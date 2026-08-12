#ifndef FVIZ_FEA_UNSTRUCTURED_GRID_H
#define FVIZ_FEA_UNSTRUCTURED_GRID_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizPlane.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/Mesh/FVizPoints.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizUnstructuredGrid FVizUnstructuredGrid;
#define FVIZ_TYPE_UNSTRUCTURED_GRID UINT64_C(0xD27A6F1B90C4E835)

FVIZ_API FVizResult fviz_unstructured_grid_create(FVizUnstructuredGrid** out_grid);
FVIZ_API void fviz_unstructured_grid_clear(FVizUnstructuredGrid* grid);
FVIZ_API FVizPoints* fviz_unstructured_grid_points(FVizUnstructuredGrid* grid);
FVIZ_API FVizCellArray* fviz_unstructured_grid_cells(FVizUnstructuredGrid* grid);
FVIZ_API FVizResult fviz_unstructured_grid_add_point(FVizUnstructuredGrid* grid, FVizVec3 point, uint32_t* out_id);
FVIZ_API FVizResult fviz_unstructured_grid_add_cell(FVizUnstructuredGrid* grid, FVizCellType type, FVizSize point_count, const uint32_t* point_ids);
FVIZ_API FVizAttributeSet* fviz_unstructured_grid_point_data(FVizUnstructuredGrid* grid);
FVIZ_API FVizAttributeSet* fviz_unstructured_grid_cell_data(FVizUnstructuredGrid* grid);
FVIZ_API FVizAttributeSet* fviz_unstructured_grid_field_data(FVizUnstructuredGrid* grid);
FVIZ_API FVizSize fviz_unstructured_grid_point_count(const FVizUnstructuredGrid* grid);
FVIZ_API FVizSize fviz_unstructured_grid_cell_count(const FVizUnstructuredGrid* grid);
FVIZ_API FVizBounds fviz_unstructured_grid_bounds(const FVizUnstructuredGrid* grid);
FVIZ_API FVizResult fviz_unstructured_grid_validate(const FVizUnstructuredGrid* grid);
FVIZ_API FVizResult fviz_unstructured_grid_extract_surface(const FVizUnstructuredGrid* grid, FVizPolyData** out_surface);
FVIZ_API FVizResult fviz_unstructured_grid_extract_surface_scalars(
    const FVizUnstructuredGrid* grid,
    FVizPolyData** out_surface);
FVIZ_API FVizResult fviz_unstructured_grid_slice(
    const FVizUnstructuredGrid* grid,
    FVizPlane plane,
    FVizPolyData** out_slice);
FVIZ_API FVizResult fviz_unstructured_grid_threshold_cells(
    const FVizUnstructuredGrid* grid,
    const char* scalar_name,
    double minimum,
    double maximum,
    FVizUnstructuredGrid** out_grid);

FVIZ_EXTERN_C_END

#endif /* FVIZ_FEA_UNSTRUCTURED_GRID_H */
