#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Interaction/FVizRenderWindowInteractor.h>
#include <FViz/Rendering/FVizRenderWindow.h>
#include <FViz/Rendering/FVizLight.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizGL.h>
#include <FViz/Rendering/FVizGLDevice.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizScalarLegend.h>
#include <FViz/Rendering/FVizRenderWindowPrivate.h>

#define FVIZ_WINDOW_CLASS_NAME "FEAVizRenderWindowClass"
#define FVIZ_WM_REQUEST_RENDER (WM_APP + 0x371u)

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
typedef BOOL(WINAPI* PFNFVIZWGLSWAPINTERVALEXTPROC)(int interval);

#define FVIZ_WGL_DRAW_TO_WINDOW_ARB 0x2001
#define FVIZ_WGL_ACCELERATION_ARB 0x2003
#define FVIZ_WGL_SUPPORT_OPENGL_ARB 0x2010
#define FVIZ_WGL_DOUBLE_BUFFER_ARB 0x2011
#define FVIZ_WGL_COLOR_BITS_ARB 0x2014
#define FVIZ_WGL_DEPTH_BITS_ARB 0x2022
#define FVIZ_WGL_STENCIL_BITS_ARB 0x2023
#define FVIZ_WGL_SAMPLE_BUFFERS_ARB 0x2041
#define FVIZ_WGL_SAMPLES_ARB 0x2042
#define FVIZ_WGL_FULL_ACCELERATION_ARB 0x2027
#define FVIZ_WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define FVIZ_WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define FVIZ_WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define FVIZ_WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define FVIZ_WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB 0x20A9
#define FVIZ_GL_FRAMEBUFFER_SRGB 0x8DB9

static PFNFVIZWGLCHOOSEPIXELFORMATARBPROC fviz_wgl_choose_pixel_format_arb = NULL;
static PFNFVIZWGLCREATECONTEXTATTRIBSARBPROC fviz_wgl_create_context_attribs_arb = NULL;
static PFNFVIZWGLSWAPINTERVALEXTPROC fviz_wgl_swap_interval_ext = NULL;

typedef UINT(WINAPI* PFNFVIZGETDPIFORWINDOWPROC)(HWND hwnd);

static uint32_t fviz_win32_query_window_dpi(HWND hwnd)
{
    HMODULE user32;
    PFNFVIZGETDPIFORWINDOWPROC get_dpi_for_window;
    UINT dpi = 96u;
    if (hwnd == NULL) return 96u;
    user32 = GetModuleHandleA("user32.dll");
    get_dpi_for_window = user32 != NULL
        ? (PFNFVIZGETDPIFORWINDOWPROC)GetProcAddress(user32, "GetDpiForWindow")
        : NULL;
    if (get_dpi_for_window != NULL)
    {
        const UINT queried = get_dpi_for_window(hwnd);
        if (queried > 0u) dpi = queried;
    }
    return (uint32_t)dpi;
}

static PROC fviz_win32_wgl_get_proc_address(const char* name)
{
    PROC proc = wglGetProcAddress(name);
    const uintptr_t value = (uintptr_t)proc;
    return proc != NULL && value > (uintptr_t)3u && value != UINTPTR_MAX ? proc : NULL;
}

static double fviz_win32_high_resolution_seconds(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (QueryPerformanceFrequency(&frequency) == FALSE || frequency.QuadPart <= 0 ||
        QueryPerformanceCounter(&counter) == FALSE)
        return (double)GetTickCount64() / 1000.0;
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}

static LRESULT CALLBACK fviz_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
static void fviz_win32_release_gl_context(FVizRenderWindow* window, HWND hwnd);
static void fviz_win32_apply_gl_state(FVizRenderWindow* window);
static void fviz_win32_render_legacy_scene(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    int viewport_width,
    int viewport_height);
static void fviz_win32_render_legacy_gradient_background(FVizRenderer* renderer);

