#ifndef FVIZ_INTERNAL_RENDERING_RENDER_TARGET_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_RENDER_TARGET_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizRenderTarget.h>

struct FVizRenderTarget
{
    FVizObject base;
    FVizRenderTargetDesc desc;
};

#endif /* FVIZ_INTERNAL_RENDERING_RENDER_TARGET_PRIVATE_H */
