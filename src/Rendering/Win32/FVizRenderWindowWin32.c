#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Interaction/FVizRenderWindowInteractor.h>
#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizGL.h>
#include <FViz/Rendering/FVizGLDevice.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizScalarLegend.h>
#include <FViz/Rendering/FVizRenderWindowPrivate.h>

#define FVIZ_WINDOW_CLASS_NAME "FEAVizRenderWindowClass"

/* WGL extension entry points and constants (wglext.h is not shipped with the SDK). */
typedef BOOL(WINAPI* PFNFVIZWGLCHOOSEPIXELFORMATARBPROC)(
    HDC dc,
    const int* int_attrib_list,
    const FLOAT* float_attrib_list,
    UINT max_formats,
    int* out_pixel_format,
    UINT* out_num_formats);
typedef HGLRC(WINAPI* PFNFVIZWGLCREATECONTEXTATTRIBSARBPROC)(
    HDC dc,
    HGLRC share_context,
    const int* attrib_list);

#define FVIZ_WGL_DRAW_TO_WINDOW_ARB 0x2001
#define FVIZ_WGL_ACCELERATION_ARB 0x2003
#define FVIZ_WGL_SUPPORT_OPENGL_ARB 0x2010
#define FVIZ_WGL_DOUBLE_BUFFER_ARB 0x2011
#define FVIZ_WGL_COLOR_BITS_ARB 0x2014
#define FVIZ_WGL_DEPTH_BITS_ARB 0x2022
#define FVIZ_WGL_STENCIL_BITS_ARB 0x2023
#define FVIZ_WGL_FULL_ACCELERATION_ARB 0x2027
#define FVIZ_WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define FVIZ_WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define FVIZ_WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define FVIZ_WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

static PFNFVIZWGLCHOOSEPIXELFORMATARBPROC fviz_wgl_choose_pixel_format_arb = NULL;
static PFNFVIZWGLCREATECONTEXTATTRIBSARBPROC fviz_wgl_create_context_attribs_arb = NULL;

static LRESULT CALLBACK fviz_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
static void fviz_win32_release_gl_context(FVizRenderWindow* window, HWND hwnd);
static void fviz_win32_render_legacy_scene(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    int viewport_width,
    int viewport_height);

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

static HWND fviz_win32_create_probe_window(void)
{
    return CreateWindowExA(
        0,
        FVIZ_WINDOW_CLASS_NAME,
        "FEAVizGLCtxProbe",
        WS_OVERLAPPED,
        0,
        0,
        8,
        8,
        NULL,
        NULL,
        GetModuleHandleA(NULL),
        NULL);
}

static FVizResult fviz_win32_set_legacy_pixel_format(HDC dc)
{
    PIXELFORMATDESCRIPTOR pfd;
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
    pixel_format = ChoosePixelFormat(dc, &pfd);
    if (pixel_format == 0 || SetPixelFormat(dc, pixel_format, &pfd) == FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to configure OpenGL pixel format");
        return FVIZ_ERROR_GRAPHICS;
    }
    return FVIZ_OK;
}

