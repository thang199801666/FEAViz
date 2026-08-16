#ifndef FVIZ_INTERNAL_INTERACTION_ENGINEERING_WIDGETS_PRIVATE_H
#define FVIZ_INTERNAL_INTERACTION_ENGINEERING_WIDGETS_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Interaction/FVizEngineeringWidgets.h>
#include <FViz/Interaction/FVizWidgetManipulator.h>

struct FVizHandleWidget
{
    FVizObject base;
    FVizWidget* widget;
    FVizWidgetRepresentation* representation;
    FVizWidgetManipulator* manipulator;
    FVizActor* actor;
    FVizVec3 position;
    FVizVec3 interaction_start_position;
    float size_pixels;
    float pick_tolerance_pixels;
    float color[3];
};

struct FVizPlaneWidget
{
    FVizObject base;
    FVizWidget* widget;
    FVizWidgetRepresentation* representation;
    FVizWidgetManipulator* manipulator;
    FVizActor* fill_actor;
    FVizActor* outline_actor;
    FVizVec3 origin;
    FVizVec3 normal;
    FVizVec3 interaction_start_origin;
    float size;
    float color[3];
    void (*internal_changed)(struct FVizPlaneWidget*, void*);
    void* internal_changed_data;
};

struct FVizBoxWidget
{
    FVizObject base;
    FVizWidget* widget;
    FVizWidgetRepresentation* representation;
    FVizWidgetManipulator* manipulator;
    FVizActor* actor;
    FVizBounds bounds;
    FVizBounds interaction_start_bounds;
    int active_face;
    float handle_size;
    float pick_tolerance;
    float color[3];
};

struct FVizLineWidget
{
    FVizObject base;
    FVizWidget* widget;
    FVizWidgetRepresentation* representation;
    FVizWidgetManipulator* manipulator;
    FVizActor* actor;
    FVizVec3 points[2];
    FVizVec3 interaction_start_points[2];
    int active_part;
    float line_width;
    float handle_size;
    float pick_tolerance;
    float color[3];
};

struct FVizDistanceWidget
{
    FVizObject base;
    FVizWidget* widget;
    FVizWidgetRepresentation* representation;
    FVizActor* actor;
    FVizBillboardTextActor3D* label;
    FVizVec3 points[2];
    uint32_t point_count;
};

struct FVizAngleWidget
{
    FVizObject base;
    FVizWidget* widget;
    FVizWidgetRepresentation* representation;
    FVizActor* actor;
    FVizBillboardTextActor3D* label;
    FVizVec3 points[3];
    uint32_t point_count;
};

typedef struct FVizSectionCutTarget
{
    FVizActor* actor;
    FVizClipPlaneId plane_id;
} FVizSectionCutTarget;

struct FVizSectionCutWidget
{
    FVizObject base;
    FVizPlaneWidget* plane_widget;
    FVizSectionCutTarget* targets;
    FVizSize target_count;
    FVizSize target_capacity;
    FVizBool inside_out;
};

struct FVizProbeWidget
{
    FVizObject base;
    FVizWidget* widget;
    FVizWidgetRepresentation* representation;
    FVizBillboardTextActor3D* label;
    FVizSelection* selection;
    char array_name[128];
};

#endif /* FVIZ_INTERNAL_INTERACTION_ENGINEERING_WIDGETS_PRIVATE_H */
