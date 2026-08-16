#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Rendering/Win32/FVizWin32RenderControl.h>

#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_WIN32_RENDER_CONTROL_CLASS "FEAVizRenderControl"

typedef struct FVizWin32RenderControlState
{
    FVizRendererWidget* widget;
    UINT_PTR timer_id;
    uint32_t timer_interval_milliseconds;
    FVizBool timer_enabled;
} FVizWin32RenderControlState;

typedef struct FVizWin32RenderControlCreateParams
{
    FVizRenderWindowOptions options;
} FVizWin32RenderControlCreateParams;

static LRESULT CALLBACK fviz_win32_render_control_proc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

static FVizResult fviz_win32_render_control_register_class(void)
{
    static ATOM class_atom = 0;
    WNDCLASSA window_class;
    if (class_atom != 0) return FVIZ_OK;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    window_class.lpfnWndProc = fviz_win32_render_control_proc;
    window_class.hInstance = GetModuleHandleA(NULL);
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.lpszClassName = FVIZ_WIN32_RENDER_CONTROL_CLASS;
    class_atom = RegisterClassA(&window_class);
    if (class_atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to register FEAViz Win32 render control class");
        return FVIZ_ERROR_GRAPHICS;
    }
    return FVIZ_OK;
}

static FVizWin32RenderControlState* fviz_win32_render_control_state(HWND hwnd)
{
    return hwnd != NULL
        ? (FVizWin32RenderControlState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA)
        : NULL;
}

static LRESULT CALLBACK fviz_win32_render_control_proc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    FVizWin32RenderControlState* state = fviz_win32_render_control_state(hwnd);
    switch (message)
    {
        case WM_CREATE:
        {
            const CREATESTRUCTA* create = (const CREATESTRUCTA*)lparam;
            const FVizWin32RenderControlCreateParams* params =
                create != NULL ? (const FVizWin32RenderControlCreateParams*)create->lpCreateParams : NULL;
            RECT rect;
            int width = 1;
            int height = 1;
            if (params == NULL) return -1;
            state = (FVizWin32RenderControlState*)fviz_alloc(sizeof(*state));
            if (state == NULL) return -1;
            (void)memset(state, 0, sizeof(*state));
            state->timer_interval_milliseconds = 16u;
            state->timer_enabled = FVIZ_TRUE;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)state);
            if (GetClientRect(hwnd, &rect) != FALSE)
            {
                width = rect.right - rect.left;
                height = rect.bottom - rect.top;
                if (width <= 0) width = 1;
                if (height <= 0) height = 1;
            }
            if (fviz_renderer_widget_create_attached_with_options(
                    hwnd, width, height, &params->options, &state->widget) != FVIZ_OK)
            {
                SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
                fviz_free(state);
                return -1;
            }
            (void)fviz_renderer_widget_sync_host_size(state->widget);
            (void)fviz_renderer_widget_show(state->widget);
            state->timer_id = SetTimer(hwnd, 1u, state->timer_interval_milliseconds, NULL);
            return 0;
        }
        case WM_SIZE:
            if (state != NULL && state->widget != NULL && wparam != SIZE_MINIMIZED)
                (void)fviz_renderer_widget_sync_host_size(state->widget);
            return 0;
#if defined(WM_DPICHANGED_AFTERPARENT)
        case WM_DPICHANGED_AFTERPARENT:
            if (state != NULL && state->widget != NULL)
                (void)fviz_renderer_widget_sync_host_size(state->widget);
            return 0;
