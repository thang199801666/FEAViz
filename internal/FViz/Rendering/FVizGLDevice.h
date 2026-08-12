#ifndef FVIZ_INTERNAL_RENDERING_GL_DEVICE_H
#define FVIZ_INTERNAL_RENDERING_GL_DEVICE_H

#include <FViz/Core/FVizResult.h>
#include <FViz/Rendering/FVizRenderPass.h>

typedef struct FVizGLFunctions FVizGLFunctions;
typedef struct FVizGLDevice FVizGLDevice;
typedef struct FVizRenderer FVizRenderer;
typedef struct FVizScalarLegend FVizScalarLegend;

FVizGLDevice* fviz_internal_gl_device_create(const FVizGLFunctions* functions);
void fviz_internal_gl_device_destroy(FVizGLDevice* device);
FVizResult fviz_internal_gl_device_render_stage(
    FVizGLDevice* device,
    FVizRenderer* renderer,
    float aspect_ratio,
    FVizRenderPassStage stage);
FVizResult fviz_internal_gl_device_render_legend(
    FVizGLDevice* device,
    const FVizScalarLegend* legend,
    int width,
    int height);
FVizResult fviz_internal_gl_device_select(
    FVizGLDevice* device,
    FVizRenderer* renderer,
    float aspect_ratio,
    int x,
    int y,
    FVizSize* out_actor_index,
    FVizSize* out_primitive_id,
    float* out_depth);

#endif /* FVIZ_INTERNAL_RENDERING_GL_DEVICE_H */
