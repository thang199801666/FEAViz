#ifndef FVIZ_MATH_BOUNDS_H
#define FVIZ_MATH_BOUNDS_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizBounds
{
    FVizVec3 min;
    FVizVec3 max;
    FVizBool valid;
} FVizBounds;

FVIZ_API FVizBounds fviz_bounds_empty(void);
FVIZ_API void fviz_bounds_reset(FVizBounds* bounds);
FVIZ_API void fviz_bounds_include_point(FVizBounds* bounds, FVizVec3 point);
FVIZ_API void fviz_bounds_include_bounds(FVizBounds* bounds, const FVizBounds* other);
FVIZ_API FVizVec3 fviz_bounds_center(const FVizBounds* bounds);
FVIZ_API FVizVec3 fviz_bounds_size(const FVizBounds* bounds);
FVIZ_API float fviz_bounds_radius(const FVizBounds* bounds);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_BOUNDS_H */
