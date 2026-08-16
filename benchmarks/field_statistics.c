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
    const int64_t extent[6] = {0, 1999, 0, 999, 0, 0};
    FVizImageData* image = NULL;
    FVizDataArray* field = NULL;
    FVizFieldStatisticsOptions options;
    FVizFieldStatistics statistics;
    FVizSize count;
    FVizSize i;
    double start;
    double finish;
    if (fviz_image_data_create(&image) != FVIZ_OK ||
        fviz_image_data_set_extent(image, extent) != FVIZ_OK ||
        fviz_image_data_allocate_point_scalars(
            image, "Vector", FVIZ_DATA_FLOAT32, 3u, &field) != FVIZ_OK)
        goto fail;
    count = fviz_image_data_point_count(image);
    {
        float* values = (float*)fviz_data_array_data(field);
        for (i = 0u; i < count; ++i)
        {
            values[i * 3u + 0u] = (float)(i % 257u) - 128.0f;
            values[i * 3u + 1u] = (float)(i % 101u) * 0.25f;
            values[i * 3u + 2u] = (float)(i % 53u) * -0.5f;
        }
    }
    fviz_object_modified((FVizObject*)field);
    fviz_field_statistics_options_initialize(&options);
    options.magnitude = FVIZ_TRUE;
    start = wall_seconds();
    if (fviz_field_statistics_compute(
            (FVizDataObject*)image, "Vector", &options, &statistics) != FVIZ_OK)
        goto fail;
    finish = wall_seconds();
    if (statistics.valid == FVIZ_FALSE || statistics.finite_tuple_count != count)
        goto fail;
    puts("tuples,seconds,tuples_per_second,min,max");
    printf("%llu,%.9f,%.2f,%.9f,%.9f\n",
        (unsigned long long)count,
        finish - start,
        (double)count / (finish - start),
        statistics.minimum.value,
        statistics.maximum.value);
    fviz_release(field);
    fviz_release(image);
    return 0;
fail:
    fviz_release(field);
    fviz_release(image);
    return 1;
}
