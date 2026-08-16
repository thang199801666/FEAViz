#include <stdio.h>
#include <FViz/FViz.h>

int main(void)
{
    FVizRenderer* renderer = NULL;
    FVizWeightedOITOptions oit;
    FVizRenderTargetDesc target_desc;
    FVizRenderTarget* target = NULL;

    if (fviz_renderer_create(&renderer) != FVIZ_OK) return 1;
    fviz_renderer_set_transparency_mode(renderer, FVIZ_TRANSPARENCY_WEIGHTED_BLENDED);
    fviz_weighted_oit_options_initialize(&oit);
    oit.weight_scale = 8.0f;
    oit.depth_weight = 3.0f;
    if (fviz_renderer_set_weighted_oit_options(renderer, &oit) != FVIZ_OK) return 2;

    fviz_render_target_desc_initialize(&target_desc);
    target_desc.width = 1280u;
    target_desc.height = 720u;
    target_desc.samples = 4u;
    if (fviz_render_target_desc_add_attachment(&target_desc,
            FVIZ_RENDER_ATTACHMENT_COLOR0, FVIZ_RENDER_FORMAT_RGBA16_FLOAT, FVIZ_TRUE) != FVIZ_OK ||
        fviz_render_target_desc_add_attachment(&target_desc,
            FVIZ_RENDER_ATTACHMENT_DEPTH_STENCIL, FVIZ_RENDER_FORMAT_DEPTH24_STENCIL8, FVIZ_FALSE) != FVIZ_OK ||
        fviz_render_target_create(&target_desc, &target) != FVIZ_OK)
        return 3;

    printf("FEAViz transparency pipeline: mode=%d, target=%ux%u, samples=%u, estimated=%llu bytes\n",
        (int)fviz_renderer_transparency_mode(renderer), target_desc.width, target_desc.height,
        target_desc.samples, (unsigned long long)fviz_render_target_estimated_bytes(target));
    printf("Weighted OIT: accumulation + revealage + fullscreen composite; sorted alpha remains fallback.\n");

    fviz_release(target);
    fviz_release(renderer);
    return 0;
}
