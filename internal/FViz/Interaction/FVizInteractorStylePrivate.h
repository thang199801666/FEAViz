#ifndef FVIZ_INTERNAL_INTERACTION_INTERACTOR_STYLE_PRIVATE_H
#define FVIZ_INTERNAL_INTERACTION_INTERACTOR_STYLE_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Interaction/FVizInteractorStyle.h>

struct FVizInteractorStyle
{
    FVizObject base;
    float orbit_sensitivity;
    float pan_sensitivity;
    float dolly_factor;
    int last_x;
    int last_y;
    FVizBool left_down;
    FVizBool middle_down;
    FVizBool right_down;
    int rubber_start_x;
    int rubber_start_y;
    int rubber_end_x;
    int rubber_end_y;
    FVizBool rubber_active;
    FVizBool rubber_completed;
};

#endif /* FVIZ_INTERNAL_INTERACTION_INTERACTOR_STYLE_PRIVATE_H */
