#ifndef FVIZ_MATH_VEC4_H
#define FVIZ_MATH_VEC4_H

#include <FViz/Core/FVizApi.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizVec4
{
    float x;
    float y;
    float z;
    float w;
} FVizVec4;

FVIZ_API FVizVec4 fviz_vec4(float x, float y, float z, float w);
FVIZ_API FVizVec4 fviz_vec4_add(FVizVec4 a, FVizVec4 b);
FVIZ_API FVizVec4 fviz_vec4_sub(FVizVec4 a, FVizVec4 b);
FVIZ_API FVizVec4 fviz_vec4_scale(FVizVec4 v, float scalar);
FVIZ_API float fviz_vec4_dot(FVizVec4 a, FVizVec4 b);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_VEC4_H */
