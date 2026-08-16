#ifndef FVIZ_DATA_RECTILINEAR_GRID_H
#define FVIZ_DATA_RECTILINEAR_GRID_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizCellArray.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRectilinearGrid FVizRectilinearGrid;
#define FVIZ_TYPE_RECTILINEAR_GRID UINT64_C(0xD2E489716B50CA3F)

FVIZ_API FVizResult fviz_rectilinear_grid_create(FVizRectilinearGrid** out_grid);
FVIZ_API void fviz_rectilinear_grid_clear(FVizRectilinearGrid* grid);
FVIZ_API FVizResult fviz_rectilinear_grid_set_extent(
    FVizRectilinearGrid* grid, const int64_t extent[6]);
FVIZ_API void fviz_rectilinear_grid_extent(
    const FVizRectilinearGrid* grid, int64_t out_extent[6]);
FVIZ_API void fviz_rectilinear_grid_dimensions(
    const FVizRectilinearGrid* grid, FVizSize out_dimensions[3]);
FVIZ_API uint32_t fviz_rectilinear_grid_dimension(const FVizRectilinearGrid* grid);
FVIZ_API FVizSize fviz_rectilinear_grid_point_count(const FVizRectilinearGrid* grid);
FVIZ_API FVizSize fviz_rectilinear_grid_cell_count(const FVizRectilinearGrid* grid);
FVIZ_API FVizCellType fviz_rectilinear_grid_cell_type(const FVizRectilinearGrid* grid);

/* axis: 0=X, 1=Y, 2=Z. Coordinate arrays are retained, must have one component,
 * and their tuple count must equal the corresponding extent dimension. */
FVIZ_API FVizResult fviz_rectilinear_grid_set_coordinates(
    FVizRectilinearGrid* grid, uint32_t axis, FVizDataArray* coordinates);
FVIZ_API FVizResult fviz_rectilinear_grid_set_coordinate_values(
    FVizRectilinearGrid* grid, uint32_t axis, const double* values, FVizSize count);
FVIZ_API FVizDataArray* fviz_rectilinear_grid_coordinates(FVizRectilinearGrid* grid, uint32_t axis);
FVIZ_API const FVizDataArray* fviz_rectilinear_grid_const_coordinates(
    const FVizRectilinearGrid* grid, uint32_t axis);
FVIZ_API FVizResult fviz_rectilinear_grid_point(
    const FVizRectilinearGrid* grid, FVizId point_id, FVizVec3* out_point);
FVIZ_API FVizBounds fviz_rectilinear_grid_bounds(const FVizRectilinearGrid* grid);

FVIZ_API FVizResult fviz_rectilinear_grid_point_id(
    const FVizRectilinearGrid* grid, int64_t i, int64_t j, int64_t k, FVizId* out_point_id);
FVIZ_API FVizResult fviz_rectilinear_grid_point_ijk(
    const FVizRectilinearGrid* grid, FVizId point_id, int64_t out_ijk[3]);
FVIZ_API FVizResult fviz_rectilinear_grid_cell_id(
    const FVizRectilinearGrid* grid, int64_t i, int64_t j, int64_t k, FVizId* out_cell_id);
FVIZ_API FVizResult fviz_rectilinear_grid_cell_ijk(
    const FVizRectilinearGrid* grid, FVizId cell_id, int64_t out_ijk[3]);
FVIZ_API FVizResult fviz_rectilinear_grid_cell_point_ids(
    const FVizRectilinearGrid* grid, FVizId cell_id, FVizId out_point_ids[8], uint32_t* out_point_count);

FVIZ_API FVizAttributeSet* fviz_rectilinear_grid_point_data(FVizRectilinearGrid* grid);
FVIZ_API FVizAttributeSet* fviz_rectilinear_grid_cell_data(FVizRectilinearGrid* grid);
FVIZ_API FVizAttributeSet* fviz_rectilinear_grid_field_data(FVizRectilinearGrid* grid);
FVIZ_API const FVizAttributeSet* fviz_rectilinear_grid_const_point_data(const FVizRectilinearGrid* grid);
FVIZ_API const FVizAttributeSet* fviz_rectilinear_grid_const_cell_data(const FVizRectilinearGrid* grid);
FVIZ_API const FVizAttributeSet* fviz_rectilinear_grid_const_field_data(const FVizRectilinearGrid* grid);
FVIZ_API FVizResult fviz_rectilinear_grid_validate(const FVizRectilinearGrid* grid);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_RECTILINEAR_GRID_H */
