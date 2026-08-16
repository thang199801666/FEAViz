#include <stdint.h>
#include <FViz/Rendering/FVizRendering.h>

int main(void)
{
    FVizRenderTargetDesc desc;
    FVizRenderTargetDesc copy;
    FVizRenderTarget* target = NULL;
    FVizRenderer* renderer = NULL;
    FVizWeightedOITOptions oit;
    uint64_t bytes;

    fviz_render_target_desc_initialize(&desc);
    desc.width = 1920u;
    desc.height = 1080u;
    desc.samples = 4u;
    if (fviz_render_target_desc_add_attachment(&desc, FVIZ_RENDER_ATTACHMENT_COLOR0,
            FVIZ_RENDER_FORMAT_RGBA16_FLOAT, FVIZ_TRUE) != FVIZ_OK) return 1;
    if (fviz_render_target_desc_add_attachment(&desc, FVIZ_RENDER_ATTACHMENT_DEPTH,
            FVIZ_RENDER_FORMAT_DEPTH32_FLOAT, FVIZ_TRUE) != FVIZ_OK) return 2;
    if (fviz_render_target_desc_validate(&desc) != FVIZ_OK) return 3;
    if (fviz_render_target_create(&desc, &target) != FVIZ_OK || target == NULL) return 4;
    bytes = fviz_render_target_estimated_bytes(target);
    if (bytes != UINT64_C(1920) * UINT64_C(1080) * UINT64_C(12) * UINT64_C(4)) return 5;
    if (fviz_render_target_resize(target, 1280u, 720u) != FVIZ_OK) return 6;
    fviz_render_target_get_desc(target, &copy);
    if (copy.width != 1280u || copy.height != 720u || copy.attachment_count != 2u) return 7;

    if (fviz_renderer_create(&renderer) != FVIZ_OK || renderer == NULL) return 8;
    fviz_renderer_set_transparency_mode(renderer, FVIZ_TRANSPARENCY_WEIGHTED_BLENDED);
    if (fviz_renderer_transparency_mode(renderer) != FVIZ_TRANSPARENCY_WEIGHTED_BLENDED) return 9;
    fviz_weighted_oit_options_initialize(&oit);
    oit.weight_scale = 12.0f;
    oit.depth_weight = 4.0f;
    if (fviz_renderer_set_weighted_oit_options(renderer, &oit) != FVIZ_OK) return 10;
    fviz_renderer_get_weighted_oit_options(renderer, &oit);
    if (oit.weight_scale != 12.0f || oit.depth_weight != 4.0f) return 11;

    fviz_release(renderer);
    fviz_release(target);
    return 0;
}
