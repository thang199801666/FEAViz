#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <FViz/FViz.h>

#define ID_VIEWPORT          2001
#define ID_BTN_RESET_CAMERA  2101
#define ID_BTN_WIREFRAME     2102
#define ID_BTN_TOGGLE_SPHERE 2103
#define ID_BTN_BACKGROUND    2104

#define TOOLBAR_HEIGHT 54
#define TOOLBAR_MARGIN 8
#define BUTTON_WIDTH   130
#define BUTTON_HEIGHT  32
#define BUTTON_GAP     8

static HWND g_render_control = NULL;
static HWND g_toggle_sphere_button = NULL;

static FVizRendererWidget* g_widget = NULL; /* borrowed from render control */
static FVizCubeSource* g_cube_source = NULL;
static FVizSphereSource* g_sphere_source = NULL;
static FVizActor* g_cube_actor = NULL;
static FVizActor* g_sphere_actor = NULL;

static FVizBool g_wireframe = FVIZ_FALSE;
static FVizBool g_sphere_visible = FVIZ_TRUE;
static FVizBool g_light_background = FVIZ_FALSE;

static void show_fviz_error(HWND owner, const char* operation)
{
    char message[1024];
    const char* detail = fviz_last_error_message();
    if (detail == NULL || detail[0] == '\0') detail = "Unknown FEAViz error";

    wsprintfA(message, "%s failed.\n\n%s", operation, detail);
    MessageBoxA(owner, message, "FEAViz error", MB_OK | MB_ICONERROR);
}

static void request_render(void)
{
    if (g_render_control != NULL)
        (void)fviz_win32_render_control_request_render(g_render_control);
}

static void release_scene(void)
{
    /* The renderer/scene also retains its actors. These release calls only drop
     * the references owned by this demo. The render control is destroyed after
     * this function, which releases the renderer-side references. */
    fviz_release(g_sphere_actor);
    g_sphere_actor = NULL;

    fviz_release(g_cube_actor);
    g_cube_actor = NULL;

    fviz_release(g_sphere_source);
    g_sphere_source = NULL;

    fviz_release(g_cube_source);
    g_cube_source = NULL;

    g_widget = NULL;
}

static FVizResult create_scene(void)
{
    FVizRenderer* renderer;
    FVizResult result;

    g_widget = fviz_win32_render_control_renderer_widget(g_render_control);
    if (g_widget == NULL) return FVIZ_ERROR_INVALID_STATE;

    renderer = fviz_renderer_widget_renderer(g_widget);
    if (renderer == NULL) return FVIZ_ERROR_INVALID_STATE;

    /* ---------------------------------------------------------------------
     * Cube
     * ------------------------------------------------------------------ */
    result = fviz_cube_source_create(&g_cube_source);
    if (result != FVIZ_OK) return result;

    result = fviz_cube_source_set_lengths(g_cube_source, 2.6, 1.7, 1.0);
    if (result != FVIZ_OK) return result;

    result = fviz_cube_source_update(g_cube_source);
    if (result != FVIZ_OK) return result;

    result = fviz_actor_create(&g_cube_actor);
    if (result != FVIZ_OK) return result;

    result = fviz_actor_set_poly_data(g_cube_actor, fviz_cube_source_output(g_cube_source));
    if (result != FVIZ_OK) return result;

    fviz_actor_set_position(g_cube_actor, fviz_vec3(-1.25f, 0.0f, 0.0f));
    fviz_actor_set_color(g_cube_actor, 0.13f, 0.55f, 0.92f);
    fviz_actor_set_edge_visibility(g_cube_actor, FVIZ_TRUE);
    fviz_actor_set_edge_color(g_cube_actor, 0.03f, 0.08f, 0.13f);
    fviz_actor_set_material(g_cube_actor, 0.18f, 0.72f, 0.30f, 42.0f);

    result = fviz_renderer_widget_add_actor(g_widget, g_cube_actor);
    if (result != FVIZ_OK) return result;

    /* ---------------------------------------------------------------------
     * Sphere
     * ------------------------------------------------------------------ */
    result = fviz_sphere_source_create(&g_sphere_source);
    if (result != FVIZ_OK) return result;

    result = fviz_sphere_source_set_radius(g_sphere_source, 0.82);
    if (result != FVIZ_OK) return result;

    result = fviz_sphere_source_set_resolution(g_sphere_source, 48, 32);
    if (result != FVIZ_OK) return result;

    result = fviz_sphere_source_update(g_sphere_source);
    if (result != FVIZ_OK) return result;

    result = fviz_actor_create(&g_sphere_actor);
    if (result != FVIZ_OK) return result;

    result = fviz_actor_set_poly_data(g_sphere_actor, fviz_sphere_source_output(g_sphere_source));
    if (result != FVIZ_OK) return result;

    fviz_actor_set_position(g_sphere_actor, fviz_vec3(1.45f, 0.1f, 0.0f));
    fviz_actor_set_color(g_sphere_actor, 0.94f, 0.52f, 0.15f);
    fviz_actor_set_material(g_sphere_actor, 0.16f, 0.70f, 0.46f, 64.0f);

    result = fviz_renderer_widget_add_actor(g_widget, g_sphere_actor);
    if (result != FVIZ_OK) return result;

    /* ---------------------------------------------------------------------
     * View
     * ------------------------------------------------------------------ */
    fviz_renderer_set_background(renderer, 0.045f, 0.055f, 0.075f);
    fviz_renderer_set_background2(renderer, 0.12f, 0.15f, 0.21f);
    fviz_renderer_set_gradient_background(renderer, FVIZ_TRUE);
    fviz_renderer_fit_camera(renderer, 1.30f);

    result = fviz_renderer_widget_show(g_widget);
    if (result != FVIZ_OK) return result;

    return fviz_renderer_widget_render(g_widget);
}

