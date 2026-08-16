#include <math.h>

#include <FViz/Math/FVizVec3.h>

FVizVec3 fviz_vec3(float x, float y, float z)
{
    FVizVec3 result = {x, y, z};
    return result;
}

FVizVec3 fviz_vec3_add(FVizVec3 a, FVizVec3 b)
{
    return fviz_vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

FVizVec3 fviz_vec3_sub(FVizVec3 a, FVizVec3 b)
{
    return fviz_vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

FVizVec3 fviz_vec3_scale(FVizVec3 v, float scalar)
{
    return fviz_vec3(v.x * scalar, v.y * scalar, v.z * scalar);
}

float fviz_vec3_dot(FVizVec3 a, FVizVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

FVizVec3 fviz_vec3_cross(FVizVec3 a, FVizVec3 b)
{
    return fviz_vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

float fviz_vec3_length(FVizVec3 v)
{
    return sqrtf(fviz_vec3_dot(v, v));
}

FVizVec3 fviz_vec3_normalize(FVizVec3 v)
{
    const float length = fviz_vec3_length(v);
    if (length <= 1.0e-20f)
    {
        return fviz_vec3(0.0f, 0.0f, 0.0f);
    }
    return fviz_vec3_scale(v, 1.0f / length);
}
