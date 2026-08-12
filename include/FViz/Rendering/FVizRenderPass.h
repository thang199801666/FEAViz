#ifndef FVIZ_RENDERING_RENDER_PASS_H
#define FVIZ_RENDERING_RENDER_PASS_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRenderPass FVizRenderPass;
typedef struct FVizRenderer FVizRenderer;

#define FVIZ_TYPE_RENDER_PASS UINT64_C(0x45E22F2BE7B49D61)

typedef enum FVizRenderPassStage
{
    FVIZ_RENDER_PASS_CLEAR = 0,
    FVIZ_RENDER_PASS_OPAQUE = 1,
    FVIZ_RENDER_PASS_TRANSLUCENT = 2,
    FVIZ_RENDER_PASS_EDGE = 3,
    FVIZ_RENDER_PASS_SELECTION = 4,
    FVIZ_RENDER_PASS_OVERLAY = 5
} FVizRenderPassStage;

typedef struct FVizRenderPassContext
{
    uint32_t struct_size;
    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
    float aspect_ratio;
    void* backend_context;
} FVizRenderPassContext;

typedef FVizResult (*FVizRenderPassExecuteFn)(
    FVizRenderPass* pass,
    FVizRenderer* renderer,
    const FVizRenderPassContext* context,
    void* user_data);
typedef void (*FVizRenderPassDestroyFn)(void* user_data);

FVIZ_API FVizResult fviz_render_pass_create(
    FVizRenderPassStage stage,
    FVizRenderPassExecuteFn execute,
    void* user_data,
    FVizRenderPassDestroyFn destroy_user_data,
    FVizRenderPass** out_pass);
FVIZ_API FVizRenderPassStage fviz_render_pass_stage(const FVizRenderPass* pass);
FVIZ_API FVizBool fviz_render_pass_is_custom(const FVizRenderPass* pass);
FVIZ_API FVizResult fviz_render_pass_execute(
    FVizRenderPass* pass,
    FVizRenderer* renderer,
    const FVizRenderPassContext* context);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_RENDER_PASS_H */
