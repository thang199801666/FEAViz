#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

typedef struct LoaderState
{
    uint64_t calls;
} LoaderState;

static FVizResult load_frame(
    FVizId key,
    FVizCancellationToken* cancellation,
    void* user_data,
    FVizDataObject** out_frame)
{
    LoaderState* state = (LoaderState*)user_data;
    FVizDataSet* frame = NULL;
    if (fviz_cancellation_token_is_cancelled(cancellation) != FVIZ_FALSE)
        return FVIZ_ERROR_CANCELLED;
    ++state->calls;
    if (fviz_data_set_create(&frame) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_data_set_set_point_count(frame, (FVizSize)key + 1u) != FVIZ_OK)
    {
        fviz_release(frame);
        return fviz_last_error_code();
    }
    *out_frame = (FVizDataObject*)frame;
    return FVIZ_OK;
}

int main(void)
{
    LoaderState state = {0u};
    FVizTemporalFrameCache* cache = NULL;
    FVizTemporalFrameCacheOptions options;
    FVizTemporalFrameCacheStatistics statistics;
    FVizDataObject* frame = NULL;
    FVizCancellationToken* cancellation = NULL;
    const FVizId prefetch_keys[2] = {4u, 5u};
    FVizTemporalFrameFuture* future = NULL;
    FVizTemporalPrefetchQueue* queue = NULL;

    CHECK(fviz_temporal_frame_cache_create(load_frame, &state, &cache) == FVIZ_OK);
    fviz_temporal_frame_cache_options_initialize(&options);
    options.frame_capacity = 2u;
    CHECK(fviz_temporal_frame_cache_set_options(cache, &options) == FVIZ_OK);
    CHECK(fviz_temporal_frame_cache_get(cache, 1u, NULL, &frame) == FVIZ_OK);
    fviz_release(frame); frame = NULL;
    CHECK(fviz_temporal_frame_cache_get(cache, 2u, NULL, &frame) == FVIZ_OK);
    fviz_release(frame); frame = NULL;
    CHECK(fviz_temporal_frame_cache_get(cache, 1u, NULL, &frame) == FVIZ_OK);
    fviz_release(frame); frame = NULL;
    CHECK(state.calls == 2u);
    CHECK(fviz_temporal_frame_cache_get(cache, 3u, NULL, &frame) == FVIZ_OK);
    fviz_release(frame); frame = NULL;
    fviz_temporal_frame_cache_get_statistics(cache, &statistics);
    CHECK(statistics.entries == 2u && statistics.hits == 1u);
    CHECK(statistics.misses == 3u && statistics.evictions == 1u);
    CHECK(fviz_temporal_frame_cache_get(cache, 2u, NULL, &frame) == FVIZ_OK);
    fviz_release(frame); frame = NULL;
    CHECK(state.calls == 4u);

    CHECK(fviz_cancellation_token_create(&cancellation) == FVIZ_OK);
    fviz_cancellation_token_cancel(cancellation);
    CHECK(fviz_temporal_frame_cache_prefetch(
        cache, prefetch_keys, 2u, cancellation) == FVIZ_ERROR_CANCELLED);
    fviz_cancellation_token_reset(cancellation);
    CHECK(fviz_temporal_frame_cache_prefetch(
        cache, prefetch_keys, 2u, cancellation) == FVIZ_OK);

    options.byte_capacity = 1u;
    CHECK(fviz_temporal_frame_cache_set_options(cache, &options) == FVIZ_OK);
    CHECK(fviz_temporal_frame_cache_get(cache, 9u, NULL, &frame) == FVIZ_OK);
    fviz_release(frame); frame = NULL;
    fviz_temporal_frame_cache_get_statistics(cache, &statistics);
    CHECK(statistics.entries == 0u);
    CHECK(statistics.oversize_skips >= 1u);

    options.byte_capacity = 0u;
    options.frame_capacity = 2u;
    CHECK(fviz_temporal_frame_cache_set_options(cache, &options) == FVIZ_OK);
    CHECK(fviz_temporal_frame_cache_request_async(cache, 12u, &future) == FVIZ_OK);
    CHECK(fviz_temporal_frame_future_wait(future, &frame) == FVIZ_OK);
    CHECK(frame != NULL);
    CHECK(fviz_data_set_point_count((const FVizDataSet*)frame) == 13u);
    fviz_release(frame); frame = NULL;
    CHECK(fviz_temporal_frame_future_ready(future) == FVIZ_TRUE);
    CHECK(fviz_temporal_frame_future_wait(future, &frame) == FVIZ_OK);
    CHECK(frame != NULL);
    fviz_release(frame); frame = NULL;
    fviz_temporal_frame_future_destroy(future);
    future = NULL;

    options.frame_capacity = 4u;
    CHECK(fviz_temporal_frame_cache_set_options(cache, &options) == FVIZ_OK);
    CHECK(fviz_temporal_prefetch_queue_create(cache, 4u, &queue) == FVIZ_OK);
    CHECK(fviz_temporal_prefetch_queue_request_window(
        queue, 20u, 1, 2u, 1u) == FVIZ_OK);
    CHECK(fviz_temporal_prefetch_queue_wait_idle(queue) == FVIZ_OK);
    CHECK(fviz_temporal_prefetch_queue_pending_count(queue) == 0u);
    {
        const uint64_t calls_before = state.calls;
        CHECK(fviz_temporal_frame_cache_get(cache, 20u, NULL, &frame) == FVIZ_OK);
        CHECK(state.calls == calls_before);
        fviz_release(frame); frame = NULL;
    }
    CHECK(fviz_temporal_prefetch_queue_request_window(
        queue, 30u, -1, 2u, 1u) == FVIZ_OK);
    fviz_temporal_prefetch_queue_cancel(queue);
    CHECK(fviz_temporal_prefetch_queue_wait_idle(queue) == FVIZ_OK);
    fviz_temporal_prefetch_queue_destroy(queue);
    queue = NULL;

    fviz_cancellation_token_destroy(cancellation);
    fviz_temporal_frame_future_destroy(future);
    fviz_temporal_prefetch_queue_destroy(queue);
    fviz_release(cache);
    return 0;
}
