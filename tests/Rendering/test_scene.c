#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    FVizPolyData* data = NULL;
    FVizActor* actor = NULL;
    FVizScene* scene = NULL;
    FVizRenderer* renderer = NULL;
    uint32_t a,b,c;
    FVizBounds bounds;

    CHECK(fviz_poly_data_create(&data) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(-1,0,0), &a) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(1,0,0), &b) == FVIZ_OK);
    CHECK(fviz_poly_data_add_point(data, fviz_vec3(0,2,0), &c) == FVIZ_OK);
    CHECK(fviz_poly_data_add_triangle(data,a,b,c) == FVIZ_OK);
    CHECK(fviz_poly_data_compute_normals(data) == FVIZ_OK);
    CHECK(fviz_actor_create(&actor) == FVIZ_OK);
    CHECK(fviz_actor_set_poly_data(actor, data) == FVIZ_OK);
    CHECK(fviz_scene_create(&scene) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(scene, actor) == FVIZ_OK);
    CHECK(fviz_scene_actor_count(scene) == 1u);
    bounds = fviz_scene_bounds(scene);
    CHECK(bounds.valid == FVIZ_TRUE);
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_renderer_set_scene(renderer, scene) == FVIZ_OK);
    fviz_renderer_fit_camera(renderer, 1.2f);
    CHECK(fviz_vec3_length(fviz_vec3_sub(fviz_camera_position(fviz_renderer_camera(renderer)), fviz_camera_target(fviz_renderer_camera(renderer)))) > 0.0f);
    CHECK(fviz_scene_remove_actor(scene, actor) == FVIZ_OK);
    CHECK(fviz_scene_actor_count(scene) == 0u);

    fviz_release(renderer);
    fviz_release(scene);
    fviz_release(actor);
    fviz_release(data);
    return 0;
}
