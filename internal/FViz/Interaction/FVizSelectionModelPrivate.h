#ifndef FVIZ_INTERNAL_INTERACTION_SELECTION_MODEL_PRIVATE_H
#define FVIZ_INTERNAL_INTERACTION_SELECTION_MODEL_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Interaction/FVizSelectionModel.h>

struct FVizSelectionModel
{
    FVizObject base;
    FVizSelection* selection;
    FVizSelection* hover_selection;
    FVizSelectionAssociation association;
    FVizSelectionModifier modifier;
    float hover_update_rate;
    double last_hover_timestamp;
    int last_hover_x;
    int last_hover_y;
    FVizBool have_hover_sample;
};

#endif /* FVIZ_INTERNAL_INTERACTION_SELECTION_MODEL_PRIVATE_H */