static FVizResult fviz_win32_make_render_context_current(FVizRenderWindow* window)
{
    if (window == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (window->external_opengl != FVIZ_FALSE)
        return window->external_surface.make_current != NULL
            ? window->external_surface.make_current(window->external_surface.user_data)
            : FVIZ_ERROR_INVALID_STATE;
    if (window->native_dc == NULL || window->native_gl_context == NULL)
        return FVIZ_ERROR_INVALID_STATE;
    return wglMakeCurrent((HDC)window->native_dc, (HGLRC)window->native_gl_context) != FALSE
        ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

static void fviz_win32_done_render_context(FVizRenderWindow* window)
{
    if (window != NULL && window->external_opengl != FVIZ_FALSE &&
        window->external_surface.done_current != NULL)
        window->external_surface.done_current(window->external_surface.user_data);
}

static uint32_t fviz_win32_default_framebuffer(FVizRenderWindow* window)
{
    if (window != NULL && window->external_opengl != FVIZ_FALSE &&
        window->external_surface.get_default_framebuffer != NULL)
        return window->external_surface.get_default_framebuffer(window->external_surface.user_data);
    return 0u;
}

static FVizResult fviz_win32_present_render_context(FVizRenderWindow* window)
{
    if (window == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (window->external_opengl != FVIZ_FALSE)
        return window->external_surface.present != NULL
            ? window->external_surface.present(window->external_surface.user_data)
            : FVIZ_OK;
    return SwapBuffers((HDC)window->native_dc) != FALSE ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

static FVizResult fviz_win32_initialize_external_gl(FVizRenderWindow* window)
{
    FVizGLFunctions functions;
    FVizResult result;
    GLint sample_buffers = 0;
    GLint samples = 0;
    if (window == NULL || window->external_opengl == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_win32_make_render_context_current(window);
    if (result != FVIZ_OK)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "external OpenGL host failed to make its context current");
        return result;
    }
    result = fviz_internal_gl_load(&functions);
    if (result != FVIZ_OK)
    {
        fviz_win32_done_render_context(window);
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "external OpenGL context must provide OpenGL 3.3-class entry points");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    window->gl_device = fviz_internal_gl_device_create(&functions);
    if (window->gl_device == NULL)
    {
        fviz_win32_done_render_context(window);
        return fviz_last_error_code();
    }
    fviz_internal_gl_device_set_memory_options(
        (FVizGLDevice*)window->gl_device, &window->gpu_memory_options);
    window->gl_modern = FVIZ_TRUE;
    window->fxaa_supported = fviz_internal_gl_device_fxaa_supported((const FVizGLDevice*)window->gl_device);
    window->weighted_oit_supported = fviz_internal_gl_device_weighted_oit_supported((const FVizGLDevice*)window->gl_device);
    window->shader_lines_supported = fviz_internal_gl_device_shader_lines_supported((const FVizGLDevice*)window->gl_device);
    window->text_rendering_supported = fviz_internal_gl_device_text_supported((const FVizGLDevice*)window->gl_device);
    window->integer_selection_supported = fviz_internal_gl_device_integer_selection_supported((const FVizGLDevice*)window->gl_device);
    window->gpu_timing_supported = fviz_internal_gl_device_gpu_timing_supported((const FVizGLDevice*)window->gl_device);
    glGetIntegerv(0x80A8, &sample_buffers);
    glGetIntegerv(0x80A9, &samples);
    window->actual_multisamples = window->external_surface.sample_count > 0u
        ? window->external_surface.sample_count
        : (sample_buffers > 0 && samples > 1 ? (uint32_t)samples : 0u);
    window->srgb_supported = window->external_surface.srgb_capable != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    window->swap_control_supported = FVIZ_FALSE;
    fviz_win32_apply_gl_state(window);
    fviz_win32_done_render_context(window);
    return FVIZ_OK;
}

static FVizResult fviz_win32_register_class(void)
{
    static ATOM class_atom = 0;
    WNDCLASSA window_class;
    if (class_atom != 0) return FVIZ_OK;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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
        fviz_wgl_choose_pixel_format_arb = (PFNFVIZWGLCHOOSEPIXELFORMATARBPROC)fviz_win32_wgl_get_proc_address("wglChoosePixelFormatARB");
    }
    if (fviz_wgl_create_context_attribs_arb == NULL)
    {
        fviz_wgl_create_context_attribs_arb = (PFNFVIZWGLCREATECONTEXTATTRIBSARBPROC)fviz_win32_wgl_get_proc_address("wglCreateContextAttribsARB");
    }
    if (fviz_wgl_swap_interval_ext == NULL)
    {
        fviz_wgl_swap_interval_ext = (PFNFVIZWGLSWAPINTERVALEXTPROC)fviz_win32_wgl_get_proc_address("wglSwapIntervalEXT");
    }
    return fviz_wgl_choose_pixel_format_arb != NULL && fviz_wgl_create_context_attribs_arb != NULL
        ? FVIZ_TRUE
        : FVIZ_FALSE;
}

static FVizBool fviz_win32_choose_modern_pixel_format(
    HDC dc,
    uint32_t requested_samples,
    FVizBool request_srgb,
    int* out_format,
    FVizBool* out_srgb)
{
    uint32_t samples;
    int srgb_attempt;
    if (out_srgb != NULL) *out_srgb = FVIZ_FALSE;
    if (dc == NULL || out_format == NULL || fviz_wgl_choose_pixel_format_arb == NULL)
        return FVIZ_FALSE;
    for (srgb_attempt = request_srgb != FVIZ_FALSE ? 1 : 0; srgb_attempt >= 0; --srgb_attempt)
    {
        samples = requested_samples > 16u ? 16u : requested_samples;
        while (samples >= 2u)
        {
            int attributes[32];
            int n = 0;
            UINT num_formats = 0u;
#define FVIZ_WGL_ATTR(name_, value_) do { attributes[n++] = (name_); attributes[n++] = (value_); } while (0)
            FVIZ_WGL_ATTR(FVIZ_WGL_DRAW_TO_WINDOW_ARB, TRUE);
            FVIZ_WGL_ATTR(FVIZ_WGL_SUPPORT_OPENGL_ARB, TRUE);
            FVIZ_WGL_ATTR(FVIZ_WGL_DOUBLE_BUFFER_ARB, TRUE);
            FVIZ_WGL_ATTR(FVIZ_WGL_COLOR_BITS_ARB, 32);
            FVIZ_WGL_ATTR(FVIZ_WGL_DEPTH_BITS_ARB, 24);
            FVIZ_WGL_ATTR(FVIZ_WGL_STENCIL_BITS_ARB, 8);
            FVIZ_WGL_ATTR(FVIZ_WGL_ACCELERATION_ARB, FVIZ_WGL_FULL_ACCELERATION_ARB);
            FVIZ_WGL_ATTR(FVIZ_WGL_SAMPLE_BUFFERS_ARB, TRUE);
            FVIZ_WGL_ATTR(FVIZ_WGL_SAMPLES_ARB, (int)samples);
            if (srgb_attempt != 0) FVIZ_WGL_ATTR(FVIZ_WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, TRUE);
            attributes[n] = 0;
#undef FVIZ_WGL_ATTR
            if (fviz_wgl_choose_pixel_format_arb(
                    dc, attributes, NULL, 1u, out_format, &num_formats) != FALSE &&
                num_formats > 0u)
            {
                if (out_srgb != NULL) *out_srgb = srgb_attempt != 0 ? FVIZ_TRUE : FVIZ_FALSE;
                return FVIZ_TRUE;
            }
            samples /= 2u;
        }
        {
            int attributes[24];
            int n = 0;
            UINT num_formats = 0u;
#define FVIZ_WGL_ATTR(name_, value_) do { attributes[n++] = (name_); attributes[n++] = (value_); } while (0)
            FVIZ_WGL_ATTR(FVIZ_WGL_DRAW_TO_WINDOW_ARB, TRUE);
            FVIZ_WGL_ATTR(FVIZ_WGL_SUPPORT_OPENGL_ARB, TRUE);
            FVIZ_WGL_ATTR(FVIZ_WGL_DOUBLE_BUFFER_ARB, TRUE);
            FVIZ_WGL_ATTR(FVIZ_WGL_COLOR_BITS_ARB, 32);
            FVIZ_WGL_ATTR(FVIZ_WGL_DEPTH_BITS_ARB, 24);
            FVIZ_WGL_ATTR(FVIZ_WGL_STENCIL_BITS_ARB, 8);
            FVIZ_WGL_ATTR(FVIZ_WGL_ACCELERATION_ARB, FVIZ_WGL_FULL_ACCELERATION_ARB);
            if (srgb_attempt != 0) FVIZ_WGL_ATTR(FVIZ_WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, TRUE);
            attributes[n] = 0;
#undef FVIZ_WGL_ATTR
            if (fviz_wgl_choose_pixel_format_arb(
                    dc, attributes, NULL, 1u, out_format, &num_formats) != FALSE &&
                num_formats > 0u)
            {
                if (out_srgb != NULL) *out_srgb = srgb_attempt != 0 ? FVIZ_TRUE : FVIZ_FALSE;
                return FVIZ_TRUE;
            }
        }
        if (request_srgb == FVIZ_FALSE) break;
    }
    return FVIZ_FALSE;
}

static void fviz_win32_apply_gl_state(FVizRenderWindow* window)
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    if (window->actual_multisamples > 1u) glEnable(0x809D); /* GL_MULTISAMPLE */
    else glDisable(0x809D);
    if (window->srgb_enabled != FVIZ_FALSE && window->srgb_supported != FVIZ_FALSE)
        glEnable(FVIZ_GL_FRAMEBUFFER_SRGB);
    else
        glDisable(FVIZ_GL_FRAMEBUFFER_SRGB);
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
    FVizBool pixel_format_set = FVIZ_FALSE;

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
        const int context_attribs[] = {
            FVIZ_WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            FVIZ_WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            FVIZ_WGL_CONTEXT_PROFILE_MASK_ARB, FVIZ_WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        int modern_format = 0;
        dc = GetDC(hwnd);
        if (dc != NULL &&
            fviz_win32_choose_modern_pixel_format(
                dc, window->requested_multisamples, window->srgb_enabled, &modern_format,
                &window->srgb_supported) == FVIZ_TRUE)
        {
            PIXELFORMATDESCRIPTOR modern_pfd;
            if (DescribePixelFormat(dc, modern_format, sizeof(modern_pfd), &modern_pfd) != 0 &&
                SetPixelFormat(dc, modern_format, &modern_pfd) != FALSE)
            {
                HGLRC modern_context;
                pixel_format_set = FVIZ_TRUE;
                modern_context = fviz_wgl_create_context_attribs_arb(dc, NULL, context_attribs);
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
        if (dc != NULL &&
            (pixel_format_set != FVIZ_FALSE || fviz_win32_set_legacy_pixel_format(dc) == FVIZ_OK))
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
        fviz_internal_gl_device_set_memory_options(
            (FVizGLDevice*)window->gl_device, &window->gpu_memory_options);
        window->fxaa_supported = fviz_internal_gl_device_fxaa_supported(
            (const FVizGLDevice*)window->gl_device);
        window->weighted_oit_supported = fviz_internal_gl_device_weighted_oit_supported(
            (const FVizGLDevice*)window->gl_device);
        window->shader_lines_supported = fviz_internal_gl_device_shader_lines_supported(
            (const FVizGLDevice*)window->gl_device);
        window->text_rendering_supported = fviz_internal_gl_device_text_supported(
            (const FVizGLDevice*)window->gl_device);
        window->integer_selection_supported = fviz_internal_gl_device_integer_selection_supported(
            (const FVizGLDevice*)window->gl_device);
        window->gpu_timing_supported = fviz_internal_gl_device_gpu_timing_supported(
            (const FVizGLDevice*)window->gl_device);
    }
    {
        GLint sample_buffers = 0;
        GLint samples = 0;
        glGetIntegerv(0x80A8, &sample_buffers); /* GL_SAMPLE_BUFFERS */
        glGetIntegerv(0x80A9, &samples);        /* GL_SAMPLES */
        window->actual_multisamples = sample_buffers > 0 && samples > 1
            ? (uint32_t)samples : 0u;
    }
    window->swap_control_supported = fviz_wgl_swap_interval_ext != NULL ? FVIZ_TRUE : FVIZ_FALSE;
    if (fviz_wgl_swap_interval_ext != NULL)
        (void)fviz_wgl_swap_interval_ext(window->swap_interval);
    fviz_win32_apply_gl_state(window);
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_create_platform(FVizRenderWindow* window)
{
    RECT rect;
    HWND hwnd;
    const FVizBool attached = window->host_native_handle != NULL ? FVIZ_TRUE : FVIZ_FALSE;
    DWORD style;
    if (window->external_opengl != FVIZ_FALSE)
    {
        FVizResult result = fviz_win32_initialize_external_gl(window);
        if (result == FVIZ_OK) window->visible = FVIZ_TRUE;
        return result;
    }
    style = attached != FVIZ_FALSE
        ? (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN)
        : (WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    if (fviz_win32_register_class() != FVIZ_OK) return fviz_last_error_code();
    rect.left = 0; rect.top = 0; rect.right = window->width; rect.bottom = window->height;
    if (attached == FVIZ_FALSE) (void)AdjustWindowRect(&rect, style, FALSE);
    hwnd = CreateWindowExA(
        0,
        FVIZ_WINDOW_CLASS_NAME,
        window->title,
        style,
        attached != FVIZ_FALSE ? 0 : CW_USEDEFAULT,
        attached != FVIZ_FALSE ? 0 : CW_USEDEFAULT,
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
    window->dpi = fviz_win32_query_window_dpi(hwnd);
    if (fviz_win32_create_gl_context(window, hwnd) != FVIZ_OK)
    {
        DestroyWindow(hwnd);
        window->native_window = NULL;
        return fviz_last_error_code();
    }
    /* CreateWindow can deliver WM_SIZE before native_window is assigned. If
     * that early message requested a frame, arm it now that the child HWND
     * and GL context both exist. */
    if (window->render_requested != FVIZ_FALSE)
        fviz_internal_render_window_schedule_render_platform(window);
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_show_platform(FVizRenderWindow* window)
{
    HWND hwnd;
    if (window != NULL && window->external_opengl != FVIZ_FALSE)
    {
        window->visible = FVIZ_TRUE;
        fviz_render_window_request_render(window);
        return FVIZ_OK;
    }
    hwnd = (HWND)window->native_window;
    if (hwnd == NULL) return FVIZ_ERROR_INVALID_STATE;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    window->visible = FVIZ_TRUE;
    return FVIZ_OK;
}

static void fviz_win32_render_legacy_gradient_background(FVizRenderer* renderer)
{
    float bottom[3];
    float top[3];
    fviz_renderer_get_background(renderer, &bottom[0], &bottom[1], &bottom[2]);
    fviz_renderer_get_background2(renderer, &top[0], &top[1], &top[2]);
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glBegin(GL_QUADS);
    glColor3fv(bottom); glVertex2f(-1.0f, -1.0f);
    glColor3fv(bottom); glVertex2f( 1.0f, -1.0f);
    glColor3fv(top);    glVertex2f( 1.0f,  1.0f);
    glColor3fv(top);    glVertex2f(-1.0f,  1.0f);
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

static void fviz_win32_render_actor(const FVizActor* actor)
{
    const FVizPolyData* data;
    const FVizVec3* points;
    const FVizVec3* normals;
    const uint32_t* indices;
    FVizSize index_count;
    FVizMat4 model;
    float red;
    float green;
    float blue;
    float ambient;
    float diffuse;
    float specular;
    float specular_power;
    GLfloat material_ambient[4];
    GLfloat material_diffuse[4];
    GLfloat material_specular[4];

    if (actor == NULL || fviz_actor_is_visible(actor) == FVIZ_FALSE) return;
    data = fviz_actor_const_poly_data(actor);
    if (data == NULL || fviz_poly_data_triangle_count(data) == 0u) return;
    points = fviz_poly_data_points(data);
    normals = fviz_poly_data_normals(data);
    indices = fviz_poly_data_triangle_indices(data);
    index_count = fviz_poly_data_triangle_count(data) * 3u;
    if (points == NULL || indices == NULL || index_count > (FVizSize)INT_MAX) return;

    fviz_actor_get_color(actor, &red, &green, &blue);
    fviz_actor_get_material(actor, &ambient, &diffuse, &specular, &specular_power);
    material_ambient[0] = red * ambient;
    material_ambient[1] = green * ambient;
    material_ambient[2] = blue * ambient;
    material_ambient[3] = fviz_actor_opacity(actor);
    material_diffuse[0] = red * diffuse;
    material_diffuse[1] = green * diffuse;
    material_diffuse[2] = blue * diffuse;
    material_diffuse[3] = fviz_actor_opacity(actor);
    material_specular[0] = specular;
    material_specular[1] = specular;
    material_specular[2] = specular;
    material_specular[3] = 1.0f;
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material_specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, specular_power > 128.0f ? 128.0f : specular_power);
    glColor4f(red, green, blue, fviz_actor_opacity(actor));
    glShadeModel(fviz_actor_shading_mode(actor) == FVIZ_SHADING_FLAT ? GL_FLAT : GL_SMOOTH);
    if (fviz_actor_cull_mode(actor) == FVIZ_CULL_NONE)
        glDisable(GL_CULL_FACE);
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(fviz_actor_cull_mode(actor) == FVIZ_CULL_FRONT ? GL_FRONT : GL_BACK);
    }
    glPolygonMode(GL_FRONT_AND_BACK, fviz_actor_wireframe(actor) == FVIZ_TRUE ? GL_LINE : GL_FILL);
    model = fviz_actor_transform_matrix(actor);
    glPushMatrix();
    glMultMatrixf(model.m);
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
    glPopMatrix();
}

FVizResult fviz_internal_render_window_render_platform(FVizRenderWindow* window)
{
    HDC dc = (HDC)window->native_dc;
    HGLRC context = (HGLRC)window->native_gl_context;
    FVizSize renderer_count = fviz_render_window_renderer_count(window);
    FVizSize rendered_count = 0u;
    int last_layer = -1;
    const double frame_start_seconds = fviz_win32_high_resolution_seconds();
    double render_end_seconds;
    double present_end_seconds;
    FVizBool fxaa_applied = FVIZ_FALSE;
    FVizTransparencyMode requested_transparency = FVIZ_TRANSPARENCY_SORTED;
    FVizTransparencyMode applied_transparency = FVIZ_TRANSPARENCY_SORTED;
    const uint32_t target_framebuffer = fviz_win32_default_framebuffer(window);

    if ((window->external_opengl == FVIZ_FALSE && (dc == NULL || context == NULL)) ||
        (window->external_opengl != FVIZ_FALSE && window->gl_device == NULL) ||
        renderer_count == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
            "render window has no valid OpenGL context/device or renderer");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_win32_make_render_context_current(window) != FVIZ_OK)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to activate FEAViz OpenGL context");
        return FVIZ_ERROR_GRAPHICS;
    }
    if (window->gl_device != NULL)
        fviz_internal_gl_device_bind_framebuffer((FVizGLDevice*)window->gl_device, target_framebuffer);
    if (window->height <= 0) window->height = 1;
    glViewport(0, 0, window->width, window->height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    if (window->gl_modern != FVIZ_FALSE && window->gl_device != NULL)
        fviz_internal_gl_device_begin_frame((FVizGLDevice*)window->gl_device);
    fviz_internal_render_window_clear_pass_statistics(window);

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
            const FVizRenderGraph* pass_graph;
            FVizRenderPassContext pass_context;
            FVizResult renderer_result = FVIZ_OK;
            if (fviz_renderer_layer(renderer) != current_layer) continue;
            (void)fviz_object_invoke_event(
                (FVizObject*)renderer, FVIZ_EVENT_RENDER_START, window);
            renderer_result = fviz_renderer_update(renderer);
            if (renderer_result == FVIZ_OK)
                renderer_result = fviz_renderer_compile_render_graph(renderer);
            if (renderer_result != FVIZ_OK)
            {
                (void)fviz_object_invoke_event(
                    (FVizObject*)renderer, FVIZ_EVENT_RENDER_END, &renderer_result);
                glDisable(GL_SCISSOR_TEST);
                fviz_win32_done_render_context(window);
                return renderer_result;
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
            pass_graph = fviz_renderer_render_graph(renderer);
            if (pass_graph == NULL)
            {
                renderer_result = fviz_last_error_code();
                (void)fviz_object_invoke_event(
                    (FVizObject*)renderer, FVIZ_EVENT_RENDER_END, &renderer_result);
                glDisable(GL_SCISSOR_TEST);
                fviz_win32_done_render_context(window);
                return renderer_result;
            }
            for (pass_index = 0u;
                 pass_index < fviz_render_graph_execution_count(pass_graph);
                 ++pass_index)
            {
                FVizRenderPass* pass =
                    fviz_render_graph_execution_pass(pass_graph, pass_index);
                FVizResult pass_result = FVIZ_OK;
                const FVizRenderGraphPassId graph_pass_id =
                    fviz_render_graph_execution_pass_id(pass_graph, pass_index);
                const char* pass_name =
                    fviz_render_graph_pass_name(pass_graph, graph_pass_id);
                const double pass_start_seconds = fviz_win32_high_resolution_seconds();
                if (fviz_render_pass_is_custom(pass) != FVIZ_FALSE)
                {
                    FVizGLStateSnapshot state_snapshot;
                    FVizBool restore_state = FVIZ_FALSE;
                    if (window->gl_modern != FVIZ_FALSE && window->gl_device != NULL)
                    {
                        fviz_internal_gl_device_capture_state(
                            (FVizGLDevice*)window->gl_device, &state_snapshot);
                        restore_state = FVIZ_TRUE;
                    }
                    pass_result = fviz_render_pass_execute(pass, renderer, &pass_context);
                    if (restore_state != FVIZ_FALSE)
                    {
                        const FVizResult restore_result =
                            fviz_internal_gl_device_restore_state(
                                (FVizGLDevice*)window->gl_device, &state_snapshot);
                        if (pass_result == FVIZ_OK) pass_result = restore_result;
                    }
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
                                if (fviz_renderer_gradient_background(renderer) != FVIZ_FALSE)
                                {
                                    if (window->gl_modern != FVIZ_FALSE && window->gl_device != NULL)
                                    {
                                        float background2[3];
                                        fviz_renderer_get_background2(
                                            renderer, &background2[0], &background2[1], &background2[2]);
                                        pass_result = fviz_internal_gl_device_render_gradient_background(
                                            (FVizGLDevice*)window->gl_device, background, background2);
                                    }
                                    else
                                        fviz_win32_render_legacy_gradient_background(renderer);
                                }
                            }
                            break;
                        case FVIZ_RENDER_PASS_OPAQUE:
                            if (window->gl_modern == FVIZ_TRUE && window->gl_device != NULL)
                                pass_result = fviz_internal_gl_device_render_stage(
                                    (FVizGLDevice*)window->gl_device,
                                    renderer,
                                    pass_context.aspect_ratio,
                                    viewport_width, viewport_height,
                                    FVIZ_RENDER_PASS_OPAQUE);
                            else
                                fviz_win32_render_legacy_scene(
                                    window, renderer, viewport_width, viewport_height);
                            break;
                        case FVIZ_RENDER_PASS_TRANSLUCENT:
                            requested_transparency = fviz_renderer_transparency_mode(renderer);
                            if (window->gl_modern == FVIZ_TRUE && window->gl_device != NULL &&
                                requested_transparency == FVIZ_TRANSPARENCY_WEIGHTED_BLENDED &&
                                window->weighted_oit_supported != FVIZ_FALSE)
                            {
                                pass_result = fviz_internal_gl_device_render_weighted_oit(
                                    (FVizGLDevice*)window->gl_device, renderer,
                                    viewport_x, viewport_y, viewport_width, viewport_height,
                                    window->actual_multisamples > 1u ? window->actual_multisamples : 1u,
                                    pass_context.aspect_ratio, target_framebuffer);
                                if (pass_result == FVIZ_OK)
                                    applied_transparency = FVIZ_TRANSPARENCY_WEIGHTED_BLENDED;
                            }
                            else if (window->gl_modern == FVIZ_TRUE && window->gl_device != NULL)
                            {
                                pass_result = fviz_internal_gl_device_render_stage(
                                    (FVizGLDevice*)window->gl_device, renderer,
                                    pass_context.aspect_ratio, viewport_width, viewport_height,
                                    FVIZ_RENDER_PASS_TRANSLUCENT);
                                applied_transparency = FVIZ_TRANSPARENCY_SORTED;
                            }
                            break;
                        case FVIZ_RENDER_PASS_EDGE:
                            if (window->gl_modern == FVIZ_TRUE && window->gl_device != NULL)
                                pass_result = fviz_internal_gl_device_render_stage(
                                    (FVizGLDevice*)window->gl_device, renderer,
                                    pass_context.aspect_ratio, viewport_width, viewport_height,
                                    FVIZ_RENDER_PASS_EDGE);
                            break;
                        case FVIZ_RENDER_PASS_OVERLAY:
                        {
                            FVizScalarLegend* legend = fviz_renderer_scalar_legend(renderer);
                            if (window->gl_modern == FVIZ_TRUE && window->gl_device != NULL)
                            {
                                if (legend != NULL)
                                    pass_result = fviz_internal_gl_device_render_legend(
                                        (FVizGLDevice*)window->gl_device,
                                        legend, viewport_width, viewport_height,
                                        fviz_render_window_content_scale(window));
                                if (pass_result == FVIZ_OK && window->text_rendering_supported != FVIZ_FALSE)
                                    pass_result = fviz_internal_gl_device_render_text_actors(
                                        (FVizGLDevice*)window->gl_device, renderer,
                                        viewport_width, viewport_height,
                                        fviz_render_window_content_scale(window));
                            }
                            break;
                        }
                        case FVIZ_RENDER_PASS_SELECTION:
                        default:
                            break;
                    }
                }
                fviz_internal_render_window_record_pass_statistics(
                    window, renderer, graph_pass_id, pass_index, pass, pass_name,
                    fviz_win32_high_resolution_seconds() - pass_start_seconds,
                    pass_result);
                if (pass_result != FVIZ_OK)
                {
                    renderer_result = pass_result;
                    (void)fviz_object_invoke_event(
                        (FVizObject*)renderer, FVIZ_EVENT_RENDER_END, &renderer_result);
                    glDisable(GL_SCISSOR_TEST);
                    fviz_win32_done_render_context(window);
                    return renderer_result;
                }
            }
            (void)fviz_object_invoke_event(
                (FVizObject*)renderer, FVIZ_EVENT_RENDER_END, &renderer_result);
            ++rendered_count;
        }
        if (current_layer == INT_MAX) break;
        last_layer = current_layer;
    }
    glDisable(GL_SCISSOR_TEST);
    if (window->gl_modern != FVIZ_FALSE && window->gl_device != NULL)
        fviz_internal_gl_device_end_frame((FVizGLDevice*)window->gl_device);
    {
        const FVizBool apply_fxaa = window->fxaa_enabled != FVIZ_FALSE &&
            window->fxaa_supported != FVIZ_FALSE &&
            window->gl_modern != FVIZ_FALSE && window->gl_device != NULL &&
            (window->adaptive_antialiasing == FVIZ_FALSE ||
             fviz_render_window_frame_quality(window) == FVIZ_FRAME_QUALITY_STILL)
            ? FVIZ_TRUE : FVIZ_FALSE;
        if (apply_fxaa != FVIZ_FALSE)
        {
            FVizResult fxaa_result;
            glViewport(0, 0, window->width, window->height);
            fxaa_result = fviz_internal_gl_device_apply_fxaa(
                (FVizGLDevice*)window->gl_device, window->width, window->height,
                &window->fxaa_options,
                window->srgb_enabled != FVIZ_FALSE && window->srgb_supported != FVIZ_FALSE,
                target_framebuffer);
            if (fxaa_result == FVIZ_OK) fxaa_applied = FVIZ_TRUE;
            else if (fxaa_result != FVIZ_ERROR_NOT_SUPPORTED)
            {
                fviz_win32_done_render_context(window);
                return fxaa_result;
            }
        }
    }

    render_end_seconds = fviz_win32_high_resolution_seconds();
    if (fviz_win32_present_render_context(window) != FVIZ_OK)
    {
        fviz_win32_done_render_context(window);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "OpenGL presentation failed");
        return FVIZ_ERROR_GRAPHICS;
    }
    present_end_seconds = fviz_win32_high_resolution_seconds();
    {
        const uint64_t next_frame = window->last_statistics.frame_number + 1u;
        FVizGLFrameStatistics gl_statistics;
        (void)memset(&gl_statistics, 0, sizeof(gl_statistics));
        if (window->gl_modern != FVIZ_FALSE && window->gl_device != NULL)
            fviz_internal_gl_device_get_frame_statistics(
                (const FVizGLDevice*)window->gl_device, &gl_statistics);
        (void)memset(&window->last_statistics, 0, sizeof(window->last_statistics));
        window->last_statistics.struct_size = (uint32_t)sizeof(window->last_statistics);
        window->last_statistics.frame_number = next_frame;
        window->last_statistics.render_seconds = render_end_seconds - frame_start_seconds;
        window->last_statistics.present_seconds = present_end_seconds - render_end_seconds;
        window->last_statistics.draw_calls = gl_statistics.draw_calls;
        window->last_statistics.triangles = gl_statistics.triangles;
        window->last_statistics.lines = gl_statistics.lines;
        window->last_statistics.gpu_uploads = gl_statistics.gpu_uploads;
        window->last_statistics.gpu_upload_bytes = gl_statistics.gpu_upload_bytes;
        window->last_statistics.resident_actor_resources = gl_statistics.resident_actor_resources;
        window->last_statistics.resident_mesh_gpu_bytes = gl_statistics.resident_mesh_gpu_bytes;
        window->last_statistics.gpu_mesh_byte_budget = gl_statistics.gpu_mesh_byte_budget;
        window->last_statistics.gpu_resource_evictions = gl_statistics.gpu_resource_evictions;
        window->last_statistics.gpu_mesh_budget_exceeded =
            gl_statistics.gpu_mesh_budget_exceeded;
        window->last_statistics.resident_geometry_gpu_bytes =
            gl_statistics.resident_geometry_gpu_bytes;
        window->last_statistics.resident_attribute_gpu_bytes =
            gl_statistics.resident_attribute_gpu_bytes;
        window->last_statistics.resident_instance_gpu_bytes =
            gl_statistics.resident_instance_gpu_bytes;
        window->last_statistics.resident_render_target_gpu_bytes =
            gl_statistics.resident_render_target_gpu_bytes;
        window->last_statistics.pinned_gpu_resources =
            gl_statistics.pinned_gpu_resources;
        window->last_statistics.custom_pass_state_restorations =
            gl_statistics.custom_pass_state_restorations;
        window->last_statistics.actors_considered = gl_statistics.actors_considered;
        window->last_statistics.actors_frustum_culled = gl_statistics.actors_frustum_culled;
        window->last_statistics.actors_small_object_culled = gl_statistics.actors_small_object_culled;
        window->last_statistics.actors_visible_after_culling = gl_statistics.actors_visible_after_culling;
        window->last_statistics.gpu_frame_nanoseconds = gl_statistics.gpu_frame_nanoseconds;
        window->last_statistics.gpu_timing_valid = gl_statistics.gpu_timing_valid;
        window->last_statistics.sample_count = window->actual_multisamples;
        window->last_statistics.fxaa_applied = fxaa_applied;
        window->last_statistics.interaction_active = window->interaction_active;
        window->last_statistics.srgb_applied = window->srgb_enabled != FVIZ_FALSE &&
            window->srgb_supported != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        window->last_statistics.transparency_mode_requested = requested_transparency;
        window->last_statistics.transparency_mode_applied = applied_transparency;
        {
            FVizSize renderer_index;
            uint64_t highest_generation = 0u;
            uint32_t pass_count = 0u;
            for (renderer_index = 0u; renderer_index < renderer_count; ++renderer_index)
            {
                FVizRenderer* statistics_renderer =
                    fviz_render_window_renderer_at(window, renderer_index);
                const FVizRenderGraph* graph =
                    fviz_renderer_render_graph(statistics_renderer);
                FVizRenderGraphStatistics graph_statistics;
                fviz_render_graph_get_statistics(graph, &graph_statistics);
                if (graph_statistics.compile_generation > highest_generation)
                    highest_generation = graph_statistics.compile_generation;
                if (UINT32_MAX - pass_count < graph_statistics.pass_count)
                    pass_count = UINT32_MAX;
                else pass_count += graph_statistics.pass_count;
            }
            window->last_statistics.render_graph_compile_generation = highest_generation;
            window->last_statistics.render_graph_pass_count = pass_count;
        }
    }
    fviz_win32_done_render_context(window);
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
    FVizSize enabled_light_count = 0u;

    camera = fviz_renderer_camera(renderer);
    (void)window;
    projection = fviz_camera_projection_matrix(
        camera, (float)viewport_width / (float)viewport_height);
    view = fviz_camera_view_matrix(camera);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection.m);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.m);

    for (i = 0u; i < FVIZ_RENDERER_MAX_LIGHTS; ++i) glDisable(GL_LIGHT0 + (GLenum)i);
    for (i = 0u; i < fviz_renderer_light_count(renderer) && enabled_light_count < FVIZ_RENDERER_MAX_LIGHTS; ++i)
    {
        const FVizLight* light = fviz_renderer_light_at(renderer, i);
        GLfloat position[4];
        GLfloat diffuse[4];
        GLfloat ambient[4];
        FVizVec3 light_position;
        float red;
        float green;
        float blue;
        const GLenum slot = GL_LIGHT0 + (GLenum)enabled_light_count;
        if (light == NULL || fviz_light_enabled(light) == FVIZ_FALSE ||
            fviz_light_intensity(light) <= 0.0f)
            continue;
        light_position = fviz_light_type(light) == FVIZ_LIGHT_HEADLIGHT
            ? fviz_camera_position(camera)
            : fviz_light_position(light);
        fviz_light_get_color(light, &red, &green, &blue);
        position[0] = light_position.x; position[1] = light_position.y;
        position[2] = light_position.z; position[3] = 1.0f;
        diffuse[0] = red * fviz_light_intensity(light);
        diffuse[1] = green * fviz_light_intensity(light);
        diffuse[2] = blue * fviz_light_intensity(light);
        diffuse[3] = 1.0f;
        ambient[0] = red * 0.15f; ambient[1] = green * 0.15f;
        ambient[2] = blue * 0.15f; ambient[3] = 1.0f;
        glEnable(slot);
        glLightfv(slot, GL_POSITION, position);
        glLightfv(slot, GL_DIFFUSE, diffuse);
        glLightfv(slot, GL_AMBIENT, ambient);
        ++enabled_light_count;
    }

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
        if (window->external_opengl != FVIZ_FALSE)
            (void)fviz_win32_make_render_context_current(window);
        else if (dc != NULL && context != NULL)
            (void)wglMakeCurrent(dc, context);
        fviz_internal_gl_device_destroy((FVizGLDevice*)window->gl_device);
        window->gl_device = NULL;
        fviz_win32_done_render_context(window);
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
    window->actual_multisamples = 0u;
    window->srgb_supported = FVIZ_FALSE;
    window->weighted_oit_supported = FVIZ_FALSE;
    window->shader_lines_supported = FVIZ_FALSE;
    window->text_rendering_supported = FVIZ_FALSE;
    window->integer_selection_supported = FVIZ_FALSE;
    window->gpu_timing_supported = FVIZ_FALSE;
    window->swap_control_supported = FVIZ_FALSE;
    window->fxaa_supported = FVIZ_FALSE;
}

