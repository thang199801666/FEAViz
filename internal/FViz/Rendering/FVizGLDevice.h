#ifndef FVIZ_INTERNAL_RENDERING_GL_DEVICE_H
#define FVIZ_INTERNAL_RENDERING_GL_DEVICE_H

#include <FViz/Core/FVizResult.h>

typedef struct FVizGLFunctions FVizGLFunctions;
typedef struct FVizGLDevice FVizGLDevice;
typedef struct FVizRenderer FVizRenderer;

FVizGLDevice* fviz_internal_gl_device_create(const FVizGLFunctions* functions);
void fviz_internal_gl_device_destroy(FVizGLDevice* device);
FVizResult fviz_internal_gl_device_render(
    FVizGLDevice* device,
    FVizRenderer* renderer,
    float aspect_ratio);

#endif /* FVIZ_INTERNAL_RENDERING_GL_DEVICE_H */
