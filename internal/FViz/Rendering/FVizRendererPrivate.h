#ifndef FVIZ_INTERNAL_RENDERING_RENDERER_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_RENDERER_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizScalarLegend.h>

struct FVizRenderer
{
    FVizObject base;
    FVizScene* scene;
    FVizCamera* camera;
    FVizScalarLegend* scalar_legend;
    float background[3];
    float viewport[4];
    int layer;
    FVizBool interactive;
};

#endif /* FVIZ_INTERNAL_RENDERING_RENDERER_PRIVATE_H */
