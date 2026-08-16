#ifndef FVIZ_RENDERING_WIN32_RENDER_CONTROL_H
#define FVIZ_RENDERING_WIN32_RENDER_CONTROL_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Rendering/FVizRendererWidget.h>

FVIZ_EXTERN_C_BEGIN

/* Reusable Win32 child control that owns an embedded FVizRendererWidget.
 * All HWNDs are represented as void* to keep windows.h out of public headers.
 * The returned renderer widget is borrowed and remains valid while the control
 * HWND exists. */
FVIZ_API FVizResult fviz_win32_render_control_create(void* parent_hwnd, int control_id, int x, int y, int width,
                                                     int height, const FVizRenderWindowOptions* options,
                                                     void** out_control_hwnd);

FVIZ_API void fviz_win32_render_control_destroy(void* control_hwnd);
FVIZ_API FVizRendererWidget* fviz_win32_render_control_renderer_widget(void* control_hwnd);
FVIZ_API FVizResult fviz_win32_render_control_set_bounds(void* control_hwnd, int x, int y, int width, int height);
FVIZ_API FVizResult fviz_win32_render_control_render(void* control_hwnd);
FVIZ_API FVizResult fviz_win32_render_control_request_render(void* control_hwnd);
FVIZ_API FVizBool fviz_win32_render_control_render_requested(void* control_hwnd);
FVIZ_API FVizResult fviz_win32_render_control_set_timer_pump(void* control_hwnd, FVizBool enabled,
                                                             uint32_t interval_milliseconds);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_WIN32_RENDER_CONTROL_H */
