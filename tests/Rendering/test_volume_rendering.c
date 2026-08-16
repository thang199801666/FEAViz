#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { \
    (void)fprintf(stderr, "volume check failed at line %d: %s\n", __LINE__, #expr); \
    return __LINE__; \
} } while (0)

static FVizResult create_sphere_volume(FVizImageData** out_image)
{
    const int64_t extent[6] = {0, 15, 0, 15, 0, 15};
    const double origin[3] = {0.0, 0.0, 0.0};
    const double spacing[3] = {0.25, 0.25, 0.25};
    FVizImageData* image = NULL;
    FVizDataArray* scalars = NULL;
    float* values;
    FVizSize n;
    FVizResult result;
    if (out_image == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_image = NULL;
    result = fviz_image_data_create(&image);
    if (result != FVIZ_OK) return result;
    if (fviz_image_data_set_extent(image, extent) != FVIZ_OK ||
        fviz_image_data_set_origin(image, origin) != FVIZ_OK ||
        fviz_image_data_set_spacing(image, spacing) != FVIZ_OK ||
        fviz_image_data_allocate_point_scalars(image, "Density", FVIZ_DATA_FLOAT32, 1u, &scalars) != FVIZ_OK)
    {
        fviz_release(image);
        return fviz_last_error_code();
    }
    values = (float*)fviz_data_array_data(scalars);
    if (values == NULL)
    {
        fviz_release(image);
        return FVIZ_ERROR_GRAPHICS;
    }
    for (n = 0u; n < fviz_data_array_tuple_count(scalars); ++n)
    {
        int64_t pijk[3];
        const double dx = 4.0;
        const double dy = 4.0;
        const double dz = 4.0;
        if (fviz_image_data_point_ijk(image, (FVizId)n, pijk) != FVIZ_OK)
        {
            fviz_release(image);
            return FVIZ_ERROR_INTERNAL;
        }
        values[n] = (float)(1.0 -
            (double)((pijk[0] - 8) * (pijk[0] - 8) / (dx * dx) +
                     (pijk[1] - 8) * (pijk[1] - 8) / (dy * dy) +
                     (pijk[2] - 8) * (pijk[2] - 8) / (dz * dz)));
    }
    *out_image = image;
    return FVIZ_OK;
}

static FVizResult create_volume_actor(FVizActor** out_actor)
{
    FVizImageData* image = NULL;
    FVizVolumeMapper* mapper = NULL;
    FVizActor* actor = NULL;
    FVizResult result;
    if (out_actor == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_actor = NULL;
    result = create_sphere_volume(&image);
    if (result != FVIZ_OK) return result;
    result = fviz_volume_mapper_create(&mapper);
    if (result != FVIZ_OK) goto fail;
    result = fviz_volume_mapper_set_image_data(mapper, image);
    if (result != FVIZ_OK) goto fail;
    fviz_volume_mapper_add_color_point(mapper, -1.0f, 0.0f, 0.0f, 0.6f);
    fviz_volume_mapper_add_color_point(mapper, 0.0f, 0.1f, 0.7f, 1.0f);
    fviz_volume_mapper_add_color_point(mapper, 1.0f, 1.0f, 0.4f, 0.1f);
    fviz_volume_mapper_add_opacity_point(mapper, -1.0f, 0.0f);
    fviz_volume_mapper_add_opacity_point(mapper, 0.4f, 0.25f);
    fviz_volume_mapper_add_opacity_point(mapper, 1.0f, 0.95f);
    fviz_volume_mapper_set_scalar_range(mapper, -1.0f, 1.0f);
    fviz_volume_mapper_set_sampling_step(mapper, 0.1f);
    fviz_volume_mapper_set_shading(mapper, FVIZ_TRUE);
    result = fviz_actor_create(&actor);
    if (result != FVIZ_OK) goto fail;
    result = fviz_actor_set_volume_mapper(actor, mapper);
    if (result != FVIZ_OK) goto fail;
    *out_actor = actor;
    fviz_release(mapper);
    fviz_release(image);
    return FVIZ_OK;
fail:
    fviz_release(actor);
    fviz_release(mapper);
    fviz_release(image);
    return fviz_last_error_code();
}

int main(void)
{
    const int width = 96;
    const int height = 72;
    const FVizSize bytes = (FVizSize)width * (FVizSize)height * 4u;
    FVizRenderWindow* window = NULL;
    FVizRenderer* renderer;
    FVizScene* scene;
    FVizActor* volume_actor = NULL;
    uint8_t* framebuffer = NULL;
    FVizSize i;
    FVizSize colored_pixels = 0u;
    FVizRenderStatistics statistics;

    if (fviz_render_window_supported() == FVIZ_FALSE) return 0;
    CHECK(fviz_render_window_create_offscreen(width, height, &window) == FVIZ_OK);
    fviz_render_window_set_fxaa(window, FVIZ_FALSE);
    renderer = fviz_render_window_renderer(window);
    scene = fviz_renderer_scene(renderer);
    fviz_renderer_set_background(renderer, 0.0f, 0.0f, 0.0f);
    CHECK(create_volume_actor(&volume_actor) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(scene, volume_actor) == FVIZ_OK);
    fviz_renderer_fit_camera(renderer, 1.5f);

    framebuffer = (uint8_t*)fviz_alloc(bytes);
    CHECK(framebuffer != NULL);
    CHECK(fviz_render_window_render(window) == FVIZ_OK);
    CHECK(fviz_render_window_read_rgba8(window, framebuffer, bytes) == FVIZ_OK);
    for (i = 0u; i < bytes; i += 4u)
        if (framebuffer[i] > 8u || framebuffer[i + 1u] > 8u || framebuffer[i + 2u] > 8u)
            ++colored_pixels;
    CHECK(colored_pixels > 150u);
    fviz_render_window_get_statistics(window, &statistics);
    CHECK(statistics.draw_calls >= 1u);

    fviz_free(framebuffer);
    fviz_release(volume_actor);
    fviz_release(window);
    return 0;
}
