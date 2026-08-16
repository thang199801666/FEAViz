#ifndef FVIZ_RENDERING_EXTERNAL_OPENGL_SURFACE_H
#define FVIZ_RENDERING_EXTERNAL_OPENGL_SURFACE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef FVizResult (*FVizExternalOpenGLMakeCurrentFn)(void* user_data);
typedef void (*FVizExternalOpenGLDoneCurrentFn)(void* user_data);
typedef FVizResult (*FVizExternalOpenGLPresentFn)(void* user_data);
typedef FVizResult (*FVizExternalOpenGLGetFramebufferSizeFn)(
    void* user_data,
    int* out_width,
    int* out_height);
typedef uint32_t (*FVizExternalOpenGLGetDefaultFramebufferFn)(void* user_data);
typedef void (*FVizExternalOpenGLRequestRenderFn)(void* user_data);

/*
 * Host-owned OpenGL surface/context contract.
 *
 * FEAViz never destroys the host context or surface. make_current() must make
 * an OpenGL 3.3+ compatible context current on the calling thread. present()
 * is optional because toolkits such as QOpenGLWindow present automatically
 * after paintGL(). get_default_framebuffer() is optional and defaults to 0;
 * it allows FEAViz to render correctly into toolkit-managed framebuffer objects.
 */
typedef struct FVizExternalOpenGLSurface
{
    uint32_t struct_size;
    void* user_data;
    FVizExternalOpenGLMakeCurrentFn make_current;
    FVizExternalOpenGLDoneCurrentFn done_current;
    FVizExternalOpenGLPresentFn present;
    FVizExternalOpenGLGetFramebufferSizeFn get_framebuffer_size;
    FVizExternalOpenGLGetDefaultFramebufferFn get_default_framebuffer;
    FVizExternalOpenGLRequestRenderFn request_render;
    uint32_t sample_count;
    FVizBool srgb_capable;
} FVizExternalOpenGLSurface;

FVIZ_API void fviz_external_opengl_surface_initialize(FVizExternalOpenGLSurface* surface);
FVIZ_API FVizResult fviz_external_opengl_surface_validate(const FVizExternalOpenGLSurface* surface);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_EXTERNAL_OPENGL_SURFACE_H */
