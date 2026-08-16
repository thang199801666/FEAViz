#ifndef FVIZ_DATA_STRUCTURED_GRID_H
#define FVIZ_DATA_STRUCTURED_GRID_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizCellArray.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizStructuredGrid FVizStructuredGrid;
#define FVIZ_TYPE_STRUCTURED_GRID UINT64_C(0xE62B4A1D93C7580F)

FVIZ_DATA_API FVizResult fviz_structured_grid_create(FVizStructuredGrid** out_grid);
FVIZ_DATA_API void fviz_structured_grid_clear(FVizStructuredGrid* grid);

/* Inclusive structured extent [xmin,xmax,ymin,ymax,zmin,zmax]. Empty extents
 * use max < min on at least one axis. Points are stored explicitly while cell
 * connectivity is implicit from the extent. */
FVIZ_DATA_API FVizResult fviz_structured_grid_set_extent(FVizStructuredGrid* grid, const int64_t extent[6]);
FVIZ_DATA_API void fviz_structured_grid_extent(const FVizStructuredGrid* grid, int64_t out_extent[6]);
FVIZ_DATA_API void fviz_structured_grid_dimensions(const FVizStructuredGrid* grid, FVizSize out_dimensions[3]);
FVIZ_DATA_API uint32_t fviz_structured_grid_dimension(const FVizStructuredGrid* grid);
FVIZ_DATA_API FVizSize fviz_structured_grid_point_count(const FVizStructuredGrid* grid);
FVIZ_DATA_API FVizSize fviz_structured_grid_cell_count(const FVizStructuredGrid* grid);
FVIZ_DATA_API FVizCellType fviz_structured_grid_cell_type(const FVizStructuredGrid* grid);

FVIZ_DATA_API FVizResult fviz_structured_grid_set_points(FVizStructuredGrid* grid, const FVizVec3* points,
                                                    FVizSize point_count);
FVIZ_DATA_API FVizResult fviz_structured_grid_set_point(FVizStructuredGrid* grid, FVizSize point_id, FVizVec3 point);
FVIZ_DATA_API const FVizVec3* fviz_structured_grid_points(const FVizStructuredGrid* grid);
FVIZ_DATA_API FVizResult fviz_structured_grid_point(const FVizStructuredGrid* grid, FVizId point_id, FVizVec3* out_point);
FVIZ_DATA_API FVizBounds fviz_structured_grid_bounds(const FVizStructuredGrid* grid);

FVIZ_DATA_API FVizResult fviz_structured_grid_point_id(const FVizStructuredGrid* grid, int64_t i, int64_t j, int64_t k,
                                                  FVizId* out_point_id);
FVIZ_DATA_API FVizResult fviz_structured_grid_point_ijk(const FVizStructuredGrid* grid, FVizId point_id, int64_t out_ijk[3]);
FVIZ_DATA_API FVizResult fviz_structured_grid_cell_id(const FVizStructuredGrid* grid, int64_t i, int64_t j, int64_t k,
                                                 FVizId* out_cell_id);
FVIZ_DATA_API FVizResult fviz_structured_grid_cell_ijk(const FVizStructuredGrid* grid, FVizId cell_id, int64_t out_ijk[3]);
FVIZ_DATA_API FVizResult fviz_structured_grid_cell_point_ids(const FVizStructuredGrid* grid, FVizId cell_id,
                                                        FVizId out_point_ids[8], uint32_t* out_point_count);

FVIZ_DATA_API FVizAttributeSet* fviz_structured_grid_point_data(FVizStructuredGrid* grid);
FVIZ_DATA_API FVizAttributeSet* fviz_structured_grid_cell_data(FVizStructuredGrid* grid);
FVIZ_DATA_API FVizAttributeSet* fviz_structured_grid_field_data(FVizStructuredGrid* grid);
FVIZ_DATA_API const FVizAttributeSet* fviz_structured_grid_const_point_data(const FVizStructuredGrid* grid);
FVIZ_DATA_API const FVizAttributeSet* fviz_structured_grid_const_cell_data(const FVizStructuredGrid* grid);
FVIZ_DATA_API const FVizAttributeSet* fviz_structured_grid_const_field_data(const FVizStructuredGrid* grid);
FVIZ_DATA_API FVizResult fviz_structured_grid_validate(const FVizStructuredGrid* grid);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_STRUCTURED_GRID_H */
