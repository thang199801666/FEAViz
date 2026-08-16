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
    const FVizSize actor_count = 5000u;
    const FVizSize query_count = 5000u;
    FVizScene* scene = NULL;
    FVizMTime checksum = 0u;
    FVizSize i;
    double start;
    double seconds;

    if (fviz_scene_create(&scene) != FVIZ_OK) return 1;
    if (fviz_scene_reserve(scene, actor_count) != FVIZ_OK) return 2;

    for (i = 0u; i < actor_count; ++i)
    {
        FVizActor* actor = NULL;
        if (fviz_actor_create(&actor) != FVIZ_OK) return 3;
        if (fviz_scene_add_actor(scene, actor) != FVIZ_OK)
        {
            fviz_release(actor);
            return 4;
        }
        fviz_release(actor);
    }

    start = wall_seconds();
    for (i = 0u; i < query_count; ++i)
        checksum ^= fviz_object_mtime((const FVizObject*)scene);
    seconds = wall_seconds() - start;

    puts("actors,queries,seconds,ns_per_query,checksum");
    printf("%llu,%llu,%.9f,%.3f,%llu\n",
        (unsigned long long)actor_count,
        (unsigned long long)query_count,
        seconds,
        seconds > 0.0 ? seconds * 1.0e9 / (double)query_count : 0.0,
        (unsigned long long)checksum);

    fviz_release(scene);
    return 0;
}
