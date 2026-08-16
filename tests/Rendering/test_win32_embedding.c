#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
#if defined(_WIN32)
    HWND host;
    HWND child;
    HWND second_host;
    HWND replacement_host;
    HWND recreated_child;
    HWND control;
    HWND control_child;
    RECT client;
    FVizRendererWidget* widget = NULL;
    FVizRenderWindow* window;
    int width = 0;
    int height = 0;
    uint64_t request_serial = 0u;

    if (fviz_render_window_supported() == FVIZ_FALSE) return 0;

    host = CreateWindowExA(
        0,
        "STATIC",
        "FEAViz embed test host",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 360,
        NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(host != NULL);
    CHECK(GetClientRect(host, &client) != FALSE);
    CHECK(client.right > client.left && client.bottom > client.top);

    CHECK(fviz_renderer_widget_create_attached(
        host,
        client.right - client.left,
        client.bottom - client.top,
        &widget) == FVIZ_OK);
    CHECK(widget != NULL);
    CHECK(fviz_renderer_widget_is_attached(widget) == FVIZ_TRUE);
    CHECK(fviz_renderer_widget_host_native_handle(widget) == host);

    window = fviz_renderer_widget_window(widget);
    CHECK(fviz_render_window_is_attached(window) == FVIZ_TRUE);
    CHECK(fviz_render_window_host_native_handle(window) == host);
    child = (HWND)fviz_renderer_widget_native_handle(widget);
    CHECK(child != NULL && IsWindow(child) != FALSE);
    CHECK(GetParent(child) == host);
    CHECK(((DWORD)GetWindowLongPtrA(child, GWL_STYLE) & WS_CHILD) != 0u);

    CHECK(SetWindowPos(host, NULL, 0, 0, 720, 520,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE);
    CHECK(fviz_renderer_widget_sync_host_size(widget) == FVIZ_OK);
    CHECK(GetClientRect(host, &client) != FALSE);
    fviz_render_window_get_size(window, &width, &height);
    CHECK(width == client.right - client.left);
    CHECK(height == client.bottom - client.top);

    CHECK(fviz_renderer_widget_start(widget) == FVIZ_ERROR_INVALID_STATE);

    second_host = CreateWindowExA(
        0, "STATIC", "FEAViz second embed test host",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 420,
        NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(second_host != NULL);
    CHECK(fviz_renderer_widget_reparent(widget, second_host) == FVIZ_OK);
    CHECK(fviz_renderer_widget_host_native_handle(widget) == second_host);
    CHECK(GetParent(child) == second_host);
    CHECK(fviz_renderer_widget_sync_host_size(widget) == FVIZ_OK);

    /* Simulate a toolkit destroying the native host during docking. Windows
     * destroys the FEAViz child too; reparent must recover the native surface
     * without replacing the high-level renderer widget. */
    CHECK(DestroyWindow(second_host) != FALSE);
    CHECK(IsWindow(child) == FALSE);
    replacement_host = CreateWindowExA(
        0, "STATIC", "FEAViz replacement embed test host",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 460,
        NULL, NULL, GetModuleHandleA(NULL), NULL);
    CHECK(replacement_host != NULL);
    CHECK(fviz_renderer_widget_reparent(widget, replacement_host) == FVIZ_OK);
    CHECK(fviz_renderer_widget_host_native_handle(widget) == replacement_host);
    recreated_child = (HWND)fviz_renderer_widget_native_handle(widget);
    CHECK(recreated_child != NULL && IsWindow(recreated_child) != FALSE);
    CHECK(GetParent(recreated_child) == replacement_host);
    CHECK(fviz_render_window_state(fviz_renderer_widget_window(widget)) != FVIZ_RENDER_WINDOW_FINALIZED);

    fviz_release(widget);
    widget = NULL;
    CHECK(IsWindow(recreated_child) == FALSE);
    DestroyWindow(replacement_host);

    control = NULL;
    CHECK(fviz_win32_render_control_create(
        host, 1001, 4, 6, 320, 240, NULL, (void**)&control) == FVIZ_OK);
    CHECK(control != NULL && IsWindow(control) != FALSE);
    widget = fviz_win32_render_control_renderer_widget(control);
    CHECK(widget != NULL);
    CHECK(fviz_renderer_widget_host_native_handle(widget) == control);
    control_child = (HWND)fviz_renderer_widget_native_handle(widget);
    CHECK(control_child != NULL && GetParent(control_child) == control);
    /* Creating the control with WS_VISIBLE delivers WM_SHOWWINDOW, which
     * already requested a render frame. Consume that initial request so the
     * serial assertions below measure a fresh request_render. */
    CHECK(fviz_win32_render_control_render(control) == FVIZ_OK);
    CHECK(fviz_win32_render_control_render_requested(control) == FVIZ_FALSE);
    request_serial = fviz_render_window_render_request_serial(
        fviz_renderer_widget_window(widget));
    CHECK(fviz_win32_render_control_request_render(control) == FVIZ_OK);
    CHECK(fviz_win32_render_control_render_requested(control) == FVIZ_TRUE);
    CHECK(fviz_render_window_render_request_serial(
        fviz_renderer_widget_window(widget)) == request_serial + 1u);
    CHECK(fviz_win32_render_control_request_render(control) == FVIZ_OK);
    CHECK(fviz_render_window_render_request_serial(
        fviz_renderer_widget_window(widget)) == request_serial + 1u);
    CHECK(fviz_win32_render_control_render(control) == FVIZ_OK);
    CHECK(fviz_win32_render_control_render_requested(control) == FVIZ_FALSE);
    CHECK(fviz_win32_render_control_set_timer_pump(control, FVIZ_FALSE, 0u) == FVIZ_OK);
    CHECK(fviz_win32_render_control_set_timer_pump(control, FVIZ_TRUE, 10u) == FVIZ_OK);
    CHECK(fviz_win32_render_control_set_timer_pump(control, FVIZ_TRUE, 0u) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_win32_render_control_set_bounds(control, 8, 10, 400, 280) == FVIZ_OK);
    CHECK(GetClientRect(control, &client) != FALSE);
    fviz_render_window_get_size(fviz_renderer_widget_window(widget), &width, &height);
    CHECK(width == client.right - client.left);
    CHECK(height == client.bottom - client.top);
    fviz_win32_render_control_destroy(control);
    CHECK(IsWindow(control) == FALSE);
    CHECK(IsWindow(control_child) == FALSE);

    DestroyWindow(host);
    return 0;
#else
    return 0;
#endif
}
