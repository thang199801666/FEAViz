#include <limits.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Interaction/FVizRenderWindowInteractor.h>
#include <FViz/Rendering/FVizRenderWindow.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizRenderWindowInteractorPrivate.h>
#include <FViz/Rendering/FVizCamera.h>
#include <FViz/Rendering/FVizRenderWindowPrivate.h>
#include <FViz/Rendering/FVizScene.h>
#include <FViz/Spatial/FVizBVH.h>

static void fviz_render_window_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_render_window_class = {
    FVIZ_TYPE_RENDER_WINDOW,
    "FVizRenderWindow",
    &g_fviz_object_class,
    fviz_render_window_destroy,
    NULL
};

static void fviz_render_window_destroy(FVizObject* object)
{
    FVizRenderWindow* window = (FVizRenderWindow*)object;
    FVizSize i;
    fviz_internal_render_window_destroy_platform(window);
    fviz_internal_render_window_interactor_detach(window->interactor);
    fviz_release(window->interactor);
    window->interactor = NULL;
    for (i = 0u; i < fviz_array_count(window->renderers); ++i)
        fviz_release(*(FVizRenderer**)fviz_array_at(window->renderers, i));
    fviz_release(window->renderers);
    window->renderers = NULL;
    window->renderer = NULL;
    fviz_release(window->pick_bvh);
    window->pick_bvh = NULL;
    fviz_release((FVizPolyData*)window->pick_poly_data);
    window->pick_poly_data = NULL;
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
    if (fviz_array_create(sizeof(FVizRenderer*), &window->renderers) != FVIZ_OK)
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
    *out_window = window;
    return FVIZ_OK;
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
        fviz_release(*(FVizRenderer**)fviz_array_at(window->renderers, i));
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
        if (*(FVizRenderer**)fviz_array_at(window->renderers, i) == renderer)
            return FVIZ_OK;
    }
    if (fviz_retain(renderer) == NULL) return fviz_last_error_code();
    if (fviz_array_push(window->renderers, &renderer) != FVIZ_OK)
    {
        fviz_release(renderer);
        return fviz_last_error_code();
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
            fviz_release(items[i]);
            if (i + 1u < count)
                (void)memmove(&items[i], &items[i + 1u], (size_t)(count - i - 1u) * sizeof(FVizRenderer*));
            (void)fviz_array_resize(window->renderers, count - 1u);
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
    FVizRenderer** renderer = window != NULL
        ? (FVizRenderer**)fviz_array_at(window->renderers, index)
        : NULL;
    return renderer != NULL ? *renderer : NULL;
}

FVizRenderer* fviz_render_window_find_renderer(FVizRenderWindow* window, int x, int y)
{
    FVizRenderer* selected = NULL;
    int selected_layer = INT_MIN;
    FVizSize i;
    float normalized_x;
    float normalized_y;
    if (window == NULL || window->width <= 0 || window->height <= 0 ||
        x < 0 || y < 0 || x >= window->width || y >= window->height)
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

FVizResult fviz_render_window_process_events(FVizRenderWindow* window)
{
    if (window == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_internal_render_window_process_events_platform(window);
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

static FVizResult fviz_render_window_ensure_pick_bvh(
    FVizRenderWindow* window,
    FVizRenderer* renderer)
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
        if (window->pick_bvh != NULL &&
            window->pick_poly_data == data &&
            window->pick_bvh_mtime == mtime)
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

FVizResult fviz_render_window_pick(
    FVizRenderWindow* window,
    int x,
    int y,
    FVizRayHit* out_hit)
{
    FVizCamera* camera;
    FVizRenderer* renderer;
    FVizRay ray;
    FVizResult result;
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
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "window, renderer and out_hit must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    renderer = fviz_render_window_find_renderer(window, x, y);
    if (renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "no interactive renderer contains the pick position");
        return FVIZ_ERROR_NOT_FOUND;
    }
    result = fviz_render_window_ensure_pick_bvh(window, renderer);
    if (result != FVIZ_OK) return result;
    if (window->pick_bvh == NULL || fviz_bvh_valid(window->pick_bvh) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "no pickable geometry in the scene");
        return FVIZ_ERROR_NOT_FOUND;
    }
    camera = fviz_renderer_camera(renderer);
    if (camera == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "renderer has no camera");
        return FVIZ_ERROR_INVALID_STATE;
    }
    fviz_renderer_get_viewport(
        renderer, &minimum_x, &minimum_y, &maximum_x, &maximum_y);
    viewport_x = (int)(minimum_x * (float)window->width);
    viewport_y = (int)((1.0f - maximum_y) * (float)window->height);
    viewport_width = (int)((maximum_x - minimum_x) * (float)window->width);
    viewport_height = (int)((maximum_y - minimum_y) * (float)window->height);
    if (viewport_width < 1) viewport_width = 1;
    if (viewport_height < 1) viewport_height = 1;
    ray = fviz_camera_pick_ray(
        camera, viewport_width, viewport_height, x - viewport_x, y - viewport_y);
    if (fviz_bvh_ray_cast(window->pick_bvh, ray, out_hit) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "pick ray did not intersect the scene");
        return FVIZ_ERROR_NOT_FOUND;
    }
    return FVIZ_OK;
}

