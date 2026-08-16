#include <math.h>
#include <string.h>

#include <FViz/Math/FVizMat4.h>

FVizMat4 fviz_mat4_identity(void)
{
    FVizMat4 result;
    (void)memset(&result, 0, sizeof(result));
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

FVizMat4 fviz_mat4_perspective(float fov_y_radians, float aspect, float near_plane, float far_plane)
{
    FVizMat4 result;
    const float f = 1.0f / tanf(fov_y_radians * 0.5f);
    (void)memset(&result, 0, sizeof(result));
    if (aspect <= 0.0f) aspect = 1.0f;
    if (near_plane <= 0.0f) near_plane = 0.01f;
    if (far_plane <= near_plane) far_plane = near_plane + 1000.0f;
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

FVizMat4 fviz_mat4_orthographic(float left, float right, float bottom, float top, float near_plane, float far_plane)
{
    FVizMat4 result = fviz_mat4_identity();
    const float width = right - left;
    const float height = top - bottom;
    const float depth = far_plane - near_plane;
    if (width == 0.0f || height == 0.0f || depth == 0.0f)
    {
        return result;
    }
    result.m[0] = 2.0f / width;
    result.m[5] = 2.0f / height;
    result.m[10] = -2.0f / depth;
    result.m[12] = -(right + left) / width;
    result.m[13] = -(top + bottom) / height;
    result.m[14] = -(far_plane + near_plane) / depth;
    return result;
}

FVizMat4 fviz_mat4_look_at(FVizVec3 eye, FVizVec3 target, FVizVec3 up)
{
    const FVizVec3 forward = fviz_vec3_normalize(fviz_vec3_sub(target, eye));
    const FVizVec3 side = fviz_vec3_normalize(fviz_vec3_cross(forward, up));
    const FVizVec3 corrected_up = fviz_vec3_cross(side, forward);
    FVizMat4 result = fviz_mat4_identity();

    result.m[0] = side.x;
    result.m[4] = side.y;
    result.m[8] = side.z;
    result.m[1] = corrected_up.x;
    result.m[5] = corrected_up.y;
    result.m[9] = corrected_up.z;
    result.m[2] = -forward.x;
    result.m[6] = -forward.y;
    result.m[10] = -forward.z;
    result.m[12] = -fviz_vec3_dot(side, eye);
    result.m[13] = -fviz_vec3_dot(corrected_up, eye);
    result.m[14] = fviz_vec3_dot(forward, eye);
    return result;
}

FVizMat4 fviz_mat4_multiply(FVizMat4 a, FVizMat4 b)
{
    FVizMat4 result;
    int column;
    int row;
    for (column = 0; column < 4; ++column)
    {
        for (row = 0; row < 4; ++row)
        {
            result.m[column * 4 + row] =
                a.m[0 * 4 + row] * b.m[column * 4 + 0] + a.m[1 * 4 + row] * b.m[column * 4 + 1] +
                a.m[2 * 4 + row] * b.m[column * 4 + 2] + a.m[3 * 4 + row] * b.m[column * 4 + 3];
        }
    }
    return result;
}
