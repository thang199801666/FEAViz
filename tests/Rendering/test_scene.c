#include <math.h>
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

    fviz_actor_set_position(actor, fviz_vec3(1.0f, 2.0f, 3.0f));
    CHECK(fviz_actor_position(actor).x == 1.0f && fviz_actor_position(actor).y == 2.0f);
    fviz_actor_set_orientation(actor, fviz_quat_from_axis_angle(fviz_vec3(0, 1, 0), FVIZ_PI_F * 0.5f));
    fviz_actor_set_scale(actor, fviz_vec3(2.0f, 2.0f, 2.0f));
    {
        FVizMat4 transform = fviz_actor_transform_matrix(actor);
        CHECK(transform.m[12] == 1.0f && transform.m[13] == 2.0f && transform.m[14] == 3.0f);
        CHECK(fabsf(transform.m[2] + 2.0f) < 1.0e-5f);
        CHECK(fabsf(transform.m[5] - 2.0f) < 1.0e-5f);
        CHECK(fabsf(transform.m[8] - 2.0f) < 1.0e-5f);
    }
    fviz_actor_set_orientation(actor, fviz_quat_identity());
    fviz_actor_set_scale(actor, fviz_vec3(2.0f, 2.0f, 2.0f));
    {
        FVizMat4 transform = fviz_actor_transform_matrix(actor);
        CHECK(transform.m[0] == 2.0f && transform.m[5] == 2.0f && transform.m[10] == 2.0f);
    }
    fviz_actor_set_scale(actor, fviz_vec3(1.0f, 1.0f, 1.0f));
    fviz_actor_set_position(actor, fviz_vec3(0.0f, 0.0f, 0.0f));
    {
        FVizMat4 transform = fviz_actor_transform_matrix(actor);
        CHECK(transform.m[0] == 1.0f && transform.m[5] == 1.0f && transform.m[10] == 1.0f);
    }

    fviz_release(renderer);
    fviz_release(scene);
    fviz_release(actor);
    fviz_release(data);
    return 0;
}
