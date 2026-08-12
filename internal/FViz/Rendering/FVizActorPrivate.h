#ifndef FVIZ_INTERNAL_RENDERING_ACTOR_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_ACTOR_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizActor.h>

struct FVizActor
{
    FVizObject base;
    FVizPolyData* poly_data;
    float color[3];
    FVizBool visible;
    FVizBool wireframe;
};

#endif /* FVIZ_INTERNAL_RENDERING_ACTOR_PRIVATE_H */