static HWND create_button(
    HWND parent,
    HINSTANCE instance,
    int id,
    const char* text,
    int x)
{
    return CreateWindowExA(
        0,
        "BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        x,
        TOOLBAR_MARGIN,
        BUTTON_WIDTH,
        BUTTON_HEIGHT,
        parent,
        (HMENU)(INT_PTR)id,
        instance,
        NULL);
}

static void layout_children(HWND hwnd)
{
    RECT client;
    int width;
    int height;
    int viewport_height;

    GetClientRect(hwnd, &client);
    width = client.right - client.left;
    height = client.bottom - client.top;
    viewport_height = height - TOOLBAR_HEIGHT;
    if (viewport_height < 1) viewport_height = 1;

    if (g_render_control != NULL)
    {
        (void)fviz_win32_render_control_set_bounds(
            g_render_control,
            0,
            TOOLBAR_HEIGHT,
            width,
            viewport_height);
    }
}

static void reset_camera(void)
{
    FVizRenderer* renderer;
    if (g_widget == NULL) return;

    renderer = fviz_renderer_widget_renderer(g_widget);
    if (renderer == NULL) return;

    fviz_renderer_fit_camera(renderer, 1.30f);
    request_render();
}

static void toggle_wireframe(void)
{
    g_wireframe = g_wireframe ? FVIZ_FALSE : FVIZ_TRUE;

    if (g_cube_actor != NULL)
        fviz_actor_set_wireframe(g_cube_actor, g_wireframe);
    if (g_sphere_actor != NULL)
        fviz_actor_set_wireframe(g_sphere_actor, g_wireframe);

    request_render();
}

static void toggle_sphere(void)
{
    if (g_sphere_actor == NULL) return;

    g_sphere_visible = g_sphere_visible ? FVIZ_FALSE : FVIZ_TRUE;
    fviz_actor_set_visible(g_sphere_actor, g_sphere_visible);

    if (g_toggle_sphere_button != NULL)
    {
        SetWindowTextA(
            g_toggle_sphere_button,
            g_sphere_visible ? "Hide Sphere" : "Show Sphere");
    }

    request_render();
}

