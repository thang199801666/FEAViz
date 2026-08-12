#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRenderWindowPrivate.h>

static void fviz_render_window_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_render_window_class = {
    FVIZ_TYPE_RENDER_WINDOW,
    "FVizRenderWindow",
    &g_fviz_object_class,
    fviz_render_window_destroy
};

static void fviz_render_window_destroy(FVizObject* object)
{
    FVizRenderWindow* window = (FVizRenderWindow*)object;
    fviz_internal_render_window_destroy_platform(window);
    fviz_release(window->renderer);
    window->renderer = NULL;
    fviz_free(window->title);
    window->title = NULL;
}

FVizResult fviz_render_window_create(
    int width,
    int height,
    const char* title,
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
    window = (FVizRenderWindow*)fviz_internal_object_allocate(sizeof(FVizRenderWindow), &g_fviz_render_window_class, NULL);
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
    if (fviz_renderer_create(&window->renderer) != FVIZ_OK)
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
    *out_window = window;
    return FVIZ_OK;
}

FVizResult fviz_render_window_set_renderer(FVizRenderWindow* window, FVizRenderer* renderer)
{
    if (window == NULL || renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window and renderer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_retain(renderer) == NULL) return fviz_last_error_code();
    fviz_release(window->renderer);
    window->renderer = renderer;
    return FVIZ_OK;
}

FVizRenderer* fviz_render_window_renderer(FVizRenderWindow* window)
{
    return window != NULL ? window->renderer : NULL;
}

FVizResult fviz_render_window_show(FVizRenderWindow* window)
{
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_internal_render_window_show_platform(window);
}

FVizResult fviz_render_window_render(FVizRenderWindow* window)
{
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_internal_render_window_render_platform(window);
}

FVizResult fviz_render_window_run(FVizRenderWindow* window)
{
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_internal_render_window_run_platform(window);
}

void fviz_render_window_request_close(FVizRenderWindow* window)
{
    if (window != NULL)
    {
        window->close_requested = FVIZ_TRUE;
        fviz_internal_render_window_request_close_platform(window);
    }
}

void* fviz_render_window_native_handle(FVizRenderWindow* window)
{
    return window != NULL ? window->native_window : NULL;
}

FVizBool fviz_render_window_supported(void)
{
    return fviz_internal_render_window_supported_platform();
}
