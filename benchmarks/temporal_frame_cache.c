#include <stdio.h>
#include <time.h>

#include <FViz/FViz.h>

typedef struct LoaderState { uint64_t loads; } LoaderState;

static double wall_seconds(void)
{
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}

static FVizResult load_frame(
    FVizId key,
    FVizCancellationToken* cancellation,
    void* user_data,
    FVizDataObject** out_frame)
{
    LoaderState* state = (LoaderState*)user_data;
    FVizDataSet* frame = NULL;
    FVIZ_UNUSED(cancellation);
    ++state->loads;
    if (fviz_data_set_create(&frame) != FVIZ_OK ||
        fviz_data_set_set_point_count(frame, (FVizSize)(key % 1024u)) != FVIZ_OK)
    {
        fviz_release(frame);
        return fviz_last_error_code();
    }
    *out_frame = (FVizDataObject*)frame;
    return FVIZ_OK;
}

int main(void)
{
    const FVizSize request_count = 1000000u;
    LoaderState state = {0u};
    FVizTemporalFrameCache* cache = NULL;
    FVizTemporalFrameCacheOptions options;
    FVizTemporalFrameCacheStatistics statistics;
    FVizSize request;
    double started;
    double elapsed;
    if (fviz_temporal_frame_cache_create(load_frame, &state, &cache) != FVIZ_OK) return 1;
    fviz_temporal_frame_cache_options_initialize(&options);
    options.frame_capacity = 64u;
    if (fviz_temporal_frame_cache_set_options(cache, &options) != FVIZ_OK) return 2;
    started = wall_seconds();
    for (request = 0u; request < request_count; ++request)
    {
        FVizDataObject* frame = NULL;
        const FVizId key = (FVizId)(request % 32u);
        if (fviz_temporal_frame_cache_get(cache, key, NULL, &frame) != FVIZ_OK) return 3;
        fviz_release(frame);
    }
    elapsed = wall_seconds() - started;
    fviz_temporal_frame_cache_get_statistics(cache, &statistics);
    puts("requests,loads,hits,seconds,ns_per_request,resident_bytes");
    printf("%llu,%llu,%llu,%.9f,%.3f,%llu\n",
        (unsigned long long)request_count,
        (unsigned long long)state.loads,
        (unsigned long long)statistics.hits,
        elapsed,
        elapsed * 1.0e9 / (double)request_count,
        (unsigned long long)statistics.bytes);
    fviz_release(cache);
    return state.loads == 32u && statistics.hits == request_count - 32u ? 0 : 4;
}
