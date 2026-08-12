#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Rendering/FVizRenderer.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRendererPrivate.h>
#include <FViz/Rendering/FVizScalarLegendPrivate.h>

static void fviz_renderer_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_renderer_class = {
    FVIZ_TYPE_RENDERER,
    "FVizRenderer",
    &g_fviz_object_class,
    fviz_renderer_destroy,
    NULL
};

static void fviz_renderer_destroy(FVizObject* object)
{
    FVizRenderer* renderer = (FVizRenderer*)object;
    fviz_release(renderer->scene);
    fviz_release(renderer->camera);
    fviz_release(renderer->scalar_legend);
    if (renderer->passes != NULL)
    {
        FVizSize i;
        for (i = 0u; i < fviz_array_count(renderer->passes); ++i)
            fviz_release(*(FVizRenderPass**)fviz_array_at(renderer->passes, i));
    }
    fviz_release(renderer->passes);
    renderer->scene = NULL;
    renderer->camera = NULL;
    renderer->scalar_legend = NULL;
    renderer->passes = NULL;
}

FVizResult fviz_renderer_create(FVizRenderer** out_renderer)
{
    FVizRenderer* renderer;
    if (out_renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_renderer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_renderer = NULL;
    renderer = (FVizRenderer*)fviz_internal_object_allocate(sizeof(FVizRenderer), &g_fviz_renderer_class, NULL);
    if (renderer == NULL) return fviz_last_error_code();
    if (fviz_scene_create(&renderer->scene) != FVIZ_OK ||
        fviz_camera_create(&renderer->camera) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizRenderPass*), &renderer->passes) != FVIZ_OK ||
        fviz_renderer_reset_standard_passes(renderer) != FVIZ_OK)
    {
        fviz_release(renderer);
        return fviz_last_error_code();
    }
    renderer->background[0] = 0.10f;
    renderer->background[1] = 0.12f;
    renderer->background[2] = 0.16f;
    renderer->viewport[0] = 0.0f;
    renderer->viewport[1] = 0.0f;
    renderer->viewport[2] = 1.0f;
    renderer->viewport[3] = 1.0f;
    renderer->interactive = FVIZ_TRUE;
    *out_renderer = renderer;
    return FVIZ_OK;
}

