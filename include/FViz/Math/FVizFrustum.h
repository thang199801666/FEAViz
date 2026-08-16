#ifndef FVIZ_MATH_FRUSTUM_H
#define FVIZ_MATH_FRUSTUM_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Math/FVizPlane.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizFrustumPlane
{
    FVIZ_FRUSTUM_LEFT = 0,
    FVIZ_FRUSTUM_RIGHT = 1,
    FVIZ_FRUSTUM_BOTTOM = 2,
    FVIZ_FRUSTUM_TOP = 3,
    FVIZ_FRUSTUM_NEAR = 4,
    FVIZ_FRUSTUM_FAR = 5,
    FVIZ_FRUSTUM_PLANE_COUNT = 6
} FVizFrustumPlane;

typedef struct FVizFrustum
{
    FVizPlane planes[FVIZ_FRUSTUM_PLANE_COUNT];
    FVizBool valid;
} FVizFrustum;

FVIZ_CORE_API FVizFrustum fviz_frustum_from_view_projection(FVizMat4 view_projection);
FVIZ_CORE_API FVizBool fviz_frustum_contains_point(const FVizFrustum* frustum, FVizVec3 point);
FVIZ_CORE_API FVizBool fviz_frustum_intersects_bounds(const FVizFrustum* frustum, FVizBounds bounds);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_FRUSTUM_H */
