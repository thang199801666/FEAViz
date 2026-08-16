#ifndef FVIZ_RENDERING_SCENE_H
#define FVIZ_RENDERING_SCENE_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Rendering/FVizActor.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizScene FVizScene;
#define FVIZ_TYPE_SCENE UINT64_C(0x9810A0A846D24035)

FVIZ_API FVizResult fviz_scene_create(FVizScene** out_scene);
FVIZ_API FVizResult fviz_scene_reserve(FVizScene* scene, FVizSize actor_capacity);
FVIZ_API FVizResult fviz_scene_add_actor(FVizScene* scene, FVizActor* actor);
/* Bulk insertion reserves storage up front and emits one scene ModifiedEvent for the batch. */
FVIZ_API FVizResult fviz_scene_add_actors(FVizScene* scene, FVizActor* const* actors, FVizSize actor_count);
FVIZ_API FVizResult fviz_scene_remove_actor(FVizScene* scene, FVizActor* actor);
FVIZ_API void fviz_scene_clear(FVizScene* scene);
FVIZ_API FVizSize fviz_scene_actor_count(const FVizScene* scene);
FVIZ_API FVizActor* fviz_scene_actor(FVizScene* scene, FVizSize index);
FVIZ_API const FVizActor* fviz_scene_const_actor(const FVizScene* scene, FVizSize index);
FVIZ_API FVizBounds fviz_scene_bounds(const FVizScene* scene);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_SCENE_H */
