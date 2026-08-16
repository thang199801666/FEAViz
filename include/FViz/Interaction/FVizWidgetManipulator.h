#ifndef FVIZ_INTERACTION_WIDGET_MANIPULATOR_H
#define FVIZ_INTERACTION_WIDGET_MANIPULATOR_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Interaction/FVizEvent.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Rendering/FVizRenderer.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizWidgetManipulator FVizWidgetManipulator;
#define FVIZ_TYPE_WIDGET_MANIPULATOR UINT64_C(0xE4321D7690BC5AF8)

typedef enum FVizWidgetManipulatorMode
{
    FVIZ_WIDGET_MANIPULATOR_VIEW_PLANE = 0,
    FVIZ_WIDGET_MANIPULATOR_PLANE = 1,
    FVIZ_WIDGET_MANIPULATOR_AXIS = 2
} FVizWidgetManipulatorMode;

FVIZ_INTERACTION_API FVizResult fviz_widget_manipulator_create(FVizWidgetManipulator** out_manipulator);
FVIZ_INTERACTION_API void fviz_widget_manipulator_set_mode(FVizWidgetManipulator* manipulator, FVizWidgetManipulatorMode mode);
FVIZ_INTERACTION_API FVizWidgetManipulatorMode fviz_widget_manipulator_mode(const FVizWidgetManipulator* manipulator);
FVIZ_INTERACTION_API void fviz_widget_manipulator_set_origin(FVizWidgetManipulator* manipulator, FVizVec3 origin);
FVIZ_INTERACTION_API FVizVec3 fviz_widget_manipulator_origin(const FVizWidgetManipulator* manipulator);
FVIZ_INTERACTION_API FVizResult fviz_widget_manipulator_set_axis(FVizWidgetManipulator* manipulator, FVizVec3 axis);
FVIZ_INTERACTION_API FVizVec3 fviz_widget_manipulator_axis(const FVizWidgetManipulator* manipulator);
FVIZ_INTERACTION_API FVizResult fviz_widget_manipulator_set_plane_normal(FVizWidgetManipulator* manipulator, FVizVec3 normal);
FVIZ_INTERACTION_API FVizVec3 fviz_widget_manipulator_plane_normal(const FVizWidgetManipulator* manipulator);
FVIZ_INTERACTION_API FVizResult fviz_widget_manipulator_begin(FVizWidgetManipulator* manipulator, FVizRenderer* renderer,
                                                  const FVizInteractionEvent* event, FVizVec3 reference_world);
FVIZ_INTERACTION_API FVizResult fviz_widget_manipulator_update(FVizWidgetManipulator* manipulator, FVizRenderer* renderer,
                                                   const FVizInteractionEvent* event, FVizVec3* out_world,
                                                   FVizVec3* out_delta);
FVIZ_INTERACTION_API void fviz_widget_manipulator_end(FVizWidgetManipulator* manipulator);
FVIZ_INTERACTION_API FVizBool fviz_widget_manipulator_active(const FVizWidgetManipulator* manipulator);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_WIDGET_MANIPULATOR_H */
