#include <FViz/FViz.h>

#include <stdio.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

int main(void)
{
    const FVizSize actor_count = 20000u;
    FVizRenderer* renderer = NULL;
    FVizPolyData* data = NULL;
    FVizActor** actors = NULL;
    FVizSize i;
    FVizSize visible = 0u;
    double build_start;
    double build_seconds;
    double cull_start;
    double cull_seconds;

    actors = (FVizActor**)fviz_alloc(actor_count * sizeof(*actors));
    if (actors == NULL || fviz_renderer_create(&renderer) != FVIZ_OK ||
        fviz_poly_data_create(&data) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3(-0.4f,-0.4f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3( 0.4f,-0.4f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3( 0.0f, 0.4f,0.0f), NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangle(data,0u,1u,2u) != FVIZ_OK)
        return 1;
    {
        FVizCamera* camera = fviz_renderer_camera(renderer);
        fviz_camera_set_position(camera, fviz_vec3(0.0f,0.0f,30.0f));
        fviz_camera_set_target(camera, fviz_vec3(0.0f,0.0f,0.0f));
        fviz_camera_set_up(camera, fviz_vec3(0.0f,1.0f,0.0f));
        fviz_camera_set_perspective(camera,45.0f,0.1f,2000.0f);
    }
    build_start = wall_seconds();
    for (i=0u;i<actor_count;++i)
    {
        const FVizSize gx = i % 200u;
        const FVizSize gy = i / 200u;
        const float x = ((float)gx - 100.0f) * 3.0f;
        const float y = ((float)gy - 50.0f) * 3.0f;
        actors[i] = NULL;
        if (fviz_actor_create(&actors[i]) != FVIZ_OK ||
            fviz_actor_set_poly_data(actors[i], data) != FVIZ_OK)
            return 2;
        fviz_actor_set_position(actors[i], fviz_vec3(x,y,0.0f));
        if (fviz_scene_add_actor(fviz_renderer_scene(renderer), actors[i]) != FVIZ_OK)
            return 3;
    }
    build_seconds = wall_seconds() - build_start;
    /* Warm world-bounds caches once, then measure the steady-state camera-move path. */
    for (i=0u;i<actor_count;++i)
        (void)fviz_renderer_actor_in_frustum(renderer,actors[i],16.0f/9.0f);
    cull_start = wall_seconds();
    for (i=0u;i<actor_count;++i)
        if (fviz_renderer_actor_in_frustum(renderer,actors[i],16.0f/9.0f) != FVIZ_FALSE)
            ++visible;
    cull_seconds = wall_seconds() - cull_start;
    if (visible == 0u || visible >= actor_count) return 4;

    puts("actors,visible,build_seconds,cull_seconds,cull_tests_per_second");
    printf("%llu,%llu,%.9f,%.9f,%.0f\n",
        (unsigned long long)actor_count,
        (unsigned long long)visible,
        build_seconds,
        cull_seconds,
        cull_seconds > 0.0 ? (double)actor_count / cull_seconds : 0.0);

    for (i=0u;i<actor_count;++i) fviz_release(actors[i]);
    fviz_free(actors);
    fviz_release(data);
    fviz_release(renderer);
    return 0;
}
