#ifndef FVIZ_INTERNAL_RENDERING_TEXT_ACTOR_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_TEXT_ACTOR_PRIVATE_H
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Rendering/FVizTextActor.h>
struct FVizTextActor2D
{
    FVizObject base;
    FVizString* text;
    FVizTextProperty* property;
    FVizObserverTag property_modified_tag;
    float position[2];
    FVizTextCoordinateSystem coordinate_system;
    FVizBool visible;
};
struct FVizBillboardTextActor3D
{
    FVizObject base;
    FVizString* text;
    FVizTextProperty* property;
    FVizObserverTag property_modified_tag;
    FVizVec3 world_position;
    float pixel_offset[2];
    FVizBool depth_test;
    FVizBool visible;
};
#endif
