#include <string.h>

#include <FViz/Math/FVizMat3.h>

FVizMat3 fviz_mat3_identity(void)
{
    FVizMat3 result;
    (void)memset(&result, 0, sizeof(result));
    result.m[0] = 1.0f;
    result.m[4] = 1.0f;
    result.m[8] = 1.0f;
    return result;
}

FVizMat3 fviz_mat3_multiply(FVizMat3 a, FVizMat3 b)
{
    FVizMat3 result;
    int column;
    int row;
    for (column = 0; column < 3; ++column)
    {
        for (row = 0; row < 3; ++row)
        {
            result.m[column * 3 + row] =
                a.m[0 * 3 + row] * b.m[column * 3 + 0] +
                a.m[1 * 3 + row] * b.m[column * 3 + 1] +
                a.m[2 * 3 + row] * b.m[column * 3 + 2];
        }
    }
    return result;
}

FVizMat3 fviz_mat3_transpose(FVizMat3 m)
{
    FVizMat3 result;
    result.m[0] = m.m[0];
    result.m[1] = m.m[3];
    result.m[2] = m.m[6];
    result.m[3] = m.m[1];
    result.m[4] = m.m[4];
    result.m[5] = m.m[7];
    result.m[6] = m.m[2];
    result.m[7] = m.m[5];
    result.m[8] = m.m[8];
    return result;
}

FVizMat3 fviz_mat3_from_quaternion(FVizQuat q)
{
    FVizMat3 result;
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;

    result.m[0] = 1.0f - 2.0f * (yy + zz);
    result.m[1] = 2.0f * (xy + wz);
    result.m[2] = 2.0f * (xz - wy);
    result.m[3] = 2.0f * (xy - wz);
    result.m[4] = 1.0f - 2.0f * (xx + zz);
    result.m[5] = 2.0f * (yz + wx);
    result.m[6] = 2.0f * (xz + wy);
    result.m[7] = 2.0f * (yz - wx);
    result.m[8] = 1.0f - 2.0f * (xx + yy);
    return result;
}

FVizVec3 fviz_mat3_transform_vec3(FVizMat3 m, FVizVec3 v)
{
    FVizVec3 result;
    result.x = m.m[0] * v.x + m.m[3] * v.y + m.m[6] * v.z;
    result.y = m.m[1] * v.x + m.m[4] * v.y + m.m[7] * v.z;
    result.z = m.m[2] * v.x + m.m[5] * v.y + m.m[8] * v.z;
    return result;
}

FVizMat3 fviz_mat3_inverse(FVizMat3 m)
{
    const float a = m.m[0];
    const float b = m.m[3];
    const float c = m.m[6];
    const float d = m.m[1];
    const float e = m.m[4];
    const float f = m.m[7];
    const float g = m.m[2];
    const float h = m.m[5];
    const float i = m.m[8];
    const float determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    FVizMat3 result;
    float inv_determinant;

    if (determinant == 0.0f)
    {
        return fviz_mat3_identity();
    }
    inv_determinant = 1.0f / determinant;
    result.m[0] = (e * i - f * h) * inv_determinant;
    result.m[1] = (f * g - d * i) * inv_determinant;
    result.m[2] = (d * h - e * g) * inv_determinant;
    result.m[3] = (c * h - b * i) * inv_determinant;
    result.m[4] = (a * i - c * g) * inv_determinant;
    result.m[5] = (b * g - a * h) * inv_determinant;
    result.m[6] = (b * f - c * e) * inv_determinant;
    result.m[7] = (c * d - a * f) * inv_determinant;
    result.m[8] = (a * e - b * d) * inv_determinant;
    return result;
}
