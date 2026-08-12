#ifndef FVIZ_INTERNAL_INTERACTION_SELECTION_PRIVATE_H
#define FVIZ_INTERNAL_INTERACTION_SELECTION_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Interaction/FVizSelection.h>

typedef struct FVizSelectionItem
{
    FVizActor* actor;
    FVizSelectionAssociation association;
    FVizSize id;
} FVizSelectionItem;

struct FVizSelection
{
    FVizObject base;
    FVizArray* items;
};

#endif /* FVIZ_INTERNAL_INTERACTION_SELECTION_PRIVATE_H */