static FVizBool fviz_win32_load_wgl_extensions(void)
{
    if (fviz_wgl_choose_pixel_format_arb == NULL)
    {
        fviz_wgl_choose_pixel_format_arb = (PFNFVIZWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
    }
    if (fviz_wgl_create_context_attribs_arb == NULL)
    {
        fviz_wgl_create_context_attribs_arb = (PFNFVIZWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    }
    return fviz_wgl_choose_pixel_format_arb != NULL && fviz_wgl_create_context_attribs_arb != NULL
        ? FVIZ_TRUE
        : FVIZ_FALSE;
}

static void fviz_win32_apply_gl_state(FVizRenderWindow* window)
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    if (window->gl_modern == FVIZ_FALSE)
    {
        glEnable(GL_NORMALIZE);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glShadeModel(GL_SMOOTH);
    }
}

static FVizResult fviz_win32_create_gl_context(FVizRenderWindow* window, HWND hwnd)
{
    HWND probe_window = NULL;
    HDC probe_dc = NULL;
    HGLRC probe_context = NULL;
    HDC dc = NULL;
    HGLRC context = NULL;
    FVizBool modern = FVIZ_FALSE;

    probe_window = fviz_win32_create_probe_window();
    if (probe_window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to create FEAViz GL probe window");
        return FVIZ_ERROR_GRAPHICS;
    }
    probe_dc = GetDC(probe_window);
    if (probe_dc == NULL)
    {
        (void)DestroyWindow(probe_window);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "GetDC failed for FEAViz GL probe window");
        return FVIZ_ERROR_GRAPHICS;
    }
    if (fviz_win32_set_legacy_pixel_format(probe_dc) != FVIZ_OK)
    {
        (void)ReleaseDC(probe_window, probe_dc);
        (void)DestroyWindow(probe_window);
        return fviz_last_error_code();
    }
    probe_context = wglCreateContext(probe_dc);
    if (probe_context == NULL || wglMakeCurrent(probe_dc, probe_context) == FALSE)
    {
        if (probe_context != NULL) (void)wglDeleteContext(probe_context);
        (void)ReleaseDC(probe_window, probe_dc);
        (void)DestroyWindow(probe_window);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to create FEAViz OpenGL probe context");
        return FVIZ_ERROR_GRAPHICS;
    }

    if (fviz_win32_load_wgl_extensions() == FVIZ_TRUE)
    {
        const int pixel_attribs[] = {
            FVIZ_WGL_DRAW_TO_WINDOW_ARB, TRUE,
            FVIZ_WGL_SUPPORT_OPENGL_ARB, TRUE,
            FVIZ_WGL_DOUBLE_BUFFER_ARB, TRUE,
            FVIZ_WGL_COLOR_BITS_ARB, 32,
            FVIZ_WGL_DEPTH_BITS_ARB, 24,
            FVIZ_WGL_STENCIL_BITS_ARB, 8,
            FVIZ_WGL_ACCELERATION_ARB, FVIZ_WGL_FULL_ACCELERATION_ARB,
            0
        };
        const int context_attribs[] = {
            FVIZ_WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            FVIZ_WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            FVIZ_WGL_CONTEXT_PROFILE_MASK_ARB, FVIZ_WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        int modern_format = 0;
        UINT num_formats = 0;
        dc = GetDC(hwnd);
        if (dc != NULL &&
            fviz_wgl_choose_pixel_format_arb(dc, pixel_attribs, NULL, 1, &modern_format, &num_formats) != FALSE &&
            num_formats > 0)
        {
            PIXELFORMATDESCRIPTOR modern_pfd;
            if (DescribePixelFormat(dc, modern_format, sizeof(modern_pfd), &modern_pfd) != 0 &&
                SetPixelFormat(dc, modern_format, &modern_pfd) != FALSE)
            {
                HGLRC modern_context = fviz_wgl_create_context_attribs_arb(dc, NULL, context_attribs);
                if (modern_context != NULL)
                {
                    if (wglMakeCurrent(dc, modern_context) != FALSE)
                    {
                        context = modern_context;
                        modern = FVIZ_TRUE;
                    }
                    else
                    {
                        (void)wglDeleteContext(modern_context);
                    }
                }
            }
        }
    }

    if (context == NULL)
    {
        if (dc == NULL) dc = GetDC(hwnd);
        if (dc != NULL && fviz_win32_set_legacy_pixel_format(dc) == FVIZ_OK)
        {
            context = wglCreateContext(dc);
            if (context == NULL || wglMakeCurrent(dc, context) == FALSE)
            {
                if (context != NULL)
                {
                    (void)wglDeleteContext(context);
                    context = NULL;
                }
            }
        }
    }

    if (probe_context != NULL)
    {
        if (wglGetCurrentContext() == probe_context) (void)wglMakeCurrent(NULL, NULL);
        (void)wglDeleteContext(probe_context);
    }
    if (probe_dc != NULL) (void)ReleaseDC(probe_window, probe_dc);
    if (probe_window != NULL) (void)DestroyWindow(probe_window);

    if (context == NULL || dc == NULL)
    {
        if (dc != NULL && hwnd != NULL) (void)ReleaseDC(hwnd, dc);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to create OpenGL rendering context");
        return FVIZ_ERROR_GRAPHICS;
    }

    window->native_dc = dc;
    window->native_gl_context = context;
    window->gl_modern = modern;

    if (modern == FVIZ_TRUE)
    {
        FVizGLFunctions functions;
        if (fviz_internal_gl_load(&functions) != FVIZ_OK)
        {
            fviz_win32_release_gl_context(window, hwnd);
            return fviz_last_error_code();
        }
        window->gl_device = fviz_internal_gl_device_create(&functions);
        if (window->gl_device == NULL)
        {
            fviz_win32_release_gl_context(window, hwnd);
            return fviz_last_error_code();
        }
    }
    fviz_win32_apply_gl_state(window);
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_create_platform(FVizRenderWindow* window)
{
    RECT rect;
    HWND hwnd;
    DWORD style = window->host_native_handle != NULL ? WS_CHILD | WS_VISIBLE : WS_OVERLAPPEDWINDOW;
    if (fviz_win32_register_class() != FVIZ_OK) return fviz_last_error_code();
    rect.left = 0; rect.top = 0; rect.right = window->width; rect.bottom = window->height;
    (void)AdjustWindowRect(&rect, style, FALSE);
    hwnd = CreateWindowExA(
        0,
        FVIZ_WINDOW_CLASS_NAME,
        window->title,
        style,
        window->host_native_handle != NULL ? 0 : CW_USEDEFAULT,
        window->host_native_handle != NULL ? 0 : CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        (HWND)window->host_native_handle,
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
    FVizSize renderer_count = fviz_render_window_renderer_count(window);
    FVizSize rendered_count = 0u;
    int last_layer = -1;

    if (dc == NULL || context == NULL || renderer_count == 0u)
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
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);

    while (rendered_count < renderer_count)
    {
        FVizSize i;
        int current_layer = INT_MAX;
        for (i = 0u; i < renderer_count; ++i)
        {
            FVizRenderer* renderer = fviz_render_window_renderer_at(window, i);
            const int layer = fviz_renderer_layer(renderer);
            if (layer > last_layer && layer < current_layer) current_layer = layer;
        }
        if (current_layer == INT_MAX && last_layer == INT_MAX) break;
        for (i = 0u; i < renderer_count; ++i)
        {
            FVizRenderer* renderer = fviz_render_window_renderer_at(window, i);
            float viewport[4];
            float background[3];
            int viewport_x;
            int viewport_y;
            int viewport_width;
            int viewport_height;
            FVizSize pass_index;
            FVizRenderPassContext pass_context;
            if (fviz_renderer_layer(renderer) != current_layer) continue;
            if (fviz_renderer_update(renderer) != FVIZ_OK)
            {
                glDisable(GL_SCISSOR_TEST);
                return fviz_last_error_code();
            }
            fviz_renderer_get_viewport(
                renderer, &viewport[0], &viewport[1], &viewport[2], &viewport[3]);
            viewport_x = (int)(viewport[0] * (float)window->width);
            viewport_y = (int)(viewport[1] * (float)window->height);
            viewport_width = (int)((viewport[2] - viewport[0]) * (float)window->width);
            viewport_height = (int)((viewport[3] - viewport[1]) * (float)window->height);
            if (viewport_width < 1) viewport_width = 1;
            if (viewport_height < 1) viewport_height = 1;
            glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
            glScissor(viewport_x, viewport_y, viewport_width, viewport_height);
            (void)memset(&pass_context, 0, sizeof(pass_context));
            pass_context.struct_size = (uint32_t)sizeof(pass_context);
            pass_context.viewport_x = viewport_x;
            pass_context.viewport_y = viewport_y;
            pass_context.viewport_width = viewport_width;
            pass_context.viewport_height = viewport_height;
            pass_context.aspect_ratio = (float)viewport_width / (float)viewport_height;
            pass_context.backend_context = window;
            for (pass_index = 0u; pass_index < fviz_renderer_pass_count(renderer); ++pass_index)
            {
                FVizRenderPass* pass = fviz_renderer_pass_at(renderer, pass_index);
                FVizResult pass_result = FVIZ_OK;
                if (fviz_render_pass_is_custom(pass) != FVIZ_FALSE)
                {
                    pass_result = fviz_render_pass_execute(pass, renderer, &pass_context);
                }
                else
                {
                    switch (fviz_render_pass_stage(pass))
                    {
                        case FVIZ_RENDER_PASS_CLEAR:
                            if (current_layer == 0)
                            {
                                fviz_renderer_get_background(
                                    renderer, &background[0], &background[1], &background[2]);
                                glClearColor(background[0], background[1], background[2], 1.0f);
                                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                            }
                            break;
                        case FVIZ_RENDER_PASS_OPAQUE:
                            if (window->gl_modern == FVIZ_TRUE && window->gl_device != NULL)
                                pass_result = fviz_internal_gl_device_render_stage(
                                    (FVizGLDevice*)window->gl_device,
                                    renderer,
                                    pass_context.aspect_ratio,
                                    FVIZ_RENDER_PASS_OPAQUE);
                            else
                                fviz_win32_render_legacy_scene(
                                    window, renderer, viewport_width, viewport_height);
                            break;
                        case FVIZ_RENDER_PASS_TRANSLUCENT:
                        case FVIZ_RENDER_PASS_EDGE:
                            if (window->gl_modern == FVIZ_TRUE && window->gl_device != NULL)
                                pass_result = fviz_internal_gl_device_render_stage(
                                    (FVizGLDevice*)window->gl_device,
                                    renderer,
                                    pass_context.aspect_ratio,
                                    fviz_render_pass_stage(pass));
                            break;
                        case FVIZ_RENDER_PASS_OVERLAY:
                        {
                            FVizScalarLegend* legend = fviz_renderer_scalar_legend(renderer);
                            if (window->gl_modern == FVIZ_TRUE && window->gl_device != NULL && legend != NULL)
                                pass_result = fviz_internal_gl_device_render_legend(
                                    (FVizGLDevice*)window->gl_device,
                                    legend,
                                    viewport_width,
                                    viewport_height);
                            break;
                        }
                        case FVIZ_RENDER_PASS_SELECTION:
                        default:
                            break;
                    }
                }
                if (pass_result != FVIZ_OK)
                {
                    glDisable(GL_SCISSOR_TEST);
                    return pass_result;
                }
            }
            ++rendered_count;
        }
        if (current_layer == INT_MAX) break;
        last_layer = current_layer;
    }
    glDisable(GL_SCISSOR_TEST);

    if (SwapBuffers(dc) == FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "SwapBuffers failed");
        return FVIZ_ERROR_GRAPHICS;
    }
    return FVIZ_OK;
}

static void fviz_win32_render_legacy_scene(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    int viewport_width,
    int viewport_height)
{
    FVizCamera* camera;
    FVizScene* scene;
    FVizMat4 projection;
    FVizMat4 view;
    FVizSize i;
    GLfloat light_position[4] = {0.4f, 0.8f, 1.0f, 0.0f};
    GLfloat light_diffuse[4] = {0.95f, 0.95f, 0.95f, 1.0f};
    GLfloat light_ambient[4] = {0.22f, 0.22f, 0.25f, 1.0f};

    camera = fviz_renderer_camera(renderer);
    (void)window;
    projection = fviz_camera_projection_matrix(
        camera, (float)viewport_width / (float)viewport_height);
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
    if (window->gl_device != NULL)
    {
        if (dc != NULL && context != NULL) (void)wglMakeCurrent(dc, context);
        fviz_internal_gl_device_destroy((FVizGLDevice*)window->gl_device);
        window->gl_device = NULL;
    }
    if (context != NULL)
    {
        if (wglGetCurrentContext() == context) (void)wglMakeCurrent(NULL, NULL);
        (void)wglDeleteContext(context);
    }
    if (dc != NULL && hwnd != NULL) (void)ReleaseDC(hwnd, dc);
    window->native_gl_context = NULL;
    window->native_dc = NULL;
    window->gl_modern = FVIZ_FALSE;
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

static FVizInteractionEvent fviz_win32_interaction_event(
    FVizInteractionEventType type,
    FVizMouseButton button,
    int x,
    int y)
{
    FVizInteractionEvent event;
    ZeroMemory(&event, sizeof(event));
    event.type = type;
    event.button = button;
    event.x = x;
    event.y = y;
    event.timestamp_seconds = (double)GetTickCount64() / 1000.0;
    event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? FVIZ_TRUE : FVIZ_FALSE;
    event.control = (GetKeyState(VK_CONTROL) & 0x8000) != 0 ? FVIZ_TRUE : FVIZ_FALSE;
    event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0 ? FVIZ_TRUE : FVIZ_FALSE;
    return event;
}

FVizResult fviz_internal_render_window_process_events_platform(FVizRenderWindow* window)
{
    MSG message;
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    while (PeekMessageA(&message, NULL, 0u, 0u, PM_REMOVE) != FALSE)
    {
        if (message.message == WM_QUIT) window->close_requested = FVIZ_TRUE;
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    (void)fviz_render_window_interactor_process_timers(
        window->interactor, (double)GetTickCount64() / 1000.0);
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_resize_platform(FVizRenderWindow* window)
{
    RECT rect;
    DWORD style;
    HWND hwnd;
    if (window == NULL || window->native_window == NULL) return FVIZ_ERROR_INVALID_STATE;
    hwnd = (HWND)window->native_window;
    style = (DWORD)GetWindowLongPtrA(hwnd, GWL_STYLE);
    rect.left = 0;
    rect.top = 0;
    rect.right = window->width;
    rect.bottom = window->height;
    (void)AdjustWindowRect(&rect, style, FALSE);
    if (SetWindowPos(
            hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) == FALSE)
        return FVIZ_ERROR_GRAPHICS;
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_read_rgba8_platform(
    FVizRenderWindow* window,
    uint8_t* pixels)
{
    if (window == NULL || pixels == NULL || window->native_dc == NULL ||
        window->native_gl_context == NULL)
        return FVIZ_ERROR_INVALID_STATE;
    if (wglMakeCurrent(
            (HDC)window->native_dc, (HGLRC)window->native_gl_context) == FALSE)
        return FVIZ_ERROR_GRAPHICS;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, window->width, window->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

FVizResult fviz_internal_render_window_read_depth_f32_platform(
    FVizRenderWindow* window,
    float* depth)
{
    if (window == NULL || depth == NULL || window->native_dc == NULL ||
        window->native_gl_context == NULL)
        return FVIZ_ERROR_INVALID_STATE;
    if (wglMakeCurrent(
            (HDC)window->native_dc, (HGLRC)window->native_gl_context) == FALSE)
        return FVIZ_ERROR_GRAPHICS;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, window->width, window->height, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
    return glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

FVizResult fviz_internal_render_window_hardware_pick_platform(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    int x,
    int y,
    FVizSize* out_actor_index,
    FVizSize* out_primitive_id,
    float* out_depth)
{
    float viewport[4];
    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
    FVizResult result;
    if (window == NULL || renderer == NULL || out_actor_index == NULL ||
        out_primitive_id == NULL || out_depth == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (window->gl_modern == FVIZ_FALSE || window->gl_device == NULL)
        return FVIZ_ERROR_NOT_SUPPORTED;
    if (wglMakeCurrent(
            (HDC)window->native_dc, (HGLRC)window->native_gl_context) == FALSE)
        return FVIZ_ERROR_GRAPHICS;
    fviz_renderer_get_viewport(
        renderer, &viewport[0], &viewport[1], &viewport[2], &viewport[3]);
    viewport_x = (int)(viewport[0] * (float)window->width);
    viewport_y = (int)(viewport[1] * (float)window->height);
    viewport_width = (int)((viewport[2] - viewport[0]) * (float)window->width);
    viewport_height = (int)((viewport[3] - viewport[1]) * (float)window->height);
    if (viewport_width < 1 || viewport_height < 1) return FVIZ_ERROR_NOT_FOUND;
    glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(viewport_x, viewport_y, viewport_width, viewport_height);
    result = fviz_internal_gl_device_select(
        (FVizGLDevice*)window->gl_device,
        renderer,
        (float)viewport_width / (float)viewport_height,
        x,
        window->height - y - 1,
        out_actor_index,
        out_primitive_id,
        out_depth);
    glDisable(GL_SCISSOR_TEST);
    (void)fviz_internal_render_window_render_platform(window);
    return result;
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
        {
            FVizInteractionEvent event;
            window->width = (int)LOWORD(lparam);
            window->height = (int)HIWORD(lparam);
            event = fviz_win32_interaction_event(FVIZ_INTERACTION_RESIZE, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            event.width = window->width;
            event.height = window->height;
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            if (window->width > 0 && window->height > 0) (void)fviz_internal_render_window_render_platform(window);
            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_EXPOSE, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            BeginPaint(hwnd, &paint);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            (void)fviz_internal_render_window_render_platform(window);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_SETFOCUS:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_FOCUS_IN, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            return 0;
        }
        case WM_KILLFOCUS:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_FOCUS_OUT, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            FVizInteractionEvent event;
            window->left_mouse_down = FVIZ_TRUE;
            window->left_mouse_dragged = FVIZ_FALSE;
            window->last_mouse_x = GET_X_LPARAM(lparam);
            window->last_mouse_y = GET_Y_LPARAM(lparam);
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_LEFT,
                window->last_mouse_x, window->last_mouse_y);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            SetCapture(hwnd);
            return 0;
        }
        case WM_MBUTTONDOWN:
        {
            FVizInteractionEvent event;
            window->middle_mouse_down = FVIZ_TRUE;
            window->last_mouse_x = GET_X_LPARAM(lparam);
            window->last_mouse_y = GET_Y_LPARAM(lparam);
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_MIDDLE,
                window->last_mouse_x, window->last_mouse_y);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            SetCapture(hwnd);
            return 0;
        }
        case WM_RBUTTONDOWN:
        {
            FVizInteractionEvent event;
            window->right_mouse_down = FVIZ_TRUE;
            window->last_mouse_x = GET_X_LPARAM(lparam);
            window->last_mouse_y = GET_Y_LPARAM(lparam);
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_RIGHT,
                window->last_mouse_x, window->last_mouse_y);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            SetCapture(hwnd);
            return 0;
        }
        case WM_LBUTTONUP:
        {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_LEFT, x, y);
            window->left_mouse_down = FVIZ_FALSE;
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            if (window->left_mouse_dragged == FVIZ_FALSE)
            {
                if (window->pick_callback != NULL)
                {
                    FVizRayHit hit;
                    if (fviz_render_window_pick(window, x, y, &hit) == FVIZ_OK)
                    {
                        window->pick_callback(window, x, y, &hit, window->pick_user_data);
                    }
                }
            }
            if (window->middle_mouse_down == FVIZ_FALSE && window->right_mouse_down == FVIZ_FALSE) ReleaseCapture();
            return 0;
        }
        case WM_MBUTTONUP:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_MIDDLE,
                GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            window->middle_mouse_down = FVIZ_FALSE;
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            if (window->left_mouse_down == FVIZ_FALSE && window->right_mouse_down == FVIZ_FALSE) ReleaseCapture();
            return 0;
        }
        case WM_RBUTTONUP:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_RIGHT,
                GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            window->right_mouse_down = FVIZ_FALSE;
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            if (window->left_mouse_down == FVIZ_FALSE && window->middle_mouse_down == FVIZ_FALSE) ReleaseCapture();
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            const int dx = x - window->last_mouse_x;
            const int dy = y - window->last_mouse_y;
            FVizInteractionEvent event;
            if (window->mouse_inside == FVIZ_FALSE)
            {
                TRACKMOUSEEVENT tracking;
                FVizInteractionEvent enter_event = fviz_win32_interaction_event(
                    FVIZ_INTERACTION_ENTER, FVIZ_MOUSE_BUTTON_NONE, x, y);
                ZeroMemory(&tracking, sizeof(tracking));
                tracking.cbSize = sizeof(tracking);
                tracking.dwFlags = TME_LEAVE;
                tracking.hwndTrack = hwnd;
                (void)TrackMouseEvent(&tracking);
                window->mouse_inside = FVIZ_TRUE;
                (void)fviz_render_window_interactor_process_event(window->interactor, &enter_event);
            }
            if (window->left_mouse_down == FVIZ_TRUE)
            {
                if (abs(dx) > 2 || abs(dy) > 2) window->left_mouse_dragged = FVIZ_TRUE;
            }
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE, x, y);
            if (fviz_render_window_interactor_process_event(window->interactor, &event) == FVIZ_TRUE)
                InvalidateRect(hwnd, NULL, FALSE);
            window->last_mouse_x = x;
            window->last_mouse_y = y;
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_WHEEL, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            event.wheel_delta = (float)delta / (float)WHEEL_DELTA;
            if (fviz_render_window_interactor_process_event(window->interactor, &event) == FVIZ_TRUE)
                InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_MOUSELEAVE:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_LEAVE, FVIZ_MOUSE_BUTTON_NONE,
                window->last_mouse_x, window->last_mouse_y);
            window->mouse_inside = FVIZ_FALSE;
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            return 0;
        }
        case WM_KEYDOWN:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_KEY_DOWN, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            event.key = wparam == VK_ESCAPE ? FVIZ_KEY_ESCAPE : (int)wparam;
            if (fviz_render_window_interactor_process_event(window->interactor, &event) == FVIZ_TRUE)
            {
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            window->close_requested = FVIZ_TRUE;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            fviz_win32_release_gl_context(window, hwnd);
            window->native_window = NULL;
            window->state = FVIZ_RENDER_WINDOW_FINALIZED;
            /* Child/offscreen windows must not terminate the host application's loop. */
            if (window->host_native_handle == NULL && window->offscreen == FVIZ_FALSE)
                PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}
