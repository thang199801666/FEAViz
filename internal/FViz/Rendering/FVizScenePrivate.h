#ifndef FVIZ_INTERNAL_RENDERING_SCENE_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_SCENE_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizScene.h>

struct FVizScene
{
    FVizObject base;
    FVizArray* actors;
    FVizArray* actor_modified_tags;
};

#endif /* FVIZ_INTERNAL_RENDERING_SCENE_PRIVATE_H */
