#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizProvenance.h>
#include <FViz/Interaction/FVizRenderWindowInteractor.h>
#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizRenderWindowInteractorPrivate.h>
#include <FViz/Rendering/FVizCamera.h>
#include <FViz/Rendering/FVizGlyphMapper.h>
#include <FViz/Rendering/FVizGLDevice.h>
#include <FViz/Rendering/FVizRenderWindowPrivate.h>
#include <FViz/Rendering/FVizScene.h>
#include <FViz/Spatial/FVizBVH.h>

static void fviz_render_window_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_render_window_class = {FVIZ_TYPE_RENDER_WINDOW, "FVizRenderWindow",
                                                           &g_fviz_object_class, fviz_render_window_destroy, NULL};

static FVizBool fviz_render_window_renderer_modified(FVizObject* caller, FVizEventId event_id, void* call_data,
                                                     void* client_data)
{
    FVizRenderWindow* window = (FVizRenderWindow*)client_data;
    FVIZ_UNUSED(caller);
    FVIZ_UNUSED(event_id);
    FVIZ_UNUSED(call_data);
    if (window != NULL) fviz_render_window_request_render_reason(window, FVIZ_RENDER_REQUEST_SCENE);
    return FVIZ_FALSE;
}

static void fviz_render_window_destroy(FVizObject* object)
{
    FVizRenderWindow* window = (FVizRenderWindow*)object;
    FVizSize i;
    fviz_internal_render_window_destroy_platform(window);
    fviz_internal_render_window_interactor_detach(window->interactor);
    fviz_release(window->interactor);
    window->interactor = NULL;
    for (i = 0u; i < fviz_array_count(window->renderers); ++i)
    {
        FVizRenderer* renderer = *(FVizRenderer**)fviz_array_at(window->renderers, i);
        FVizObserverTag* tag = window->renderer_modified_tags != NULL
                                   ? (FVizObserverTag*)fviz_array_at(window->renderer_modified_tags, i)
                                   : NULL;
        if (renderer != NULL && tag != NULL && *tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)renderer, *tag);
        fviz_release(renderer);
    }
    fviz_release(window->renderer_modified_tags);
    fviz_release(window->renderers);
    fviz_release(window->pass_statistics);
    window->renderer_modified_tags = NULL;
    window->renderers = NULL;
    window->pass_statistics = NULL;
    window->renderer = NULL;
    fviz_release(window->pick_bvh);
    window->pick_bvh = NULL;
    fviz_release((FVizPolyData*)window->pick_poly_data);
    window->pick_poly_data = NULL;
    fviz_free(window->title);
    window->title = NULL;
}

static FVizResult fviz_render_window_create_internal(int width, int height, const char* title, FVizBool offscreen,
                                                     void* host_native_handle,
                                                     const FVizExternalOpenGLSurface* external_surface,
                                                     const FVizRenderWindowOptions* options,
                                                     FVizRenderWindow** out_window)
{
    FVizRenderWindow* window;
    FVizSize title_length;
    FVizResult result;
    if (out_window == NULL || width <= 0 || height <= 0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "render window requires positive dimensions and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_window = NULL;
    if (title == NULL) title = "FEAViz";
    window =
        (FVizRenderWindow*)fviz_internal_object_allocate(sizeof(FVizRenderWindow), &g_fviz_render_window_class, NULL);
    if (window == NULL) return fviz_last_error_code();
    title_length = (FVizSize)strlen(title);
    window->title = (char*)fviz_alloc(title_length + 1u);
    if (window->title == NULL)
    {
        fviz_release(window);
        return fviz_last_error_code();
    }
    (void)memcpy(window->title, title, title_length + 1u);
    window->width = width;
    window->height = height;
    window->dpi = 96u;
    window->requested_multisamples = options != NULL ? options->multisamples : 4u;
    window->fxaa_enabled = options != NULL ? options->fxaa : FVIZ_TRUE;
    fviz_fxaa_options_initialize(&window->fxaa_options);
    window->adaptive_antialiasing =
        options != NULL ? (options->adaptive_antialiasing != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE) : FVIZ_TRUE;
    window->swap_interval = options != NULL ? options->swap_interval : 1;
    window->srgb_enabled = FVIZ_TRUE;
    if (options != NULL && (options->struct_size == 0u ||
                            options->struct_size >= offsetof(FVizRenderWindowOptions, srgb) + sizeof(options->srgb)))
        window->srgb_enabled = options->srgb != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    window->last_statistics.struct_size = (uint32_t)sizeof(window->last_statistics);
    fviz_frame_scheduler_options_initialize(&window->frame_scheduler_options);
    fviz_gpu_memory_options_initialize(&window->gpu_memory_options);
    window->frame_scheduler_statistics.struct_size = (uint32_t)sizeof(window->frame_scheduler_statistics);
    window->offscreen = offscreen;
    window->host_native_handle = host_native_handle;
    if (external_surface != NULL)
    {
        window->external_surface = *external_surface;
        window->external_surface.struct_size = (uint32_t)sizeof(window->external_surface);
        window->external_opengl = FVIZ_TRUE;
    }
    window->state = FVIZ_RENDER_WINDOW_CREATED;
    if (fviz_array_create(sizeof(FVizRenderer*), &window->renderers) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizObserverTag), &window->renderer_modified_tags) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizRenderPassStatistics), &window->pass_statistics) != FVIZ_OK)
    {
        fviz_release(window);
        return fviz_last_error_code();
    }
    {
        FVizRenderer* default_renderer = NULL;
        if (fviz_renderer_create(&default_renderer) != FVIZ_OK ||
            fviz_render_window_add_renderer(window, default_renderer) != FVIZ_OK)
        {
            fviz_release(default_renderer);
            fviz_release(window);
            return fviz_last_error_code();
        }
        fviz_release(default_renderer);
    }
    if (fviz_internal_render_window_interactor_create(window, &window->interactor) != FVIZ_OK)
    {
        fviz_release(window);
        return fviz_last_error_code();
    }
    result = fviz_internal_render_window_create_platform(window);
    if (result != FVIZ_OK)
    {
        fviz_release(window);
        return result;
    }
    window->state = offscreen != FVIZ_FALSE ? FVIZ_RENDER_WINDOW_OFFSCREEN : FVIZ_RENDER_WINDOW_INITIALIZED;
    *out_window = window;
    return FVIZ_OK;
}

void fviz_fxaa_options_initialize(FVizFXAAOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->relative_threshold = 0.125f;
    options->absolute_threshold = 0.0312f;
    options->span_max = 8.0f;
}

void fviz_render_window_options_initialize(FVizRenderWindowOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->multisamples = 4u;
    options->fxaa = FVIZ_TRUE;
    options->swap_interval = 1;
    options->adaptive_antialiasing = FVIZ_TRUE;
    options->srgb = FVIZ_TRUE;
}

void fviz_frame_scheduler_options_initialize(FVizFrameSchedulerOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->interactive_quality = FVIZ_TRUE;
}

void fviz_gpu_memory_options_initialize(FVizGPUMemoryOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
}

static FVizResult fviz_render_window_validate_options(const FVizRenderWindowOptions* options)
{
    if (options == NULL) return FVIZ_OK;
    if (options->struct_size != 0u && options->struct_size < offsetof(FVizRenderWindowOptions, srgb))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (options->multisamples > 32u) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (options->swap_interval < -1 || options->swap_interval > 4) return FVIZ_ERROR_INVALID_ARGUMENT;
    return FVIZ_OK;
}

FVizResult fviz_render_window_create_with_options(int width, int height, const char* title,
                                                  const FVizRenderWindowOptions* options, FVizRenderWindow** out_window)
{
    if (fviz_render_window_validate_options(options) != FVIZ_OK) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_render_window_create_internal(width, height, title, FVIZ_FALSE, NULL, NULL, options, out_window);
}

FVizResult fviz_render_window_create_offscreen_with_options(int width, int height,
                                                            const FVizRenderWindowOptions* options,
                                                            FVizRenderWindow** out_window)
{
    if (fviz_render_window_validate_options(options) != FVIZ_OK) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_render_window_create_internal(width, height, "FEAViz Offscreen", FVIZ_TRUE, NULL, NULL, options,
                                              out_window);
}

