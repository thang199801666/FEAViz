#ifndef FVIZ_MATH_VEC2_H
#define FVIZ_MATH_VEC2_H

#include <FViz/Core/FVizApi.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizVec2
{
    float x;
    float y;
} FVizVec2;

FVIZ_CORE_API FVizVec2 fviz_vec2(float x, float y);
FVIZ_CORE_API FVizVec2 fviz_vec2_add(FVizVec2 a, FVizVec2 b);
FVIZ_CORE_API FVizVec2 fviz_vec2_sub(FVizVec2 a, FVizVec2 b);
FVIZ_CORE_API FVizVec2 fviz_vec2_scale(FVizVec2 v, float scalar);
FVIZ_CORE_API float fviz_vec2_dot(FVizVec2 a, FVizVec2 b);
FVIZ_CORE_API float fviz_vec2_length(FVizVec2 v);
FVIZ_CORE_API FVizVec2 fviz_vec2_normalize(FVizVec2 v);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_VEC2_H */
