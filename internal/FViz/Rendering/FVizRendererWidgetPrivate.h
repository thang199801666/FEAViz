#ifndef FVIZ_INTERNAL_RENDERING_RENDERER_WIDGET_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_RENDERER_WIDGET_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizRendererWidget.h>

struct FVizRendererWidget
{
    FVizObject base;
    FVizRenderWindow* window;
};

#endif /* FVIZ_INTERNAL_RENDERING_RENDERER_WIDGET_PRIVATE_H */
