#include <FViz/Core/FVizError.h>
#include <FViz/Math/FVizMat3.h>
#include <FViz/Math/FVizTransform.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Math/FVizTransformPrivate.h>

static const FVizObjectClass g_fviz_transform_class = {
    FVIZ_TYPE_TRANSFORM,
    "FVizTransform",
    &g_fviz_object_class,
    NULL,
    NULL
};

FVizResult fviz_transform_create(FVizTransform** out_transform)
{
    FVizTransform* transform;
    if (out_transform == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_transform must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_transform = NULL;
    transform = (FVizTransform*)fviz_internal_object_allocate(
        sizeof(FVizTransform), &g_fviz_transform_class, NULL);
    if (transform == NULL) return fviz_last_error_code();
    transform->matrix = fviz_mat4_identity();
    *out_transform = transform;
    return FVIZ_OK;
}

void fviz_transform_identity(FVizTransform* transform)
{
    if (transform == NULL) return;
    transform->matrix = fviz_mat4_identity();
    fviz_object_modified((FVizObject*)transform);
}

void fviz_transform_set_matrix(FVizTransform* transform, FVizMat4 matrix)
{
    if (transform == NULL) return;
    transform->matrix = matrix;
    fviz_object_modified((FVizObject*)transform);
}

FVizMat4 fviz_transform_matrix(const FVizTransform* transform)
{
    return transform != NULL ? transform->matrix : fviz_mat4_identity();
}

void fviz_transform_concatenate(FVizTransform* transform, FVizMat4 matrix)
{
    if (transform == NULL) return;
    transform->matrix = fviz_mat4_multiply(transform->matrix, matrix);
    fviz_object_modified((FVizObject*)transform);
}

void fviz_transform_translate(FVizTransform* transform, FVizVec3 translation)
{
    FVizMat4 matrix = fviz_mat4_identity();
    matrix.m[12] = translation.x;
    matrix.m[13] = translation.y;
    matrix.m[14] = translation.z;
    fviz_transform_concatenate(transform, matrix);
}

void fviz_transform_scale(FVizTransform* transform, FVizVec3 scale)
{
    FVizMat4 matrix = fviz_mat4_identity();
    matrix.m[0] = scale.x;
    matrix.m[5] = scale.y;
    matrix.m[10] = scale.z;
    fviz_transform_concatenate(transform, matrix);
}

void fviz_transform_rotate(FVizTransform* transform, FVizQuat rotation)
{
    const FVizMat3 source = fviz_mat3_from_quaternion(fviz_quat_normalize(rotation));
    FVizMat4 matrix = fviz_mat4_identity();
    matrix.m[0] = source.m[0];
    matrix.m[1] = source.m[1];
    matrix.m[2] = source.m[2];
    matrix.m[4] = source.m[3];
    matrix.m[5] = source.m[4];
    matrix.m[6] = source.m[5];
    matrix.m[8] = source.m[6];
    matrix.m[9] = source.m[7];
    matrix.m[10] = source.m[8];
    fviz_transform_concatenate(transform, matrix);
}

static FVizVec3 fviz_transform_apply(
    const FVizTransform* transform,
    FVizVec3 value,
    float homogeneous)
{
    const FVizMat4 matrix = fviz_transform_matrix(transform);
    FVizVec3 result;
    result.x = matrix.m[0] * value.x + matrix.m[4] * value.y +
        matrix.m[8] * value.z + matrix.m[12] * homogeneous;
    result.y = matrix.m[1] * value.x + matrix.m[5] * value.y +
        matrix.m[9] * value.z + matrix.m[13] * homogeneous;
    result.z = matrix.m[2] * value.x + matrix.m[6] * value.y +
        matrix.m[10] * value.z + matrix.m[14] * homogeneous;
    return result;
}

FVizVec3 fviz_transform_point(const FVizTransform* transform, FVizVec3 point)
{
    return fviz_transform_apply(transform, point, 1.0f);
}

FVizVec3 fviz_transform_vector(const FVizTransform* transform, FVizVec3 vector)
{
    return fviz_transform_apply(transform, vector, 0.0f);
}
