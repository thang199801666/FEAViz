#ifndef FVIZ_DATA_TEMPORAL_FRAME_CACHE_H
#define FVIZ_DATA_TEMPORAL_FRAME_CACHE_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataObject.h>
#include <FViz/Parallel/FVizParallel.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizTemporalFrameCache FVizTemporalFrameCache;
typedef struct FVizTemporalFrameFuture FVizTemporalFrameFuture;
typedef struct FVizTemporalPrefetchQueue FVizTemporalPrefetchQueue;
#define FVIZ_TYPE_TEMPORAL_FRAME_CACHE UINT64_C(0x7EB67E53D024195A)

typedef FVizResult (*FVizTemporalFrameLoaderFn)(FVizId frame_key, FVizCancellationToken* cancellation, void* user_data,
                                                FVizDataObject** out_frame);

typedef struct FVizTemporalFrameCacheOptions
{
    uint32_t struct_size;
    FVizSize frame_capacity;
    FVizSize byte_capacity;
} FVizTemporalFrameCacheOptions;

typedef struct FVizTemporalFrameCacheStatistics
{
    uint32_t struct_size;
    FVizSize entries;
    FVizSize bytes;
    FVizSize frame_capacity;
    FVizSize byte_capacity;
    uint64_t hits;
    uint64_t misses;
    uint64_t loads;
    uint64_t load_failures;
    uint64_t evictions;
    uint64_t oversize_skips;
} FVizTemporalFrameCacheStatistics;

FVIZ_DATA_API void fviz_temporal_frame_cache_options_initialize(FVizTemporalFrameCacheOptions* options);
FVIZ_DATA_API FVizResult fviz_temporal_frame_cache_create(FVizTemporalFrameLoaderFn loader, void* user_data,
                                                     FVizTemporalFrameCache** out_cache);
FVIZ_DATA_API FVizResult fviz_temporal_frame_cache_set_options(FVizTemporalFrameCache* cache,
                                                          const FVizTemporalFrameCacheOptions* options);
FVIZ_DATA_API void fviz_temporal_frame_cache_get_options(const FVizTemporalFrameCache* cache,
                                                    FVizTemporalFrameCacheOptions* out_options);
/* Returns a retained frame. The caller owns one reference whether the frame was
 * loaded or came from cache. Cache operations are synchronous and deterministic. */
FVIZ_DATA_API FVizResult fviz_temporal_frame_cache_get(FVizTemporalFrameCache* cache, FVizId frame_key,
                                                  FVizCancellationToken* cancellation, FVizDataObject** out_frame);
FVIZ_DATA_API FVizResult fviz_temporal_frame_cache_prefetch(FVizTemporalFrameCache* cache, const FVizId* frame_keys,
                                                       FVizSize frame_count, FVizCancellationToken* cancellation);
/* Starts a background request. The future retains the cache and owns its
 * cancellation token; destroy waits for an in-flight loader to exit. */
FVIZ_DATA_API FVizResult fviz_temporal_frame_cache_request_async(FVizTemporalFrameCache* cache, FVizId frame_key,
                                                            FVizTemporalFrameFuture** out_future);
FVIZ_DATA_API void fviz_temporal_frame_future_cancel(FVizTemporalFrameFuture* future);
FVIZ_DATA_API FVizBool fviz_temporal_frame_future_ready(const FVizTemporalFrameFuture* future);
/* Waits and returns a retained frame on success. Repeated waits are allowed. */
FVIZ_DATA_API FVizResult fviz_temporal_frame_future_wait(FVizTemporalFrameFuture* future, FVizDataObject** out_frame);
FVIZ_DATA_API void fviz_temporal_frame_future_destroy(FVizTemporalFrameFuture* future);
FVIZ_DATA_API FVizResult fviz_temporal_prefetch_queue_create(FVizTemporalFrameCache* cache, FVizSize pending_capacity,
                                                        FVizTemporalPrefetchQueue** out_queue);
/* Higher priority executes first; equal priorities preserve submission order. */
FVIZ_DATA_API FVizResult fviz_temporal_prefetch_queue_request(FVizTemporalPrefetchQueue* queue, FVizId frame_key,
                                                         int priority);
/* Cancels the active request and clears pending work. */
FVIZ_DATA_API void fviz_temporal_prefetch_queue_cancel(FVizTemporalPrefetchQueue* queue);
/* Enqueues current, then direction-biased adjacent frames. Changing non-zero
 * direction cancels stale work from the previous scrub direction. */
FVIZ_DATA_API FVizResult fviz_temporal_prefetch_queue_request_window(FVizTemporalPrefetchQueue* queue, FVizId current_key,
                                                                int direction, FVizSize ahead_count,
                                                                FVizSize behind_count);
FVIZ_DATA_API FVizResult fviz_temporal_prefetch_queue_wait_idle(FVizTemporalPrefetchQueue* queue);
FVIZ_DATA_API FVizSize fviz_temporal_prefetch_queue_pending_count(const FVizTemporalPrefetchQueue* queue);
FVIZ_DATA_API void fviz_temporal_prefetch_queue_destroy(FVizTemporalPrefetchQueue* queue);
FVIZ_DATA_API FVizResult fviz_temporal_frame_cache_invalidate(FVizTemporalFrameCache* cache, FVizId frame_key);
FVIZ_DATA_API void fviz_temporal_frame_cache_clear(FVizTemporalFrameCache* cache);
FVIZ_DATA_API void fviz_temporal_frame_cache_get_statistics(const FVizTemporalFrameCache* cache,
                                                       FVizTemporalFrameCacheStatistics* out_statistics);
FVIZ_DATA_API void fviz_temporal_frame_cache_reset_statistics(FVizTemporalFrameCache* cache);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_TEMPORAL_FRAME_CACHE_H */
