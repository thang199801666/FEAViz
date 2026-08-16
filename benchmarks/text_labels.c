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
    const FVizSize label_count = 20000u;
    const FVizSize mtime_queries = 200000u;
    FVizLabelSet3D* labels = NULL;
    FVizTextProperty* property;
    FVizTextMetrics metrics;
    FVizSize i;
    double start;
    double build_seconds;
    double mtime_seconds;
    double measure_seconds;
    volatile FVizMTime sink = 0u;

    if (fviz_label_set_3d_create(&labels) != FVIZ_OK ||
        fviz_label_set_3d_reserve(labels, label_count) != FVIZ_OK)
        return 1;
    property = fviz_label_set_3d_text_property(labels);
    fviz_text_property_set_font_size(property, 12.0f);

    start = wall_seconds();
    for (i = 0u; i < label_count; ++i)
    {
        char text[32];
        FVizVec3 p = fviz_vec3((float)(i % 200u), (float)(i / 200u), (float)(i % 17u) * 0.1f);
        (void)snprintf(text, sizeof(text), "Node %llu", (unsigned long long)(i + 1u));
        if (fviz_label_set_3d_add(labels, p, text, NULL) != FVIZ_OK) return 2;
    }
    build_seconds = wall_seconds() - start;
    if (fviz_label_set_3d_count(labels) != label_count) return 3;

    start = wall_seconds();
    for (i = 0u; i < mtime_queries; ++i)
        sink ^= fviz_object_mtime((const FVizObject*)labels);
    mtime_seconds = wall_seconds() - start;

    start = wall_seconds();
    for (i = 0u; i < label_count; ++i)
    {
        if (fviz_text_measure_utf8(property, fviz_label_set_3d_text_at(labels, i), &metrics) != FVIZ_OK)
            return 4;
        sink ^= (FVizMTime)metrics.glyph_count;
    }
    measure_seconds = wall_seconds() - start;

    puts("labels,build_seconds,mtime_queries,mtime_seconds,measure_seconds,labels_per_second");
    printf("%llu,%.9f,%llu,%.9f,%.9f,%.0f\n",
        (unsigned long long)label_count,
        build_seconds,
        (unsigned long long)mtime_queries,
        mtime_seconds,
        measure_seconds,
        build_seconds > 0.0 ? (double)label_count / build_seconds : 0.0);

    if (sink == (FVizMTime)UINT64_MAX) puts("unreachable");
    fviz_release(labels);
    return 0;
}
