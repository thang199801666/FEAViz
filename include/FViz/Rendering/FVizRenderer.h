#ifndef FVIZ_RENDERING_RENDERER_H
#define FVIZ_RENDERING_RENDERER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizRay.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Math/FVizFrustum.h>
#include <FViz/Rendering/FVizCamera.h>
#include <FViz/Rendering/FVizLight.h>
#include <FViz/Rendering/FVizLabelSet.h>
#include <FViz/Rendering/FVizRenderGraph.h>
#include <FViz/Rendering/FVizRenderPass.h>
#include <FViz/Rendering/FVizScalarLegend.h>
#include <FViz/Rendering/FVizTextActor.h>
#include <FViz/Rendering/FVizScene.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRenderer FVizRenderer;

typedef enum FVizTransparencyMode
{
    FVIZ_TRANSPARENCY_SORTED = 0,
    FVIZ_TRANSPARENCY_WEIGHTED_BLENDED = 1,
    FVIZ_TRANSPARENCY_DEPTH_PEELING = 2
} FVizTransparencyMode;

typedef struct FVizWeightedOITOptions
{
    uint32_t struct_size;
    float weight_scale;
    float depth_weight;
    float minimum_weight;
    float alpha_cutoff;
} FVizWeightedOITOptions;
#define FVIZ_TYPE_RENDERER UINT64_C(0xB5EBD4F126C68F53)

FVIZ_API FVizResult fviz_renderer_create(FVizRenderer** out_renderer);
FVIZ_API FVizResult fviz_renderer_set_scene(FVizRenderer* renderer, FVizScene* scene);
FVIZ_API FVizScene* fviz_renderer_scene(FVizRenderer* renderer);
FVIZ_API FVizCamera* fviz_renderer_camera(FVizRenderer* renderer);
FVIZ_API void fviz_renderer_set_frustum_culling(FVizRenderer* renderer, FVizBool enabled);
FVIZ_API FVizBool fviz_renderer_frustum_culling(const FVizRenderer* renderer);
FVIZ_API FVizResult fviz_renderer_get_frustum(
    const FVizRenderer* renderer, float aspect_ratio, FVizFrustum* out_frustum);
FVIZ_API FVizBool fviz_renderer_actor_in_frustum(
    const FVizRenderer* renderer, const FVizActor* actor, float aspect_ratio);
FVIZ_API void fviz_renderer_set_small_object_culling(FVizRenderer* renderer, FVizBool enabled);
FVIZ_API FVizBool fviz_renderer_small_object_culling(const FVizRenderer* renderer);
FVIZ_API FVizResult fviz_renderer_set_small_object_threshold_pixels(
    FVizRenderer* renderer, float diameter_pixels);
FVIZ_API float fviz_renderer_small_object_threshold_pixels(const FVizRenderer* renderer);
FVIZ_API float fviz_renderer_actor_projected_diameter_pixels(
    const FVizRenderer* renderer,
    const FVizActor* actor,
    float aspect_ratio,
    int viewport_height);
FVIZ_API FVizBool fviz_renderer_actor_is_renderable(
    const FVizRenderer* renderer,
    const FVizActor* actor,
    float aspect_ratio,
    int viewport_height);
FVIZ_API void fviz_weighted_oit_options_initialize(FVizWeightedOITOptions* options);
FVIZ_API void fviz_renderer_set_transparency_mode(FVizRenderer* renderer, FVizTransparencyMode mode);
FVIZ_API FVizTransparencyMode fviz_renderer_transparency_mode(const FVizRenderer* renderer);
FVIZ_API FVizResult fviz_renderer_set_weighted_oit_options(
    FVizRenderer* renderer, const FVizWeightedOITOptions* options);
FVIZ_API void fviz_renderer_get_weighted_oit_options(
    const FVizRenderer* renderer, FVizWeightedOITOptions* out_options);
