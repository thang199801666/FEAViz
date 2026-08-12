#ifndef FVIZ_MATH_MAT4_H
#define FVIZ_MATH_MAT4_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizMat4
{
    float m[16];
} FVizMat4;

FVIZ_API FVizMat4 fviz_mat4_identity(void);
FVIZ_API FVizMat4 fviz_mat4_perspective(float fov_y_radians, float aspect, float near_plane, float far_plane);
FVIZ_API FVizMat4 fviz_mat4_orthographic(float left, float right, float bottom, float top, float near_plane, float far_plane);
FVIZ_API FVizMat4 fviz_mat4_look_at(FVizVec3 eye, FVizVec3 target, FVizVec3 up);
FVIZ_API FVizMat4 fviz_mat4_multiply(FVizMat4 a, FVizMat4 b);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_MAT4_H */
