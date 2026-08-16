#include <limits.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizTemporalFrameCache.h>
#include <FViz/Parallel/FVizExecutor.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizObjectPrivate.h>

typedef struct FVizTemporalFrameEntry
{
    FVizId key;
    FVizDataObject* frame;
    FVizSize bytes;
    uint64_t last_use;
} FVizTemporalFrameEntry;

struct FVizTemporalFrameCache
{
    FVizObject base;
    FVizTemporalFrameLoaderFn loader;
    void* user_data;
    FVizArray* entries;
    FVizTemporalFrameCacheOptions options;
    FVizTemporalFrameCacheStatistics statistics;
    uint64_t use_serial;
    FVizBool mutex_initialized;
    FVizExecutor* executor;
#if defined(_WIN32)
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
};

struct FVizTemporalFrameFuture
{
    FVizTemporalFrameCache* cache;
    FVizFuture* task;
    FVizDataObject* frame;
};

typedef struct FVizTemporalAsyncTask
{
    FVizTemporalFrameCache* cache;
    FVizId frame_key;
} FVizTemporalAsyncTask;

typedef struct FVizTemporalPrefetchEntry
{
    FVizId key;
    int priority;
    uint64_t serial;
} FVizTemporalPrefetchEntry;

struct FVizTemporalPrefetchQueue
{
    FVizTemporalFrameCache* cache;
    FVizTemporalPrefetchEntry* entries;
    FVizSize count;
    FVizSize capacity;
    uint64_t serial;
    int direction;
    FVizBool stopping;
    FVizBool active;
    /* Own cancellation token. cancel()/direction reversal cancel this token
     * instead of the drain future: a cancelled future would be short-circuited
     * by the executor without ever running fviz_temporal_prefetch_run, leaving
     * queue->active stuck at TRUE and blocking wait_idle/destroy forever. The
     * drain task itself always runs and observes the token, so the active flag
     * is reliably cleared. */
    FVizCancellationToken* cancellation;
    /* The prefetch queue runs on the cache's shared executor instead of a
     * dedicated thread. A single in-flight future represents the current drain;
     * it is re-submitted for the next best entry after each load. */
    FVizFuture* active_task;
#if defined(_WIN32)
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE condition;
#else
    pthread_mutex_t mutex;
    pthread_cond_t condition;
#endif
};

static void fviz_temporal_cache_lock(FVizTemporalFrameCache* cache)
{
#if defined(_WIN32)
    EnterCriticalSection(&cache->mutex);
#else
    (void)pthread_mutex_lock(&cache->mutex);
#endif
}

static void fviz_temporal_cache_unlock(FVizTemporalFrameCache* cache)
{
#if defined(_WIN32)
    LeaveCriticalSection(&cache->mutex);
#else
    (void)pthread_mutex_unlock(&cache->mutex);
#endif
}

static void fviz_temporal_frame_cache_destroy(FVizObject* object);
static void fviz_temporal_frame_cache_clear_unlocked(FVizTemporalFrameCache* cache);

static const FVizObjectClass g_fviz_temporal_frame_cache_class = {FVIZ_TYPE_TEMPORAL_FRAME_CACHE,
                                                                  "FVizTemporalFrameCache", &g_fviz_object_class,
                                                                  fviz_temporal_frame_cache_destroy, NULL};

static void fviz_temporal_frame_entry_release(FVizTemporalFrameEntry* entry)
{
    if (entry == NULL) return;
    fviz_release(entry->frame);
    entry->frame = NULL;
}

static void fviz_temporal_frame_cache_destroy(FVizObject* object)
{
    FVizTemporalFrameCache* cache = (FVizTemporalFrameCache*)object;
    fviz_executor_destroy(cache->executor);
    cache->executor = NULL;
    fviz_temporal_frame_cache_clear_unlocked(cache);
    fviz_release(cache->entries);
    if (cache->mutex_initialized != FVIZ_FALSE)
    {
#if defined(_WIN32)
        DeleteCriticalSection(&cache->mutex);
#else
        (void)pthread_mutex_destroy(&cache->mutex);
#endif
    }
}

void fviz_temporal_frame_cache_options_initialize(FVizTemporalFrameCacheOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->frame_capacity = 3u;
}

