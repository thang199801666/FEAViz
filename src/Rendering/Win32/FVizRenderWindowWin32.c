#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>

#include <limits.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRenderWindowPrivate.h>

#define FVIZ_WINDOW_CLASS_NAME "FEAVizRenderWindowClass"

static LRESULT CALLBACK fviz_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

static FVizResult fviz_win32_register_class(void)
{
    static ATOM class_atom = 0;
    WNDCLASSA window_class;
    if (class_atom != 0) return FVIZ_OK;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = fviz_window_proc;
    window_class.hInstance = GetModuleHandleA(NULL);
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.lpszClassName = FVIZ_WINDOW_CLASS_NAME;
    class_atom = RegisterClassA(&window_class);
    if (class_atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to register FEAViz Win32 window class");
        return FVIZ_ERROR_GRAPHICS;
    }
    return FVIZ_OK;
}

static FVizResult fviz_win32_create_gl_context(FVizRenderWindow* window, HWND hwnd)
{
    PIXELFORMATDESCRIPTOR pfd;
    HDC dc;
    HGLRC context;
    int pixel_format;

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = (WORD)sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    dc = GetDC(hwnd);
    if (dc == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "GetDC failed for FEAViz render window");
        return FVIZ_ERROR_GRAPHICS;
    }
    pixel_format = ChoosePixelFormat(dc, &pfd);
    if (pixel_format == 0 || SetPixelFormat(dc, pixel_format, &pfd) == FALSE)
    {
        ReleaseDC(hwnd, dc);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to configure OpenGL pixel format");
        return FVIZ_ERROR_GRAPHICS;
    }
    context = wglCreateContext(dc);
    if (context == NULL || wglMakeCurrent(dc, context) == FALSE)
    {
        if (context != NULL) wglDeleteContext(context);
        ReleaseDC(hwnd, dc);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to create OpenGL rendering context");
        return FVIZ_ERROR_GRAPHICS;
    }
    window->native_dc = dc;
    window->native_gl_context = context;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_create_platform(FVizRenderWindow* window)
{
    RECT rect;
    HWND hwnd;
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (fviz_win32_register_class() != FVIZ_OK) return fviz_last_error_code();
    rect.left = 0; rect.top = 0; rect.right = window->width; rect.bottom = window->height;
    (void)AdjustWindowRect(&rect, style, FALSE);
    hwnd = CreateWindowExA(
        0,
        FVIZ_WINDOW_CLASS_NAME,
        window->title,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        GetModuleHandleA(NULL),
        window);
    if (hwnd == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to create FEAViz Win32 render window");
        return FVIZ_ERROR_GRAPHICS;
    }
    window->native_window = hwnd;
    if (fviz_win32_create_gl_context(window, hwnd) != FVIZ_OK)
    {
        DestroyWindow(hwnd);
        window->native_window = NULL;
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_show_platform(FVizRenderWindow* window)
{
    HWND hwnd = (HWND)window->native_window;
    if (hwnd == NULL) return FVIZ_ERROR_INVALID_STATE;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    window->visible = FVIZ_TRUE;
    return FVIZ_OK;
}

static void fviz_win32_render_actor(const FVizActor* actor)
{
    const FVizPolyData* data;
    const FVizVec3* points;
    const FVizVec3* normals;
    const uint32_t* indices;
    FVizSize index_count;
    float red;
    float green;
    float blue;

    if (actor == NULL || fviz_actor_is_visible(actor) == FVIZ_FALSE) return;
    data = fviz_actor_const_poly_data(actor);
    if (data == NULL || fviz_poly_data_triangle_count(data) == 0u) return;
    points = fviz_poly_data_points(data);
    normals = fviz_poly_data_normals(data);
    indices = fviz_poly_data_triangle_indices(data);
    index_count = fviz_poly_data_triangle_count(data) * 3u;
    if (points == NULL || indices == NULL || index_count > (FVizSize)INT_MAX) return;

    fviz_actor_get_color(actor, &red, &green, &blue);
    glColor3f(red, green, blue);
    glPolygonMode(GL_FRONT_AND_BACK, fviz_actor_wireframe(actor) == FVIZ_TRUE ? GL_LINE : GL_FILL);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, (GLsizei)sizeof(FVizVec3), points);
    if (normals != NULL)
    {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, (GLsizei)sizeof(FVizVec3), normals);
    }
    glDrawElements(GL_TRIANGLES, (GLsizei)index_count, GL_UNSIGNED_INT, indices);
    if (normals != NULL) glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

FVizResult fviz_internal_render_window_render_platform(FVizRenderWindow* window)
{
    HDC dc = (HDC)window->native_dc;
    HGLRC context = (HGLRC)window->native_gl_context;
    FVizRenderer* renderer = window->renderer;
    FVizCamera* camera;
    FVizScene* scene;
    FVizMat4 projection;
    FVizMat4 view;
    FVizSize i;
    float background[3];
    GLfloat light_position[4] = {0.4f, 0.8f, 1.0f, 0.0f};
    GLfloat light_diffuse[4] = {0.95f, 0.95f, 0.95f, 1.0f};
    GLfloat light_ambient[4] = {0.22f, 0.22f, 0.25f, 1.0f};

    if (dc == NULL || context == NULL || renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "render window has no valid OpenGL context or renderer");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (wglMakeCurrent(dc, context) == FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to activate FEAViz OpenGL context");
        return FVIZ_ERROR_GRAPHICS;
    }

    if (window->height <= 0) window->height = 1;
    glViewport(0, 0, window->width, window->height);
    fviz_renderer_get_background(renderer, &background[0], &background[1], &background[2]);
    glClearColor(background[0], background[1], background[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera = fviz_renderer_camera(renderer);
    projection = fviz_camera_projection_matrix(camera, (float)window->width / (float)window->height);
    view = fviz_camera_view_matrix(camera);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection.m);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.m);

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);

    scene = fviz_renderer_scene(renderer);
    for (i = 0u; scene != NULL && i < fviz_scene_actor_count(scene); ++i)
    {
        fviz_win32_render_actor(fviz_scene_const_actor(scene, i));
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if (SwapBuffers(dc) == FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "SwapBuffers failed");
        return FVIZ_ERROR_GRAPHICS;
    }
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_run_platform(FVizRenderWindow* window)
{
    MSG message;
    if (window->visible == FVIZ_FALSE)
    {
        FVizResult show_result = fviz_internal_render_window_show_platform(window);
        if (show_result != FVIZ_OK) return show_result;
    }
    (void)fviz_internal_render_window_render_platform(window);
    while (window->close_requested == FVIZ_FALSE && GetMessageA(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    return FVIZ_OK;
}

static void fviz_win32_release_gl_context(FVizRenderWindow* window, HWND hwnd)
{
    HGLRC context = (HGLRC)window->native_gl_context;
    HDC dc = (HDC)window->native_dc;
    if (context != NULL)
    {
        if (wglGetCurrentContext() == context) (void)wglMakeCurrent(NULL, NULL);
        (void)wglDeleteContext(context);
    }
    if (dc != NULL && hwnd != NULL) (void)ReleaseDC(hwnd, dc);
    window->native_gl_context = NULL;
    window->native_dc = NULL;
}

void fviz_internal_render_window_destroy_platform(FVizRenderWindow* window)
{
    HWND hwnd = (HWND)window->native_window;
    fviz_win32_release_gl_context(window, hwnd);
    if (hwnd != NULL && IsWindow(hwnd) != FALSE) (void)DestroyWindow(hwnd);
    window->native_window = NULL;
}

void fviz_internal_render_window_request_close_platform(FVizRenderWindow* window)
{
    HWND hwnd = (HWND)window->native_window;
    if (hwnd != NULL) PostMessageA(hwnd, WM_CLOSE, 0, 0);
}

FVizBool fviz_internal_render_window_supported_platform(void) { return FVIZ_TRUE; }

static void fviz_win32_toggle_wireframe(FVizRenderWindow* window)
{
    FVizScene* scene = fviz_renderer_scene(window->renderer);
    FVizSize i;
    for (i = 0u; scene != NULL && i < fviz_scene_actor_count(scene); ++i)
    {
        FVizActor* actor = fviz_scene_actor(scene, i);
        fviz_actor_set_wireframe(actor, fviz_actor_wireframe(actor) == FVIZ_TRUE ? FVIZ_FALSE : FVIZ_TRUE);
    }
}

static LRESULT CALLBACK fviz_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    FVizRenderWindow* window = (FVizRenderWindow*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCTA* create = (const CREATESTRUCTA*)lparam;
        window = (FVizRenderWindow*)create->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    }
    if (window == NULL) return DefWindowProcA(hwnd, message, wparam, lparam);

    switch (message)
    {
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            window->width = (int)LOWORD(lparam);
            window->height = (int)HIWORD(lparam);
            if (window->width > 0 && window->height > 0) (void)fviz_internal_render_window_render_platform(window);
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            BeginPaint(hwnd, &paint);
            (void)fviz_internal_render_window_render_platform(window);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_LBUTTONDOWN:
            window->left_mouse_down = FVIZ_TRUE;
            window->last_mouse_x = GET_X_LPARAM(lparam);
            window->last_mouse_y = GET_Y_LPARAM(lparam);
            SetCapture(hwnd);
            return 0;
        case WM_MBUTTONDOWN:
            window->middle_mouse_down = FVIZ_TRUE;
            window->last_mouse_x = GET_X_LPARAM(lparam);
            window->last_mouse_y = GET_Y_LPARAM(lparam);
            SetCapture(hwnd);
            return 0;
        case WM_LBUTTONUP:
            window->left_mouse_down = FVIZ_FALSE;
            if (window->middle_mouse_down == FVIZ_FALSE) ReleaseCapture();
            return 0;
        case WM_MBUTTONUP:
            window->middle_mouse_down = FVIZ_FALSE;
            if (window->left_mouse_down == FVIZ_FALSE) ReleaseCapture();
            return 0;
        case WM_MOUSEMOVE:
        {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            const int dx = x - window->last_mouse_x;
            const int dy = y - window->last_mouse_y;
            FVizCamera* camera = fviz_renderer_camera(window->renderer);
            if (window->left_mouse_down == FVIZ_TRUE)
            {
                fviz_camera_orbit(camera, -(float)dx * 0.008f, -(float)dy * 0.008f);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            else if (window->middle_mouse_down == FVIZ_TRUE)
            {
                const float distance = fviz_vec3_length(fviz_vec3_sub(fviz_camera_position(camera), fviz_camera_target(camera)));
                const float scale = distance * 0.0015f;
                fviz_camera_pan(camera, -(float)dx * scale, (float)dy * scale);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            window->last_mouse_x = x;
            window->last_mouse_y = y;
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
            FVizCamera* camera = fviz_renderer_camera(window->renderer);
            fviz_camera_dolly(camera, delta > 0 ? 0.85f : 1.18f);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE)
            {
                PostMessageA(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (wparam == 'F')
            {
                fviz_renderer_fit_camera(window->renderer, 1.2f);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (wparam == 'W')
            {
                fviz_win32_toggle_wireframe(window);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        case WM_CLOSE:
            window->close_requested = FVIZ_TRUE;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            fviz_win32_release_gl_context(window, hwnd);
            window->native_window = NULL;
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}
