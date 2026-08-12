#include <math.h>

#include <FViz/Math/FVizQuat.h>

FVizQuat fviz_quat_identity(void)
{
    FVizQuat result = {0.0f, 0.0f, 0.0f, 1.0f};
    return result;
}

FVizQuat fviz_quat_from_axis_angle(FVizVec3 axis, float angle_radians)
{
    const float half_angle = angle_radians * 0.5f;
    const float sine = sinf(half_angle);
    const FVizVec3 n = fviz_vec3_normalize(axis);
    FVizQuat result;
    result.x = n.x * sine;
    result.y = n.y * sine;
    result.z = n.z * sine;
    result.w = cosf(half_angle);
    return result;
}

FVizQuat fviz_quat_multiply(FVizQuat a, FVizQuat b)
{
    FVizQuat result;
    result.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    result.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    result.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    result.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    return result;
}

FVizQuat fviz_quat_normalize(FVizQuat q)
{
    const float length = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length <= 1.0e-20f)
    {
        return fviz_quat_identity();
    }
    q.x /= length;
    q.y /= length;
    q.z /= length;
    q.w /= length;
    return q;
}

float fviz_quat_dot(FVizQuat a, FVizQuat b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

FVizVec3 fviz_quat_rotate_vec3(FVizQuat q, FVizVec3 v)
{
    const FVizQuat p = {v.x, v.y, v.z, 0.0f};
    const FVizQuat conjugate = {-q.x, -q.y, -q.z, q.w};
    const FVizQuat rotated = fviz_quat_multiply(fviz_quat_multiply(q, p), conjugate);
    return fviz_vec3(rotated.x, rotated.y, rotated.z);
}