FVizResult fviz_render_window_create(int width, int height, const char* title, FVizRenderWindow** out_window)
{
    FVizRenderWindowOptions options;
    fviz_render_window_options_initialize(&options);
    return fviz_render_window_create_with_options(width, height, title, &options, out_window);
}

FVizResult fviz_render_window_create_offscreen(int width, int height, FVizRenderWindow** out_window)
{
    FVizRenderWindowOptions options;
    fviz_render_window_options_initialize(&options);
    return fviz_render_window_create_offscreen_with_options(width, height, &options, out_window);
}

FVizResult fviz_render_window_create_attached_with_options(void* host_native_handle, int width, int height,
                                                           const FVizRenderWindowOptions* options,
                                                           FVizRenderWindow** out_window)
{
    if (host_native_handle == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "attached render window requires a host native handle");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_render_window_validate_options(options) != FVIZ_OK) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_render_window_create_internal(width, height, "FEAViz Child", FVIZ_FALSE, host_native_handle, NULL,
                                              options, out_window);
}

FVizResult fviz_render_window_create_attached(void* host_native_handle, int width, int height,
                                              FVizRenderWindow** out_window)
{
    FVizRenderWindowOptions options;
    fviz_render_window_options_initialize(&options);
    return fviz_render_window_create_attached_with_options(host_native_handle, width, height, &options, out_window);
}

FVizResult fviz_render_window_create_external_opengl_with_options(int width, int height,
                                                                  const FVizExternalOpenGLSurface* surface,
                                                                  const FVizRenderWindowOptions* options,
                                                                  FVizRenderWindow** out_window)
{
    FVizResult result;
    result = fviz_external_opengl_surface_validate(surface);
    if (result != FVIZ_OK) return result;
    result = fviz_render_window_validate_options(options);
    if (result != FVIZ_OK) return result;
    return fviz_render_window_create_internal(width, height, "FEAViz External OpenGL", FVIZ_FALSE, NULL, surface,
                                              options, out_window);
}

FVizResult fviz_render_window_create_external_opengl(int width, int height, const FVizExternalOpenGLSurface* surface,
                                                     FVizRenderWindow** out_window)
{
    FVizRenderWindowOptions options;
    fviz_render_window_options_initialize(&options);
    return fviz_render_window_create_external_opengl_with_options(width, height, surface, &options, out_window);
}

FVizResult fviz_render_window_sync_external_surface_size(FVizRenderWindow* window)
{
    int width;
    int height;
    FVizResult result;
    if (window == NULL || window->external_opengl == FVIZ_FALSE) return FVIZ_ERROR_INVALID_STATE;
    if (window->external_surface.get_framebuffer_size == NULL) return FVIZ_OK;
    width = window->width;
    height = window->height;
    result = window->external_surface.get_framebuffer_size(window->external_surface.user_data, &width, &height);
    if (result != FVIZ_OK) return result;
    if (width <= 0 || height <= 0) return FVIZ_ERROR_INVALID_STATE;
    return fviz_render_window_resize(window, width, height);
}

