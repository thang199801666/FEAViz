#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRenderWindowPrivate.h>

FVizResult fviz_internal_render_window_create_platform(FVizRenderWindow* window)
{
    FVIZ_UNUSED(window);
    fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "native render window backend is currently implemented for Windows only");
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
void fviz_internal_render_window_destroy_platform(FVizRenderWindow* window) { FVIZ_UNUSED(window); }
void fviz_internal_render_window_request_close_platform(FVizRenderWindow* window) { FVIZ_UNUSED(window); }
FVizBool fviz_internal_render_window_supported_platform(void) { return FVIZ_FALSE; }
