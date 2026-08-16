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
    enum { STEP_COUNT = 5000, QUERY_COUNT = 20000 };
    FVizTemporalDataSet* temporal = NULL;
    FVizPolyData* frame = NULL;
    volatile FVizMTime checksum = 0u;
    uint32_t i;
    double start;
    double finish;

    if (fviz_temporal_data_set_create(&temporal) != FVIZ_OK ||
        fviz_poly_data_create(&frame) != FVIZ_OK ||
        fviz_temporal_data_set_reserve(temporal, STEP_COUNT) != FVIZ_OK)
        goto fail;
    for (i = 0u; i < STEP_COUNT; ++i)
        if (fviz_temporal_data_set_add_step(
                temporal, (double)i, (FVizDataObject*)frame, NULL) != FVIZ_OK)
            goto fail;

    start = wall_seconds();
    for (i = 0u; i < QUERY_COUNT; ++i)
        checksum ^= fviz_object_mtime((const FVizObject*)temporal);
    finish = wall_seconds();

    puts("steps,queries,seconds,nanoseconds_per_mtime,checksum");
    printf("%u,%u,%.9f,%.3f,%llu\n",
        (unsigned)STEP_COUNT,
        (unsigned)QUERY_COUNT,
        finish - start,
        (finish - start) * 1.0e9 / (double)QUERY_COUNT,
        (unsigned long long)checksum);

    fviz_release(frame);
    fviz_release(temporal);
    return 0;
fail:
    fviz_release(frame);
    fviz_release(temporal);
    return 1;
}