FVizResult fviz_renderer_set_scene(FVizRenderer* renderer, FVizScene* scene)
{
    if (renderer == NULL || scene == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "renderer and scene must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_retain(scene) == NULL) return fviz_last_error_code();
    fviz_release(renderer->scene);
    renderer->scene = scene;
    return FVIZ_OK;
}
FVizScene* fviz_renderer_scene(FVizRenderer* renderer) { return renderer != NULL ? renderer->scene : NULL; }
FVizCamera* fviz_renderer_camera(FVizRenderer* renderer) { return renderer != NULL ? renderer->camera : NULL; }

FVizResult fviz_renderer_update(FVizRenderer* renderer)
{
    FVizSize i;
    if (renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "renderer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; renderer->scene != NULL && i < fviz_scene_actor_count(renderer->scene); ++i)
    {
        FVizActor* actor = fviz_scene_actor(renderer->scene, i);
        if (actor != NULL && fviz_mapper_update(fviz_actor_mapper(actor)) != FVIZ_OK)
            return fviz_last_error_code();
    }
    return FVIZ_OK;
}

void fviz_renderer_set_background(FVizRenderer* renderer, float red, float green, float blue)
{
    if (renderer == NULL) return;
    renderer->background[0] = red; renderer->background[1] = green; renderer->background[2] = blue;
}
void fviz_renderer_get_background(const FVizRenderer* renderer, float* red, float* green, float* blue)
{
    if (renderer == NULL) return;
    if (red != NULL) *red = renderer->background[0];
    if (green != NULL) *green = renderer->background[1];
    if (blue != NULL) *blue = renderer->background[2];
}

FVizResult fviz_renderer_set_viewport(
    FVizRenderer* renderer,
    float minimum_x,
    float minimum_y,
    float maximum_x,
    float maximum_y)
{
    if (renderer == NULL || minimum_x < 0.0f || minimum_y < 0.0f ||
        maximum_x > 1.0f || maximum_y > 1.0f ||
        minimum_x >= maximum_x || minimum_y >= maximum_y)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "renderer viewport must be a non-empty normalized rectangle");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    renderer->viewport[0] = minimum_x;
    renderer->viewport[1] = minimum_y;
    renderer->viewport[2] = maximum_x;
    renderer->viewport[3] = maximum_y;
    fviz_object_modified((FVizObject*)renderer);
    return FVIZ_OK;
}

void fviz_renderer_get_viewport(
    const FVizRenderer* renderer,
    float* minimum_x,
    float* minimum_y,
    float* maximum_x,
    float* maximum_y)
{
    if (renderer == NULL) return;
    if (minimum_x != NULL) *minimum_x = renderer->viewport[0];
    if (minimum_y != NULL) *minimum_y = renderer->viewport[1];
    if (maximum_x != NULL) *maximum_x = renderer->viewport[2];
    if (maximum_y != NULL) *maximum_y = renderer->viewport[3];
}

void fviz_renderer_set_layer(FVizRenderer* renderer, int layer)
{
    if (layer < 0) layer = 0;
    if (renderer != NULL && renderer->layer != layer)
    {
        renderer->layer = layer;
        fviz_object_modified((FVizObject*)renderer);
    }
}

int fviz_renderer_layer(const FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->layer : 0;
}

void fviz_renderer_set_interactive(FVizRenderer* renderer, FVizBool interactive)
{
    if (renderer != NULL)
        renderer->interactive = interactive != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_renderer_interactive(const FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->interactive : FVIZ_FALSE;
}

FVizBool fviz_renderer_contains_normalized_point(
    const FVizRenderer* renderer,
    float x,
    float y)
{
    return renderer != NULL && x >= renderer->viewport[0] && x <= renderer->viewport[2] &&
        y >= renderer->viewport[1] && y <= renderer->viewport[3]
        ? FVIZ_TRUE
        : FVIZ_FALSE;
}
void fviz_renderer_fit_camera(FVizRenderer* renderer, float padding)
{
    if (renderer != NULL)
    {
        (void)fviz_renderer_update(renderer);
        const FVizBounds bounds = fviz_scene_bounds(renderer->scene);
        fviz_camera_fit_bounds(renderer->camera, &bounds, padding);
    }
}

void fviz_renderer_set_scalar_legend(FVizRenderer* renderer, FVizScalarLegend* legend)
{
    if (renderer == NULL) return;
    if (legend != NULL && fviz_retain(legend) == NULL) return;
    fviz_release(renderer->scalar_legend);
    renderer->scalar_legend = legend;
}

FVizScalarLegend* fviz_renderer_scalar_legend(FVizRenderer* renderer)
{
    return renderer != NULL ? renderer->scalar_legend : NULL;
}

FVizResult fviz_renderer_add_pass(FVizRenderer* renderer, FVizRenderPass* pass)
{
    FVizSize count;
    FVizSize index;
    FVizRenderPass** items;
    if (renderer == NULL || pass == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "renderer and render pass are required");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_array_count(renderer->passes);
    for (index = 0u; index < count; ++index)
        if (*(FVizRenderPass**)fviz_array_at(renderer->passes, index) == pass) return FVIZ_OK;
    if (fviz_retain(pass) == NULL) return fviz_last_error_code();
    if (fviz_array_resize(renderer->passes, count + 1u) != FVIZ_OK)
    {
        fviz_release(pass);
        return fviz_last_error_code();
    }
    items = (FVizRenderPass**)fviz_array_data(renderer->passes);
    index = count;
    while (index > 0u &&
           fviz_render_pass_stage(items[index - 1u]) > fviz_render_pass_stage(pass))
    {
        items[index] = items[index - 1u];
        --index;
    }
    items[index] = pass;
    fviz_object_modified((FVizObject*)renderer);
    return FVIZ_OK;
}

FVizResult fviz_renderer_remove_pass(FVizRenderer* renderer, FVizRenderPass* pass)
{
    FVizSize count;
    FVizSize index;
    FVizRenderPass** items;
    if (renderer == NULL || pass == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_array_count(renderer->passes);
    items = (FVizRenderPass**)fviz_array_data(renderer->passes);
    for (index = 0u; index < count; ++index)
    {
        if (items[index] == pass)
        {
            fviz_release(items[index]);
            if (index + 1u < count)
                (void)memmove(
                    &items[index], &items[index + 1u],
                    (size_t)(count - index - 1u) * sizeof(*items));
            (void)fviz_array_resize(renderer->passes, count - 1u);
            fviz_object_modified((FVizObject*)renderer);
            return FVIZ_OK;
        }
    }
    return FVIZ_ERROR_NOT_FOUND;
}

FVizResult fviz_renderer_reset_standard_passes(FVizRenderer* renderer)
{
    static const FVizRenderPassStage stages[] = {
        FVIZ_RENDER_PASS_CLEAR,
        FVIZ_RENDER_PASS_OPAQUE,
        FVIZ_RENDER_PASS_TRANSLUCENT,
        FVIZ_RENDER_PASS_EDGE,
        FVIZ_RENDER_PASS_SELECTION,
        FVIZ_RENDER_PASS_OVERLAY
    };
    FVizSize i;
    if (renderer == NULL || renderer->passes == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < fviz_array_count(renderer->passes); ++i)
        fviz_release(*(FVizRenderPass**)fviz_array_at(renderer->passes, i));
    fviz_array_clear(renderer->passes);
    for (i = 0u; i < sizeof(stages) / sizeof(stages[0]); ++i)
    {
        FVizRenderPass* pass = NULL;
        if (fviz_render_pass_create(stages[i], NULL, NULL, NULL, &pass) != FVIZ_OK ||
            fviz_renderer_add_pass(renderer, pass) != FVIZ_OK)
        {
            fviz_release(pass);
            return fviz_last_error_code();
        }
        fviz_release(pass);
    }
    return FVIZ_OK;
}

FVizSize fviz_renderer_pass_count(const FVizRenderer* renderer)
{
    return renderer != NULL ? fviz_array_count(renderer->passes) : 0u;
}

FVizRenderPass* fviz_renderer_pass_at(FVizRenderer* renderer, FVizSize index)
{
    FVizRenderPass** pass = renderer != NULL
        ? (FVizRenderPass**)fviz_array_at(renderer->passes, index)
        : NULL;
    return pass != NULL ? *pass : NULL;
}

static FVizResult fviz_renderer_transform_point(
    FVizMat4 matrix,
    FVizVec3 input,
    FVizBool divide,
    FVizVec3* output)
{
    float x;
    float y;
    float z;
    float w;
    if (output == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    x = matrix.m[0] * input.x + matrix.m[4] * input.y + matrix.m[8] * input.z + matrix.m[12];
    y = matrix.m[1] * input.x + matrix.m[5] * input.y + matrix.m[9] * input.z + matrix.m[13];
    z = matrix.m[2] * input.x + matrix.m[6] * input.y + matrix.m[10] * input.z + matrix.m[14];
    w = matrix.m[3] * input.x + matrix.m[7] * input.y + matrix.m[11] * input.z + matrix.m[15];
    if (divide != FVIZ_FALSE)
    {
        if (w == 0.0f) return FVIZ_ERROR_INVALID_STATE;
        x /= w;
        y /= w;
        z /= w;
    }
    *output = fviz_vec3(x, y, z);
    return FVIZ_OK;
}

FVizResult fviz_renderer_world_to_view(
    const FVizRenderer* renderer,
    FVizVec3 world,
    FVizVec3* out_view)
{
    if (renderer == NULL || renderer->camera == NULL || out_view == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_renderer_transform_point(
        fviz_camera_view_matrix(renderer->camera), world, FVIZ_FALSE, out_view);
}

FVizResult fviz_renderer_view_to_ndc(
    const FVizRenderer* renderer,
    FVizVec3 view,
    float aspect_ratio,
    FVizVec3* out_ndc)
{
    if (renderer == NULL || renderer->camera == NULL || out_ndc == NULL || aspect_ratio <= 0.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_renderer_transform_point(
        fviz_camera_projection_matrix(renderer->camera, aspect_ratio),
        view, FVIZ_TRUE, out_ndc);
}

FVizResult fviz_renderer_ndc_to_display(
    const FVizRenderer* renderer,
    FVizVec3 ndc,
    int window_width,
    int window_height,
    FVizVec3* out_display)
{
    float viewport_width;
    float viewport_height;
    if (renderer == NULL || out_display == NULL || window_width <= 0 || window_height <= 0)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    viewport_width = (renderer->viewport[2] - renderer->viewport[0]) * (float)window_width;
    viewport_height = (renderer->viewport[3] - renderer->viewport[1]) * (float)window_height;
    *out_display = fviz_vec3(
        renderer->viewport[0] * (float)window_width + (ndc.x * 0.5f + 0.5f) * viewport_width,
        (1.0f - renderer->viewport[3]) * (float)window_height +
            (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_height,
        ndc.z * 0.5f + 0.5f);
    return FVIZ_OK;
}

FVizResult fviz_renderer_display_to_world_ray(
    const FVizRenderer* renderer,
    float display_x,
    float display_y,
    int window_width,
    int window_height,
    FVizRay* out_ray)
{
    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
    if (renderer == NULL || renderer->camera == NULL || out_ray == NULL ||
        window_width <= 0 || window_height <= 0)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    viewport_x = (int)(renderer->viewport[0] * (float)window_width);
    viewport_y = (int)((1.0f - renderer->viewport[3]) * (float)window_height);
    viewport_width = (int)((renderer->viewport[2] - renderer->viewport[0]) * (float)window_width);
    viewport_height = (int)((renderer->viewport[3] - renderer->viewport[1]) * (float)window_height);
    if (viewport_width < 1 || viewport_height < 1 ||
        display_x < (float)viewport_x || display_y < (float)viewport_y ||
        display_x >= (float)(viewport_x + viewport_width) ||
        display_y >= (float)(viewport_y + viewport_height))
        return FVIZ_ERROR_NOT_FOUND;
    *out_ray = fviz_camera_pick_ray(
        renderer->camera,
        viewport_width,
        viewport_height,
        (int)(display_x - (float)viewport_x),
        (int)(display_y - (float)viewport_y));
    return FVIZ_OK;
}
