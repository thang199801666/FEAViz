#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <FViz/FViz.h>

static HWND g_control = NULL;
static FVizRendererWidget* g_widget = NULL;
static FVizCubeSource* g_cube = NULL;
static FVizActor* g_actor = NULL;

static void release_scene(void)
{
    fviz_release(g_actor);
    g_actor = NULL;
    fviz_release(g_cube);
    g_cube = NULL;
    g_widget = NULL; /* borrowed from g_control */
}

static FVizResult create_scene(void)
{
    FVizRenderer* renderer;
    FVizPolyData* cube_data;
    FVizResult result;

    g_widget = fviz_win32_render_control_renderer_widget(g_control);
    if (g_widget == NULL) return FVIZ_ERROR_INVALID_STATE;

    result = fviz_cube_source_create(&g_cube);
    if (result != FVIZ_OK) return result;
    result = fviz_cube_source_set_lengths(g_cube, 2.0, 1.4, 1.0);
    if (result != FVIZ_OK) return result;
    result = fviz_cube_source_update(g_cube);
    if (result != FVIZ_OK) return result;
    cube_data = fviz_cube_source_output(g_cube);

    result = fviz_actor_create(&g_actor);
    if (result != FVIZ_OK) return result;
    result = fviz_actor_set_poly_data(g_actor, cube_data);
    if (result != FVIZ_OK) return result;
    fviz_actor_set_color(g_actor, 0.18f, 0.63f, 0.94f);

    renderer = fviz_renderer_widget_renderer(g_widget);
    fviz_renderer_set_background(renderer, 0.055f, 0.065f, 0.085f);
    result = fviz_renderer_widget_add_actor(g_widget, g_actor);
    if (result != FVIZ_OK) return result;
    fviz_renderer_fit_camera(renderer, 1.35f);

    result = fviz_renderer_widget_show(g_widget);
    if (result != FVIZ_OK) return result;
    return fviz_renderer_widget_render(g_widget);
}

static LRESULT CALLBACK main_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        case WM_CREATE:
        {
            if (fviz_win32_render_control_create(
                    hwnd, 1001, 0, 0, 1, 1, NULL, (void**)&g_control) != FVIZ_OK ||
                create_scene() != FVIZ_OK)
            {
                MessageBoxA(hwnd, fviz_last_error_message(), "FEAViz embedding failed", MB_OK | MB_ICONERROR);
                return -1;
            }
            return 0;
        }
        case WM_SIZE:
        {
            const int width = (int)LOWORD(lparam);
            const int height = (int)HIWORD(lparam);
            if (g_control != NULL && width > 0 && height > 0)
                (void)fviz_win32_render_control_set_bounds(g_control, 0, 0, width, height);
            return 0;
        }
        case WM_SETFOCUS:
            if (g_control != NULL) (void)SetFocus(g_control);
            return 0;
        case WM_DESTROY:
            release_scene();
            if (g_control != NULL) fviz_win32_render_control_destroy(g_control);
            g_control = NULL;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show_command)
{
    WNDCLASSA window_class;
    HWND window;
    MSG message;
    (void)previous;
    (void)command_line;

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = main_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.lpszClassName = "FVizWin32EmbedExample";
    if (RegisterClassA(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 1;

    window = CreateWindowExA(
        0,
        window_class.lpszClassName,
        "FEAViz - Embedded in Win32 GUI",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 760,
        NULL, NULL, instance, NULL);
    if (window == NULL) return 2;

    ShowWindow(window, show_command);
    UpdateWindow(window);

    /* The host application owns this loop. The embedded FEAViz child HWND is
     * dispatched naturally; do not call fviz_renderer_widget_start(). */
    while (GetMessageA(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    return (int)message.wParam;
}
