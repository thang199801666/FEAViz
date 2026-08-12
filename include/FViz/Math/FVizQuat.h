#ifndef FVIZ_MATH_QUAT_H
#define FVIZ_MATH_QUAT_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizQuat
{
    float x;
    float y;
    float z;
    float w;
} FVizQuat;

FVIZ_API FVizQuat fviz_quat_identity(void);
FVIZ_API FVizQuat fviz_quat_from_axis_angle(FVizVec3 axis, float angle_radians);
FVIZ_API FVizQuat fviz_quat_multiply(FVizQuat a, FVizQuat b);
FVIZ_API FVizQuat fviz_quat_normalize(FVizQuat q);
FVIZ_API FVizVec3 fviz_quat_rotate_vec3(FVizQuat q, FVizVec3 v);
FVIZ_API float fviz_quat_dot(FVizQuat a, FVizQuat b);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_QUAT_H */
