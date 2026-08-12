#include <FViz/Math/FVizVec4.h>

FVizVec4 fviz_vec4(float x, float y, float z, float w)
{
    FVizVec4 result = {x, y, z, w};
    return result;
}

FVizVec4 fviz_vec4_add(FVizVec4 a, FVizVec4 b)
{
    return fviz_vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

FVizVec4 fviz_vec4_sub(FVizVec4 a, FVizVec4 b)
{
    return fviz_vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

FVizVec4 fviz_vec4_scale(FVizVec4 v, float scalar)
{
    return fviz_vec4(v.x * scalar, v.y * scalar, v.z * scalar, v.w * scalar);
}

float fviz_vec4_dot(FVizVec4 a, FVizVec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
