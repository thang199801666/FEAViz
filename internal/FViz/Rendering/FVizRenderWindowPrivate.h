#ifndef FVIZ_INTERNAL_RENDERING_RENDER_WINDOW_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_RENDER_WINDOW_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizArray.h>
#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Spatial/FVizBVH.h>

struct FVizRenderWindow
{
    FVizObject base;
    FVizRenderer* renderer;
    FVizArray* renderers;
    FVizRenderWindowInteractor* interactor;
    int width;
    int height;
    char* title;
    void* native_window;
    void* host_native_handle;
    void* native_dc;
    void* native_gl_context;
    void* gl_device;
    FVizBVH* pick_bvh;
    const FVizPolyData* pick_poly_data;
    FVizMTime pick_bvh_mtime;
    FVizPickCallbackFn pick_callback;
    void* pick_user_data;
    FVizBool left_mouse_dragged;
    FVizBool gl_modern;
    FVizBool visible;
    FVizBool offscreen;
    FVizRenderWindowState state;
    FVizBool close_requested;
    FVizBool left_mouse_down;
    FVizBool middle_mouse_down;
    FVizBool right_mouse_down;
    FVizBool mouse_inside;
    int last_mouse_x;
    int last_mouse_y;
};

FVizResult fviz_internal_render_window_create_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_show_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_render_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_run_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_process_events_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_resize_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_read_rgba8_platform(FVizRenderWindow* window, uint8_t* pixels);
FVizResult fviz_internal_render_window_read_depth_f32_platform(FVizRenderWindow* window, float* depth);
FVizResult fviz_internal_render_window_hardware_pick_platform(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    int x,
    int y,
    FVizSize* out_actor_index,
    FVizSize* out_primitive_id,
    float* out_depth);
void fviz_internal_render_window_destroy_platform(FVizRenderWindow* window);
void fviz_internal_render_window_request_close_platform(FVizRenderWindow* window);
FVizBool fviz_internal_render_window_supported_platform(void);

#endif /* FVIZ_INTERNAL_RENDERING_RENDER_WINDOW_PRIVATE_H */
