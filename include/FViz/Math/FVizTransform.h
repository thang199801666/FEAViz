#ifndef FVIZ_MATH_TRANSFORM_H
#define FVIZ_MATH_TRANSFORM_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Math/FVizQuat.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizTransform FVizTransform;
#define FVIZ_TYPE_TRANSFORM UINT64_C(0x3E907A51C4D268BF)

FVIZ_CORE_API FVizResult fviz_transform_create(FVizTransform** out_transform);
FVIZ_CORE_API void fviz_transform_identity(FVizTransform* transform);
FVIZ_CORE_API void fviz_transform_set_matrix(FVizTransform* transform, FVizMat4 matrix);
FVIZ_CORE_API FVizMat4 fviz_transform_matrix(const FVizTransform* transform);
FVIZ_CORE_API void fviz_transform_concatenate(FVizTransform* transform, FVizMat4 matrix);
FVIZ_CORE_API void fviz_transform_translate(FVizTransform* transform, FVizVec3 translation);
FVIZ_CORE_API void fviz_transform_scale(FVizTransform* transform, FVizVec3 scale);
FVIZ_CORE_API void fviz_transform_rotate(FVizTransform* transform, FVizQuat rotation);
FVIZ_CORE_API FVizVec3 fviz_transform_point(const FVizTransform* transform, FVizVec3 point);
FVIZ_CORE_API FVizVec3 fviz_transform_vector(const FVizTransform* transform, FVizVec3 vector);

FVIZ_EXTERN_C_END

#endif /* FVIZ_MATH_TRANSFORM_H */