FVizResult fviz_internal_render_window_release_external_opengl_platform(FVizRenderWindow* window)
{
    if (window == NULL || window->external_opengl == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_win32_release_gl_context(window, NULL);
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_reinitialize_external_opengl_platform(FVizRenderWindow* window)
{
    if (window == NULL || window->external_opengl == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (window->gl_device != NULL) fviz_win32_release_gl_context(window, NULL);
    return fviz_win32_initialize_external_gl(window);
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

FVizResult fviz_internal_render_window_set_swap_interval_platform(
    FVizRenderWindow* window,
    int interval)
{
    if (window == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (window->external_opengl != FVIZ_FALSE) return FVIZ_ERROR_NOT_SUPPORTED;
    if (window->native_dc == NULL || window->native_gl_context == NULL)
        return FVIZ_ERROR_INVALID_STATE;
    if (wglMakeCurrent((HDC)window->native_dc, (HGLRC)window->native_gl_context) == FALSE)
        return FVIZ_ERROR_GRAPHICS;
    if (fviz_wgl_swap_interval_ext == NULL) return FVIZ_ERROR_NOT_SUPPORTED;
    return fviz_wgl_swap_interval_ext(interval) != FALSE ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

FVizResult fviz_internal_render_window_release_gpu_mesh_resources_platform(
    FVizRenderWindow* window)
{
    FVizResult result;
    if (window == NULL || window->gl_device == NULL)
        return FVIZ_ERROR_INVALID_STATE;
    result = fviz_win32_make_render_context_current(window);
    if (result != FVIZ_OK) return result;
    fviz_internal_gl_device_release_mesh_resources(
        (FVizGLDevice*)window->gl_device);
    fviz_win32_done_render_context(window);
    window->last_statistics.resident_actor_resources = 0u;
    window->last_statistics.resident_mesh_gpu_bytes = 0u;
    window->last_statistics.gpu_mesh_budget_exceeded = FVIZ_FALSE;
    fviz_render_window_request_render_reason(window, FVIZ_RENDER_REQUEST_SCENE);
    return FVIZ_OK;
}

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

static void fviz_win32_cancel_pointer_interaction(FVizRenderWindow* window)
{
    if (window == NULL) return;
    window->left_mouse_down = FVIZ_FALSE;
    window->middle_mouse_down = FVIZ_FALSE;
    window->right_mouse_down = FVIZ_FALSE;
    window->x1_mouse_down = FVIZ_FALSE;
    window->x2_mouse_down = FVIZ_FALSE;
    window->left_mouse_dragged = FVIZ_FALSE;
    fviz_render_window_interactor_cancel_interaction(window->interactor);
    if (window->native_window != NULL)
    {
        HWND hwnd = (HWND)window->native_window;
        if (GetCapture() == hwnd) ReleaseCapture();
        fviz_render_window_request_render(window);
    }
}

void fviz_internal_render_window_schedule_render_platform(FVizRenderWindow* window)
{
    HWND hwnd;
    if (window == NULL || window->offscreen != FVIZ_FALSE) return;
    if (window->external_opengl != FVIZ_FALSE)
    {
        if (window->external_surface.request_render != NULL)
            window->external_surface.request_render(window->external_surface.user_data);
        return;
    }
    hwnd = (HWND)window->native_window;
    if (hwnd != NULL && IsWindow(hwnd) != FALSE)
        (void)PostMessageA(hwnd, FVIZ_WM_REQUEST_RENDER, 0u, 0);
}

FVizResult fviz_internal_render_window_process_events_platform(FVizRenderWindow* window)
{
    MSG message;
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (window->external_opengl != FVIZ_FALSE)
    {
        (void)fviz_render_window_interactor_process_timers(
            window->interactor, (double)GetTickCount64() / 1000.0);
        return FVIZ_OK;
    }
    while (PeekMessageA(
            &message,
            window->host_native_handle != NULL ? (HWND)window->native_window : NULL,
            0u, 0u, PM_REMOVE) != FALSE)
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
    if (window == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (window->external_opengl != FVIZ_FALSE) return FVIZ_OK;
    if (window->native_window == NULL) return FVIZ_ERROR_INVALID_STATE;
    hwnd = (HWND)window->native_window;
    style = (DWORD)GetWindowLongPtrA(hwnd, GWL_STYLE);
    rect.left = 0;
    rect.top = 0;
    rect.right = window->width;
    rect.bottom = window->height;
    if (window->host_native_handle == NULL)
        (void)AdjustWindowRect(&rect, style, FALSE);
    if (SetWindowPos(
            hwnd, NULL,
            window->host_native_handle != NULL ? 0 : rect.left,
            window->host_native_handle != NULL ? 0 : rect.top,
            rect.right - rect.left, rect.bottom - rect.top,
            (window->host_native_handle != NULL ? 0u : SWP_NOMOVE) |
                SWP_NOZORDER | SWP_NOACTIVATE) == FALSE)
        return FVIZ_ERROR_GRAPHICS;
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_sync_host_size_platform(FVizRenderWindow* window)
{
    RECT rect;
    HWND host;
    int width;
    int height;
    if (window == NULL || window->host_native_handle == NULL || window->native_window == NULL)
        return FVIZ_ERROR_INVALID_STATE;
    host = (HWND)window->host_native_handle;
    if (IsWindow(host) == FALSE || GetClientRect(host, &rect) == FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "attached host HWND is no longer valid");
        return FVIZ_ERROR_INVALID_STATE;
    }
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    /* Minimized/hidden hosts can legitimately have an empty client area. */
    if (width <= 0 || height <= 0) return FVIZ_OK;
    if (window->width == width && window->height == height) return FVIZ_OK;
    return fviz_render_window_resize(window, width, height);
}

FVizResult fviz_internal_render_window_reparent_platform(
    FVizRenderWindow* window,
    void* host_native_handle)
{
    HWND child;
    HWND host;
    HWND previous_parent;
    LONG_PTR style;
    if (window == NULL || window->native_window == NULL || host_native_handle == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    child = (HWND)window->native_window;
    host = (HWND)host_native_handle;
    if (IsWindow(child) == FALSE || IsWindow(host) == FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "render child or new host HWND is invalid");
        return FVIZ_ERROR_INVALID_STATE;
    }
    SetLastError(ERROR_SUCCESS);
    previous_parent = SetParent(child, host);
    if (previous_parent == NULL && GetLastError() != ERROR_SUCCESS)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "failed to reparent FEAViz child HWND");
        return FVIZ_ERROR_INVALID_STATE;
    }
    style = GetWindowLongPtrA(child, GWL_STYLE);
    style |= (LONG_PTR)(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    style &= ~(LONG_PTR)WS_POPUP;
    (void)SetWindowLongPtrA(child, GWL_STYLE, style);
    if (SetWindowPos(child, NULL, 0, 0, 1, 1,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED) == FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to position reparented FEAViz child HWND");
        return FVIZ_ERROR_GRAPHICS;
    }
    window->dpi = fviz_win32_query_window_dpi(host);
    return FVIZ_OK;
}

FVizResult fviz_internal_render_window_read_rgba8_platform(
    FVizRenderWindow* window,
    uint8_t* pixels)
{
    uint32_t target_framebuffer;
    GLenum read_buffer;
    GLint previous_read_buffer = GL_BACK;
    FVizResult current_result;
    FVizResult result;
    if (window == NULL || pixels == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    current_result = fviz_win32_make_render_context_current(window);
    if (current_result != FVIZ_OK) return current_result;
    target_framebuffer = fviz_win32_default_framebuffer(window);
    if (window->gl_device != NULL)
        fviz_internal_gl_device_bind_framebuffer((FVizGLDevice*)window->gl_device, target_framebuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    read_buffer = window->external_opengl != FVIZ_FALSE
        ? (target_framebuffer != 0u ? FVIZ_GL_COLOR_ATTACHMENT0 : GL_BACK)
        : GL_FRONT;
    glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);
    glReadBuffer(read_buffer);
    glReadPixels(0, 0, window->width, window->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    result = glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
    /* Restore the read buffer: leaving GL_FRONT selected would make the next
     * FXAA blit resolve from a stale front buffer (frame feedback). */
    glReadBuffer((GLenum)previous_read_buffer);
    fviz_win32_done_render_context(window);
    return result;
}

FVizResult fviz_internal_render_window_read_depth_f32_platform(
    FVizRenderWindow* window,
    float* depth)
{
    uint32_t target_framebuffer;
    FVizResult current_result;
    FVizResult result;
    if (window == NULL || depth == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    current_result = fviz_win32_make_render_context_current(window);
    if (current_result != FVIZ_OK) return current_result;
    target_framebuffer = fviz_win32_default_framebuffer(window);
    if (window->gl_device != NULL)
        fviz_internal_gl_device_bind_framebuffer((FVizGLDevice*)window->gl_device, target_framebuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, window->width, window->height, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
    result = glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
    fviz_win32_done_render_context(window);
    return result;
}

FVizResult fviz_internal_render_window_hardware_pick_platform(
    FVizRenderWindow* window,
    FVizRenderer* renderer,
    int x,
    int y,
    FVizSelectionAssociation association,
    FVizSize* out_actor_index,
    FVizSize* out_primitive_id,
    float* out_depth)
{
    float viewport[4];
    GLint previous_viewport[4];
    GLint previous_scissor_box[4];
    GLboolean scissor_was_enabled;
    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
    uint32_t target_framebuffer;
    FVizResult current_result;
    FVizResult result;
    if (window == NULL || renderer == NULL || out_actor_index == NULL ||
        out_primitive_id == NULL || out_depth == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (window->gl_modern == FVIZ_FALSE || window->gl_device == NULL)
        return FVIZ_ERROR_NOT_SUPPORTED;
    current_result = fviz_win32_make_render_context_current(window);
    if (current_result != FVIZ_OK) return current_result;
    target_framebuffer = fviz_win32_default_framebuffer(window);
    fviz_internal_gl_device_bind_framebuffer((FVizGLDevice*)window->gl_device, target_framebuffer);
    fviz_renderer_get_viewport(
        renderer, &viewport[0], &viewport[1], &viewport[2], &viewport[3]);
    viewport_x = (int)(viewport[0] * (float)window->width);
    viewport_y = (int)(viewport[1] * (float)window->height);
    viewport_width = (int)((viewport[2] - viewport[0]) * (float)window->width);
    viewport_height = (int)((viewport[3] - viewport[1]) * (float)window->height);
    if (viewport_width < 1 || viewport_height < 1)
    {
        fviz_win32_done_render_context(window);
        return FVIZ_ERROR_NOT_FOUND;
    }

    /* Hardware picking temporarily narrows the viewport/scissor to the target
       renderer. Preserve the caller's GL state so a pick never leaks selection
       state into the next application render or an embedding host. */
    glGetIntegerv(0x0BA2, previous_viewport);     /* GL_VIEWPORT */
    glGetIntegerv(0x0C10, previous_scissor_box); /* GL_SCISSOR_BOX */
    scissor_was_enabled = glIsEnabled(GL_SCISSOR_TEST);

    glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(viewport_x, viewport_y, viewport_width, viewport_height);
    result = fviz_internal_gl_device_select(
        (FVizGLDevice*)window->gl_device,
        renderer,
        (float)viewport_width / (float)viewport_height,
        x - viewport_x,
        (window->height - y - 1) - viewport_y,
        viewport_width,
        viewport_height,
        association,
        out_actor_index,
        out_primitive_id,
        out_depth);
    glViewport(previous_viewport[0],
               previous_viewport[1],
               previous_viewport[2],
               previous_viewport[3]);
    glScissor(previous_scissor_box[0],
              previous_scissor_box[1],
              previous_scissor_box[2],
              previous_scissor_box[3]);
    if (scissor_was_enabled != GL_FALSE)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    fviz_win32_done_render_context(window);
    (void)fviz_internal_render_window_render_platform(window);
    return result;
}

static void fviz_win32_render_interaction_if_due(
    FVizRenderWindow* window,
    FVizBool handled,
    FVizBool force)
{
    double desired_rate = 0.0;
    double still_rate = 0.0;
    double now;
    double minimum_interval;
    if (window == NULL || handled == FVIZ_FALSE || window->interactor == NULL) return;
    now = (double)GetTickCount64() / 1000.0;
    fviz_render_window_interactor_get_update_rates(
        window->interactor, &desired_rate, &still_rate);
    if (window->interaction_active != FVIZ_FALSE)
    {
        if (window->frame_scheduler_options.interactive_target_fps > 0.0)
            desired_rate = window->frame_scheduler_options.interactive_target_fps;
    }
    else
    {
        desired_rate = window->frame_scheduler_options.still_target_fps > 0.0
            ? window->frame_scheduler_options.still_target_fps
            : still_rate;
    }
    minimum_interval = desired_rate > 0.0 ? 1.0 / desired_rate : 0.0;
    if (force != FVIZ_FALSE || window->interaction_active == FVIZ_FALSE ||
        window->last_interaction_render_seconds <= 0.0 ||
        now - window->last_interaction_render_seconds >= minimum_interval)
    {
        if (force != FVIZ_FALSE)
        {
            if (fviz_render_window_render(window) == FVIZ_OK)
                window->last_interaction_render_seconds = now;
        }
        else
        {
            /* WM_PAINT is naturally coalesced by Windows. During dense mouse
             * motion request a frame instead of rendering recursively inside
             * the input message handler. */
            fviz_render_window_request_render_reason(
                window,
                window->interaction_active != FVIZ_FALSE
                    ? FVIZ_RENDER_REQUEST_INTERACTION
                    : FVIZ_RENDER_REQUEST_SCENE);
            window->last_interaction_render_seconds = now;
        }
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
        case FVIZ_WM_REQUEST_RENDER:
            /* Post first, invalidate later.  This guarantees a request raised
             * from RenderEnd/observers while WM_PAINT is active survives the
             * subsequent EndPaint validation step. */
            if (window->render_requested != FVIZ_FALSE)
                (void)InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_DPICHANGED:
        {
            const RECT* suggested = (const RECT*)lparam;
            const uint32_t new_dpi = (uint32_t)LOWORD(wparam);
            const uint32_t previous_dpi = window->dpi;
            window->dpi = new_dpi > 0u ? new_dpi : fviz_win32_query_window_dpi(hwnd);
            if (window->dpi != previous_dpi)
            {
                fviz_object_modified((FVizObject*)window);
                (void)fviz_object_invoke_event(
                    (FVizObject*)window, FVIZ_EVENT_WINDOW_DPI_CHANGED, &window->dpi);
            }
            if (suggested != NULL && window->host_native_handle == NULL &&
                window->offscreen == FVIZ_FALSE)
            {
                (void)SetWindowPos(
                    hwnd, NULL, suggested->left, suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            fviz_render_window_request_render(window);
            return 0;
        }
#if defined(WM_DPICHANGED_AFTERPARENT)
        case WM_DPICHANGED_AFTERPARENT:
        {
            const uint32_t previous_dpi = window->dpi;
            window->dpi = fviz_win32_query_window_dpi(hwnd);
            if (window->dpi != previous_dpi)
            {
                fviz_object_modified((FVizObject*)window);
                (void)fviz_object_invoke_event(
                    (FVizObject*)window, FVIZ_EVENT_WINDOW_DPI_CHANGED, &window->dpi);
            }
            if (window->host_native_handle != NULL)
                (void)fviz_render_window_sync_host_size(window);
            fviz_render_window_request_render(window);
            return 0;
        }
#endif
        case WM_SIZE:
        {
            FVizInteractionEvent event;
            const int new_width = (int)LOWORD(lparam);
            const int new_height = (int)HIWORD(lparam);
            const FVizBool size_changed = new_width != window->width || new_height != window->height
                ? FVIZ_TRUE : FVIZ_FALSE;
            window->width = new_width;
            window->height = new_height;
            if (size_changed != FVIZ_FALSE)
            {
                fviz_object_modified((FVizObject*)window);
                (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_WINDOW_RESIZE, NULL);
            }
            event = fviz_win32_interaction_event(FVIZ_INTERACTION_RESIZE, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            event.width = window->width;
            event.height = window->height;
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            if (window->width > 0 && window->height > 0)
                fviz_render_window_request_render(window);
            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_EXPOSE, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            BeginPaint(hwnd, &paint);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            (void)fviz_render_window_render(window);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_SETFOCUS:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_FOCUS_IN, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_WINDOW_FOCUS_IN, &event);
            return 0;
        }
        case WM_KILLFOCUS:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_FOCUS_OUT, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_WINDOW_FOCUS_OUT, &event);
            fviz_win32_cancel_pointer_interaction(window);
            return 0;
        }
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
            fviz_win32_cancel_pointer_interaction(window);
            return 0;
        case WM_LBUTTONDOWN:
        {
            FVizInteractionEvent event;
            window->left_mouse_down = FVIZ_TRUE;
            window->left_mouse_dragged = FVIZ_FALSE;
            window->last_mouse_x = GET_X_LPARAM(lparam);
            window->last_mouse_y = GET_Y_LPARAM(lparam);
            window->left_mouse_press_x = window->last_mouse_x;
            window->left_mouse_press_y = window->last_mouse_y;
            (void)SetFocus(hwnd);
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_LEFT,
                window->last_mouse_x, window->last_mouse_y);
            SetCapture(hwnd);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            return 0;
        }
        case WM_MBUTTONDOWN:
        {
            FVizInteractionEvent event;
            window->middle_mouse_down = FVIZ_TRUE;
            window->last_mouse_x = GET_X_LPARAM(lparam);
            window->last_mouse_y = GET_Y_LPARAM(lparam);
            (void)SetFocus(hwnd);
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_MIDDLE,
                window->last_mouse_x, window->last_mouse_y);
            SetCapture(hwnd);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            return 0;
        }
        case WM_RBUTTONDOWN:
        {
            FVizInteractionEvent event;
            window->right_mouse_down = FVIZ_TRUE;
            window->last_mouse_x = GET_X_LPARAM(lparam);
            window->last_mouse_y = GET_Y_LPARAM(lparam);
            (void)SetFocus(hwnd);
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, FVIZ_MOUSE_BUTTON_RIGHT,
                window->last_mouse_x, window->last_mouse_y);
            SetCapture(hwnd);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            return 0;
        }
        case WM_LBUTTONUP:
        {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_LEFT, x, y);
            window->left_mouse_down = FVIZ_FALSE;
            {
                const FVizBool handled = fviz_render_window_interactor_process_event(window->interactor, &event);
                fviz_win32_render_interaction_if_due(window, handled, FVIZ_TRUE);
            }
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
            if (window->middle_mouse_down == FVIZ_FALSE && window->right_mouse_down == FVIZ_FALSE &&
                window->x1_mouse_down == FVIZ_FALSE && window->x2_mouse_down == FVIZ_FALSE) ReleaseCapture();
            return 0;
        }
        case WM_MBUTTONUP:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_MIDDLE,
                GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            window->middle_mouse_down = FVIZ_FALSE;
            {
                const FVizBool handled = fviz_render_window_interactor_process_event(window->interactor, &event);
                fviz_win32_render_interaction_if_due(window, handled, FVIZ_TRUE);
            }
            if (window->left_mouse_down == FVIZ_FALSE && window->right_mouse_down == FVIZ_FALSE &&
                window->x1_mouse_down == FVIZ_FALSE && window->x2_mouse_down == FVIZ_FALSE) ReleaseCapture();
            return 0;
        }
        case WM_RBUTTONUP:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_UP, FVIZ_MOUSE_BUTTON_RIGHT,
                GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            window->right_mouse_down = FVIZ_FALSE;
            {
                const FVizBool handled = fviz_render_window_interactor_process_event(window->interactor, &event);
                fviz_win32_render_interaction_if_due(window, handled, FVIZ_TRUE);
            }
            if (window->left_mouse_down == FVIZ_FALSE && window->middle_mouse_down == FVIZ_FALSE &&
                window->x1_mouse_down == FVIZ_FALSE && window->x2_mouse_down == FVIZ_FALSE) ReleaseCapture();
            return 0;
        }
        case WM_XBUTTONDOWN:
        {
            const WORD native_button = GET_XBUTTON_WPARAM(wparam);
            const FVizMouseButton button = native_button == XBUTTON1
                ? FVIZ_MOUSE_BUTTON_X1 : FVIZ_MOUSE_BUTTON_X2;
            FVizInteractionEvent event;
            if (button == FVIZ_MOUSE_BUTTON_X1) window->x1_mouse_down = FVIZ_TRUE;
            else window->x2_mouse_down = FVIZ_TRUE;
            window->last_mouse_x = GET_X_LPARAM(lparam);
            window->last_mouse_y = GET_Y_LPARAM(lparam);
            (void)SetFocus(hwnd);
            SetCapture(hwnd);
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_DOWN, button,
                window->last_mouse_x, window->last_mouse_y);
            (void)fviz_render_window_interactor_process_event(window->interactor, &event);
            return TRUE;
        }
        case WM_XBUTTONUP:
        {
            const WORD native_button = GET_XBUTTON_WPARAM(wparam);
            const FVizMouseButton button = native_button == XBUTTON1
                ? FVIZ_MOUSE_BUTTON_X1 : FVIZ_MOUSE_BUTTON_X2;
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_BUTTON_UP, button,
                GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            if (button == FVIZ_MOUSE_BUTTON_X1) window->x1_mouse_down = FVIZ_FALSE;
            else window->x2_mouse_down = FVIZ_FALSE;
            {
                const FVizBool handled = fviz_render_window_interactor_process_event(window->interactor, &event);
                fviz_win32_render_interaction_if_due(window, handled, FVIZ_TRUE);
            }
            if (window->left_mouse_down == FVIZ_FALSE && window->middle_mouse_down == FVIZ_FALSE &&
                window->right_mouse_down == FVIZ_FALSE && window->x1_mouse_down == FVIZ_FALSE &&
                window->x2_mouse_down == FVIZ_FALSE) ReleaseCapture();
            return TRUE;
        }
        case WM_XBUTTONDBLCLK:
        {
            const WORD native_button = GET_XBUTTON_WPARAM(wparam);
            const FVizMouseButton button = native_button == XBUTTON1
                ? FVIZ_MOUSE_BUTTON_X1 : FVIZ_MOUSE_BUTTON_X2;
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_DOUBLE_CLICK, button,
                GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            {
                const FVizBool handled = fviz_render_window_interactor_process_event(window->interactor, &event);
                fviz_win32_render_interaction_if_due(window, handled, FVIZ_TRUE);
            }
            return TRUE;
        }
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        {
            FVizMouseButton button = message == WM_LBUTTONDBLCLK
                ? FVIZ_MOUSE_BUTTON_LEFT
                : (message == WM_MBUTTONDBLCLK ? FVIZ_MOUSE_BUTTON_MIDDLE : FVIZ_MOUSE_BUTTON_RIGHT);
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_DOUBLE_CLICK, button,
                GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            {
                const FVizBool handled = fviz_render_window_interactor_process_event(window->interactor, &event);
                fviz_win32_render_interaction_if_due(window, handled, FVIZ_TRUE);
            }
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
                const int total_dx = x - window->left_mouse_press_x;
                const int total_dy = y - window->left_mouse_press_y;
                if (abs(total_dx) > 2 || abs(total_dy) > 2) window->left_mouse_dragged = FVIZ_TRUE;
            }
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_MOVE, FVIZ_MOUSE_BUTTON_NONE, x, y);
            event.delta_x = dx;
            event.delta_y = dy;
            {
                const FVizBool handled = fviz_render_window_interactor_process_event(window->interactor, &event);
                fviz_win32_render_interaction_if_due(window, handled, FVIZ_FALSE);
            }
            window->last_mouse_x = x;
            window->last_mouse_y = y;
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
            POINT client_point;
            FVizInteractionEvent event;
            client_point.x = GET_X_LPARAM(lparam);
            client_point.y = GET_Y_LPARAM(lparam);
            (void)ScreenToClient(hwnd, &client_point);
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_WHEEL, FVIZ_MOUSE_BUTTON_NONE,
                (int)client_point.x, (int)client_point.y);
            event.wheel_delta = (float)delta / (float)WHEEL_DELTA;
            event.delta_y = (int)delta;
            {
                const FVizBool handled = fviz_render_window_interactor_process_event(window->interactor, &event);
                fviz_win32_render_interaction_if_due(window, handled, FVIZ_TRUE);
            }
            return 0;
        }
        case WM_MOUSEHWHEEL:
        {
            const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
            POINT client_point;
            FVizInteractionEvent event;
            client_point.x = GET_X_LPARAM(lparam);
            client_point.y = GET_Y_LPARAM(lparam);
            (void)ScreenToClient(hwnd, &client_point);
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_MOUSE_WHEEL, FVIZ_MOUSE_BUTTON_NONE,
                (int)client_point.x, (int)client_point.y);
            /* Horizontal wheel is exposed through delta_x. Keep wheel_delta
             * zero so camera/actor dolly styles do not treat tilt as zoom. */
            event.wheel_delta = 0.0f;
            event.delta_x = (int)delta;
            {
                const FVizBool handled = fviz_render_window_interactor_process_event(window->interactor, &event);
                fviz_win32_render_interaction_if_due(window, handled, FVIZ_TRUE);
            }
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
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_KEY_DOWN, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            event.key = wparam == VK_ESCAPE ? FVIZ_KEY_ESCAPE : (int)wparam;
            if (fviz_render_window_interactor_process_event(window->interactor, &event) == FVIZ_TRUE)
            {
                if (event.key == FVIZ_KEY_ESCAPE && window->host_native_handle != NULL)
                    fviz_win32_cancel_pointer_interaction(window);
                fviz_win32_render_interaction_if_due(window, FVIZ_TRUE, FVIZ_TRUE);
                return 0;
            }
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            FVizInteractionEvent event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_KEY_UP, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            event.key = wparam == VK_ESCAPE ? FVIZ_KEY_ESCAPE : (int)wparam;
            if (fviz_render_window_interactor_process_event(window->interactor, &event) == FVIZ_TRUE)
                return 0;
            break;
        }
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_UNICHAR:
        {
            FVizInteractionEvent event;
            if (message == WM_UNICHAR && wparam == UNICODE_NOCHAR) return TRUE;
            event = fviz_win32_interaction_event(
                FVIZ_INTERACTION_CHAR, FVIZ_MOUSE_BUTTON_NONE, 0, 0);
            event.character = (unsigned int)wparam;
            if (fviz_render_window_interactor_process_event(window->interactor, &event) == FVIZ_TRUE)
                return 0;
            if (message == WM_CHAR) return 0;
            break;
        }
        case WM_CLOSE:
            if (window->close_requested == FVIZ_FALSE)
            {
                window->close_requested = FVIZ_TRUE;
                (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_WINDOW_CLOSE, NULL);
            }
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
