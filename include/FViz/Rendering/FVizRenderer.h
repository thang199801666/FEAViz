#ifndef FVIZ_RENDERING_RENDERER_H
#define FVIZ_RENDERING_RENDERER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizRay.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Rendering/FVizCamera.h>
#include <FViz/Rendering/FVizRenderPass.h>
#include <FViz/Rendering/FVizScalarLegend.h>
#include <FViz/Rendering/FVizScene.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRenderer FVizRenderer;
#define FVIZ_TYPE_RENDERER UINT64_C(0xB5EBD4F126C68F53)

FVIZ_API FVizResult fviz_renderer_create(FVizRenderer** out_renderer);
FVIZ_API FVizResult fviz_renderer_set_scene(FVizRenderer* renderer, FVizScene* scene);
FVIZ_API FVizScene* fviz_renderer_scene(FVizRenderer* renderer);
FVIZ_API FVizCamera* fviz_renderer_camera(FVizRenderer* renderer);
FVIZ_API FVizResult fviz_renderer_update(FVizRenderer* renderer);
FVIZ_API void fviz_renderer_set_background(FVizRenderer* renderer, float red, float green, float blue);
FVIZ_API void fviz_renderer_get_background(const FVizRenderer* renderer, float* red, float* green, float* blue);
FVIZ_API FVizResult fviz_renderer_set_viewport(
    FVizRenderer* renderer,
    float minimum_x,
    float minimum_y,
    float maximum_x,
    float maximum_y);
FVIZ_API void fviz_renderer_get_viewport(
    const FVizRenderer* renderer,
    float* minimum_x,
    float* minimum_y,
    float* maximum_x,
    float* maximum_y);
FVIZ_API void fviz_renderer_set_layer(FVizRenderer* renderer, int layer);
FVIZ_API int fviz_renderer_layer(const FVizRenderer* renderer);
FVIZ_API void fviz_renderer_set_interactive(FVizRenderer* renderer, FVizBool interactive);
FVIZ_API FVizBool fviz_renderer_interactive(const FVizRenderer* renderer);
FVIZ_API FVizBool fviz_renderer_contains_normalized_point(
    const FVizRenderer* renderer,
    float x,
    float y);
FVIZ_API void fviz_renderer_fit_camera(FVizRenderer* renderer, float padding);
FVIZ_API void fviz_renderer_set_scalar_legend(FVizRenderer* renderer, FVizScalarLegend* legend);
FVIZ_API FVizScalarLegend* fviz_renderer_scalar_legend(FVizRenderer* renderer);
FVIZ_API FVizResult fviz_renderer_reset_standard_passes(FVizRenderer* renderer);
FVIZ_API FVizResult fviz_renderer_add_pass(FVizRenderer* renderer, FVizRenderPass* pass);
FVIZ_API FVizResult fviz_renderer_remove_pass(FVizRenderer* renderer, FVizRenderPass* pass);
FVIZ_API FVizSize fviz_renderer_pass_count(const FVizRenderer* renderer);
FVIZ_API FVizRenderPass* fviz_renderer_pass_at(FVizRenderer* renderer, FVizSize index);

FVIZ_API FVizResult fviz_renderer_world_to_view(
    const FVizRenderer* renderer,
    FVizVec3 world,
    FVizVec3* out_view);
FVIZ_API FVizResult fviz_renderer_view_to_ndc(
    const FVizRenderer* renderer,
    FVizVec3 view,
    float aspect_ratio,
    FVizVec3* out_ndc);
FVIZ_API FVizResult fviz_renderer_ndc_to_display(
    const FVizRenderer* renderer,
    FVizVec3 ndc,
    int window_width,
    int window_height,
    FVizVec3* out_display);
FVIZ_API FVizResult fviz_renderer_display_to_world_ray(
    const FVizRenderer* renderer,
    float display_x,
    float display_y,
    int window_width,
    int window_height,
    FVizRay* out_ray);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_RENDERER_H */
