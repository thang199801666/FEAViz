#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizRenderPass.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRenderPassPrivate.h>

static void fviz_render_pass_destroy(FVizObject* object)
{
    FVizRenderPass* pass = (FVizRenderPass*)object;
    if (pass->destroy_user_data != NULL) pass->destroy_user_data(pass->user_data);
    pass->user_data = NULL;
}

static const FVizObjectClass g_fviz_render_pass_class = {FVIZ_TYPE_RENDER_PASS, "FVizRenderPass", &g_fviz_object_class,
                                                         fviz_render_pass_destroy, NULL};

FVizResult fviz_render_pass_create(FVizRenderPassStage stage, FVizRenderPassExecuteFn execute, void* user_data,
                                   FVizRenderPassDestroyFn destroy_user_data, FVizRenderPass** out_pass)
{
    FVizRenderPass* pass;
    if (out_pass == NULL || stage < FVIZ_RENDER_PASS_CLEAR || stage > FVIZ_RENDER_PASS_OVERLAY)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "render pass stage and output must be valid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_pass = NULL;
    pass = (FVizRenderPass*)fviz_internal_object_allocate(sizeof(*pass), &g_fviz_render_pass_class, NULL);
    if (pass == NULL) return fviz_last_error_code();
    pass->stage = stage;
    pass->execute = execute;
    pass->user_data = user_data;
    pass->destroy_user_data = destroy_user_data;
    *out_pass = pass;
    return FVIZ_OK;
}

FVizRenderPassStage fviz_render_pass_stage(const FVizRenderPass* pass)
{
    return pass != NULL ? pass->stage : FVIZ_RENDER_PASS_CLEAR;
}

FVizBool fviz_render_pass_is_custom(const FVizRenderPass* pass)
{
    return pass != NULL && pass->execute != NULL ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizResult fviz_render_pass_execute(FVizRenderPass* pass, FVizRenderer* renderer, const FVizRenderPassContext* context)
{
    if (pass == NULL || renderer == NULL || context == NULL || context->struct_size < sizeof(*context))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "render pass execution context is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return pass->execute != NULL ? pass->execute(pass, renderer, context, pass->user_data) : FVIZ_OK;
}
