#include <FViz/FViz.h>

#include <stdio.h>

int main(void)
{
    enum { ACTOR_COUNT = 49 };
    FVizRenderer* renderer = NULL;
    FVizPolyData* triangle = NULL;
    FVizActor* actors[ACTOR_COUNT] = {0};
    FVizSelection* screen_selection = NULL;
    FVizSelection* world_selection = NULL;
    FVizSelectionModel* model = NULL;
    FVizFrustum frustum;
    FVizCamera* camera;
    FVizSize i;
    FVizSize small_count = 0u;
    const int width = 800;
    const int height = 600;

    if (fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_poly_data_create(&triangle) != FVIZ_OK ||
        fviz_selection_model_create(&model) != FVIZ_OK)
        return 1;
    if (fviz_poly_data_add_point(triangle, fviz_vec3(-0.35f,-0.35f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(triangle, fviz_vec3( 0.35f,-0.35f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(triangle, fviz_vec3( 0.00f, 0.35f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangle(triangle, 0u, 1u, 2u) != FVIZ_OK)
        return 2;

    camera = fviz_renderer_camera(renderer);
    fviz_camera_set_position(camera, fviz_vec3(0.0f,0.0f,15.0f));
    fviz_camera_set_target(camera, fviz_vec3(0.0f,0.0f,0.0f));
    fviz_camera_set_up(camera, fviz_vec3(0.0f,1.0f,0.0f));
    fviz_camera_set_perspective(camera, 45.0f, 0.1f, 100.0f);
    fviz_renderer_set_small_object_culling(renderer, FVIZ_TRUE);
    if (fviz_renderer_set_small_object_threshold_pixels(renderer, 8.0f) != FVIZ_OK)
        return 3;

    for (i = 0u; i < ACTOR_COUNT; ++i)
    {
        const int gx = (int)(i % 7u) - 3;
        const int gy = (int)(i / 7u) - 3;
        if (fviz_actor_create(&actors[i]) != FVIZ_OK ||
            fviz_actor_set_poly_data(actors[i], triangle) != FVIZ_OK)
            return 4;
        fviz_actor_set_position(actors[i], fviz_vec3((float)gx * 1.15f, (float)gy * 1.15f, 0.0f));
        if ((i % 5u) == 0u)
        {
            fviz_actor_set_scale(actors[i], fviz_vec3(0.01f,0.01f,0.01f));
            ++small_count;
        }
        if (i == 1u) fviz_actor_set_pickable(actors[i], FVIZ_FALSE);
        if (fviz_scene_add_actor(fviz_renderer_scene(renderer), actors[i]) != FVIZ_OK)
            return 5;
    }

    if (fviz_selection_select_rectangle(renderer, width, height, 0, 0, width - 1, height - 1,
            FVIZ_SELECTION_ACTOR, &screen_selection) != FVIZ_OK ||
        fviz_renderer_get_frustum(renderer, (float)width/(float)height, &frustum) != FVIZ_OK ||
        fviz_selection_select_frustum(renderer, &frustum, FVIZ_SELECTION_ACTOR, &world_selection) != FVIZ_OK)
        return 6;

    fviz_selection_model_set_association(model, FVIZ_SELECTION_ACTOR);
    if (fviz_selection_model_apply(model, screen_selection, FVIZ_SELECTION_REPLACE) != FVIZ_OK)
        return 7;
    if (fviz_selection_count(fviz_selection_model_selection(model)) !=
        fviz_selection_count(screen_selection))
        return 8;
    if (fviz_selection_model_select_frustum(model, renderer, &frustum, FVIZ_SELECTION_ADD) != FVIZ_OK)
        return 9;

    printf("FEAViz 0.25 selection/large-scene\n");
    printf("actors=%u small=%llu screen-selected=%llu world-selected=%llu model-after-add=%llu\n",
        ACTOR_COUNT,
        (unsigned long long)small_count,
        (unsigned long long)fviz_selection_count(screen_selection),
        (unsigned long long)fviz_selection_count(world_selection),
        (unsigned long long)fviz_selection_count(fviz_selection_model_selection(model)));
    printf("integer associations=Actor/Point/Cell/Edge/GlyphInstance; modifiers=Replace/Add/Subtract/Toggle\n");

    if (fviz_selection_count(world_selection) <= fviz_selection_count(screen_selection) ||
        fviz_selection_count(world_selection) != ACTOR_COUNT - 1u)
        return 10;

    fviz_release(world_selection);
    fviz_release(screen_selection);
    fviz_release(model);
    for (i = 0u; i < ACTOR_COUNT; ++i) fviz_release(actors[i]);
    fviz_release(triangle);
    fviz_release(renderer);
    return 0;
}
