#include <FViz/Core/FVizError.h>
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
    renderer->scene = NULL;
    renderer->camera = NULL;
    renderer->scalar_legend = NULL;
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
    if (fviz_scene_create(&renderer->scene) != FVIZ_OK || fviz_camera_create(&renderer->camera) != FVIZ_OK)
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