#define FVIZ_RENDERER_MAX_LIGHTS 4u
FVIZ_API FVizResult fviz_renderer_add_light(FVizRenderer* renderer, FVizLight* light);
FVIZ_API FVizResult fviz_renderer_remove_light(FVizRenderer* renderer, FVizLight* light);
FVIZ_API void fviz_renderer_remove_all_lights(FVizRenderer* renderer);
FVIZ_API FVizSize fviz_renderer_light_count(const FVizRenderer* renderer);
FVIZ_API FVizLight* fviz_renderer_light_at(FVizRenderer* renderer, FVizSize index);
FVIZ_API FVizResult fviz_renderer_update(FVizRenderer* renderer);
FVIZ_API void fviz_renderer_set_background(FVizRenderer* renderer, float red, float green, float blue);
FVIZ_API void fviz_renderer_get_background(const FVizRenderer* renderer, float* red, float* green, float* blue);
FVIZ_API void fviz_renderer_set_background2(FVizRenderer* renderer, float red, float green, float blue);
FVIZ_API void fviz_renderer_get_background2(const FVizRenderer* renderer, float* red, float* green, float* blue);
FVIZ_API void fviz_renderer_set_gradient_background(FVizRenderer* renderer, FVizBool enabled);
FVIZ_API FVizBool fviz_renderer_gradient_background(const FVizRenderer* renderer);
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
/* Recomputes the camera near/far clipping planes from the scene bounds and the
 * current camera distance, so zoom/pan/rotate never clip the model away. */
FVIZ_API void fviz_renderer_reset_clipping_range(FVizRenderer* renderer);
FVIZ_API void fviz_renderer_set_scalar_legend(FVizRenderer* renderer, FVizScalarLegend* legend);
FVIZ_API FVizScalarLegend* fviz_renderer_scalar_legend(FVizRenderer* renderer);
FVIZ_API FVizResult fviz_renderer_add_text_actor_2d(FVizRenderer* renderer, FVizTextActor2D* actor);
FVIZ_API FVizResult fviz_renderer_remove_text_actor_2d(FVizRenderer* renderer, FVizTextActor2D* actor);
FVIZ_API void fviz_renderer_remove_all_text_actors_2d(FVizRenderer* renderer);
FVIZ_API FVizSize fviz_renderer_text_actor_2d_count(const FVizRenderer* renderer);
FVIZ_API FVizTextActor2D* fviz_renderer_text_actor_2d_at(FVizRenderer* renderer, FVizSize index);
FVIZ_API FVizResult fviz_renderer_add_billboard_text_actor_3d(FVizRenderer* renderer, FVizBillboardTextActor3D* actor);
FVIZ_API FVizResult fviz_renderer_remove_billboard_text_actor_3d(FVizRenderer* renderer, FVizBillboardTextActor3D* actor);
FVIZ_API void fviz_renderer_remove_all_billboard_text_actors_3d(FVizRenderer* renderer);
FVIZ_API FVizSize fviz_renderer_billboard_text_actor_3d_count(const FVizRenderer* renderer);
FVIZ_API FVizBillboardTextActor3D* fviz_renderer_billboard_text_actor_3d_at(FVizRenderer* renderer, FVizSize index);
FVIZ_API FVizResult fviz_renderer_add_label_set_3d(FVizRenderer* renderer, FVizLabelSet3D* label_set);
FVIZ_API FVizResult fviz_renderer_remove_label_set_3d(FVizRenderer* renderer, FVizLabelSet3D* label_set);
FVIZ_API void fviz_renderer_remove_all_label_sets_3d(FVizRenderer* renderer);
FVIZ_API FVizSize fviz_renderer_label_set_3d_count(const FVizRenderer* renderer);
FVIZ_API FVizLabelSet3D* fviz_renderer_label_set_3d_at(FVizRenderer* renderer, FVizSize index);
FVIZ_API FVizResult fviz_renderer_reset_standard_passes(FVizRenderer* renderer);
FVIZ_API FVizResult fviz_renderer_add_pass(FVizRenderer* renderer, FVizRenderPass* pass);
FVIZ_API FVizResult fviz_renderer_remove_pass(FVizRenderer* renderer, FVizRenderPass* pass);
FVIZ_API FVizSize fviz_renderer_pass_count(const FVizRenderer* renderer);
FVIZ_API FVizRenderPass* fviz_renderer_pass_at(FVizRenderer* renderer, FVizSize index);
/* Compiles the compatibility pass list into a stable executable graph. The
 * graph is rebuilt lazily after pass mutation; the returned graph is borrowed. */
FVIZ_API FVizResult fviz_renderer_compile_render_graph(FVizRenderer* renderer);
FVIZ_API const FVizRenderGraph* fviz_renderer_render_graph(FVizRenderer* renderer);

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
FVIZ_API FVizResult fviz_renderer_world_to_display(
    const FVizRenderer* renderer,
    FVizVec3 world,
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
