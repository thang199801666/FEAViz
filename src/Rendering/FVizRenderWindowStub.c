#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRenderWindowPrivate.h>

FVizResult fviz_internal_render_window_create_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                            "native render window backend is currently implemented for Windows only");
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_show_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_render_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_run_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_process_events_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

void fviz_internal_render_window_schedule_render_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
}

FVizResult fviz_internal_render_window_release_external_opengl_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_reinitialize_external_opengl_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_resize_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_sync_host_size_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_reparent_platform(FVizRenderWindow* window, void* host_native_handle)
{
    FVIZ_UNUSED(window);
    FVIZ_UNUSED(host_native_handle);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_read_rgba8_platform(FVizRenderWindow* window, uint8_t* pixels)
{
    FVIZ_UNUSED(window);
    FVIZ_UNUSED(pixels);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_read_depth_f32_platform(FVizRenderWindow* window, float* depth)
{
    FVIZ_UNUSED(window);
    FVIZ_UNUSED(depth);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_hardware_pick_platform(FVizRenderWindow* window, FVizRenderer* renderer, int x,
                                                              int y, FVizSelectionAssociation association,
                                                              FVizSize* out_actor_index, FVizSize* out_primitive_id,
                                                              float* out_depth)
{
    FVIZ_UNUSED(window);
    FVIZ_UNUSED(renderer);
    FVIZ_UNUSED(x);
    FVIZ_UNUSED(y);
    FVIZ_UNUSED(association);
    FVIZ_UNUSED(out_actor_index);
    FVIZ_UNUSED(out_primitive_id);
    FVIZ_UNUSED(out_depth);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

void fviz_internal_render_window_destroy_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
}

void fviz_internal_render_window_request_close_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
}

FVizBool fviz_internal_render_window_supported_platform(void)
{
    return FVIZ_FALSE;
}

FVizResult fviz_internal_render_window_set_swap_interval_platform(FVizRenderWindow* window, int interval)
{
    FVIZ_UNUSED(window);
    FVIZ_UNUSED(interval);
    return FVIZ_ERROR_NOT_SUPPORTED;
}

FVizResult fviz_internal_render_window_release_gpu_mesh_resources_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    return FVIZ_ERROR_NOT_SUPPORTED;
}
