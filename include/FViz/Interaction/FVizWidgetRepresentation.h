#ifndef FVIZ_INTERACTION_WIDGET_REPRESENTATION_H
#define FVIZ_INTERACTION_WIDGET_REPRESENTATION_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Rendering/FVizActor.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizLabelSet.h>
#include <FViz/Rendering/FVizTextActor.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizWidgetRepresentation FVizWidgetRepresentation;
#define FVIZ_TYPE_WIDGET_REPRESENTATION UINT64_C(0x6C7D90B12EA8435F)

FVIZ_API FVizResult fviz_widget_representation_create(
    FVizRenderer* renderer,
    FVizWidgetRepresentation** out_representation);
FVIZ_API FVizRenderer* fviz_widget_representation_renderer(FVizWidgetRepresentation* representation);
FVIZ_API FVizResult fviz_widget_representation_add_actor(
    FVizWidgetRepresentation* representation,
    FVizActor* actor);
FVIZ_API FVizResult fviz_widget_representation_remove_actor(
    FVizWidgetRepresentation* representation,
    FVizActor* actor);
FVIZ_API void fviz_widget_representation_remove_all_actors(
    FVizWidgetRepresentation* representation);
FVIZ_API FVizSize fviz_widget_representation_actor_count(
    const FVizWidgetRepresentation* representation);
FVIZ_API FVizActor* fviz_widget_representation_actor_at(
    FVizWidgetRepresentation* representation,
    FVizSize index);
FVIZ_API FVizResult fviz_widget_representation_set_actor_visible(
    FVizWidgetRepresentation* representation, FVizActor* actor, FVizBool visible);
FVIZ_API FVizResult fviz_widget_representation_add_text_actor_2d(
    FVizWidgetRepresentation* representation,
    FVizTextActor2D* actor);
FVIZ_API FVizResult fviz_widget_representation_remove_text_actor_2d(
    FVizWidgetRepresentation* representation,
    FVizTextActor2D* actor);
FVIZ_API FVizSize fviz_widget_representation_text_actor_2d_count(
    const FVizWidgetRepresentation* representation);
FVIZ_API FVizTextActor2D* fviz_widget_representation_text_actor_2d_at(
    FVizWidgetRepresentation* representation,
    FVizSize index);
FVIZ_API FVizResult fviz_widget_representation_set_text_actor_2d_visible(
    FVizWidgetRepresentation* representation, FVizTextActor2D* actor, FVizBool visible);
FVIZ_API FVizResult fviz_widget_representation_add_billboard_text_actor_3d(
    FVizWidgetRepresentation* representation,
    FVizBillboardTextActor3D* actor);
FVIZ_API FVizResult fviz_widget_representation_remove_billboard_text_actor_3d(
    FVizWidgetRepresentation* representation,
    FVizBillboardTextActor3D* actor);
FVIZ_API FVizSize fviz_widget_representation_billboard_text_actor_3d_count(
    const FVizWidgetRepresentation* representation);
FVIZ_API FVizBillboardTextActor3D* fviz_widget_representation_billboard_text_actor_3d_at(
    FVizWidgetRepresentation* representation,
    FVizSize index);
FVIZ_API FVizResult fviz_widget_representation_set_billboard_text_actor_3d_visible(
    FVizWidgetRepresentation* representation, FVizBillboardTextActor3D* actor, FVizBool visible);
FVIZ_API FVizResult fviz_widget_representation_add_label_set_3d(
    FVizWidgetRepresentation* representation,
    FVizLabelSet3D* label_set);
FVIZ_API FVizResult fviz_widget_representation_remove_label_set_3d(
    FVizWidgetRepresentation* representation,
    FVizLabelSet3D* label_set);
FVIZ_API FVizSize fviz_widget_representation_label_set_3d_count(
    const FVizWidgetRepresentation* representation);
FVIZ_API FVizLabelSet3D* fviz_widget_representation_label_set_3d_at(
    FVizWidgetRepresentation* representation,
    FVizSize index);
FVIZ_API FVizResult fviz_widget_representation_set_label_set_3d_visible(
    FVizWidgetRepresentation* representation, FVizLabelSet3D* label_set, FVizBool visible);
FVIZ_API void fviz_widget_representation_remove_all_annotations(
    FVizWidgetRepresentation* representation);
FVIZ_API void fviz_widget_representation_set_visible(
    FVizWidgetRepresentation* representation,
    FVizBool visible);
FVIZ_API FVizBool fviz_widget_representation_visible(
    const FVizWidgetRepresentation* representation);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_WIDGET_REPRESENTATION_H */
