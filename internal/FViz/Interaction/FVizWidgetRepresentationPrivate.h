#ifndef FVIZ_INTERNAL_INTERACTION_WIDGET_REPRESENTATION_PRIVATE_H
#define FVIZ_INTERNAL_INTERACTION_WIDGET_REPRESENTATION_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Interaction/FVizWidgetRepresentation.h>

typedef struct FVizWidgetRepresentationEntry
{
    FVizObject* object;
    FVizBool local_visible;
} FVizWidgetRepresentationEntry;

struct FVizWidgetRepresentation
{
    FVizObject base;
    FVizRenderer* renderer;
    FVizWidgetRepresentationEntry* actors;
    FVizSize actor_count;
    FVizSize actor_capacity;
    FVizWidgetRepresentationEntry* text_actors_2d;
    FVizSize text_actor_2d_count;
    FVizSize text_actor_2d_capacity;
    FVizWidgetRepresentationEntry* billboard_text_actors_3d;
    FVizSize billboard_text_actor_3d_count;
    FVizSize billboard_text_actor_3d_capacity;
    FVizWidgetRepresentationEntry* label_sets_3d;
    FVizSize label_set_3d_count;
    FVizSize label_set_3d_capacity;
    FVizBool visible;
};

#endif /* FVIZ_INTERNAL_INTERACTION_WIDGET_REPRESENTATION_PRIVATE_H */
