#include <FViz/FViz.h>

#include <stdint.h>
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
    const FVizSize field_count = 256u;
    const FVizSize repetitions = 1000000u;
    FVizAttributeSet* attributes = NULL;
    FVizDataArray* array = NULL;
    char name[32];
    FVizSize i;
    uintptr_t checksum = 0u;
    double started;
    double elapsed;

    if (fviz_attribute_set_create(&attributes) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 1u, &array) != FVIZ_OK)
        return 1;
    for (i = 0u; i < field_count; ++i)
    {
        (void)snprintf(name, sizeof(name), "field_%04llu", (unsigned long long)i);
        if (fviz_attribute_set_add(attributes, name, array) != FVIZ_OK) return 2;
    }

    started = wall_seconds();
    for (i = 0u; i < repetitions; ++i)
    {
        FVizDataArray* found = fviz_attribute_set_get(attributes, "field_0255");
        if (found == NULL) return 3;
        checksum ^= (uintptr_t)found + (uintptr_t)i;
    }
    elapsed = wall_seconds() - started;

    puts("fields,repetitions,seconds,ns_per_lookup,checksum");
    printf("%llu,%llu,%.9f,%.3f,%llu\n",
        (unsigned long long)field_count,
        (unsigned long long)repetitions,
        elapsed,
        elapsed * 1.0e9 / (double)repetitions,
        (unsigned long long)checksum);

    fviz_release(array);
    fviz_release(attributes);
    return 0;
}
