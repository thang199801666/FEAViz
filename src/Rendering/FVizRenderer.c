#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizRenderer.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRendererPrivate.h>

static void fviz_renderer_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_renderer_class = {
    FVIZ_TYPE_RENDERER,
    "FVizRenderer",
    &g_fviz_object_class,
    fviz_renderer_destroy
};

static void fviz_renderer_destroy(FVizObject* object)
{
    FVizRenderer* renderer = (FVizRenderer*)object;
    fviz_release(renderer->scene);
    fviz_release(renderer->camera);
    renderer->scene = NULL;
    renderer->camera = NULL;
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
void fviz_renderer_fit_camera(FVizRenderer* renderer, float padding)
{
    if (renderer != NULL)
    {
        const FVizBounds bounds = fviz_scene_bounds(renderer->scene);
        fviz_camera_fit_bounds(renderer->camera, &bounds, padding);
    }
}