FVizResult fviz_temporal_frame_cache_create(FVizTemporalFrameLoaderFn loader, void* user_data,
                                            FVizTemporalFrameCache** out_cache)
{
    FVizTemporalFrameCache* cache;
    if (loader == NULL || out_cache == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_cache = NULL;
    cache = (FVizTemporalFrameCache*)fviz_internal_object_allocate(sizeof(*cache), &g_fviz_temporal_frame_cache_class,
                                                                   NULL);
    if (cache == NULL) return fviz_last_error_code();
    cache->loader = loader;
    cache->user_data = user_data;
    fviz_temporal_frame_cache_options_initialize(&cache->options);
    cache->statistics.struct_size = (uint32_t)sizeof(cache->statistics);
    {
        FVizExecutorOptions executor_options;
        fviz_executor_options_initialize(&executor_options);
        /* Cache mutation/load publication is serialized by cache->mutex, so a
         * single shared worker eliminates per-request threads without idle contention. */
        executor_options.thread_count = 1u;
        if (fviz_executor_create(&executor_options, &cache->executor) != FVIZ_OK)
        {
            fviz_release(cache);
            return fviz_last_error_code();
        }
    }
#if defined(_WIN32)
    InitializeCriticalSection(&cache->mutex);
    cache->mutex_initialized = FVIZ_TRUE;
#else
    if (pthread_mutex_init(&cache->mutex, NULL) != 0)
    {
        fviz_release(cache);
        return FVIZ_ERROR_INTERNAL;
    }
    cache->mutex_initialized = FVIZ_TRUE;
#endif
    if (fviz_array_create(sizeof(FVizTemporalFrameEntry), &cache->entries) != FVIZ_OK)
    {
        fviz_release(cache);
        return fviz_last_error_code();
    }
    *out_cache = cache;
    return FVIZ_OK;
}

static FVizSize fviz_temporal_frame_cache_find(const FVizTemporalFrameCache* cache, FVizId key)
{
    FVizSize index;
    for (index = 0u; index < fviz_array_count(cache->entries); ++index)
    {
        const FVizTemporalFrameEntry* entry = (const FVizTemporalFrameEntry*)fviz_array_const_at(cache->entries, index);
        if (entry->key == key) return index;
    }
    return SIZE_MAX;
}

static void fviz_temporal_frame_cache_remove_at(FVizTemporalFrameCache* cache, FVizSize index, FVizBool eviction)
{
    FVizTemporalFrameEntry* entries = (FVizTemporalFrameEntry*)fviz_array_data(cache->entries);
    const FVizSize count = fviz_array_count(cache->entries);
    if (index >= count) return;
    if (cache->statistics.bytes >= entries[index].bytes) cache->statistics.bytes -= entries[index].bytes;
    else
        cache->statistics.bytes = 0u;
    fviz_temporal_frame_entry_release(&entries[index]);
    if (index + 1u < count)
        (void)memmove(&entries[index], &entries[index + 1u], (size_t)(count - index - 1u) * sizeof(*entries));
    (void)fviz_array_resize(cache->entries, count - 1u);
    if (eviction != FVIZ_FALSE && cache->statistics.evictions != UINT64_MAX) ++cache->statistics.evictions;
    cache->statistics.entries = fviz_array_count(cache->entries);
}

static void fviz_temporal_frame_cache_enforce_budget(FVizTemporalFrameCache* cache)
{
    while (fviz_array_count(cache->entries) > cache->options.frame_capacity ||
           (cache->options.byte_capacity != 0u && cache->statistics.bytes > cache->options.byte_capacity))
    {
        FVizSize index;
        FVizSize oldest = 0u;
        uint64_t oldest_use = UINT64_MAX;
        for (index = 0u; index < fviz_array_count(cache->entries); ++index)
        {
            const FVizTemporalFrameEntry* entry =
                (const FVizTemporalFrameEntry*)fviz_array_const_at(cache->entries, index);
            if (entry->last_use < oldest_use)
            {
                oldest = index;
                oldest_use = entry->last_use;
            }
        }
        if (fviz_array_count(cache->entries) == 0u) break;
        fviz_temporal_frame_cache_remove_at(cache, oldest, FVIZ_TRUE);
    }
}

FVizResult fviz_temporal_frame_cache_set_options(FVizTemporalFrameCache* cache,
                                                 const FVizTemporalFrameCacheOptions* options)
{
    if (cache == NULL || options == NULL || (options->struct_size != 0u && options->struct_size < sizeof(*options)))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_temporal_cache_lock(cache);
    cache->options = *options;
    cache->options.struct_size = (uint32_t)sizeof(cache->options);
    fviz_temporal_frame_cache_enforce_budget(cache);
    cache->statistics.frame_capacity = cache->options.frame_capacity;
    cache->statistics.byte_capacity = cache->options.byte_capacity;
    fviz_object_modified((FVizObject*)cache);
    fviz_temporal_cache_unlock(cache);
    return FVIZ_OK;
}

void fviz_temporal_frame_cache_get_options(const FVizTemporalFrameCache* cache,
                                           FVizTemporalFrameCacheOptions* out_options)
{
    if (out_options == NULL) return;
    fviz_temporal_frame_cache_options_initialize(out_options);
    if (cache != NULL)
    {
        fviz_temporal_cache_lock((FVizTemporalFrameCache*)cache);
        *out_options = cache->options;
        fviz_temporal_cache_unlock((FVizTemporalFrameCache*)cache);
    }
}

static FVizResult fviz_temporal_frame_cache_get_unlocked(FVizTemporalFrameCache* cache, FVizId frame_key,
                                                         FVizCancellationToken* cancellation,
                                                         FVizDataObject** out_frame)
{
    FVizSize index;
    FVizDataObject* frame = NULL;
    FVizSize bytes;
    FVizTemporalFrameEntry entry;
    FVizResult result;
    if (cache == NULL || out_frame == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_frame = NULL;
    if (cancellation != NULL && fviz_cancellation_token_is_cancelled(cancellation) != FVIZ_FALSE)
        return FVIZ_ERROR_CANCELLED;
    index = fviz_temporal_frame_cache_find(cache, frame_key);
    if (index != SIZE_MAX)
    {
        FVizTemporalFrameEntry* cached = (FVizTemporalFrameEntry*)fviz_array_at(cache->entries, index);
        cached->last_use = ++cache->use_serial;
        if (cache->statistics.hits != UINT64_MAX) ++cache->statistics.hits;
        *out_frame = (FVizDataObject*)fviz_retain(cached->frame);
        return *out_frame != NULL ? FVIZ_OK : fviz_last_error_code();
    }
    if (cache->statistics.misses != UINT64_MAX) ++cache->statistics.misses;
    result = cache->loader(frame_key, cancellation, cache->user_data, &frame);
    if (result != FVIZ_OK || frame == NULL || fviz_data_object_is_data_object(frame) == FVIZ_FALSE)
    {
        fviz_release(frame);
        if (cache->statistics.load_failures != UINT64_MAX) ++cache->statistics.load_failures;
        return result != FVIZ_OK ? result : FVIZ_ERROR_INVALID_STATE;
    }
    if (cache->statistics.loads != UINT64_MAX) ++cache->statistics.loads;
    *out_frame = frame;
    bytes = fviz_data_object_memory_size(frame);
    if (cache->options.frame_capacity == 0u ||
        (cache->options.byte_capacity != 0u && bytes > cache->options.byte_capacity))
    {
        if (cache->statistics.oversize_skips != UINT64_MAX) ++cache->statistics.oversize_skips;
        return FVIZ_OK;
    }
    entry.key = frame_key;
    entry.frame = (FVizDataObject*)fviz_retain(frame);
    entry.bytes = bytes;
    entry.last_use = ++cache->use_serial;
    if (entry.frame == NULL) return fviz_last_error_code();
    if (bytes > SIZE_MAX - cache->statistics.bytes)
    {
        fviz_release(entry.frame);
        return FVIZ_ERROR_OVERFLOW;
    }
    if (fviz_array_push(cache->entries, &entry) != FVIZ_OK)
    {
        fviz_release(entry.frame);
        return fviz_last_error_code();
    }
    cache->statistics.bytes += bytes;
    cache->statistics.entries = fviz_array_count(cache->entries);
    fviz_temporal_frame_cache_enforce_budget(cache);
    return FVIZ_OK;
}

FVizResult fviz_temporal_frame_cache_get(FVizTemporalFrameCache* cache, FVizId frame_key,
                                         FVizCancellationToken* cancellation, FVizDataObject** out_frame)
{
    FVizResult result;
    if (cache == NULL || out_frame == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_temporal_cache_lock(cache);
    result = fviz_temporal_frame_cache_get_unlocked(cache, frame_key, cancellation, out_frame);
    fviz_temporal_cache_unlock(cache);
    return result;
}

static FVizResult fviz_temporal_frame_future_task(FVizCancellationToken* cancellation, void* user_data,
                                                  void** out_value)
{
    FVizTemporalAsyncTask* task = (FVizTemporalAsyncTask*)user_data;
    return fviz_temporal_frame_cache_get(task->cache, task->frame_key, cancellation, (FVizDataObject**)out_value);
}

static void fviz_temporal_async_task_destroy(void* user_data)
{
    FVizTemporalAsyncTask* task = (FVizTemporalAsyncTask*)user_data;
    if (task == NULL) return;
    fviz_release(task->cache);
    fviz_free(task);
}

FVizResult fviz_temporal_frame_cache_request_async(FVizTemporalFrameCache* cache, FVizId frame_key,
                                                   FVizTemporalFrameFuture** out_future)
{
    FVizTemporalFrameFuture* future;
    FVizTemporalAsyncTask* task;
    if (cache == NULL || out_future == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_future = NULL;
    future = (FVizTemporalFrameFuture*)fviz_alloc(sizeof(*future));
    task = (FVizTemporalAsyncTask*)fviz_alloc(sizeof(*task));
    if (future == NULL || task == NULL)
    {
        fviz_free(task);
        fviz_free(future);
        return fviz_last_error_code();
    }
    (void)memset(future, 0, sizeof(*future));
    future->cache = (FVizTemporalFrameCache*)fviz_retain(cache);
    task->cache = (FVizTemporalFrameCache*)fviz_retain(cache);
    task->frame_key = frame_key;
    if (future->cache == NULL || task->cache == NULL ||
        fviz_executor_submit(cache->executor, 0, fviz_temporal_frame_future_task, task,
                             fviz_temporal_async_task_destroy, fviz_release, &future->task) != FVIZ_OK)
    {
        if (future->task == NULL) fviz_temporal_async_task_destroy(task);
        fviz_release(future->cache);
        fviz_free(future);
        return fviz_last_error_code();
    }
    *out_future = future;
    return FVIZ_OK;
}

void fviz_temporal_frame_future_cancel(FVizTemporalFrameFuture* future)
{
    if (future != NULL) fviz_future_cancel(future->task);
}

FVizBool fviz_temporal_frame_future_ready(const FVizTemporalFrameFuture* future)
{
    return future != NULL ? fviz_future_ready(future->task) : FVIZ_FALSE;
}

FVizResult fviz_temporal_frame_future_wait(FVizTemporalFrameFuture* future, FVizDataObject** out_frame)
{
    if (future == NULL || out_frame == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_frame = NULL;
    {
        const FVizResult result = fviz_future_wait(future->task);
        if (result != FVIZ_OK) return result;
    }
    if (future->frame == NULL)
    {
        void* value = NULL;
        const FVizResult take_result = fviz_future_take_value(future->task, &value);
        if (take_result == FVIZ_OK) future->frame = (FVizDataObject*)value;
        else if (take_result != FVIZ_ERROR_NOT_FOUND)
            return take_result;
    }
    if (future->frame != NULL)
    {
        *out_frame = (FVizDataObject*)fviz_retain(future->frame);
        if (*out_frame == NULL) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

void fviz_temporal_frame_future_destroy(FVizTemporalFrameFuture* future)
{
    if (future == NULL) return;
    fviz_temporal_frame_future_cancel(future);
    fviz_future_destroy(future->task);
    fviz_release(future->frame);
    fviz_release(future->cache);
    fviz_free(future);
}

static void fviz_temporal_prefetch_lock(FVizTemporalPrefetchQueue* queue)
{
#if defined(_WIN32)
    EnterCriticalSection(&queue->mutex);
#else
    (void)pthread_mutex_lock(&queue->mutex);
#endif
}

static void fviz_temporal_prefetch_unlock(FVizTemporalPrefetchQueue* queue)
{
#if defined(_WIN32)
    LeaveCriticalSection(&queue->mutex);
#else
    (void)pthread_mutex_unlock(&queue->mutex);
#endif
}

static void fviz_temporal_prefetch_wake(FVizTemporalPrefetchQueue* queue)
{
#if defined(_WIN32)
    WakeAllConditionVariable(&queue->condition);
#else
    (void)pthread_cond_broadcast(&queue->condition);
#endif
}

/* Runs on the cache's shared executor. Picks the highest-priority pending entry,
 * loads it into the cache, then re-submits itself if more work remains. The
 * cancellation token comes from the in-flight future, so
 * fviz_temporal_prefetch_queue_cancel cancels the running load. Each task owns
 * the previous task's future and destroys it in its destroy callback, by which
 * point that future is guaranteed complete (execution is serial on the cache's
 * single-threaded executor). */
typedef struct FVizTemporalPrefetchTask
{
    FVizTemporalPrefetchQueue* queue;
    FVizFuture* previous;
} FVizTemporalPrefetchTask;

static void fviz_temporal_prefetch_task_destroy(void* user_data);

static FVizResult fviz_temporal_prefetch_run(FVizCancellationToken* cancellation, void* user_data, void** out_value)
{
    FVizTemporalPrefetchTask* task = (FVizTemporalPrefetchTask*)user_data;
    FVizTemporalPrefetchQueue* queue = task->queue;
    FVizTemporalPrefetchEntry work;
    FVizSize best;
    FVizSize index;
    FVizDataObject* frame = NULL;
    FVizBool cancelled;
    FVIZ_UNUSED(cancellation);
    FVIZ_UNUSED(out_value);
    fviz_temporal_prefetch_lock(queue);
    /* The queue's own token is consulted instead of the future's: a cancelled
     * future is completed by the executor without running this function, which
     * would leave queue->active stuck at TRUE. A cancelled token means the
     * current drain is stale (cancel() or a direction reversal): drop it, but
     * if fresh work has already been queued by the reversal, reset the token
     * and continue as the new generation. */
    cancelled = queue->cancellation != NULL && fviz_cancellation_token_is_cancelled(queue->cancellation) != FVIZ_FALSE;
    if (cancelled != FVIZ_FALSE && queue->stopping == FVIZ_FALSE && queue->count > 0u)
    {
        if (queue->cancellation != NULL) fviz_cancellation_token_reset(queue->cancellation);
        cancelled = FVIZ_FALSE;
    }
    if (queue->stopping != FVIZ_FALSE || queue->count == 0u || cancelled != FVIZ_FALSE)
    {
        queue->active = FVIZ_FALSE;
        fviz_temporal_prefetch_wake(queue);
        fviz_temporal_prefetch_unlock(queue);
        return FVIZ_OK;
    }
    best = 0u;
    for (index = 1u; index < queue->count; ++index)
        if (queue->entries[index].priority > queue->entries[best].priority ||
            (queue->entries[index].priority == queue->entries[best].priority &&
             queue->entries[index].serial < queue->entries[best].serial))
            best = index;
    work = queue->entries[best];
    if (best + 1u < queue->count)
        (void)memmove(&queue->entries[best], &queue->entries[best + 1u],
                      (size_t)(queue->count - best - 1u) * sizeof(*queue->entries));
    --queue->count;
    fviz_temporal_prefetch_unlock(queue);
    (void)fviz_temporal_frame_cache_get(queue->cache, work.key, queue->cancellation, &frame);
    fviz_release(frame);

    /* Re-submit for the next best entry if work remains. The drain always
     * submits at the executor's default priority so prefetch interleaves fairly
     * with asynchronous cache requests on the shared single-thread worker;
     * entry priorities are used only for internal ordering. */
    fviz_temporal_prefetch_lock(queue);
    if (queue->stopping == FVIZ_FALSE && queue->count > 0u)
    {
        FVizTemporalPrefetchTask* next = (FVizTemporalPrefetchTask*)fviz_alloc(sizeof(*next));
        FVizFuture* next_future = NULL;
        FVizFuture* current = NULL;
        if (next == NULL)
        {
            queue->active = FVIZ_FALSE;
            fviz_temporal_prefetch_wake(queue);
            fviz_temporal_prefetch_unlock(queue);
            return FVIZ_OK;
        }
        current = queue->active_task;
        /* The running task's own future is still executing, so it is handed to
         * the successor for destruction; serial single-thread execution
         * guarantees it is complete before the successor's destroy runs. */
        next->queue = queue;
        next->previous = current;
        queue->active_task = NULL;
        fviz_temporal_prefetch_unlock(queue);
        if (fviz_executor_submit(queue->cache->executor, 0, fviz_temporal_prefetch_run, next,
                                 fviz_temporal_prefetch_task_destroy, NULL, &next_future) != FVIZ_OK)
        {
            /* Restore ownership of the still-executing current future; it is
             * released by wait_idle/destroy. Do not destroy it here. */
            next->previous = NULL;
            fviz_temporal_prefetch_task_destroy(next);
            fviz_temporal_prefetch_lock(queue);
            queue->active_task = current;
            queue->active = FVIZ_FALSE;
            fviz_temporal_prefetch_wake(queue);
            fviz_temporal_prefetch_unlock(queue);
            return FVIZ_OK;
        }
        fviz_temporal_prefetch_lock(queue);
        queue->active_task = next_future;
        fviz_temporal_prefetch_unlock(queue);
        return FVIZ_OK;
    }
    /* No more work: the current future stays owned by queue->active_task and is
     * destroyed by wait_idle/destroy or handed to a later kick. Only the active
     * flag is cleared so waiters can observe the drain finished. */
    queue->active = FVIZ_FALSE;
    fviz_temporal_prefetch_wake(queue);
    fviz_temporal_prefetch_unlock(queue);
    return FVIZ_OK;
}

static void fviz_temporal_prefetch_task_destroy(void* user_data)
{
    FVizTemporalPrefetchTask* task = (FVizTemporalPrefetchTask*)user_data;
    if (task == NULL) return;
    if (task->previous != NULL) fviz_future_destroy(task->previous);
    fviz_free(task);
}

FVizResult fviz_temporal_prefetch_queue_create(FVizTemporalFrameCache* cache, FVizSize pending_capacity,
                                               FVizTemporalPrefetchQueue** out_queue)
{
    FVizTemporalPrefetchQueue* queue;
    FVizSize bytes;
    if (cache == NULL || out_queue == NULL || pending_capacity == 0u) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_queue = NULL;
    queue = (FVizTemporalPrefetchQueue*)fviz_alloc(sizeof(*queue));
    if (queue == NULL) return fviz_last_error_code();
    (void)memset(queue, 0, sizeof(*queue));
    if (fviz_size_multiply(pending_capacity, sizeof(*queue->entries), &bytes) != FVIZ_OK)
    {
        fviz_free(queue);
        return fviz_last_error_code();
    }
    queue->entries = (FVizTemporalPrefetchEntry*)fviz_alloc(bytes);
    queue->cache = (FVizTemporalFrameCache*)fviz_retain(cache);
    queue->capacity = pending_capacity;
    if (queue->entries == NULL || queue->cache == NULL ||
        fviz_cancellation_token_create(&queue->cancellation) != FVIZ_OK)
    {
        fviz_free(queue->entries);
        fviz_release(queue->cache);
        fviz_cancellation_token_destroy(queue->cancellation);
        fviz_free(queue);
        return fviz_last_error_code();
    }
#if defined(_WIN32)
    InitializeCriticalSection(&queue->mutex);
    InitializeConditionVariable(&queue->condition);
#else
    if (pthread_mutex_init(&queue->mutex, NULL) != 0 || pthread_cond_init(&queue->condition, NULL) != 0)
    {
        fviz_cancellation_token_destroy(queue->cancellation);
        fviz_free(queue->entries);
        fviz_release(queue->cache);
        fviz_free(queue);
        return FVIZ_ERROR_INTERNAL;
    }
#endif
    *out_queue = queue;
    return FVIZ_OK;
}

/* Starts a drain task on the cache's shared executor when idle. Owns the queue
 * lock. The previous drain's final future (already complete) is handed to the
 * new task for deferred destruction. */
static FVizResult fviz_temporal_prefetch_kick(FVizTemporalPrefetchQueue* queue)
{
    FVizTemporalPrefetchTask* task;
    FVizFuture* next_future = NULL;
    FVizResult result;
    fviz_temporal_prefetch_lock(queue);
    if (queue->active != FVIZ_FALSE || queue->stopping != FVIZ_FALSE || queue->count == 0u)
    {
        fviz_temporal_prefetch_unlock(queue);
        return FVIZ_OK;
    }
    task = (FVizTemporalPrefetchTask*)fviz_alloc(sizeof(*task));
    if (task == NULL)
    {
        fviz_temporal_prefetch_unlock(queue);
        return fviz_last_error_code();
    }
    task->queue = queue;
    task->previous = queue->active_task;
    queue->active_task = NULL;
    queue->active = FVIZ_TRUE;
    /* A fresh drain starts from a clean token so it can actually load frames;
     * a previously cancelled token would make the first run exit immediately. */
    if (queue->cancellation != NULL) fviz_cancellation_token_reset(queue->cancellation);
    fviz_temporal_prefetch_unlock(queue);
    result = fviz_executor_submit(queue->cache->executor, 0, fviz_temporal_prefetch_run, task,
                                  fviz_temporal_prefetch_task_destroy, NULL, &next_future);
    if (result != FVIZ_OK)
    {
        fviz_temporal_prefetch_task_destroy(task);
        fviz_temporal_prefetch_lock(queue);
        queue->active = FVIZ_FALSE;
        fviz_temporal_prefetch_wake(queue);
        fviz_temporal_prefetch_unlock(queue);
        return result;
    }
    fviz_temporal_prefetch_lock(queue);
    queue->active_task = next_future;
    fviz_temporal_prefetch_wake(queue);
    fviz_temporal_prefetch_unlock(queue);
    return FVIZ_OK;
}

FVizResult fviz_temporal_prefetch_queue_request(FVizTemporalPrefetchQueue* queue, FVizId frame_key, int priority)
{
    FVizSize index;
    if (queue == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_temporal_prefetch_lock(queue);
    for (index = 0u; index < queue->count; ++index)
        if (queue->entries[index].key == frame_key)
        {
            if (priority > queue->entries[index].priority) queue->entries[index].priority = priority;
            fviz_temporal_prefetch_unlock(queue);
            return FVIZ_OK;
        }
    if (queue->count == queue->capacity)
    {
        FVizSize worst = 0u;
        for (index = 1u; index < queue->count; ++index)
            if (queue->entries[index].priority < queue->entries[worst].priority ||
                (queue->entries[index].priority == queue->entries[worst].priority &&
                 queue->entries[index].serial > queue->entries[worst].serial))
                worst = index;
        if (priority <= queue->entries[worst].priority)
        {
            fviz_temporal_prefetch_unlock(queue);
            return FVIZ_ERROR_BUSY;
        }
        queue->entries[worst].key = frame_key;
        queue->entries[worst].priority = priority;
        queue->entries[worst].serial = ++queue->serial;
    }
    else
    {
        queue->entries[queue->count].key = frame_key;
        queue->entries[queue->count].priority = priority;
        queue->entries[queue->count].serial = ++queue->serial;
        ++queue->count;
    }
    fviz_temporal_prefetch_unlock(queue);
    return fviz_temporal_prefetch_kick(queue);
}

void fviz_temporal_prefetch_queue_cancel(FVizTemporalPrefetchQueue* queue)
{
    if (queue == NULL) return;
    fviz_temporal_prefetch_lock(queue);
    queue->count = 0u;
    /* Abort the in-flight load through the queue's own token rather than the
     * drain future's token. A cancelled future is short-circuited by the
     * executor without running fviz_temporal_prefetch_run, so queue->active
     * would stay TRUE and wait_idle/destroy would block forever. The running
     * drain task observes the cancelled token and clears active itself. */
    if (queue->cancellation != NULL) fviz_cancellation_token_cancel(queue->cancellation);
    fviz_temporal_prefetch_wake(queue);
    fviz_temporal_prefetch_unlock(queue);
}

FVizResult fviz_temporal_prefetch_queue_request_window(FVizTemporalPrefetchQueue* queue, FVizId current_key,
                                                       int direction, FVizSize ahead_count, FVizSize behind_count)
{
    FVizSize distance;
    FVizResult result;
    FVizBool reversed = FVIZ_FALSE;
    const int normalized = direction < 0 ? -1 : (direction > 0 ? 1 : 0);
    if (queue == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_temporal_prefetch_lock(queue);
    reversed = normalized != 0 && queue->direction != 0 && normalized != queue->direction ? FVIZ_TRUE : FVIZ_FALSE;
    if (reversed != FVIZ_FALSE)
    {
        queue->count = 0u;
        /* Same rule as cancel(): cancel the queue's token, not the drain future,
         * so the running drain task can observe the change and clear active. */
        if (queue->cancellation != NULL) fviz_cancellation_token_cancel(queue->cancellation);
    }
    queue->direction = normalized;
    fviz_temporal_prefetch_wake(queue);
    fviz_temporal_prefetch_unlock(queue);
    result = fviz_temporal_prefetch_queue_request(queue, current_key, INT_MAX);
    if (result != FVIZ_OK && result != FVIZ_ERROR_BUSY) return result;
    for (distance = 1u; distance <= ahead_count; ++distance)
    {
        FVizId key;
        if ((normalized >= 0 && current_key > UINT64_MAX - distance) || (normalized < 0 && current_key < distance))
            break;
        key = normalized < 0 ? current_key - distance : current_key + distance;
        (void)fviz_temporal_prefetch_queue_request(queue, key, 1000000 - (int)distance);
    }
    for (distance = 1u; distance <= behind_count; ++distance)
    {
        FVizId key;
        if ((normalized >= 0 && current_key < distance) || (normalized < 0 && current_key > UINT64_MAX - distance))
            break;
        key = normalized < 0 ? current_key + distance : current_key - distance;
        (void)fviz_temporal_prefetch_queue_request(queue, key, 500000 - (int)distance);
    }
    return FVIZ_OK;
}

FVizResult fviz_temporal_prefetch_queue_wait_idle(FVizTemporalPrefetchQueue* queue)
{
    FVizFuture* retained;
    if (queue == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_temporal_prefetch_lock(queue);
    while (queue->count != 0u || queue->active != FVIZ_FALSE)
    {
#if defined(_WIN32)
        (void)SleepConditionVariableCS(&queue->condition, &queue->mutex, INFINITE);
#else
        (void)pthread_cond_wait(&queue->condition, &queue->mutex);
#endif
    }
    /* The drain's final future is completed by the worker after the task
     * function returns; destroy it once idle so it does not leak. A concurrent
     * request() re-kicks a fresh drain, so this only ever releases a completed
     * future. */
    retained = queue->active_task;
    queue->active_task = NULL;
    fviz_temporal_prefetch_unlock(queue);
    if (retained != NULL) fviz_future_destroy(retained);
    return FVIZ_OK;
}

FVizSize fviz_temporal_prefetch_queue_pending_count(const FVizTemporalPrefetchQueue* queue)
{
    FVizSize count;
    if (queue == NULL) return 0u;
    fviz_temporal_prefetch_lock((FVizTemporalPrefetchQueue*)queue);
    count = queue->count;
    fviz_temporal_prefetch_unlock((FVizTemporalPrefetchQueue*)queue);
    return count;
}

void fviz_temporal_prefetch_queue_destroy(FVizTemporalPrefetchQueue* queue)
{
    FVizFuture* active_task;
    if (queue == NULL) return;
    fviz_temporal_prefetch_lock(queue);
    queue->stopping = FVIZ_TRUE;
    queue->count = 0u;
    /* Abort any in-flight load through the queue's own token, not the drain
     * future's: cancelling the future would short-circuit the task before it
     * runs, leaving queue->active TRUE and this wait below forever. */
    if (queue->cancellation != NULL) fviz_cancellation_token_cancel(queue->cancellation);
    fviz_temporal_prefetch_wake(queue);
    /* Wait for the executor drain task to observe stopping and finish. The
     * active_task future is completed by the worker right after the task
     * function returns, so it is safe to destroy once active clears. */
    while (queue->active != FVIZ_FALSE)
    {
#if defined(_WIN32)
        (void)SleepConditionVariableCS(&queue->condition, &queue->mutex, INFINITE);
#else
        (void)pthread_cond_wait(&queue->condition, &queue->mutex);
#endif
    }
    active_task = queue->active_task;
    queue->active_task = NULL;
    fviz_temporal_prefetch_unlock(queue);
    if (active_task != NULL) fviz_future_destroy(active_task);
#if defined(_WIN32)
    DeleteCriticalSection(&queue->mutex);
#else
    (void)pthread_cond_destroy(&queue->condition);
    (void)pthread_mutex_destroy(&queue->mutex);
#endif
    fviz_cancellation_token_destroy(queue->cancellation);
    fviz_free(queue->entries);
    fviz_release(queue->cache);
    fviz_free(queue);
}

FVizResult fviz_temporal_frame_cache_prefetch(FVizTemporalFrameCache* cache, const FVizId* frame_keys,
                                              FVizSize frame_count, FVizCancellationToken* cancellation)
{
    FVizSize index;
    if (cache == NULL || (frame_count != 0u && frame_keys == NULL)) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (index = 0u; index < frame_count; ++index)
    {
        FVizDataObject* frame = NULL;
        FVizResult result;
        if (cancellation != NULL && fviz_cancellation_token_is_cancelled(cancellation) != FVIZ_FALSE)
            return FVIZ_ERROR_CANCELLED;
        result = fviz_temporal_frame_cache_get(cache, frame_keys[index], cancellation, &frame);
        fviz_release(frame);
        if (result != FVIZ_OK) return result;
    }
    return FVIZ_OK;
}

FVizResult fviz_temporal_frame_cache_invalidate(FVizTemporalFrameCache* cache, FVizId frame_key)
{
    FVizSize index;
    if (cache == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_temporal_cache_lock(cache);
    index = fviz_temporal_frame_cache_find(cache, frame_key);
    if (index == SIZE_MAX)
    {
        fviz_temporal_cache_unlock(cache);
        return FVIZ_ERROR_NOT_FOUND;
    }
    fviz_temporal_frame_cache_remove_at(cache, index, FVIZ_FALSE);
    fviz_object_modified((FVizObject*)cache);
    fviz_temporal_cache_unlock(cache);
    return FVIZ_OK;
}

static void fviz_temporal_frame_cache_clear_unlocked(FVizTemporalFrameCache* cache)
{
    if (cache == NULL || cache->entries == NULL) return;
    while (fviz_array_count(cache->entries) > 0u)
        fviz_temporal_frame_cache_remove_at(cache, 0u, FVIZ_FALSE);
    fviz_object_modified((FVizObject*)cache);
}

void fviz_temporal_frame_cache_clear(FVizTemporalFrameCache* cache)
{
    if (cache == NULL) return;
    fviz_temporal_cache_lock(cache);
    fviz_temporal_frame_cache_clear_unlocked(cache);
    fviz_temporal_cache_unlock(cache);
}

void fviz_temporal_frame_cache_get_statistics(const FVizTemporalFrameCache* cache,
                                              FVizTemporalFrameCacheStatistics* out_statistics)
{
    if (out_statistics == NULL) return;
    (void)memset(out_statistics, 0, sizeof(*out_statistics));
    out_statistics->struct_size = (uint32_t)sizeof(*out_statistics);
    if (cache == NULL) return;
    fviz_temporal_cache_lock((FVizTemporalFrameCache*)cache);
    *out_statistics = cache->statistics;
    out_statistics->struct_size = (uint32_t)sizeof(*out_statistics);
    out_statistics->entries = fviz_array_count(cache->entries);
    out_statistics->frame_capacity = cache->options.frame_capacity;
    out_statistics->byte_capacity = cache->options.byte_capacity;
    fviz_temporal_cache_unlock((FVizTemporalFrameCache*)cache);
}

void fviz_temporal_frame_cache_reset_statistics(FVizTemporalFrameCache* cache)
{
    FVizSize bytes;
    FVizSize entries;
    if (cache == NULL) return;
    fviz_temporal_cache_lock(cache);
    bytes = cache->statistics.bytes;
    entries = fviz_array_count(cache->entries);
    (void)memset(&cache->statistics, 0, sizeof(cache->statistics));
    cache->statistics.struct_size = (uint32_t)sizeof(cache->statistics);
    cache->statistics.bytes = bytes;
    cache->statistics.entries = entries;
    cache->statistics.frame_capacity = cache->options.frame_capacity;
    cache->statistics.byte_capacity = cache->options.byte_capacity;
    fviz_temporal_cache_unlock(cache);
}
