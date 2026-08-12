#ifndef FVIZ_INTERNAL_RENDERING_RENDER_WINDOW_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_RENDER_WINDOW_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizRenderWindow.h>

struct FVizRenderWindow
{
    FVizObject base;
    FVizRenderer* renderer;
    int width;
    int height;
    char* title;
    void* native_window;
    void* native_dc;
    void* native_gl_context;
    void* gl_device;
    FVizBool gl_modern;
    FVizBool visible;
    FVizBool close_requested;
    FVizBool left_mouse_down;
    FVizBool middle_mouse_down;
    int last_mouse_x;
    int last_mouse_y;
};

FVizResult fviz_internal_render_window_create_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_show_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_render_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_run_platform(FVizRenderWindow* window);
void fviz_internal_render_window_destroy_platform(FVizRenderWindow* window);
void fviz_internal_render_window_request_close_platform(FVizRenderWindow* window);
FVizBool fviz_internal_render_window_supported_platform(void);

#endif /* FVIZ_INTERNAL_RENDERING_RENDER_WINDOW_PRIVATE_H */
