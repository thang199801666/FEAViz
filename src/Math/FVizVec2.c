#include <math.h>

#include <FViz/Math/FVizVec2.h>

FVizVec2 fviz_vec2(float x, float y)
{
    FVizVec2 result = {x, y};
    return result;
}

FVizVec2 fviz_vec2_add(FVizVec2 a, FVizVec2 b)
{
    return fviz_vec2(a.x + b.x, a.y + b.y);
}

FVizVec2 fviz_vec2_sub(FVizVec2 a, FVizVec2 b)
{
    return fviz_vec2(a.x - b.x, a.y - b.y);
}

FVizVec2 fviz_vec2_scale(FVizVec2 v, float scalar)
{
    return fviz_vec2(v.x * scalar, v.y * scalar);
}

float fviz_vec2_dot(FVizVec2 a, FVizVec2 b)
{
    return a.x * b.x + a.y * b.y;
}

float fviz_vec2_length(FVizVec2 v)
{
    return sqrtf(fviz_vec2_dot(v, v));
}

FVizVec2 fviz_vec2_normalize(FVizVec2 v)
{
    const float length = fviz_vec2_length(v);
    if (length <= 1.0e-20f)
    {
        return fviz_vec2(0.0f, 0.0f);
    }
    return fviz_vec2_scale(v, 1.0f / length);
}
