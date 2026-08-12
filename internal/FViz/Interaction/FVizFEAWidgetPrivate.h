#ifndef FVIZ_INTERNAL_INTERACTION_FEA_WIDGET_PRIVATE_H
#define FVIZ_INTERNAL_INTERACTION_FEA_WIDGET_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Interaction/FVizFEAWidget.h>

struct FVizSelectionHighlight
{
    FVizObject base;
    FVizRenderer* renderer;
    FVizSelection* selection;
    FVizActor* actor;
    FVizBool enabled;
};

struct FVizOrientationAxesWidget
{
    FVizObject base;
    FVizRenderWindow* window;
    FVizRenderer* target_renderer;
    FVizRenderer* overlay_renderer;
    FVizActor* actor;
    FVizBool enabled;
};

#endif /* FVIZ_INTERNAL_INTERACTION_FEA_WIDGET_PRIVATE_H */
