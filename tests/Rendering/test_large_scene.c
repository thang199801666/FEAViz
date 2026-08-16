#include <stdio.h>
#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"CHECK failed: %s (%s:%d)\n",#expr,__FILE__,__LINE__); return 1; } } while(0)

static int make_triangle_actor(FVizActor** out_actor)
{
    FVizActor* actor = NULL;
    FVizPolyData* data = NULL;
    if (fviz_actor_create(&actor) != FVIZ_OK || fviz_poly_data_create(&data) != FVIZ_OK) return 0;
    if (fviz_poly_data_add_point(data, fviz_vec3(-0.5f,-0.5f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3( 0.5f,-0.5f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3( 0.0f, 0.5f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangle(data,0u,1u,2u) != FVIZ_OK ||
        fviz_actor_set_poly_data(actor,data) != FVIZ_OK)
    { fviz_release(data); fviz_release(actor); return 0; }
    fviz_release(data);
    *out_actor = actor;
    return 1;
}

int main(void)
{
    FVizRenderer* renderer = NULL;
    FVizActor* actor = NULL;
    FVizCamera* camera;
    FVizBounds b;
    FVizFrustum frustum;
    FVizTransform* transform = NULL;
    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(make_triangle_actor(&actor));
    camera = fviz_renderer_camera(renderer);
    fviz_camera_set_position(camera, fviz_vec3(0.0f,0.0f,5.0f));
    fviz_camera_set_target(camera, fviz_vec3(0.0f,0.0f,0.0f));
    fviz_camera_set_up(camera, fviz_vec3(0.0f,1.0f,0.0f));
    fviz_camera_set_perspective(camera,45.0f,0.1f,100.0f);
    CHECK(fviz_renderer_frustum_culling(renderer) == FVIZ_TRUE);
    CHECK(fviz_renderer_actor_in_frustum(renderer,actor,1.0f) == FVIZ_TRUE);
    CHECK(fviz_renderer_small_object_culling(renderer) == FVIZ_FALSE);
    CHECK(fviz_renderer_set_small_object_threshold_pixels(renderer, 24.0f) == FVIZ_OK);
    CHECK(fviz_renderer_actor_projected_diameter_pixels(renderer, actor, 1.0f, 1000) > 100.0f);
    fviz_renderer_set_small_object_culling(renderer, FVIZ_TRUE);
    CHECK(fviz_renderer_actor_is_renderable(renderer, actor, 1.0f, 1000) == FVIZ_TRUE);
    fviz_actor_set_scale(actor, fviz_vec3(0.001f,0.001f,0.001f));
    CHECK(fviz_renderer_actor_projected_diameter_pixels(renderer, actor, 1.0f, 1000) < 1.0f);
    CHECK(fviz_renderer_actor_is_renderable(renderer, actor, 1.0f, 1000) == FVIZ_FALSE);
    fviz_actor_set_scale(actor, fviz_vec3(1.0f,1.0f,1.0f));
    fviz_renderer_set_small_object_culling(renderer, FVIZ_FALSE);
    CHECK(fviz_renderer_get_frustum(renderer,1.0f,&frustum) == FVIZ_OK);
    CHECK(fviz_frustum_contains_point(&frustum,fviz_vec3(0.0f,0.0f,0.0f)) == FVIZ_TRUE);
    CHECK(fviz_frustum_contains_point(&frustum,fviz_vec3(1000.0f,0.0f,0.0f)) == FVIZ_FALSE);
    fviz_actor_set_position(actor,fviz_vec3(1000.0f,0.0f,0.0f));
    CHECK(fviz_renderer_actor_in_frustum(renderer,actor,1.0f) == FVIZ_FALSE);
    fviz_renderer_set_frustum_culling(renderer,FVIZ_FALSE);
    CHECK(fviz_renderer_actor_in_frustum(renderer,actor,1.0f) == FVIZ_TRUE);
    fviz_renderer_set_frustum_culling(renderer,FVIZ_TRUE);
    fviz_actor_set_position(actor,fviz_vec3(2.0f,3.0f,4.0f));
    b=fviz_actor_bounds(actor);
    CHECK(b.valid==FVIZ_TRUE);
    CHECK(b.min.x>1.49f && b.max.x<2.51f);
    CHECK(b.min.y>2.49f && b.max.y<3.51f);
    CHECK(fviz_transform_create(&transform) == FVIZ_OK);
    CHECK(fviz_actor_set_user_transform(actor,transform) == FVIZ_OK);
    b=fviz_actor_bounds(actor);
    fviz_transform_translate(transform,fviz_vec3(7.0f,0.0f,0.0f));
    b=fviz_actor_bounds(actor);
    CHECK(b.min.x>8.49f && b.max.x<9.51f);
    CHECK(fviz_actor_pickable(actor)==FVIZ_TRUE);
    fviz_actor_set_pickable(actor,FVIZ_FALSE);
    CHECK(fviz_actor_pickable(actor)==FVIZ_FALSE);
    fviz_release(transform);
    fviz_release(actor);
    fviz_release(renderer);
    return 0;
}
