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
    FVizActor* actor;
    FVizInteractionState state;
    FVizCamera* snapshot_camera;
    FVizVec3 snapshot_camera_position;
    FVizVec3 snapshot_camera_target;
    FVizVec3 snapshot_camera_up;
    float snapshot_camera_fov;
    float snapshot_camera_near;
    float snapshot_camera_far;
    float snapshot_camera_parallel_scale;
    FVizCameraProjectionMode snapshot_camera_projection;
    FVizVec3 snapshot_actor_position;
    FVizQuat snapshot_actor_orientation;
    FVizVec3 snapshot_actor_scale;
    FVizBool transaction_active;
};

#endif /* FVIZ_INTERNAL_INTERACTION_INTERACTOR_STYLE_PRIVATE_H */