FVizBool fviz_render_window_is_external_opengl(const FVizRenderWindow* window)
{
    return window != NULL && window->external_opengl != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizResult fviz_render_window_release_external_opengl_resources(FVizRenderWindow* window)
{
    if (window == NULL || window->external_opengl == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must be an external OpenGL render window");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (window->render_in_progress != FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                "external OpenGL resources cannot be released during rendering");
        return FVIZ_ERROR_INVALID_STATE;
    }
    return fviz_internal_render_window_release_external_opengl_platform(window);
}

FVizResult fviz_render_window_reinitialize_external_opengl(FVizRenderWindow* window)
{
    FVizResult result;
    if (window == NULL || window->external_opengl == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must be an external OpenGL render window");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (window->render_in_progress != FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                "external OpenGL context cannot be reinitialized during rendering");
        return FVIZ_ERROR_INVALID_STATE;
    }
    result = fviz_internal_render_window_reinitialize_external_opengl_platform(window);
    if (result == FVIZ_OK) fviz_render_window_request_render(window);
    return result;
}

FVizRenderWindowState fviz_render_window_state(const FVizRenderWindow* window)
{
    return window != NULL ? window->state : FVIZ_RENDER_WINDOW_FINALIZED;
}

FVizResult fviz_render_window_initialize(FVizRenderWindow* window)
{
    FVizResult result;
    if (window == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (window->state != FVIZ_RENDER_WINDOW_CREATED && window->state != FVIZ_RENDER_WINDOW_FINALIZED) return FVIZ_OK;
    window->close_requested = FVIZ_FALSE;
    result = fviz_internal_render_window_create_platform(window);
    if (result == FVIZ_OK)
        window->state = window->offscreen != FVIZ_FALSE ? FVIZ_RENDER_WINDOW_OFFSCREEN : FVIZ_RENDER_WINDOW_INITIALIZED;
    return result;
}

void fviz_render_window_finalize(FVizRenderWindow* window)
{
    if (window == NULL || window->state == FVIZ_RENDER_WINDOW_FINALIZED) return;
    fviz_internal_render_window_destroy_platform(window);
    window->visible = FVIZ_FALSE;
    window->state = FVIZ_RENDER_WINDOW_FINALIZED;
}

FVizResult fviz_render_window_resize(FVizRenderWindow* window, int width, int height)
{
    if (window == NULL || width <= 0 || height <= 0) return FVIZ_ERROR_INVALID_ARGUMENT;
    window->width = width;
    window->height = height;
    fviz_object_modified((FVizObject*)window);
    (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_WINDOW_RESIZE, NULL);
    if (window->state == FVIZ_RENDER_WINDOW_CREATED || window->state == FVIZ_RENDER_WINDOW_FINALIZED) return FVIZ_OK;
    return fviz_internal_render_window_resize_platform(window);
}

FVizResult fviz_render_window_sync_host_size(FVizRenderWindow* window)
{
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (window->host_native_handle == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "render window is not attached to a native host");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (window->state == FVIZ_RENDER_WINDOW_FINALIZED) return FVIZ_ERROR_INVALID_STATE;
    return fviz_internal_render_window_sync_host_size_platform(window);
}

FVizResult fviz_render_window_reparent(FVizRenderWindow* window, void* host_native_handle)
{
    FVizResult result;
    uint32_t previous_dpi;
    void* previous_host;
    FVizBool native_recreated = FVIZ_FALSE;
    if (window == NULL || host_native_handle == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window and host_native_handle must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (window->host_native_handle == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "render window was not created as an attached window");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (window->host_native_handle == host_native_handle && window->native_window != NULL &&
        window->state != FVIZ_RENDER_WINDOW_FINALIZED)
        return fviz_render_window_sync_host_size(window);

    previous_host = window->host_native_handle;
    previous_dpi = window->dpi;

    /* A GUI toolkit may destroy an old native parent before publishing the new
     * handle. Windows destroys child HWNDs with their parent, which finalizes
     * our platform surface. Reparent therefore also acts as a recovery path:
     * keep the FEAViz object graph, recreate only native/WGL resources, then
     * continue against the replacement host. */
    if (window->native_window == NULL || window->state == FVIZ_RENDER_WINDOW_FINALIZED)
    {
        window->host_native_handle = host_native_handle;
        result = fviz_render_window_initialize(window);
        if (result != FVIZ_OK)
        {
            window->host_native_handle = previous_host;
            return result;
        }
        native_recreated = FVIZ_TRUE;
    }
    else
    {
        result = fviz_internal_render_window_reparent_platform(window, host_native_handle);
        if (result != FVIZ_OK) return result;
        window->host_native_handle = host_native_handle;
    }

    fviz_object_modified((FVizObject*)window);
    if (window->dpi != previous_dpi)
    {
        (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_WINDOW_DPI_CHANGED, &window->dpi);
    }
    (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_WINDOW_REPARENTED, host_native_handle);
    result = fviz_render_window_sync_host_size(window);
    if (result == FVIZ_OK && native_recreated != FVIZ_FALSE && window->visible != FVIZ_FALSE)
        result = fviz_internal_render_window_show_platform(window);
    return result;
}

FVizResult fviz_render_window_read_rgba8(FVizRenderWindow* window, uint8_t* pixels, FVizSize capacity)
{
    FVizSize pixel_count;
    FVizSize required;
    if (window == NULL || pixels == NULL || window->state == FVIZ_RENDER_WINDOW_FINALIZED)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_size_multiply((FVizSize)window->width, (FVizSize)window->height, &pixel_count) != FVIZ_OK ||
        fviz_size_multiply(pixel_count, 4u, &required) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    if (capacity < required) return FVIZ_ERROR_OVERFLOW;
    return fviz_internal_render_window_read_rgba8_platform(window, pixels);
}

FVizResult fviz_render_window_read_depth_f32(FVizRenderWindow* window, float* depth, FVizSize capacity)
{
    FVizSize pixel_count;
    if (window == NULL || depth == NULL || window->state == FVIZ_RENDER_WINDOW_FINALIZED)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_size_multiply((FVizSize)window->width, (FVizSize)window->height, &pixel_count) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    if (capacity < pixel_count) return FVIZ_ERROR_OVERFLOW;
    return fviz_internal_render_window_read_depth_f32_platform(window, depth);
}

FVizResult fviz_render_window_write_ppm(FVizRenderWindow* window, const char* path)
{
    FVizSize pixel_count;
    FVizSize rgba_size;
    FVizSize rgb_size;
    uint8_t* rgba;
    uint8_t* rgb;
    FILE* file;
    FVizBool write_failed = FVIZ_FALSE;
    int x;
    int y;
    if (window == NULL || path == NULL || path[0] == '\0') return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_size_multiply((FVizSize)window->width, (FVizSize)window->height, &pixel_count) != FVIZ_OK ||
        fviz_size_multiply(pixel_count, 4u, &rgba_size) != FVIZ_OK ||
        fviz_size_multiply(pixel_count, 3u, &rgb_size) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    rgba = (uint8_t*)fviz_alloc(rgba_size);
    rgb = (uint8_t*)fviz_alloc(rgb_size);
    if (rgba == NULL || rgb == NULL)
    {
        fviz_free(rgb);
        fviz_free(rgba);
        return FVIZ_ERROR_OUT_OF_MEMORY;
    }
    if (fviz_render_window_read_rgba8(window, rgba, rgba_size) != FVIZ_OK)
    {
        fviz_free(rgb);
        fviz_free(rgba);
        return fviz_last_error_code();
    }
    for (y = 0; y < window->height; ++y)
    {
        const int source_y = window->height - y - 1;
        for (x = 0; x < window->width; ++x)
        {
            const FVizSize source = ((FVizSize)source_y * (FVizSize)window->width + (FVizSize)x) * 4u;
            const FVizSize destination = ((FVizSize)y * (FVizSize)window->width + (FVizSize)x) * 3u;
            rgb[destination + 0u] = rgba[source + 0u];
            rgb[destination + 1u] = rgba[source + 1u];
            rgb[destination + 2u] = rgba[source + 2u];
        }
    }
#if defined(_MSC_VER)
    if (fopen_s(&file, path, "wb") != 0) file = NULL;
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL)
    {
        fviz_free(rgb);
        fviz_free(rgba);
        return FVIZ_ERROR_IO;
    }
    if (fprintf(file, "P6\n%d %d\n255\n", window->width, window->height) < 0) write_failed = FVIZ_TRUE;
    if (write_failed == FVIZ_FALSE && fwrite(rgb, 1u, (size_t)rgb_size, file) != (size_t)rgb_size)
        write_failed = FVIZ_TRUE;
    if (fclose(file) != 0) write_failed = FVIZ_TRUE;
    if (write_failed != FVIZ_FALSE)
    {
        fviz_free(rgb);
        fviz_free(rgba);
        return FVIZ_ERROR_IO;
    }
    fviz_free(rgb);
    fviz_free(rgba);
    return FVIZ_OK;
}

void fviz_render_window_get_capabilities(const FVizRenderWindow* window, FVizRenderCapabilities* out_capabilities)
{
    if (out_capabilities == NULL) return;
    (void)memset(out_capabilities, 0, sizeof(*out_capabilities));
    out_capabilities->struct_size = (uint32_t)sizeof(*out_capabilities);
    if (window == NULL || window->state == FVIZ_RENDER_WINDOW_FINALIZED) return;
    out_capabilities->gl_major = window->gl_modern != FVIZ_FALSE ? 3u : 1u;
    out_capabilities->gl_minor = window->gl_modern != FVIZ_FALSE ? 3u : 1u;
    out_capabilities->modern_pipeline = window->gl_modern;
    out_capabilities->offscreen_supported = FVIZ_TRUE;
    out_capabilities->color_readback_supported = FVIZ_TRUE;
    out_capabilities->depth_readback_supported = FVIZ_TRUE;
    out_capabilities->multisample_supported =
        window->gl_modern != FVIZ_FALSE || window->actual_multisamples > 1u ? FVIZ_TRUE : FVIZ_FALSE;
    out_capabilities->fxaa_supported = window->fxaa_supported;
    out_capabilities->swap_control_supported = window->swap_control_supported;
    out_capabilities->sample_count = window->actual_multisamples;
    out_capabilities->srgb_supported = window->srgb_supported;
    out_capabilities->weighted_oit_supported = window->weighted_oit_supported;
    out_capabilities->shader_lines_supported = window->shader_lines_supported;
    out_capabilities->text_rendering_supported = window->text_rendering_supported;
    out_capabilities->integer_selection_supported = window->integer_selection_supported;
    out_capabilities->gpu_timing_supported = window->gpu_timing_supported;
    out_capabilities->depth_peeling_supported = window->depth_peeling_supported;
}

FVizResult fviz_render_window_set_multisamples(FVizRenderWindow* window, uint32_t samples)
{
    if (window == NULL || samples > 32u) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (window->state != FVIZ_RENDER_WINDOW_CREATED && window->state != FVIZ_RENDER_WINDOW_FINALIZED)
    {
        fviz_internal_set_error(
            FVIZ_ERROR_INVALID_STATE,
            "multisample pixel format can only be changed before initialization; finalize the window first");
        return FVIZ_ERROR_INVALID_STATE;
    }
    window->requested_multisamples = samples;
    return FVIZ_OK;
}

uint32_t fviz_render_window_multisamples(const FVizRenderWindow* window)
{
    return window != NULL ? window->requested_multisamples : 0u;
}

uint32_t fviz_render_window_actual_multisamples(const FVizRenderWindow* window)
{
    return window != NULL ? window->actual_multisamples : 0u;
}

void fviz_render_window_set_fxaa(FVizRenderWindow* window, FVizBool enabled)
{
    if (window == NULL) return;
    window->fxaa_enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_object_modified((FVizObject*)window);
}

FVizBool fviz_render_window_fxaa(const FVizRenderWindow* window)
{
    return window != NULL ? window->fxaa_enabled : FVIZ_FALSE;
}

FVizResult fviz_render_window_set_fxaa_options(FVizRenderWindow* window, const FVizFXAAOptions* options)
{
    if (window == NULL || options == NULL ||
        (options->struct_size != 0u && options->struct_size < sizeof(FVizFXAAOptions)) ||
        options->relative_threshold < 0.0f || options->relative_threshold > 1.0f ||
        options->absolute_threshold < 0.0f || options->absolute_threshold > 1.0f || options->span_max < 1.0f ||
        options->span_max > 32.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    window->fxaa_options = *options;
    window->fxaa_options.struct_size = (uint32_t)sizeof(window->fxaa_options);
    fviz_object_modified((FVizObject*)window);
    return FVIZ_OK;
}

void fviz_render_window_get_fxaa_options(const FVizRenderWindow* window, FVizFXAAOptions* out_options)
{
    if (out_options == NULL) return;
    fviz_fxaa_options_initialize(out_options);
    if (window != NULL) *out_options = window->fxaa_options;
}

void fviz_render_window_set_adaptive_antialiasing(FVizRenderWindow* window, FVizBool enabled)
{
    if (window == NULL) return;
    window->adaptive_antialiasing = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_object_modified((FVizObject*)window);
}

FVizBool fviz_render_window_adaptive_antialiasing(const FVizRenderWindow* window)
{
    return window != NULL ? window->adaptive_antialiasing : FVIZ_FALSE;
}

FVizBool fviz_render_window_interaction_active(const FVizRenderWindow* window)
{
    return window != NULL ? window->interaction_active : FVIZ_FALSE;
}

void fviz_internal_render_window_set_interaction_active(FVizRenderWindow* window, FVizBool active)
{
    FVizBool normalized;
    if (window == NULL) return;
    normalized = active != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (window->interaction_active == normalized) return;
    window->interaction_active = normalized;
    fviz_render_window_request_render_reason(
        window, normalized != FVIZ_FALSE ? FVIZ_RENDER_REQUEST_INTERACTION
                                         : (FVIZ_RENDER_REQUEST_INTERACTION | FVIZ_RENDER_REQUEST_SCENE));
}

FVizResult fviz_render_window_set_swap_interval(FVizRenderWindow* window, int interval)
{
    if (window == NULL || interval < -1 || interval > 4) return FVIZ_ERROR_INVALID_ARGUMENT;
    window->swap_interval = interval;
    if (window->state == FVIZ_RENDER_WINDOW_INITIALIZED || window->state == FVIZ_RENDER_WINDOW_VISIBLE ||
        window->state == FVIZ_RENDER_WINDOW_OFFSCREEN)
        return fviz_internal_render_window_set_swap_interval_platform(window, interval);
    return FVIZ_OK;
}

int fviz_render_window_swap_interval(const FVizRenderWindow* window)
{
    return window != NULL ? window->swap_interval : 0;
}

void fviz_render_window_set_srgb(FVizRenderWindow* window, FVizBool enabled)
{
    if (window == NULL) return;
    window->srgb_enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_object_modified((FVizObject*)window);
}

FVizBool fviz_render_window_srgb(const FVizRenderWindow* window)
{
    return window != NULL ? window->srgb_enabled : FVIZ_FALSE;
}

void fviz_render_window_get_statistics(const FVizRenderWindow* window, FVizRenderStatistics* out_statistics)
{
    if (out_statistics == NULL) return;
    (void)memset(out_statistics, 0, sizeof(*out_statistics));
    out_statistics->struct_size = (uint32_t)sizeof(*out_statistics);
    if (window != NULL)
    {
        const uint32_t struct_size = out_statistics->struct_size;
        *out_statistics = window->last_statistics;
        out_statistics->struct_size = struct_size;
    }
}

FVizSize fviz_render_window_pass_statistics_count(const FVizRenderWindow* window)
{
    return window != NULL && window->pass_statistics != NULL ? fviz_array_count(window->pass_statistics) : 0u;
}

FVizResult fviz_render_window_get_pass_statistics(const FVizRenderWindow* window, FVizSize index,
                                                  FVizRenderPassStatistics* out_statistics)
{
    const FVizRenderPassStatistics* statistics;
    if (window == NULL || out_statistics == NULL || window->pass_statistics == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    statistics = (const FVizRenderPassStatistics*)fviz_array_const_at(window->pass_statistics, index);
    if (statistics == NULL) return FVIZ_ERROR_NOT_FOUND;
    *out_statistics = *statistics;
    out_statistics->struct_size = (uint32_t)sizeof(*out_statistics);
    return FVIZ_OK;
}

void fviz_internal_render_window_clear_pass_statistics(FVizRenderWindow* window)
{
    if (window != NULL && window->pass_statistics != NULL) fviz_array_clear(window->pass_statistics);
}

void fviz_internal_render_window_record_pass_statistics(FVizRenderWindow* window, FVizRenderer* renderer,
                                                        FVizRenderGraphPassId graph_pass_id, FVizSize execution_index,
                                                        FVizRenderPass* pass, const char* name, double cpu_seconds,
                                                        FVizResult result)
{
    FVizRenderPassStatistics statistics;
    FVizSize name_length;
    if (window == NULL || window->pass_statistics == NULL || pass == NULL) return;
    (void)memset(&statistics, 0, sizeof(statistics));
    statistics.struct_size = (uint32_t)sizeof(statistics);
    statistics.renderer = renderer;
    statistics.graph_pass_id = graph_pass_id;
    statistics.execution_index = execution_index;
    statistics.stage = fviz_render_pass_stage(pass);
    statistics.cpu_seconds = cpu_seconds >= 0.0 ? cpu_seconds : 0.0;
    statistics.result = result;
    if (name == NULL) name = "pass";
    name_length = (FVizSize)strlen(name);
    if (name_length >= FVIZ_RENDER_PASS_STATISTICS_NAME_CAPACITY)
        name_length = FVIZ_RENDER_PASS_STATISTICS_NAME_CAPACITY - 1u;
    (void)memcpy(statistics.name, name, (size_t)name_length);
    statistics.name[name_length] = '\0';
    (void)fviz_array_push(window->pass_statistics, &statistics);
}

static void fviz_render_window_pick_event_initialize(FVizPickEventData* event_data, int x, int y,
                                                     FVizSelectionAssociation association, FVizBool hardware)
{
    if (event_data == NULL) return;
    (void)memset(event_data, 0, sizeof(*event_data));
    event_data->struct_size = (uint32_t)sizeof(*event_data);
    event_data->x = x;
    event_data->y = y;
    event_data->association = association;
    event_data->hardware = hardware;
    event_data->result = FVIZ_OK;
    event_data->hardware_pick.struct_size = (uint32_t)sizeof(event_data->hardware_pick);
    event_data->hardware_pick.rendered_primitive_id = SIZE_MAX;
    event_data->hardware_pick.original_cell_id = FVIZ_INVALID_ID;
    event_data->hardware_pick.original_face_id = FVIZ_INVALID_ID;
    event_data->hardware_pick.association = association;
}

static FVizBool fviz_render_window_pick_event_begin(FVizRenderWindow* window, FVizPickEventData* event_data)
{
    if (fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_START_PICK, event_data) != FVIZ_FALSE)
    {
        event_data->result = FVIZ_ERROR_BUSY;
        (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_END_PICK, event_data);
        fviz_internal_set_error(FVIZ_ERROR_BUSY, "pick was aborted by a StartPickEvent observer");
        return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

static void fviz_render_window_pick_event_end(FVizRenderWindow* window, FVizPickEventData* event_data)
{
    if (event_data->result == FVIZ_OK) (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_PICK, event_data);
    (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_END_PICK, event_data);
}

FVizResult fviz_render_window_hardware_pick(FVizRenderWindow* window, int x, int y, FVizHardwarePick* out_pick)
{
    return fviz_render_window_hardware_pick_association(window, x, y, FVIZ_SELECTION_CELL, out_pick);
}

FVizResult fviz_render_window_hardware_pick_association(FVizRenderWindow* window, int x, int y,
                                                        FVizSelectionAssociation association,
                                                        FVizHardwarePick* out_pick)
{
    FVizRenderer* renderer = NULL;
    FVizSize actor_index = 0u;
    FVizSize primitive_id = 0u;
    FVizActor* actor = NULL;
    const FVizPolyData* poly_data;
    FVizResult result = FVIZ_OK;
    FVizPickEventData event_data;
    if (window == NULL || out_pick == NULL || x < 0 || y < 0 || x >= window->width || y >= window->height ||
        association < FVIZ_SELECTION_ACTOR || association > FVIZ_SELECTION_GLYPH_INSTANCE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    (void)memset(out_pick, 0, sizeof(*out_pick));
    out_pick->struct_size = (uint32_t)sizeof(*out_pick);
    out_pick->rendered_primitive_id = SIZE_MAX;
    out_pick->original_cell_id = FVIZ_INVALID_ID;
    out_pick->original_face_id = FVIZ_INVALID_ID;
    out_pick->association = association;
    fviz_render_window_pick_event_initialize(&event_data, x, y, association, FVIZ_TRUE);
    if (fviz_render_window_pick_event_begin(window, &event_data) != FVIZ_FALSE) return FVIZ_ERROR_BUSY;
    renderer = fviz_render_window_find_renderer(window, x, y);
    if (renderer == NULL)
    {
        result = FVIZ_ERROR_NOT_FOUND;
        goto done;
    }
    result = fviz_renderer_update(renderer);
    if (result != FVIZ_OK) goto done;
    result = fviz_internal_render_window_hardware_pick_platform(window, renderer, x, y, association, &actor_index,
                                                                &primitive_id, &out_pick->depth);
    if (result != FVIZ_OK) goto done;
    actor = fviz_scene_actor(fviz_renderer_scene(renderer), actor_index);
    if (actor == NULL || fviz_actor_pickable(actor) == FVIZ_FALSE)
    {
        result = FVIZ_ERROR_NOT_FOUND;
        goto done;
    }
    poly_data = fviz_actor_const_poly_data(actor);
    out_pick->renderer = renderer;
    out_pick->actor = actor;
    out_pick->rendered_primitive_id = primitive_id;
    if (association == FVIZ_SELECTION_CELL)
    {
        if (poly_data != NULL)
        {
            const FVizAttributeSet* cell_data = fviz_poly_data_const_cell_data(poly_data);
            (void)fviz_provenance_resolve(cell_data, FVIZ_PROVENANCE_CELL, primitive_id, (FVizId)primitive_id,
                                          &out_pick->original_cell_id, NULL);
            (void)fviz_provenance_resolve(cell_data, FVIZ_PROVENANCE_FACE, primitive_id, FVIZ_INVALID_ID,
                                          &out_pick->original_face_id, NULL);
        }
    }

done:
    event_data.result = result;
    event_data.hardware_pick = *out_pick;
    fviz_render_window_pick_event_end(window, &event_data);
    if (result == FVIZ_OK)
    {
        (void)fviz_object_invoke_event((FVizObject*)renderer, FVIZ_EVENT_PICK, &event_data);
        (void)fviz_object_invoke_event((FVizObject*)actor, FVIZ_EVENT_PICK, &event_data);
    }
    return result;
}

FVizResult fviz_render_window_set_renderer(FVizRenderWindow* window, FVizRenderer* renderer)
{
    FVizSize i;
    FVizResult result;
    if (window == NULL || renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window and renderer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    /* Keep the replacement alive when it is already owned by this window. */
    if (fviz_retain(renderer) == NULL) return fviz_last_error_code();
    for (i = 0u; i < fviz_array_count(window->renderers); ++i)
    {
        FVizRenderer* existing = *(FVizRenderer**)fviz_array_at(window->renderers, i);
        FVizObserverTag* tag = (FVizObserverTag*)fviz_array_at(window->renderer_modified_tags, i);
        if (tag != NULL && *tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)existing, *tag);
        fviz_release(existing);
    }
    fviz_array_clear(window->renderer_modified_tags);
    fviz_array_clear(window->renderers);
    window->renderer = NULL;
    result = fviz_render_window_add_renderer(window, renderer);
    fviz_release(renderer);
    return result;
}

FVizRenderer* fviz_render_window_renderer(FVizRenderWindow* window)
{
    return window != NULL ? window->renderer : NULL;
}

FVizResult fviz_render_window_add_renderer(FVizRenderWindow* window, FVizRenderer* renderer)
{
    FVizSize i;
    if (window == NULL || renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window and renderer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < fviz_array_count(window->renderers); ++i)
    {
        if (*(FVizRenderer**)fviz_array_at(window->renderers, i) == renderer) return FVIZ_OK;
    }
    {
        FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
        if (fviz_retain(renderer) == NULL) return fviz_last_error_code();
        if (fviz_object_add_observer((FVizObject*)renderer, FVIZ_EVENT_MODIFIED, 0.0f,
                                     fviz_render_window_renderer_modified, window, &tag) != FVIZ_OK)
        {
            fviz_release(renderer);
            return fviz_last_error_code();
        }
        if (fviz_array_push(window->renderers, &renderer) != FVIZ_OK ||
            fviz_array_push(window->renderer_modified_tags, &tag) != FVIZ_OK)
        {
            (void)fviz_object_remove_observer((FVizObject*)renderer, tag);
            if (fviz_array_count(window->renderers) > fviz_array_count(window->renderer_modified_tags))
                (void)fviz_array_resize(window->renderers, fviz_array_count(window->renderers) - 1u);
            fviz_release(renderer);
            return fviz_last_error_code();
        }
    }
    if (window->renderer == NULL) window->renderer = renderer;
    fviz_object_modified((FVizObject*)window);
    return FVIZ_OK;
}

FVizResult fviz_render_window_remove_renderer(FVizRenderWindow* window, FVizRenderer* renderer)
{
    FVizSize i;
    FVizSize count;
    FVizRenderer** items;
    if (window == NULL || renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window and renderer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_array_count(window->renderers);
    items = (FVizRenderer**)fviz_array_data(window->renderers);
    for (i = 0u; i < count; ++i)
    {
        if (items[i] == renderer)
        {
            FVizObserverTag* tags = (FVizObserverTag*)fviz_array_data(window->renderer_modified_tags);
            if (tags != NULL && tags[i] != FVIZ_OBSERVER_TAG_INVALID)
                (void)fviz_object_remove_observer((FVizObject*)items[i], tags[i]);
            fviz_release(items[i]);
            if (i + 1u < count)
            {
                (void)memmove(&items[i], &items[i + 1u], (size_t)(count - i - 1u) * sizeof(FVizRenderer*));
                if (tags != NULL)
                    (void)memmove(&tags[i], &tags[i + 1u], (size_t)(count - i - 1u) * sizeof(FVizObserverTag));
            }
            (void)fviz_array_resize(window->renderers, count - 1u);
            (void)fviz_array_resize(window->renderer_modified_tags, count - 1u);
            window->renderer = count > 1u ? *(FVizRenderer**)fviz_array_at(window->renderers, 0u) : NULL;
            fviz_object_modified((FVizObject*)window);
            return FVIZ_OK;
        }
    }
    fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "renderer is not attached to this window");
    return FVIZ_ERROR_NOT_FOUND;
}

FVizSize fviz_render_window_renderer_count(const FVizRenderWindow* window)
{
    return window != NULL ? fviz_array_count(window->renderers) : 0u;
}

FVizRenderer* fviz_render_window_renderer_at(FVizRenderWindow* window, FVizSize index)
{
    FVizRenderer** renderer = window != NULL ? (FVizRenderer**)fviz_array_at(window->renderers, index) : NULL;
    return renderer != NULL ? *renderer : NULL;
}

FVizRenderer* fviz_render_window_find_renderer(FVizRenderWindow* window, int x, int y)
{
    FVizRenderer* selected = NULL;
    int selected_layer = INT_MIN;
    FVizSize i;
    float normalized_x;
    float normalized_y;
    if (window == NULL || window->width <= 0 || window->height <= 0 || x < 0 || y < 0 || x >= window->width ||
        y >= window->height)
        return NULL;
    normalized_x = (float)x / (float)window->width;
    normalized_y = 1.0f - (float)y / (float)window->height;
    for (i = 0u; i < fviz_array_count(window->renderers); ++i)
    {
        FVizRenderer* renderer = *(FVizRenderer**)fviz_array_at(window->renderers, i);
        const int layer = fviz_renderer_layer(renderer);
        if (fviz_renderer_interactive(renderer) == FVIZ_TRUE &&
            fviz_renderer_contains_normalized_point(renderer, normalized_x, normalized_y) == FVIZ_TRUE &&
            (selected == NULL || layer >= selected_layer))
        {
            selected = renderer;
            selected_layer = layer;
        }
    }
    return selected;
}

void fviz_render_window_get_size(const FVizRenderWindow* window, int* width, int* height)
{
    if (window == NULL) return;
    if (width != NULL) *width = window->width;
    if (height != NULL) *height = window->height;
}

uint32_t fviz_render_window_dpi(const FVizRenderWindow* window)
{
    return window != NULL && window->dpi > 0u ? window->dpi : 96u;
}

float fviz_render_window_content_scale(const FVizRenderWindow* window)
{
    return (float)fviz_render_window_dpi(window) / 96.0f;
}

FVizRenderWindowInteractor* fviz_render_window_interactor(FVizRenderWindow* window)
{
    return window != NULL ? window->interactor : NULL;
}

FVizResult fviz_render_window_show(FVizRenderWindow* window)
{
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (window->offscreen != FVIZ_FALSE) return FVIZ_ERROR_INVALID_STATE;
    if (window->state == FVIZ_RENDER_WINDOW_FINALIZED) return FVIZ_ERROR_INVALID_STATE;
    {
        FVizResult result = fviz_internal_render_window_show_platform(window);
        if (result == FVIZ_OK) window->state = FVIZ_RENDER_WINDOW_VISIBLE;
        return result;
    }
}

FVizResult fviz_render_window_render(FVizRenderWindow* window)
{
    FVizResult result;
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (window->state == FVIZ_RENDER_WINDOW_FINALIZED) return FVIZ_ERROR_INVALID_STATE;
    if (window->render_in_progress != FVIZ_FALSE)
    {
        /* A callback may request another frame, but recursive rendering on the
         * same native GL context is intentionally rejected. */
        fviz_render_window_request_render_reason(window, FVIZ_RENDER_REQUEST_DIRECT);
        fviz_internal_set_error(FVIZ_ERROR_BUSY, "render window is already rendering");
        return FVIZ_ERROR_BUSY;
    }
    window->active_render_reasons = window->pending_render_reasons != FVIZ_RENDER_REQUEST_NONE
                                        ? window->pending_render_reasons
                                        : FVIZ_RENDER_REQUEST_DIRECT;
    window->pending_render_reasons = FVIZ_RENDER_REQUEST_NONE;
    window->render_requested = FVIZ_FALSE;
    window->render_in_progress = FVIZ_TRUE;
    (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_RENDER_START, NULL);
    result = fviz_internal_render_window_render_platform(window);
    (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_RENDER_END, &result);
    window->render_in_progress = FVIZ_FALSE;
    window->frame_scheduler_statistics.last_frame_reasons = window->active_render_reasons;
    window->frame_scheduler_statistics.last_frame_quality = fviz_render_window_frame_quality(window);
    if (window->frame_scheduler_statistics.rendered_frame_count != UINT64_MAX)
        ++window->frame_scheduler_statistics.rendered_frame_count;
    window->last_statistics.request_reasons = window->active_render_reasons;
    window->last_statistics.frame_quality = window->frame_scheduler_statistics.last_frame_quality;
    if (window->render_requested != FVIZ_FALSE) fviz_internal_render_window_schedule_render_platform(window);
    return result;
}

void fviz_render_window_request_render(FVizRenderWindow* window)
{
    fviz_render_window_request_render_reason(window, FVIZ_RENDER_REQUEST_EXTERNAL);
}

void fviz_render_window_request_render_reason(FVizRenderWindow* window, FVizRenderRequestReasons reasons)
{
    if (window == NULL || window->state == FVIZ_RENDER_WINDOW_FINALIZED) return;
    if (reasons == FVIZ_RENDER_REQUEST_NONE) reasons = FVIZ_RENDER_REQUEST_EXTERNAL;
    window->pending_render_reasons |= reasons;
    if (window->frame_scheduler_statistics.request_count != UINT64_MAX)
        ++window->frame_scheduler_statistics.request_count;
    if (window->render_requested != FVIZ_FALSE)
    {
        if (window->frame_scheduler_statistics.coalesced_request_count != UINT64_MAX)
            ++window->frame_scheduler_statistics.coalesced_request_count;
        window->frame_scheduler_statistics.pending_reasons = window->pending_render_reasons;
        return;
    }
    window->render_requested = FVIZ_TRUE;
    window->frame_scheduler_statistics.pending_reasons = window->pending_render_reasons;
    if (window->render_request_serial != UINT64_MAX) ++window->render_request_serial;
    (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_RENDER_REQUESTED, &window->render_request_serial);
    fviz_internal_render_window_schedule_render_platform(window);
}

FVizBool fviz_render_window_render_requested(const FVizRenderWindow* window)
{
    return window != NULL ? window->render_requested : FVIZ_FALSE;
}

uint64_t fviz_render_window_render_request_serial(const FVizRenderWindow* window)
{
    return window != NULL ? window->render_request_serial : 0u;
}

FVizRenderRequestReasons fviz_render_window_pending_render_reasons(const FVizRenderWindow* window)
{
    return window != NULL ? window->pending_render_reasons : FVIZ_RENDER_REQUEST_NONE;
}

FVizFrameQuality fviz_render_window_frame_quality(const FVizRenderWindow* window)
{
    return window != NULL && window->frame_scheduler_options.interactive_quality != FVIZ_FALSE &&
                   window->interaction_active != FVIZ_FALSE
               ? FVIZ_FRAME_QUALITY_INTERACTIVE
               : FVIZ_FRAME_QUALITY_STILL;
}

FVizResult fviz_render_window_set_frame_scheduler_options(FVizRenderWindow* window,
                                                          const FVizFrameSchedulerOptions* options)
{
    if (window == NULL || options == NULL ||
        (options->struct_size != 0u && options->struct_size < sizeof(FVizFrameSchedulerOptions)) ||
        options->interactive_target_fps < 0.0 || options->still_target_fps < 0.0 ||
        options->interactive_target_fps > 1000.0 || options->still_target_fps > 1000.0)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    window->frame_scheduler_options = *options;
    window->frame_scheduler_options.struct_size = (uint32_t)sizeof(window->frame_scheduler_options);
    window->frame_scheduler_options.interactive_quality =
        options->interactive_quality != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_render_window_request_render_reason(window, FVIZ_RENDER_REQUEST_SCENE);
    return FVIZ_OK;
}

void fviz_render_window_get_frame_scheduler_options(const FVizRenderWindow* window,
                                                    FVizFrameSchedulerOptions* out_options)
{
    if (out_options == NULL) return;
    fviz_frame_scheduler_options_initialize(out_options);
    if (window != NULL) *out_options = window->frame_scheduler_options;
}

void fviz_render_window_get_frame_scheduler_statistics(const FVizRenderWindow* window,
                                                       FVizFrameSchedulerStatistics* out_statistics)
{
    if (out_statistics == NULL) return;
    (void)memset(out_statistics, 0, sizeof(*out_statistics));
    out_statistics->struct_size = (uint32_t)sizeof(*out_statistics);
    if (window == NULL) return;
    *out_statistics = window->frame_scheduler_statistics;
    out_statistics->struct_size = (uint32_t)sizeof(*out_statistics);
    out_statistics->pending_reasons = window->pending_render_reasons;
}

void fviz_render_window_reset_frame_scheduler_statistics(FVizRenderWindow* window)
{
    if (window == NULL) return;
    (void)memset(&window->frame_scheduler_statistics, 0, sizeof(window->frame_scheduler_statistics));
    window->frame_scheduler_statistics.struct_size = (uint32_t)sizeof(window->frame_scheduler_statistics);
    window->frame_scheduler_statistics.pending_reasons = window->pending_render_reasons;
    window->frame_scheduler_statistics.last_frame_quality = fviz_render_window_frame_quality(window);
}

FVizResult fviz_render_window_set_gpu_memory_options(FVizRenderWindow* window, const FVizGPUMemoryOptions* options)
{
    if (window == NULL || options == NULL || (options->struct_size != 0u && options->struct_size < sizeof(*options)) ||
        options->unused_resource_retention_frames > 1000000u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    window->gpu_memory_options = *options;
    window->gpu_memory_options.struct_size = (uint32_t)sizeof(window->gpu_memory_options);
    if (window->gl_device != NULL)
        fviz_internal_gl_device_set_memory_options((FVizGLDevice*)window->gl_device, &window->gpu_memory_options);
    fviz_render_window_request_render_reason(window, FVIZ_RENDER_REQUEST_SCENE);
    return FVIZ_OK;
}

void fviz_render_window_get_gpu_memory_options(const FVizRenderWindow* window, FVizGPUMemoryOptions* out_options)
{
    if (out_options == NULL) return;
    fviz_gpu_memory_options_initialize(out_options);
    if (window != NULL) *out_options = window->gpu_memory_options;
}

FVizResult fviz_render_window_release_gpu_mesh_resources(FVizRenderWindow* window)
{
    if (window == NULL || window->state == FVIZ_RENDER_WINDOW_FINALIZED) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_internal_render_window_release_gpu_mesh_resources_platform(window);
}

FVizResult fviz_render_window_render_if_requested(FVizRenderWindow* window)
{
    if (window == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    return window->render_requested != FVIZ_FALSE ? fviz_render_window_render(window) : FVIZ_OK;
}

FVizResult fviz_render_window_run(FVizRenderWindow* window)
{
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (window->state == FVIZ_RENDER_WINDOW_FINALIZED || window->offscreen != FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_STATE;
    if (window->host_native_handle != NULL || window->external_opengl != FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                "embedded/external render windows use the host application's event loop");
        return FVIZ_ERROR_INVALID_STATE;
    }
    return fviz_internal_render_window_run_platform(window);
}

FVizResult fviz_render_window_process_events(FVizRenderWindow* window)
{
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (window->state == FVIZ_RENDER_WINDOW_FINALIZED) return FVIZ_ERROR_INVALID_STATE;
    return fviz_internal_render_window_process_events_platform(window);
}

void fviz_render_window_request_close(FVizRenderWindow* window)
{
    if (window != NULL && window->close_requested == FVIZ_FALSE)
    {
        window->close_requested = FVIZ_TRUE;
        (void)fviz_object_invoke_event((FVizObject*)window, FVIZ_EVENT_WINDOW_CLOSE, NULL);
        fviz_internal_render_window_request_close_platform(window);
    }
}

void* fviz_render_window_native_handle(FVizRenderWindow* window)
{
    return window != NULL ? window->native_window : NULL;
}

void* fviz_render_window_host_native_handle(FVizRenderWindow* window)
{
    return window != NULL ? window->host_native_handle : NULL;
}

FVizBool fviz_render_window_is_attached(const FVizRenderWindow* window)
{
    return window != NULL && window->host_native_handle != NULL ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_render_window_supported(void)
{
    return fviz_internal_render_window_supported_platform();
}

static FVizResult fviz_render_window_ensure_pick_bvh(FVizRenderWindow* window, FVizRenderer* renderer)
{
    FVizScene* scene;
    FVizSize i;
    if (window == NULL || renderer == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_renderer_update(renderer) != FVIZ_OK) return fviz_last_error_code();
    scene = fviz_renderer_scene(renderer);
    if (scene == NULL) return FVIZ_OK;
    for (i = 0u; i < fviz_scene_actor_count(scene); ++i)
    {
        const FVizActor* actor = fviz_scene_const_actor(scene, i);
        const FVizPolyData* data;
        FVizMTime mtime;
        if (fviz_actor_is_visible(actor) == FVIZ_FALSE) continue;
        data = fviz_actor_const_poly_data(actor);
        if (data == NULL || fviz_poly_data_triangle_count(data) == 0u) continue;
        mtime = fviz_object_mtime((const FVizObject*)data);
        if (window->pick_bvh != NULL && window->pick_poly_data == data && window->pick_bvh_mtime == mtime)
        {
            return FVIZ_OK;
        }
        if (window->pick_bvh == NULL)
        {
            if (fviz_bvh_create(&window->pick_bvh) != FVIZ_OK) return fviz_last_error_code();
        }
        if (fviz_bvh_build(window->pick_bvh, data) != FVIZ_OK) return fviz_last_error_code();
        if (fviz_retain((FVizPolyData*)data) == NULL) return fviz_last_error_code();
        fviz_release((FVizPolyData*)window->pick_poly_data);
        window->pick_poly_data = data;
        window->pick_bvh_mtime = mtime;
        return FVIZ_OK;
    }
    return FVIZ_OK;
}

FVizResult fviz_render_window_pick(FVizRenderWindow* window, int x, int y, FVizRayHit* out_hit)
{
    FVizCamera* camera;
    FVizRenderer* renderer = NULL;
    FVizRay ray;
    FVizResult result = FVIZ_OK;
    FVizPickEventData event_data;
    float minimum_x;
    float minimum_y;
    float maximum_x;
    float maximum_y;
    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
    if (window == NULL || out_hit == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window and out_hit must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(out_hit, 0, sizeof(*out_hit));
    fviz_render_window_pick_event_initialize(&event_data, x, y, FVIZ_SELECTION_CELL, FVIZ_FALSE);
    if (fviz_render_window_pick_event_begin(window, &event_data) != FVIZ_FALSE) return FVIZ_ERROR_BUSY;
    renderer = fviz_render_window_find_renderer(window, x, y);
    if (renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "no interactive renderer contains the pick position");
        result = FVIZ_ERROR_NOT_FOUND;
        goto done;
    }
    result = fviz_render_window_ensure_pick_bvh(window, renderer);
    if (result != FVIZ_OK) goto done;
    if (window->pick_bvh == NULL || fviz_bvh_valid(window->pick_bvh) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "no pickable geometry in the scene");
        result = FVIZ_ERROR_NOT_FOUND;
        goto done;
    }
    camera = fviz_renderer_camera(renderer);
    if (camera == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "renderer has no camera");
        result = FVIZ_ERROR_INVALID_STATE;
        goto done;
    }
    fviz_renderer_get_viewport(renderer, &minimum_x, &minimum_y, &maximum_x, &maximum_y);
    viewport_x = (int)(minimum_x * (float)window->width);
    viewport_y = (int)((1.0f - maximum_y) * (float)window->height);
    viewport_width = (int)((maximum_x - minimum_x) * (float)window->width);
    viewport_height = (int)((maximum_y - minimum_y) * (float)window->height);
    if (viewport_width < 1) viewport_width = 1;
    if (viewport_height < 1) viewport_height = 1;
    ray = fviz_camera_pick_ray(camera, viewport_width, viewport_height, x - viewport_x, y - viewport_y);
    if (fviz_bvh_ray_cast(window->pick_bvh, ray, out_hit) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "pick ray did not intersect the scene");
        result = FVIZ_ERROR_NOT_FOUND;
        goto done;
    }

done:
    event_data.result = result;
    if (result == FVIZ_OK) event_data.ray_hit = *out_hit;
    fviz_render_window_pick_event_end(window, &event_data);
    if (result == FVIZ_OK && renderer != NULL)
        (void)fviz_object_invoke_event((FVizObject*)renderer, FVIZ_EVENT_PICK, &event_data);
    return result;
}

void fviz_render_window_set_pick_callback(FVizRenderWindow* window, FVizPickCallbackFn callback, void* user_data)
{
    if (window == NULL) return;
    window->pick_callback = callback;
    window->pick_user_data = user_data;
}

static FVizBool fviz_render_window_project_point(FVizRenderer* renderer, FVizActor* actor, FVizVec3 point,
                                                 int window_width, int window_height, int* out_x, int* out_y)
{
    const FVizMat4 model = fviz_actor_transform_matrix(actor);
    const FVizMat4 view = fviz_camera_view_matrix(fviz_renderer_camera(renderer));
    float viewport[4];
    int viewport_width;
    int viewport_height;
    FVizMat4 projection;
    FVizMat4 mvp;
    float x;
    float y;
    float z;
    float w;
    fviz_renderer_get_viewport(renderer, &viewport[0], &viewport[1], &viewport[2], &viewport[3]);
    viewport_width = (int)((viewport[2] - viewport[0]) * (float)window_width);
    viewport_height = (int)((viewport[3] - viewport[1]) * (float)window_height);
    if (viewport_width < 1 || viewport_height < 1) return FVIZ_FALSE;
    projection =
        fviz_camera_projection_matrix(fviz_renderer_camera(renderer), (float)viewport_width / (float)viewport_height);
    mvp = fviz_mat4_multiply(projection, fviz_mat4_multiply(view, model));
    x = mvp.m[0] * point.x + mvp.m[4] * point.y + mvp.m[8] * point.z + mvp.m[12];
    y = mvp.m[1] * point.x + mvp.m[5] * point.y + mvp.m[9] * point.z + mvp.m[13];
    z = mvp.m[2] * point.x + mvp.m[6] * point.y + mvp.m[10] * point.z + mvp.m[14];
    w = mvp.m[3] * point.x + mvp.m[7] * point.y + mvp.m[11] * point.z + mvp.m[15];
    if (w <= 0.0f || z < -w || z > w) return FVIZ_FALSE;
    x /= w;
    y /= w;
    *out_x = (int)(viewport[0] * (float)window_width + (x * 0.5f + 0.5f) * (float)viewport_width);
    *out_y = (int)((1.0f - viewport[3]) * (float)window_height + (1.0f - (y * 0.5f + 0.5f)) * (float)viewport_height);
    return FVIZ_TRUE;
}

FVizResult fviz_render_window_select_polygon(FVizRenderWindow* window, const int* xy_points, FVizSize point_count,
                                             FVizSelectionAssociation association, FVizSelection** out_selection)
{
    FVizRenderer* renderer;
    int anchor_x;
    int anchor_y;
    if (window == NULL || xy_points == NULL || point_count < 3u || out_selection == NULL ||
        association < FVIZ_SELECTION_ACTOR || association > FVIZ_SELECTION_GLYPH_INSTANCE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_selection = NULL;
    anchor_x = xy_points[0];
    anchor_y = xy_points[1];
    if (anchor_x < 0 || anchor_y < 0 || anchor_x >= window->width || anchor_y >= window->height)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    renderer = fviz_render_window_find_renderer(window, anchor_x, anchor_y);
    if (renderer == NULL) return FVIZ_ERROR_NOT_FOUND;
    return fviz_selection_select_polygon(renderer, window->width, window->height, xy_points, point_count, association,
                                         out_selection);
}

FVizResult fviz_render_window_select_rectangle_association(FVizRenderWindow* window, int start_x, int start_y,
                                                           int end_x, int end_y, FVizSelectionAssociation association,
                                                           FVizSelection** out_selection)
{
    const int minimum_x = start_x < end_x ? start_x : end_x;
    const int minimum_y = start_y < end_y ? start_y : end_y;
    const int maximum_x = start_x > end_x ? start_x : end_x;
    const int maximum_y = start_y > end_y ? start_y : end_y;
    const int polygon[8] = {minimum_x, minimum_y, maximum_x, minimum_y, maximum_x, maximum_y, minimum_x, maximum_y};
    if (window == NULL || out_selection == NULL || minimum_x < 0 || minimum_y < 0 || maximum_x >= window->width ||
        maximum_y >= window->height)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_render_window_select_polygon(window, polygon, 4u, association, out_selection);
}

FVizResult fviz_render_window_select_rectangle(FVizRenderWindow* window, int start_x, int start_y, int end_x, int end_y,
                                               FVizSelection** out_selection)
{
    return fviz_render_window_select_rectangle_association(window, start_x, start_y, end_x, end_y, FVIZ_SELECTION_CELL,
                                                           out_selection);
}

FVizResult fviz_render_window_select_at(FVizRenderWindow* window, int x, int y, FVizSelectionAssociation association,
                                        FVizSelection** out_selection)
{
    FVizHardwarePick hardware_pick;
    FVizRayHit cpu_hit;
    FVizRenderer* renderer;
    FVizActor* actor = NULL;
    FVizSize rendered_id = 0u;
    FVizSelection* selection = NULL;
    FVizResult result;
    FVizBool refine_point_or_edge = FVIZ_FALSE;
    if (window == NULL || out_selection == NULL || x < 0 || y < 0 || x >= window->width || y >= window->height ||
        association < FVIZ_SELECTION_ACTOR || association > FVIZ_SELECTION_GLYPH_INSTANCE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_selection = NULL;
    renderer = fviz_render_window_find_renderer(window, x, y);
    if (renderer == NULL) return FVIZ_ERROR_NOT_FOUND;
    result = fviz_render_window_hardware_pick_association(window, x, y, association, &hardware_pick);
    if (result == FVIZ_OK)
    {
        actor = hardware_pick.actor;
        rendered_id = hardware_pick.rendered_primitive_id;
    }
    else if ((association == FVIZ_SELECTION_POINT || association == FVIZ_SELECTION_EDGE) &&
             result == FVIZ_ERROR_NOT_FOUND)
    {
        /* Preserve a generous engineering-pick fallback when the integer point/edge
           raster missed the exact cursor pixel: use the visible cell ID and refine
           against that triangle in screen space below. */
        result = fviz_render_window_hardware_pick_association(window, x, y, FVIZ_SELECTION_CELL, &hardware_pick);
        if (result == FVIZ_OK)
        {
            actor = hardware_pick.actor;
            rendered_id = hardware_pick.rendered_primitive_id;
            refine_point_or_edge = FVIZ_TRUE;
        }
        else if (result != FVIZ_ERROR_NOT_SUPPORTED)
            return result;
    }
    if (actor == NULL && result == FVIZ_ERROR_NOT_SUPPORTED)
    {
        FVizScene* scene;
        FVizSize i;
        if (fviz_render_window_pick(window, x, y, &cpu_hit) != FVIZ_OK) return fviz_last_error_code();
        rendered_id = cpu_hit.triangle_index;
        refine_point_or_edge =
            association == FVIZ_SELECTION_POINT || association == FVIZ_SELECTION_EDGE ? FVIZ_TRUE : FVIZ_FALSE;
        scene = fviz_renderer_scene(renderer);
        for (i = 0u; scene != NULL && i < fviz_scene_actor_count(scene); ++i)
            if (fviz_actor_const_poly_data(fviz_scene_actor(scene, i)) == window->pick_poly_data)
            {
                actor = fviz_scene_actor(scene, i);
                break;
            }
    }
    else if (actor == NULL && result != FVIZ_OK)
        return result;
    if (actor == NULL) return FVIZ_ERROR_NOT_FOUND;
    if (association == FVIZ_SELECTION_POINT && refine_point_or_edge != FVIZ_FALSE)
    {
        const FVizPolyData* data = fviz_actor_const_poly_data(actor);
        const uint32_t* triangle;
        const FVizVec3* points;
        FVizSize best_point = SIZE_MAX;
        int best_distance = INT_MAX;
        FVizSize corner;
        if (data == NULL || rendered_id >= fviz_poly_data_triangle_count(data)) return FVIZ_ERROR_NOT_FOUND;
        triangle = fviz_poly_data_triangle_indices(data) + rendered_id * 3u;
        points = fviz_poly_data_points(data);
        for (corner = 0u; corner < 3u; ++corner)
        {
            int point_x;
            int point_y;
            if (fviz_render_window_project_point(renderer, actor, points[triangle[corner]], window->width,
                                                 window->height, &point_x, &point_y) != FVIZ_FALSE)
            {
                const int dx = point_x - x;
                const int dy = point_y - y;
                const int distance = dx * dx + dy * dy;
                if (distance < best_distance)
                {
                    best_distance = distance;
                    best_point = triangle[corner];
                }
            }
        }
        if (best_point == SIZE_MAX) return FVIZ_ERROR_NOT_FOUND;
        rendered_id = best_point;
    }
    else if (association == FVIZ_SELECTION_EDGE && refine_point_or_edge != FVIZ_FALSE)
    {
        const FVizPolyData* data = fviz_actor_const_poly_data(actor);
        const uint32_t* triangle;
        const FVizVec3* points;
        FVizSize edge;
        FVizSize best_edge = SIZE_MAX;
        double best_distance2 = 1.0e300;
        if (data == NULL || rendered_id >= fviz_poly_data_triangle_count(data)) return FVIZ_ERROR_NOT_FOUND;
        triangle = fviz_poly_data_triangle_indices(data) + rendered_id * 3u;
        points = fviz_poly_data_points(data);
        for (edge = 0u; edge < 3u; ++edge)
        {
            const uint32_t ia = triangle[edge];
            const uint32_t ib = triangle[(edge + 1u) % 3u];
            int ax, ay, bx, by;
            if (fviz_render_window_project_point(renderer, actor, points[ia], window->width, window->height, &ax,
                                                 &ay) != FVIZ_FALSE &&
                fviz_render_window_project_point(renderer, actor, points[ib], window->width, window->height, &bx,
                                                 &by) != FVIZ_FALSE)
            {
                const double vx = (double)bx - (double)ax;
                const double vy = (double)by - (double)ay;
                const double wx = (double)x - (double)ax;
                const double wy = (double)y - (double)ay;
                const double vv = vx * vx + vy * vy;
                double t = vv > 0.0 ? (wx * vx + wy * vy) / vv : 0.0;
                double dx;
                double dy;
                double d2;
                if (t < 0.0) t = 0.0;
                if (t > 1.0) t = 1.0;
                dx = (double)x - ((double)ax + t * vx);
                dy = (double)y - ((double)ay + t * vy);
                d2 = dx * dx + dy * dy;
                if (d2 < best_distance2)
                {
                    best_distance2 = d2;
                    best_edge = rendered_id * 3u + edge;
                }
            }
        }
        if (best_edge == SIZE_MAX) return FVIZ_ERROR_NOT_FOUND;
        rendered_id = best_edge;
    }
    else if (association == FVIZ_SELECTION_ACTOR)
        rendered_id = 0u;
    if (fviz_selection_create(&selection) != FVIZ_OK ||
        fviz_selection_add(selection, actor, association, rendered_id) != FVIZ_OK)
    {
        fviz_release(selection);
        return fviz_last_error_code();
    }
    *out_selection = selection;
    return FVIZ_OK;
}
