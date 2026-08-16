#ifndef FVIZ_INTERNAL_INTERACTION_WIDGET_MANIPULATOR_PRIVATE_H
#define FVIZ_INTERNAL_INTERACTION_WIDGET_MANIPULATOR_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Interaction/FVizWidgetManipulator.h>

struct FVizWidgetManipulator
{
    FVizObject base;
    FVizWidgetManipulatorMode mode;
    FVizVec3 origin;
    FVizVec3 axis;
    FVizVec3 plane_normal;
    FVizVec3 active_plane_normal;
    FVizVec3 last_world;
    FVizBool active;
};

#endif /* FVIZ_INTERNAL_INTERACTION_WIDGET_MANIPULATOR_PRIVATE_H */
