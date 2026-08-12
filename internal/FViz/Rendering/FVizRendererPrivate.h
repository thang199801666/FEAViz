#ifndef FVIZ_INTERNAL_RENDERING_RENDERER_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_RENDERER_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizRenderer.h>

struct FVizRenderer
{
    FVizObject base;
    FVizScene* scene;
    FVizCamera* camera;
    float background[3];
};

#endif /* FVIZ_INTERNAL_RENDERING_RENDERER_PRIVATE_H */
