#ifndef FVIZ_INTERNAL_RENDERING_RENDER_PASS_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_RENDER_PASS_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizRenderPass.h>

struct FVizRenderPass
{
    FVizObject base;
    FVizRenderPassStage stage;
    FVizRenderPassExecuteFn execute;
    void* user_data;
    FVizRenderPassDestroyFn destroy_user_data;
};

#endif /* FVIZ_INTERNAL_RENDERING_RENDER_PASS_PRIVATE_H */