void fviz_render_window_set_pick_callback(
    FVizRenderWindow* window,
    FVizPickCallbackFn callback,
    void* user_data)
{
    if (window == NULL) return;
    window->pick_callback = callback;
    window->pick_user_data = user_data;
}

static FVizBool fviz_render_window_project_point(
    FVizRenderer* renderer,
    FVizActor* actor,
    FVizVec3 point,
    int window_width,
    int window_height,
    int* out_x,
    int* out_y)
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
    fviz_renderer_get_viewport(
        renderer, &viewport[0], &viewport[1], &viewport[2], &viewport[3]);
    viewport_width = (int)((viewport[2] - viewport[0]) * (float)window_width);
    viewport_height = (int)((viewport[3] - viewport[1]) * (float)window_height);
    if (viewport_width < 1 || viewport_height < 1) return FVIZ_FALSE;
    projection = fviz_camera_projection_matrix(
        fviz_renderer_camera(renderer), (float)viewport_width / (float)viewport_height);
    mvp = fviz_mat4_multiply(projection, fviz_mat4_multiply(view, model));
    x = mvp.m[0] * point.x + mvp.m[4] * point.y + mvp.m[8] * point.z + mvp.m[12];
    y = mvp.m[1] * point.x + mvp.m[5] * point.y + mvp.m[9] * point.z + mvp.m[13];
    z = mvp.m[2] * point.x + mvp.m[6] * point.y + mvp.m[10] * point.z + mvp.m[14];
    w = mvp.m[3] * point.x + mvp.m[7] * point.y + mvp.m[11] * point.z + mvp.m[15];
    if (w <= 0.0f || z < -w || z > w) return FVIZ_FALSE;
    x /= w;
    y /= w;
    *out_x = (int)(viewport[0] * (float)window_width +
        (x * 0.5f + 0.5f) * (float)viewport_width);
    *out_y = (int)((1.0f - viewport[3]) * (float)window_height +
        (1.0f - (y * 0.5f + 0.5f)) * (float)viewport_height);
    return FVIZ_TRUE;
}

FVizResult fviz_render_window_select_rectangle(
    FVizRenderWindow* window,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    FVizSelection** out_selection)
{
    FVizSelection* selection = NULL;
    FVizRenderer* renderer;
    FVizScene* scene;
    FVizSize actor_index;
    const int minimum_x = start_x < end_x ? start_x : end_x;
    const int minimum_y = start_y < end_y ? start_y : end_y;
    const int maximum_x = start_x > end_x ? start_x : end_x;
    const int maximum_y = start_y > end_y ? start_y : end_y;
    if (window == NULL || out_selection == NULL || minimum_x < 0 || minimum_y < 0 ||
        maximum_x >= window->width || maximum_y >= window->height)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "selection rectangle is outside the render window");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_selection = NULL;
    renderer = fviz_render_window_find_renderer(
        window, (minimum_x + maximum_x) / 2, (minimum_y + maximum_y) / 2);
    if (renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "selection rectangle has no interactive renderer");
        return FVIZ_ERROR_NOT_FOUND;
    }
    if (fviz_renderer_update(renderer) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_selection_create(&selection) != FVIZ_OK) return fviz_last_error_code();
    scene = fviz_renderer_scene(renderer);
    for (actor_index = 0u; scene != NULL && actor_index < fviz_scene_actor_count(scene); ++actor_index)
    {
        FVizActor* actor = fviz_scene_actor(scene, actor_index);
        const FVizPolyData* poly_data;
        const FVizVec3* points;
        const uint32_t* indices;
        FVizSize triangle_index;
        if (actor == NULL || fviz_actor_is_visible(actor) == FVIZ_FALSE) continue;
        poly_data = fviz_actor_const_poly_data(actor);
        if (poly_data == NULL) continue;
        points = fviz_poly_data_points(poly_data);
        indices = fviz_poly_data_triangle_indices(poly_data);
        for (triangle_index = 0u;
             triangle_index < fviz_poly_data_triangle_count(poly_data);
             ++triangle_index)
        {
            const uint32_t a = indices[triangle_index * 3u + 0u];
            const uint32_t b = indices[triangle_index * 3u + 1u];
            const uint32_t c = indices[triangle_index * 3u + 2u];
            FVizVec3 centroid = fviz_vec3_scale(
                fviz_vec3_add(fviz_vec3_add(points[a], points[b]), points[c]), 1.0f / 3.0f);
            int x;
            int y;
            if (fviz_render_window_project_point(
                    renderer, actor, centroid, window->width, window->height, &x, &y) == FVIZ_TRUE &&
                x >= minimum_x && x <= maximum_x && y >= minimum_y && y <= maximum_y)
            {
                if (fviz_selection_add(
                        selection, actor, FVIZ_SELECTION_CELL, triangle_index) != FVIZ_OK)
                {
                    fviz_release(selection);
                    return fviz_last_error_code();
                }
            }
        }
    }
    *out_selection = selection;
    return FVIZ_OK;
}
