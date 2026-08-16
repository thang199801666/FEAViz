#include <FViz/FViz.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

int main(void)
{
    const FVizSize value_count = 2000000u;
    const uint32_t scan_repetitions = 10u;
    const FVizSize cached_repetitions = 1000000u;
    FVizDataArray* values = NULL;
    float* source = NULL;
    FVizSize i;
    uint32_t repetition;
    double minimum = 0.0;
    double maximum = 0.0;
    double checksum = 0.0;
    double started;
    double scan_elapsed;
    double cached_elapsed;

    if (fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &values) != FVIZ_OK) return 1;
    source = (float*)malloc((size_t)value_count * sizeof(float));
    if (source == NULL) return 2;
    for (i = 0u; i < value_count; ++i)
        source[i] = (float)((int)(i % 200003u) - 100001) * 0.001f;
    if (fviz_data_array_append_tuples(values, source, value_count) != FVIZ_OK) return 3;
    free(source);

    started = wall_seconds();
    for (repetition = 0u; repetition < scan_repetitions; ++repetition)
    {
        /* Force the cached query stale without changing the storage itself. */
        fviz_object_modified((FVizObject*)values);
        if (fviz_data_array_get_range(values, 0, FVIZ_TRUE, &minimum, &maximum) != FVIZ_OK)
            return 4;
        checksum += minimum + maximum;
    }
    scan_elapsed = wall_seconds() - started;

    /* Warm once, then measure the same MTime/query tuple repeatedly. */
    if (fviz_data_array_get_range(values, 0, FVIZ_TRUE, &minimum, &maximum) != FVIZ_OK)
        return 5;
    started = wall_seconds();
    for (i = 0u; i < cached_repetitions; ++i)
    {
        if (fviz_data_array_get_range(values, 0, FVIZ_TRUE, &minimum, &maximum) != FVIZ_OK)
            return 6;
        checksum += minimum * 1.0e-12 + maximum * 1.0e-12;
    }
    cached_elapsed = wall_seconds() - started;

    puts("values,scan_repetitions,scan_seconds,scan_ns_per_value,cached_repetitions,cached_seconds,cached_ns_per_call,minimum,maximum,checksum");
    printf("%llu,%u,%.9f,%.3f,%llu,%.9f,%.3f,%.6f,%.6f,%.6f\n",
        (unsigned long long)value_count,
        scan_repetitions,
        scan_elapsed,
        scan_elapsed * 1.0e9 / ((double)value_count * (double)scan_repetitions),
        (unsigned long long)cached_repetitions,
        cached_elapsed,
        cached_elapsed * 1.0e9 / (double)cached_repetitions,
        minimum, maximum, checksum);
    fviz_release(values);
    return 0;
}
