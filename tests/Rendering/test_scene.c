#include <math.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizBool count_scene_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    int* count = (int*)client_data;
    (void)caller; (void)event_id; (void)call_data;
    ++(*count);
    return FVIZ_FALSE;
}

int main(void)
{
    FVizPolyData* data = NULL;
    FVizActor* actor = NULL;
    FVizScene* scene = NULL;
    FVizRenderer* renderer = NULL;
    FVizTransform* user_transform = NULL;
    FVizLight* scene_light = NULL;
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
    {
        float ambient = 0.0f;
        float diffuse = 0.0f;
        float specular = 0.0f;
        float power = 0.0f;
        fviz_actor_set_material(actor, 0.2f, 0.7f, 0.6f, 64.0f);
        fviz_actor_get_material(actor, &ambient, &diffuse, &specular, &power);
        CHECK(fabsf(ambient - 0.2f) < 1.0e-6f);
        CHECK(fabsf(diffuse - 0.7f) < 1.0e-6f);
        CHECK(fabsf(specular - 0.6f) < 1.0e-6f);
        CHECK(fabsf(power - 64.0f) < 1.0e-6f);
        fviz_actor_set_shading_mode(actor, FVIZ_SHADING_FLAT);
        fviz_actor_set_cull_mode(actor, FVIZ_CULL_NONE);
        CHECK(fviz_actor_shading_mode(actor) == FVIZ_SHADING_FLAT);
        CHECK(fviz_actor_cull_mode(actor) == FVIZ_CULL_NONE);
    }
    CHECK(fviz_scene_create(&scene) == FVIZ_OK);
    CHECK(fviz_scene_add_actor(scene, actor) == FVIZ_OK);
    CHECK(fviz_scene_actor_count(scene) == 1u);
    bounds = fviz_scene_bounds(scene);
    CHECK(bounds.valid == FVIZ_TRUE);
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    {
        FVizMTime mtime;
        float r, g, blue_value;
        float x0, y0, x1, y1;
        fviz_renderer_get_background(renderer, &r, &g, &blue_value);
        mtime = fviz_object_mtime((FVizObject*)renderer);
        fviz_renderer_set_background(renderer, r, g, blue_value);
        CHECK(fviz_object_mtime((FVizObject*)renderer) == mtime);
        fviz_renderer_get_background2(renderer, &r, &g, &blue_value);
        fviz_renderer_set_background2(renderer, r, g, blue_value);
        CHECK(fviz_object_mtime((FVizObject*)renderer) == mtime);
        fviz_renderer_set_gradient_background(
            renderer, fviz_renderer_gradient_background(renderer));
        CHECK(fviz_object_mtime((FVizObject*)renderer) == mtime);
        fviz_renderer_set_frustum_culling(
            renderer, fviz_renderer_frustum_culling(renderer));
        CHECK(fviz_object_mtime((FVizObject*)renderer) == mtime);
        fviz_renderer_get_viewport(renderer, &x0, &y0, &x1, &y1);
        CHECK(fviz_renderer_set_viewport(renderer, x0, y0, x1, y1) == FVIZ_OK);
        CHECK(fviz_object_mtime((FVizObject*)renderer) == mtime);
    }
    CHECK(fviz_renderer_light_count(renderer) == 1u);
    CHECK(fviz_light_type(fviz_renderer_light_at(renderer, 0u)) == FVIZ_LIGHT_HEADLIGHT);
    CHECK(fviz_light_create(&scene_light) == FVIZ_OK);
    fviz_light_set_type(scene_light, FVIZ_LIGHT_SCENE);
    fviz_light_set_position(scene_light, fviz_vec3(3.0f, 4.0f, 5.0f));
    fviz_light_set_color(scene_light, 0.8f, 0.7f, 0.6f);
    fviz_light_set_intensity(scene_light, 1.5f);
    CHECK(fviz_renderer_add_light(renderer, scene_light) == FVIZ_OK);
    CHECK(fviz_renderer_add_light(renderer, scene_light) == FVIZ_OK);
    CHECK(fviz_renderer_light_count(renderer) == 2u);
    CHECK(fviz_light_type(fviz_renderer_light_at(renderer, 1u)) == FVIZ_LIGHT_SCENE);
    CHECK(fabsf(fviz_light_intensity(scene_light) - 1.5f) < 1.0e-6f);
    {
        float background_r = 0.0f;
        float background_g = 0.0f;
        float background_b = 0.0f;
        FVizRenderWindowOptions options;
        FVizFXAAOptions fxaa_options;
        fviz_renderer_set_background2(renderer, 0.3f, 0.4f, 0.5f);
        fviz_renderer_get_background2(renderer, &background_r, &background_g, &background_b);
        CHECK(fabsf(background_r - 0.3f) < 1.0e-6f);
        CHECK(fabsf(background_g - 0.4f) < 1.0e-6f);
        CHECK(fabsf(background_b - 0.5f) < 1.0e-6f);
        fviz_renderer_set_gradient_background(renderer, FVIZ_TRUE);
        CHECK(fviz_renderer_gradient_background(renderer) == FVIZ_TRUE);
        fviz_render_window_options_initialize(&options);
        CHECK(options.multisamples == 4u);
        CHECK(options.fxaa == FVIZ_TRUE);
        CHECK(options.swap_interval == 1);
        CHECK(options.adaptive_antialiasing == FVIZ_TRUE);
        fviz_fxaa_options_initialize(&fxaa_options);
        CHECK(fabsf(fxaa_options.relative_threshold - 0.125f) < 1.0e-6f);
        CHECK(fabsf(fxaa_options.absolute_threshold - 0.0312f) < 1.0e-6f);
        CHECK(fabsf(fxaa_options.span_max - 8.0f) < 1.0e-6f);
    }
    CHECK(fviz_renderer_set_scene(renderer, scene) == FVIZ_OK);
    {
        const FVizMTime mtime = fviz_object_mtime((FVizObject*)renderer);
        CHECK(fviz_renderer_set_scene(renderer, scene) == FVIZ_OK);
        CHECK(fviz_object_mtime((FVizObject*)renderer) == mtime);
    }
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
    CHECK(fviz_transform_create(&user_transform) == FVIZ_OK);
    fviz_transform_translate(user_transform, fviz_vec3(5.0f, 0.0f, 0.0f));
    CHECK(fviz_actor_set_user_transform(actor, user_transform) == FVIZ_OK);
    CHECK(fviz_actor_user_transform(actor) == user_transform);
    {
        FVizMat4 transform = fviz_actor_transform_matrix(actor);
        CHECK(transform.m[12] == 5.0f);
    }

    {
        FVizScene* batch_scene = NULL;
        FVizActor* batch_actors[3] = {NULL, NULL, NULL};
        FVizObserverTag scene_tag = FVIZ_OBSERVER_TAG_INVALID;
        int scene_modified_count = 0;
        FVizMTime actor_mtime;
        FVizMTime camera_mtime;
        FVizCamera* camera = fviz_renderer_camera(renderer);
        FVizSize i;
        CHECK(fviz_scene_create(&batch_scene) == FVIZ_OK);
        CHECK(fviz_scene_reserve(batch_scene, 16u) == FVIZ_OK);
        for (i = 0u; i < 3u; ++i) CHECK(fviz_actor_create(&batch_actors[i]) == FVIZ_OK);
        CHECK(fviz_object_add_observer(
            (FVizObject*)batch_scene, FVIZ_EVENT_MODIFIED, 0.0f,
            count_scene_modified, &scene_modified_count, &scene_tag) == FVIZ_OK);
        CHECK(fviz_scene_add_actors(batch_scene, batch_actors, 3u) == FVIZ_OK);
        CHECK(fviz_scene_actor_count(batch_scene) == 3u);
        CHECK(scene_modified_count == 1);
        actor_mtime = fviz_object_mtime((FVizObject*)batch_actors[0]);
        fviz_actor_set_opacity(batch_actors[0], fviz_actor_opacity(batch_actors[0]));
        fviz_actor_set_position(batch_actors[0], fviz_actor_position(batch_actors[0]));
        CHECK(fviz_object_mtime((FVizObject*)batch_actors[0]) == actor_mtime);
        camera_mtime = fviz_object_mtime((FVizObject*)camera);
        fviz_camera_set_position(camera, fviz_camera_position(camera));
        fviz_camera_pan(camera, 0.0f, 0.0f);
        fviz_camera_dolly(camera, 1.0f);
        CHECK(fviz_object_mtime((FVizObject*)camera) == camera_mtime);
        CHECK(fviz_object_remove_observer((FVizObject*)batch_scene, scene_tag) == FVIZ_OK);
        for (i = 0u; i < 3u; ++i) fviz_release(batch_actors[i]);
        fviz_release(batch_scene);
    }

    fviz_release(user_transform);
    fviz_release(scene_light);
    fviz_release(renderer);
    fviz_release(scene);
    fviz_release(actor);
    fviz_release(data);
    return 0;
}
