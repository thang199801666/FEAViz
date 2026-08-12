#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

static FVizResult create_color_wave(FVizPolyData** out_data, FVizDataArray** out_scalars)
{
    const FVizSize resolution = 64u;
    FVizPolyData* data = NULL;
    FVizDataArray* scalars = NULL;
    FVizSize z;
    FVizSize y;
    FVizSize i = 0u;

    if (fviz_poly_data_create(&data) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &scalars) != FVIZ_OK)
    {
        fviz_release(data);
        return fviz_last_error_code();
    }
    if (fviz_poly_data_reserve(data, resolution * resolution, 2u * (resolution - 1u) * (resolution - 1u)) != FVIZ_OK ||
        fviz_data_array_resize(scalars, resolution * resolution) != FVIZ_OK)
    {
        fviz_release(data);
        fviz_release(scalars);
        return fviz_last_error_code();
    }

    for (y = 0u; y < resolution; ++y)
    {
        const float fy = (float)y / (float)(resolution - 1u) * 4.0f;
        for (z = 0u; z < resolution; ++z)
        {
            const float fz = (float)z / (float)(resolution - 1u) * 4.0f;
            const FVizVec3 point = fviz_vec3(
                (float)y / (float)(resolution - 1u) * 2.0f - 1.0f,
                sinf(fy) * 0.25f * cosf(fz),
                (float)z / (float)(resolution - 1u) * 2.0f - 1.0f);
            uint32_t index;
            float scalar = sinf(fy) * cosf(fz);
            if (fviz_poly_data_add_point(data, point, &index) != FVIZ_OK ||
                fviz_data_array_set_tuple(scalars, i, &scalar) != FVIZ_OK)
            {
                fviz_release(data);
                fviz_release(scalars);
                return fviz_last_error_code();
            }
            ++i;
        }
    }
    for (y = 0u; y + 1u < resolution; ++y)
    {
        for (z = 0u; z + 1u < resolution; ++z)
        {
            const uint32_t a = (uint32_t)(y * resolution + z);
            const uint32_t b = (uint32_t)(a + 1u);
            const uint32_t c = (uint32_t)((y + 1u) * resolution + z);
            const uint32_t d = (uint32_t)(c + 1u);
            if (fviz_poly_data_add_triangle(data, a, c, b) != FVIZ_OK ||
                fviz_poly_data_add_triangle(data, b, c, d) != FVIZ_OK)
            {
                fviz_release(data);
                fviz_release(scalars);
                return fviz_last_error_code();
            }
        }
    }
    if (fviz_poly_data_compute_normals(data) != FVIZ_OK ||
        fviz_poly_data_set_scalars(data, scalars) != FVIZ_OK)
    {
        fviz_release(data);
        fviz_release(scalars);
        return fviz_last_error_code();
    }
    *out_data = data;
    *out_scalars = scalars;
    return FVIZ_OK;
}

int main(void)
{
    FVizPolyData* data = NULL;
    FVizDataArray* scalars = NULL;
    FVizActor* actor = NULL;
    FVizMapper* mapper = NULL;
    FVizRenderer* renderer = NULL;
    FVizRenderWindow* window = NULL;
    FVizResult result;

    result = create_color_wave(&data, &scalars);
    if (result != FVIZ_OK)
    {
        fprintf(stderr, "wave setup failed: %s\n", fviz_last_error_message());
        return 1;
    }
    printf("FEAViz %s | scalar-colored wave: points=%zu triangles=%zu\n",
        fviz_version_string(),
        (size_t)fviz_poly_data_point_count(data),
        (size_t)fviz_poly_data_triangle_count(data));

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        fprintf(stderr, "Native viewer backend is not available on this platform yet.\n");
        fviz_release(scalars);
        fviz_release(data);
        return 2;
    }

    if (fviz_actor_create(&actor) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor, data) != FVIZ_OK ||
        fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), actor) != FVIZ_OK ||
        fviz_render_window_create(1280, 800, "FEAViz 0.3 - Scalar Coloring", &window) != FVIZ_OK ||
        fviz_render_window_set_renderer(window, renderer) != FVIZ_OK)
    {
        fprintf(stderr, "FEAViz viewer setup failed: %s\n", fviz_last_error_message());
        fviz_release(window);
        fviz_release(renderer);
        fviz_release(actor);
        fviz_release(scalars);
        fviz_release(data);
        return 1;
    }

    mapper = fviz_actor_mapper(actor);
    fviz_mapper_set_scalar_visibility(mapper, FVIZ_TRUE);
    fviz_mapper_set_scalar_range(mapper, -1.0f, 1.0f);
    fviz_renderer_set_background(renderer, 0.08f, 0.09f, 0.12f);
    fviz_renderer_fit_camera(renderer, 1.25f);

    printf("Controls: LMB orbit | MMB pan | wheel zoom | F fit | W wireframe | Esc close\n");
    result = fviz_render_window_run(window);
    if (result != FVIZ_OK)
    {
        fprintf(stderr, "FEAViz render loop failed: %s\n", fviz_last_error_message());
    }

    fviz_release(window);
    fviz_release(renderer);
    fviz_release(actor);
    fviz_release(scalars);
    fviz_release(data);
    return result == FVIZ_OK ? 0 : 1;
}
