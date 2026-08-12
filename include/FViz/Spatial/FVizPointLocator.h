#ifndef FVIZ_SPATIAL_POINT_LOCATOR_H
#define FVIZ_SPATIAL_POINT_LOCATOR_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>
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

FVIZ_API FVizResult fviz_point_locator_create(FVizPointLocator** out_locator);
FVIZ_API FVizResult fviz_point_locator_set_grid(FVizPointLocator* locator, const FVizUnstructuredGrid* grid);
FVIZ_API const FVizUnstructuredGrid* fviz_point_locator_const_grid(const FVizPointLocator* locator);
FVIZ_API FVizBool fviz_point_locator_locate_point(
    const FVizPointLocator* locator,
    FVizVec3 point,
    FVizLocatedCell* out_result);
FVIZ_API FVizResult fviz_point_locator_interpolate_scalar(
    const FVizPointLocator* locator,
    const char* scalar_name,
    FVizVec3 point,
    float* out_value);
FVIZ_API FVizVec3 fviz_point_locator_interpolate_vector(
    const FVizPointLocator* locator,
    const char* vector_name,
    FVizVec3 point);

FVIZ_EXTERN_C_END

#endif /* FVIZ_SPATIAL_POINT_LOCATOR_H */
