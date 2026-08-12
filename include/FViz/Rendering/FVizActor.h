#ifndef FVIZ_RENDERING_ACTOR_H
#define FVIZ_RENDERING_ACTOR_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizActor FVizActor;
#define FVIZ_TYPE_ACTOR UINT64_C(0x0A99A2B9935E483C)

FVIZ_API FVizResult fviz_actor_create(FVizActor** out_actor);
FVIZ_API FVizResult fviz_actor_set_poly_data(FVizActor* actor, FVizPolyData* poly_data);
FVIZ_API FVizPolyData* fviz_actor_poly_data(FVizActor* actor);
FVIZ_API const FVizPolyData* fviz_actor_const_poly_data(const FVizActor* actor);
FVIZ_API void fviz_actor_set_color(FVizActor* actor, float red, float green, float blue);
FVIZ_API void fviz_actor_get_color(const FVizActor* actor, float* red, float* green, float* blue);
FVIZ_API void fviz_actor_set_visible(FVizActor* actor, FVizBool visible);
FVIZ_API FVizBool fviz_actor_is_visible(const FVizActor* actor);
FVIZ_API void fviz_actor_set_wireframe(FVizActor* actor, FVizBool enabled);
FVIZ_API FVizBool fviz_actor_wireframe(const FVizActor* actor);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_ACTOR_H */
