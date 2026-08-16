#ifndef FVIZ_MATH_MAT3_H
#define FVIZ_MATH_MAT3_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Math/FVizQuat.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizMat3
{
    float m[9];
} FVizMat3;

FVIZ_CORE_API FVizMat3 fviz_mat3_identity(void);
FVIZ_CORE_API FVizMat3 fviz_mat3_multiply(FVizMat3 a, FVizMat3 b);
FVIZ_CORE_API FVizMat3 fviz_mat3_transpose(FVizMat3 m);
FVIZ_CORE_API FVizMat3 fviz_mat3_from_quaternion(FVizQuat q);
FVIZ_CORE_API FVizVec3 fviz_mat3_transform_vec3(FVizMat3 m, FVizVec3 v);
FVIZ_CORE_API FVizMat3 fviz_mat3_inverse(FVizMat3 m);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_MAT3_H */
