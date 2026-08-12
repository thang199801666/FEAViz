#ifndef FVIZ_MATH_VEC3_H
#define FVIZ_MATH_VEC3_H

#include <FViz/Core/FVizApi.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizVec3
{
    float x;
    float y;
    float z;
} FVizVec3;

FVIZ_API FVizVec3 fviz_vec3(float x, float y, float z);
FVIZ_API FVizVec3 fviz_vec3_add(FVizVec3 a, FVizVec3 b);
FVIZ_API FVizVec3 fviz_vec3_sub(FVizVec3 a, FVizVec3 b);
FVIZ_API FVizVec3 fviz_vec3_scale(FVizVec3 v, float scalar);
FVIZ_API float fviz_vec3_dot(FVizVec3 a, FVizVec3 b);
FVIZ_API FVizVec3 fviz_vec3_cross(FVizVec3 a, FVizVec3 b);
FVIZ_API float fviz_vec3_length(FVizVec3 v);
FVIZ_API FVizVec3 fviz_vec3_normalize(FVizVec3 v);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_VEC3_H */
