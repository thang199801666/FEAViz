#ifndef FVIZ_SPATIAL_POINT_LOCATOR_H
#define FVIZ_SPATIAL_POINT_LOCATOR_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPointLocator FVizPointLocator;
#define FVIZ_TYPE_POINT_LOCATOR UINT64_C(0x6A1C8D3E5B7F90A2)

typedef struct FVizLocatedCell
{
    FVizVec3 point;
    FVizVec3 barycentric;
    FVizSize cell_index;
    FVizSize point_count;
} FVizLocatedCell;

FVIZ_DATA_API FVizResult fviz_point_locator_create(FVizPointLocator** out_locator);
FVIZ_DATA_API FVizResult fviz_point_locator_set_grid(FVizPointLocator* locator, const FVizUnstructuredGrid* grid);
/* Rebuild the cached AABB hierarchy after mutating the attached grid. */
FVIZ_DATA_API FVizResult fviz_point_locator_build(FVizPointLocator* locator);
/* Recompute cell/node bounds while preserving the cell partition.  This is the
 * fast path for deformed meshes whose cell count remains unchanged. */
FVIZ_DATA_API FVizResult fviz_point_locator_refit(FVizPointLocator* locator);
/* Update acceleration after input mutations: point-only deformation refits;
 * connectivity changes rebuild the hierarchy. */
FVIZ_DATA_API FVizResult fviz_point_locator_update(FVizPointLocator* locator);
FVIZ_DATA_API FVizBool fviz_point_locator_acceleration_valid(const FVizPointLocator* locator);
FVIZ_DATA_API FVizBool fviz_point_locator_refit_required(const FVizPointLocator* locator);
FVIZ_DATA_API FVizSize fviz_point_locator_indexed_cell_count(const FVizPointLocator* locator);
FVIZ_DATA_API const FVizUnstructuredGrid* fviz_point_locator_const_grid(const FVizPointLocator* locator);
FVIZ_DATA_API FVizBool fviz_point_locator_locate_point(const FVizPointLocator* locator, FVizVec3 point,
                                                  FVizLocatedCell* out_result);
FVIZ_DATA_API FVizResult fviz_point_locator_interpolate_scalar(const FVizPointLocator* locator, const char* scalar_name,
                                                          FVizVec3 point, float* out_value);
FVIZ_DATA_API FVizVec3 fviz_point_locator_interpolate_vector(const FVizPointLocator* locator, const char* vector_name,
                                                        FVizVec3 point);

FVIZ_EXTERN_C_END

#endif /* FVIZ_SPATIAL_POINT_LOCATOR_H */