static void toggle_background(void)
{
    FVizRenderer* renderer;
    if (g_widget == NULL) return;

    renderer = fviz_renderer_widget_renderer(g_widget);
    if (renderer == NULL) return;

    g_light_background = g_light_background ? FVIZ_FALSE : FVIZ_TRUE;

    if (g_light_background)
    {
        fviz_renderer_set_background(renderer, 0.72f, 0.77f, 0.84f);
        fviz_renderer_set_background2(renderer, 0.95f, 0.97f, 1.00f);
    }
    else
    {
        fviz_renderer_set_background(renderer, 0.045f, 0.055f, 0.075f);
        fviz_renderer_set_background2(renderer, 0.12f, 0.15f, 0.21f);
    }

    request_render();
}

static LRESULT CALLBACK main_window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{
    switch (message)
    {
        case WM_CREATE:
        {
            CREATESTRUCTA* create = (CREATESTRUCTA*)lparam;
            HINSTANCE instance = (HINSTANCE)create->hInstance;
            int x = TOOLBAR_MARGIN;

            if (create_button(hwnd, instance, ID_BTN_RESET_CAMERA, "Reset Camera", x) == NULL)
                return -1;
            x += BUTTON_WIDTH + BUTTON_GAP;

            if (create_button(hwnd, instance, ID_BTN_WIREFRAME, "Wireframe", x) == NULL)
                return -1;
            x += BUTTON_WIDTH + BUTTON_GAP;

            g_toggle_sphere_button =
                create_button(hwnd, instance, ID_BTN_TOGGLE_SPHERE, "Hide Sphere", x);
            if (g_toggle_sphere_button == NULL) return -1;
            x += BUTTON_WIDTH + BUTTON_GAP;

            if (create_button(hwnd, instance, ID_BTN_BACKGROUND, "Background", x) == NULL)
                return -1;

            if (fviz_win32_render_control_create(
                    hwnd,
                    ID_VIEWPORT,
                    0,
                    TOOLBAR_HEIGHT,
                    1,
                    1,
                    NULL,
                    (void**)&g_render_control) != FVIZ_OK)
            {
                show_fviz_error(hwnd, "Creating FEAViz render control");
                return -1;
            }

            if (create_scene() != FVIZ_OK)
            {
                show_fviz_error(hwnd, "Creating 3D scene");
                return -1;
            }

            layout_children(hwnd);
            return 0;
        }

        case WM_SIZE:
            layout_children(hwnd);
            return 0;

        case WM_COMMAND:
            if (HIWORD(wparam) == BN_CLICKED)
            {
                switch (LOWORD(wparam))
                {
                    case ID_BTN_RESET_CAMERA:
                        reset_camera();
                        return 0;
                    case ID_BTN_WIREFRAME:
                        toggle_wireframe();
                        return 0;
                    case ID_BTN_TOGGLE_SPHERE:
                        toggle_sphere();
                        return 0;
                    case ID_BTN_BACKGROUND:
                        toggle_background();
                        return 0;
                    default:
                        break;
                }
            }
            break;

        case WM_SETFOCUS:
            /* Give keyboard interaction back to the embedded viewport. */
            if (g_render_control != NULL)
                SetFocus(g_render_control);
            return 0;

        case WM_DESTROY:
            release_scene();

            if (g_render_control != NULL)
            {
                fviz_win32_render_control_destroy(g_render_control);
                g_render_control = NULL;
            }

            PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE previous_instance,
    LPSTR command_line,
    int show_command)
{
    WNDCLASSA window_class;
    HWND window;
    MSG message;

    (void)previous_instance;
    (void)command_line;

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = main_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    window_class.lpszClassName = "FVizSimpleGuiExample";

    if (RegisterClassA(&window_class) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return 1;
    }

    window = CreateWindowExA(
        0,
        window_class.lpszClassName,
        "FEAViz v0.34 - Simple Win32 GUI",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1180,
        780,
        NULL,
        NULL,
        instance,
        NULL);

    if (window == NULL) return 2;

    ShowWindow(window, show_command);
    UpdateWindow(window);

    /* Win32 owns the application message loop. FEAViz is just another child
     * control; do not call fviz_renderer_widget_start() here. */
    while (GetMessageA(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    return (int)message.wParam;
}
