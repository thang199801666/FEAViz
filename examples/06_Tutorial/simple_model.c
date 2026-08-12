#include <stdio.h>

#include <FViz/FViz.h>

/*
 * FEAViz simple-model tutorial
 * ---------------------------
 * Creates a cube completely in memory, then displays it in a native
 * FEAViz render window.  No OBJ/STL file is required.
 */

static FVizResult create_cube(FVizPolyData** out_mesh)
{
    static const FVizVec3 points[8] = {
        {-1.0f, -1.0f, -1.0f},
        { 1.0f, -1.0f, -1.0f},
        { 1.0f,  1.0f, -1.0f},
        {-1.0f,  1.0f, -1.0f},
        {-1.0f, -1.0f,  1.0f},
        { 1.0f, -1.0f,  1.0f},
        { 1.0f,  1.0f,  1.0f},
        {-1.0f,  1.0f,  1.0f}
    };

    /* Two counter-clockwise triangles per cube face. */
    static const uint32_t triangles[12][3] = {
        {0u, 2u, 1u}, {0u, 3u, 2u}, /* back   */
        {4u, 5u, 6u}, {4u, 6u, 7u}, /* front  */
        {0u, 1u, 5u}, {0u, 5u, 4u}, /* bottom */
        {3u, 7u, 6u}, {3u, 6u, 2u}, /* top    */
        {0u, 4u, 7u}, {0u, 7u, 3u}, /* left   */
        {1u, 2u, 6u}, {1u, 6u, 5u}  /* right  */
    };

    FVizPolyData* mesh = NULL;
    FVizSize i;
    FVizResult result;

    if (out_mesh == NULL)
    {
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;

    result = fviz_poly_data_create(&mesh);
    if (result != FVIZ_OK)
    {
        return result;
    }

    result = fviz_poly_data_reserve(mesh, 8u, 12u);
    if (result != FVIZ_OK)
    {
        fviz_release(mesh);
        return result;
    }

    for (i = 0u; i < FVIZ_ARRAY_COUNT(points); ++i)
    {
        result = fviz_poly_data_add_point(mesh, points[i], NULL);
        if (result != FVIZ_OK)
        {
            fviz_release(mesh);
            return result;
        }
    }

    for (i = 0u; i < FVIZ_ARRAY_COUNT(triangles); ++i)
    {
        result = fviz_poly_data_add_triangle(
            mesh,
            triangles[i][0],
            triangles[i][1],
            triangles[i][2]);
        if (result != FVIZ_OK)
        {
            fviz_release(mesh);
            return result;
        }
    }

    result = fviz_poly_data_compute_normals(mesh);
    if (result != FVIZ_OK)
    {
        fviz_release(mesh);
        return result;
    }

    *out_mesh = mesh;
    return FVIZ_OK;
}

int main(void)
{
    FVizPolyData* mesh = NULL;
    FVizActor* actor = NULL;
    FVizRenderer* renderer = NULL;
    FVizRenderWindow* window = NULL;
    FVizResult result;

    result = create_cube(&mesh);
    if (result != FVIZ_OK)
    {
        fprintf(stderr, "create_cube failed: %s\n", fviz_result_string(result));
        return 1;
    }

    if (fviz_render_window_supported() == FVIZ_FALSE)
    {
        fprintf(stderr, "FEAViz native render-window backend is unavailable on this platform.\n");
        fviz_release(mesh);
        return 2;
    }

    result = fviz_actor_create(&actor);
    if (result != FVIZ_OK)
    {
        goto failed;
    }

    result = fviz_actor_set_poly_data(actor, mesh);
    if (result != FVIZ_OK)
    {
        goto failed;
    }

    /* Give the cube a light-blue material color. */
    fviz_actor_set_color(actor, 0.25f, 0.65f, 0.95f);

    result = fviz_renderer_create(&renderer);
    if (result != FVIZ_OK)
    {
        goto failed;
    }

    result = fviz_scene_add_actor(fviz_renderer_scene(renderer), actor);
    if (result != FVIZ_OK)
    {
        goto failed;
    }

    fviz_renderer_set_background(renderer, 0.07f, 0.08f, 0.11f);
    fviz_renderer_fit_camera(renderer, 1.35f);

    result = fviz_render_window_create(
        1100,
        720,
        "FEAViz - Simple Cube",
        &window);
    if (result != FVIZ_OK)
    {
        goto failed;
    }

    result = fviz_render_window_set_renderer(window, renderer);
    if (result != FVIZ_OK)
    {
        goto failed;
    }

    printf("FEAViz %s\n", fviz_version_string());
    printf("Simple cube: %zu points, %zu triangles\n",
        (size_t)fviz_poly_data_point_count(mesh),
        (size_t)fviz_poly_data_triangle_count(mesh));
    printf("Controls: LMB orbit | MMB pan | wheel zoom | F fit | W wireframe | Esc close\n");

    result = fviz_render_window_run(window);
    if (result != FVIZ_OK)
    {
        fprintf(stderr, "render loop failed: %s\n", fviz_last_error_message());
    }

    fviz_release(window);
    fviz_release(renderer);
    fviz_release(actor);
    fviz_release(mesh);
    return result == FVIZ_OK ? 0 : 1;

failed:
    fprintf(stderr, "FEAViz setup failed: %s (%s)\n",
        fviz_result_string(result),
        fviz_last_error_message());
    fviz_release(window);
    fviz_release(renderer);
    fviz_release(actor);
    fviz_release(mesh);
    return 1;
}
