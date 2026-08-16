#ifndef FVIZ_INTERNAL_INTERACTION_SELECTION_PRIVATE_H
#define FVIZ_INTERNAL_INTERACTION_SELECTION_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Interaction/FVizSelection.h>

typedef struct FVizSelectionItem
{
    FVizSelectionRecord record;
} FVizSelectionItem;

struct FVizSelection
{
    FVizObject base;
    FVizArray* items;
};

typedef struct FVizNamedSelectionEntry
{
    FVizString* name;
    FVizSelection* selection;
} FVizNamedSelectionEntry;

struct FVizNamedSelectionCollection
{
    FVizObject base;
    FVizArray* entries;
};

#endif /* FVIZ_INTERNAL_INTERACTION_SELECTION_PRIVATE_H */
