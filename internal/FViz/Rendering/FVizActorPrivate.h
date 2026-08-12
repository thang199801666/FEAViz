#ifndef FVIZ_INTERNAL_RENDERING_ACTOR_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_ACTOR_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Math/FVizQuat.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Rendering/FVizActor.h>

struct FVizActor
{
    FVizObject base;
    FVizMapper* mapper;
    float color[3];
    FVizBool visible;
    FVizBool wireframe;
    FVizBool edge_visible;
    float opacity;
    float edge_color[3];
    float line_width;
    FVizVec3 position;
    FVizQuat orientation;
    FVizVec3 scale;
    FVizTransform* user_transform;
};

#endif /* FVIZ_INTERNAL_RENDERING_ACTOR_PRIVATE_H */
