#ifndef FVIZ_RENDERING_ACTOR_H
#define FVIZ_RENDERING_ACTOR_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Math/FVizQuat.h>
#include <FViz/Math/FVizTransform.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Rendering/FVizMapper.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizActor FVizActor;
#define FVIZ_TYPE_ACTOR UINT64_C(0x0A99A2B9935E483C)

FVIZ_API FVizResult fviz_actor_create(FVizActor** out_actor);
FVIZ_API FVizResult fviz_actor_set_poly_data(FVizActor* actor, FVizPolyData* poly_data);
FVIZ_API FVizPolyData* fviz_actor_poly_data(FVizActor* actor);
FVIZ_API const FVizPolyData* fviz_actor_const_poly_data(const FVizActor* actor);
FVIZ_API FVizResult fviz_actor_set_mapper(FVizActor* actor, FVizMapper* mapper);
FVIZ_API FVizMapper* fviz_actor_mapper(FVizActor* actor);
FVIZ_API void fviz_actor_set_color(FVizActor* actor, float red, float green, float blue);
FVIZ_API void fviz_actor_get_color(const FVizActor* actor, float* red, float* green, float* blue);
FVIZ_API void fviz_actor_set_visible(FVizActor* actor, FVizBool visible);
FVIZ_API FVizBool fviz_actor_is_visible(const FVizActor* actor);
FVIZ_API void fviz_actor_set_wireframe(FVizActor* actor, FVizBool enabled);
FVIZ_API FVizBool fviz_actor_wireframe(const FVizActor* actor);
FVIZ_API void fviz_actor_set_position(FVizActor* actor, FVizVec3 position);
FVIZ_API FVizVec3 fviz_actor_position(const FVizActor* actor);
FVIZ_API void fviz_actor_set_orientation(FVizActor* actor, FVizQuat orientation);
FVIZ_API FVizQuat fviz_actor_orientation(const FVizActor* actor);
FVIZ_API void fviz_actor_set_scale(FVizActor* actor, FVizVec3 scale);
FVIZ_API FVizVec3 fviz_actor_scale(const FVizActor* actor);
FVIZ_API FVizMat4 fviz_actor_transform_matrix(const FVizActor* actor);
FVIZ_API FVizResult fviz_actor_set_user_transform(FVizActor* actor, FVizTransform* transform);
FVIZ_API FVizTransform* fviz_actor_user_transform(FVizActor* actor);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_ACTOR_H */
