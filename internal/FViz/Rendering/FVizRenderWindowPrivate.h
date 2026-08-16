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
    FVizArray* renderer_modified_tags;
    FVizRenderWindowInteractor* interactor;
    int width;
    int height;
    uint32_t dpi;
    char* title;
    void* native_window;
    void* host_native_handle;
    void* native_dc;
    void* native_gl_context;
    FVizExternalOpenGLSurface external_surface;
    FVizBool external_opengl;
    void* gl_device;
    FVizBVH* pick_bvh;
    const FVizPolyData* pick_poly_data;
    FVizMTime pick_bvh_mtime;
    FVizPickCallbackFn pick_callback;
    void* pick_user_data;
    FVizBool left_mouse_dragged;
    FVizBool gl_modern;
    uint32_t requested_multisamples;
    uint32_t actual_multisamples;
    FVizBool fxaa_enabled;
    FVizBool fxaa_supported;
    FVizFXAAOptions fxaa_options;
    FVizBool adaptive_antialiasing;
    FVizBool interaction_active;
    int swap_interval;
    FVizBool swap_control_supported;
    FVizBool srgb_enabled;
    FVizBool srgb_supported;
    FVizBool weighted_oit_supported;
    FVizBool shader_lines_supported;
    FVizBool text_rendering_supported;
    FVizBool integer_selection_supported;
    FVizBool gpu_timing_supported;
    FVizRenderStatistics last_statistics;
    FVizArray* pass_statistics;
    double last_interaction_render_seconds;
    FVizBool visible;
    FVizBool offscreen;
    FVizRenderWindowState state;
    FVizBool close_requested;
    FVizBool render_requested;
    FVizBool render_in_progress;
    uint64_t render_request_serial;
    FVizRenderRequestReasons pending_render_reasons;
    FVizRenderRequestReasons active_render_reasons;
    FVizFrameSchedulerOptions frame_scheduler_options;
    FVizFrameSchedulerStatistics frame_scheduler_statistics;
    FVizGPUMemoryOptions gpu_memory_options;
    FVizBool left_mouse_down;
    FVizBool middle_mouse_down;
    FVizBool right_mouse_down;
    FVizBool x1_mouse_down;
    FVizBool x2_mouse_down;
    FVizBool mouse_inside;
    int last_mouse_x;
    int last_mouse_y;
    int left_mouse_press_x;
    int left_mouse_press_y;
};

FVizResult fviz_internal_render_window_create_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_show_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_render_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_run_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_process_events_platform(FVizRenderWindow* window);
void fviz_internal_render_window_schedule_render_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_release_external_opengl_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_reinitialize_external_opengl_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_resize_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_sync_host_size_platform(FVizRenderWindow* window);
FVizResult fviz_internal_render_window_reparent_platform(
    FVizRenderWindow* window,
    void* host_native_handle);
FVizResult fviz_internal_render_window_read_rgba8_platform(FVizRenderWindow* window, uint8_t* pixels);
FVizResult fviz_internal_render_window_read_depth_f32_platform(FVizRenderWindow* window, float* depth);
FVizResult fviz_internal_render_window_hardware_pick_platform(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    int x,
    int y,
    FVizSelectionAssociation association,
    FVizSize* out_actor_index,
    FVizSize* out_primitive_id,
    float* out_depth);
void fviz_internal_render_window_destroy_platform(FVizRenderWindow* window);
void fviz_internal_render_window_request_close_platform(FVizRenderWindow* window);
FVizBool fviz_internal_render_window_supported_platform(void);
FVizResult fviz_internal_render_window_set_swap_interval_platform(FVizRenderWindow* window, int interval);
FVizResult fviz_internal_render_window_release_gpu_mesh_resources_platform(
    FVizRenderWindow* window);
void fviz_internal_render_window_set_interaction_active(FVizRenderWindow* window, FVizBool active);
void fviz_internal_render_window_clear_pass_statistics(FVizRenderWindow* window);
void fviz_internal_render_window_record_pass_statistics(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    FVizRenderGraphPassId graph_pass_id,
    FVizSize execution_index,
    FVizRenderPass* pass,
    const char* name,
    double cpu_seconds,
    FVizResult result);

#endif /* FVIZ_INTERNAL_RENDERING_RENDER_WINDOW_PRIVATE_H */