#endif
        case WM_SETFOCUS:
            if (state != NULL && state->widget != NULL)
            {
                HWND child = (HWND)fviz_renderer_widget_native_handle(state->widget);
                if (child != NULL && IsWindow(child) != FALSE) (void)SetFocus(child);
            }
            return 0;
        case WM_ENABLE:
            if (state != NULL && state->widget != NULL)
            {
                HWND child = (HWND)fviz_renderer_widget_native_handle(state->widget);
                if (child != NULL && IsWindow(child) != FALSE) (void)EnableWindow(child, wparam != 0);
            }
            return 0;
        case WM_SHOWWINDOW:
            if (state != NULL && state->widget != NULL)
            {
                if (wparam != 0)
                {
                    (void)fviz_renderer_widget_sync_host_size(state->widget);
                    (void)fviz_renderer_widget_show(state->widget);
                    fviz_render_window_request_render(
                        fviz_renderer_widget_window(state->widget));
                    if (state->timer_enabled != FVIZ_FALSE && state->timer_id == 0u)
                        state->timer_id = SetTimer(
                            hwnd, 1u, state->timer_interval_milliseconds, NULL);
                }
                else if (state->timer_id != 0u)
                {
                    (void)KillTimer(hwnd, state->timer_id);
                    state->timer_id = 0u;
                }
            }
            return 0;
        case WM_TIMER:
            if (state != NULL && state->widget != NULL &&
                state->timer_id != 0u && wparam == state->timer_id)
            {
                FVizRenderWindowInteractor* interactor =
                    fviz_renderer_widget_interactor(state->widget);
                if (interactor != NULL)
                    (void)fviz_render_window_interactor_process_timers(
                        interactor, (double)GetTickCount64() / 1000.0);
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_NCDESTROY:
            if (state != NULL)
            {
                if (state->timer_id != 0u) (void)KillTimer(hwnd, state->timer_id);
                state->timer_id = 0u;
                SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
                fviz_release(state->widget);
                state->widget = NULL;
                fviz_free(state);
            }
            break;
        default:
            break;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

FVizResult fviz_win32_render_control_create(
    void* parent_hwnd,
    int control_id,
    int x,
    int y,
    int width,
    int height,
    const FVizRenderWindowOptions* options,
    void** out_control_hwnd)
{
    FVizRenderWindowOptions defaults;
    FVizWin32RenderControlCreateParams params;
    HWND parent = (HWND)parent_hwnd;
    HWND control;
    if (out_control_hwnd != NULL) *out_control_hwnd = NULL;
    if (parent == NULL || IsWindow(parent) == FALSE || out_control_hwnd == NULL ||
        width <= 0 || height <= 0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid Win32 render control arguments");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_win32_render_control_register_class() != FVIZ_OK) return fviz_last_error_code();
    if (options == NULL)
    {
        fviz_render_window_options_initialize(&defaults);
        params.options = defaults;
    }
    else
    {
        params.options = *options;
    }
    control = CreateWindowExA(
        0,
        FVIZ_WIN32_RENDER_CONTROL_CLASS,
        "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, width, height,
        parent,
        (HMENU)(INT_PTR)control_id,
        GetModuleHandleA(NULL),
        &params);
    if (control == NULL)
    {
        if (fviz_last_error_code() == FVIZ_OK)
            fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to create FEAViz Win32 render control");
        return fviz_last_error_code();
    }
    *out_control_hwnd = control;
    return FVIZ_OK;
}

void fviz_win32_render_control_destroy(void* control_hwnd)
{
    HWND control = (HWND)control_hwnd;
    if (control != NULL && IsWindow(control) != FALSE) (void)DestroyWindow(control);
}

FVizRendererWidget* fviz_win32_render_control_renderer_widget(void* control_hwnd)
{
    FVizWin32RenderControlState* state = fviz_win32_render_control_state((HWND)control_hwnd);
    return state != NULL ? state->widget : NULL;
}

FVizResult fviz_win32_render_control_set_bounds(
    void* control_hwnd, int x, int y, int width, int height)
{
    HWND control = (HWND)control_hwnd;
    if (control == NULL || IsWindow(control) == FALSE || width <= 0 || height <= 0)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    return SetWindowPos(
        control, NULL, x, y, width, height,
        SWP_NOZORDER | SWP_NOACTIVATE) != FALSE ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

FVizResult fviz_win32_render_control_render(void* control_hwnd)
{
    FVizRendererWidget* widget = fviz_win32_render_control_renderer_widget(control_hwnd);
    if (widget == NULL) return FVIZ_ERROR_INVALID_STATE;
    return fviz_renderer_widget_render(widget);
}

FVizResult fviz_win32_render_control_request_render(void* control_hwnd)
{
    FVizRendererWidget* widget = fviz_win32_render_control_renderer_widget(control_hwnd);
    FVizRenderWindow* window;
    if (widget == NULL) return FVIZ_ERROR_INVALID_STATE;
    window = fviz_renderer_widget_window(widget);
    if (window == NULL) return FVIZ_ERROR_INVALID_STATE;
    fviz_render_window_request_render(window);
    return FVIZ_OK;
}

FVizBool fviz_win32_render_control_render_requested(void* control_hwnd)
{
    FVizRendererWidget* widget = fviz_win32_render_control_renderer_widget(control_hwnd);
    const FVizRenderWindow* window = widget != NULL
        ? fviz_renderer_widget_window(widget)
        : NULL;
    return window != NULL ? fviz_render_window_render_requested(window) : FVIZ_FALSE;
}


FVizResult fviz_win32_render_control_set_timer_pump(
    void* control_hwnd,
    FVizBool enabled,
    uint32_t interval_milliseconds)
{
    HWND control = (HWND)control_hwnd;
    FVizWin32RenderControlState* state = fviz_win32_render_control_state(control);
    if (control == NULL || IsWindow(control) == FALSE || state == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (enabled != FVIZ_FALSE && interval_milliseconds == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (state->timer_id != 0u)
    {
        (void)KillTimer(control, state->timer_id);
        state->timer_id = 0u;
    }
    state->timer_enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (interval_milliseconds != 0u)
        state->timer_interval_milliseconds = interval_milliseconds;
    if (state->timer_enabled != FVIZ_FALSE && IsWindowVisible(control) != FALSE)
    {
        state->timer_id = SetTimer(control, 1u, state->timer_interval_milliseconds, NULL);
        if (state->timer_id == 0u) return FVIZ_ERROR_INVALID_STATE;
    }
    return FVIZ_OK;
}
