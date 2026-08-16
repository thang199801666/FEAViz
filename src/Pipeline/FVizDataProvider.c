#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Pipeline/FVizDataProvider.h>
#include <FViz/Parallel/FVizParallel.h>
#include <FViz/Parallel/FVizExecutor.h>

#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizErrorInternal.h>

typedef struct FVizDataProviderEntry
{
    uint64_t resource_key;
    FVizPipelineRequestInfo request;
    FVizDataObject* data;
    FVizSize bytes;
    uint64_t last_use;
} FVizDataProviderEntry;

struct FVizDataProvider
{
#if defined(_WIN32)
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
    FVizAtomicU32 references;
    FVizDataProviderCallbacks callbacks;
    FVizDataProviderOptions options;
    FVizDataProviderStatistics statistics;
    void* user_data;
    FVizDataProviderEntry* entries;
    FVizSize entry_count;
    uint64_t use_serial;
};

typedef struct FVizDataProviderAsyncTask
{
    FVizDataProvider* provider;
    FVizDataProviderRequest request;
} FVizDataProviderAsyncTask;

static void fviz_data_provider_lock(FVizDataProvider* provider)
{
#if defined(_WIN32)
    EnterCriticalSection(&provider->mutex);
#else
    (void)pthread_mutex_lock(&provider->mutex);
#endif
}

static void fviz_data_provider_unlock(FVizDataProvider* provider)
{
#if defined(_WIN32)
    LeaveCriticalSection(&provider->mutex);
#else
    (void)pthread_mutex_unlock(&provider->mutex);
#endif
}

static FVizBool fviz_data_provider_request_equal(const FVizDataProviderEntry* entry,
                                                 const FVizDataProviderRequest* request)
{
    const FVizPipelineRequestInfo* a = &entry->request;
    const FVizPipelineRequestInfo* b = &request->pipeline;
    uint32_t i;
    if (entry->resource_key != request->resource_key || a->type != b->type ||
        a->requested_output_port != b->requested_output_port || a->piece != b->piece ||
        a->number_of_pieces != b->number_of_pieces || a->ghost_levels != b->ghost_levels ||
        a->has_extent != b->has_extent || a->has_time != b->has_time || a->time != b->time || a->flags != b->flags)
        return FVIZ_FALSE;
    if (a->has_extent != FVIZ_FALSE)
        for (i = 0u; i < 6u; ++i)
            if (a->extent[i] != b->extent[i]) return FVIZ_FALSE;
    return FVIZ_TRUE;
}

static void fviz_data_provider_remove(FVizDataProvider* provider, FVizSize index)
{
    if (provider == NULL || index >= provider->entry_count) return;
    if (provider->entries[index].bytes <= provider->statistics.resident_bytes)
        provider->statistics.resident_bytes -= provider->entries[index].bytes;
    else
        provider->statistics.resident_bytes = 0u;
    fviz_release(provider->entries[index].data);
    if (index + 1u < provider->entry_count)
        memmove(&provider->entries[index], &provider->entries[index + 1u],
                (provider->entry_count - index - 1u) * sizeof(*provider->entries));
    --provider->entry_count;
    provider->statistics.resident_entries = provider->entry_count;
}

void fviz_data_provider_request_initialize(FVizDataProviderRequest* request)
{
    if (request == NULL) return;
    memset(request, 0, sizeof(*request));
    request->struct_size = (uint32_t)sizeof(*request);
    fviz_pipeline_request_initialize(&request->pipeline);
    request->pipeline.type = FVIZ_PIPELINE_REQUEST_DATA;
    request->allow_cache = FVIZ_TRUE;
}

void fviz_data_provider_callbacks_initialize(FVizDataProviderCallbacks* callbacks)
{
    if (callbacks == NULL) return;
    memset(callbacks, 0, sizeof(*callbacks));
    callbacks->struct_size = (uint32_t)sizeof(*callbacks);
}

void fviz_data_provider_options_initialize(FVizDataProviderOptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->cache_entry_capacity = 16u;
}

FVizResult fviz_data_provider_create(const FVizDataProviderCallbacks* callbacks, void* user_data,
                                     const FVizDataProviderOptions* options, FVizDataProvider** out_provider)
{
    FVizDataProviderOptions defaults;
    FVizDataProvider* provider;
    if (out_provider == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_provider = NULL;
    if (options == NULL)
    {
        fviz_data_provider_options_initialize(&defaults);
        options = &defaults;
    }
    if (callbacks == NULL || callbacks->struct_size < sizeof(*callbacks) || callbacks->fetch == NULL ||
        options->struct_size < sizeof(*options) ||
        options->cache_entry_capacity > (FVizSize)-1 / sizeof(FVizDataProviderEntry))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    provider = (FVizDataProvider*)fviz_alloc(sizeof(*provider));
    if (provider == NULL) return fviz_last_error_code();
    memset(provider, 0, sizeof(*provider));
#if defined(_WIN32)
    InitializeCriticalSection(&provider->mutex);
#else
    if (pthread_mutex_init(&provider->mutex, NULL) != 0)
    {
        fviz_free(provider);
        return FVIZ_ERROR_INTERNAL;
    }
#endif
    provider->references.value = 1u;
    provider->callbacks = *callbacks;
    provider->options = *options;
    provider->user_data = user_data;
    provider->statistics.struct_size = (uint32_t)sizeof(provider->statistics);
    if (options->cache_entry_capacity != 0u)
    {
        provider->entries =
            (FVizDataProviderEntry*)fviz_alloc(options->cache_entry_capacity * sizeof(*provider->entries));
        if (provider->entries == NULL)
        {
#if defined(_WIN32)
            DeleteCriticalSection(&provider->mutex);
#else
            (void)pthread_mutex_destroy(&provider->mutex);
#endif
            fviz_free(provider);
            return fviz_last_error_code();
        }
    }
    *out_provider = provider;
    return FVIZ_OK;
}

static void fviz_data_provider_clear_cache_unlocked(FVizDataProvider* provider)
{
    if (provider == NULL) return;
    while (provider->entry_count != 0u)
        fviz_data_provider_remove(provider, provider->entry_count - 1u);
}

static void fviz_data_provider_release(FVizDataProvider* provider)
{
    if (provider == NULL) return;
    if (fviz_atomic_u32_fetch_sub(&provider->references, 1u) != 1u) return;
    fviz_data_provider_clear_cache_unlocked(provider);
    if (provider->callbacks.destroy != NULL) provider->callbacks.destroy(provider->user_data);
    fviz_free(provider->entries);
#if defined(_WIN32)
    DeleteCriticalSection(&provider->mutex);
#else
    (void)pthread_mutex_destroy(&provider->mutex);
#endif
    fviz_free(provider);
}

void fviz_data_provider_destroy(FVizDataProvider* provider)
{
    fviz_data_provider_release(provider);
}

void fviz_data_provider_clear_cache(FVizDataProvider* provider)
{
    if (provider == NULL) return;
    fviz_data_provider_lock(provider);
    fviz_data_provider_clear_cache_unlocked(provider);
    fviz_data_provider_unlock(provider);
}

static FVizResult fviz_data_provider_fetch_unlocked(FVizDataProvider* provider, const FVizDataProviderRequest* request,
                                                    FVizDataObject** out_data)
{
    FVizSize i;
    FVizDataObject* data = NULL;
    FVizResult result = FVIZ_ERROR_INTERNAL;
    uint32_t attempt;
    if (out_data == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_data = NULL;
    if (provider == NULL || request == NULL || request->struct_size < sizeof(*request) ||
        request->pipeline.struct_size < sizeof(request->pipeline))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    ++provider->statistics.requests;
    if (request->pipeline.cancellation != NULL &&
        fviz_cancellation_token_is_cancelled(request->pipeline.cancellation) != FVIZ_FALSE)
        return FVIZ_ERROR_CANCELLED;
    if (request->allow_cache != FVIZ_FALSE)
        for (i = 0u; i < provider->entry_count; ++i)
            if (fviz_data_provider_request_equal(&provider->entries[i], request) != FVIZ_FALSE)
            {
                provider->entries[i].last_use = ++provider->use_serial;
                ++provider->statistics.cache_hits;
                *out_data = (FVizDataObject*)fviz_retain(provider->entries[i].data);
                return *out_data != NULL ? FVIZ_OK : fviz_last_error_code();
            }
    ++provider->statistics.cache_misses;
    for (attempt = 0u; attempt <= provider->options.retry_count; ++attempt)
    {
        if (request->pipeline.cancellation != NULL &&
            fviz_cancellation_token_is_cancelled(request->pipeline.cancellation) != FVIZ_FALSE)
        {
            result = FVIZ_ERROR_CANCELLED;
            break;
        }
        ++provider->statistics.fetch_attempts;
        result = provider->callbacks.fetch(request, provider->user_data, &data);
        if (result == FVIZ_OK) break;
        fviz_release(data);
        data = NULL;
        if ((result != FVIZ_ERROR_IO && result != FVIZ_ERROR_BUSY) || attempt == provider->options.retry_count) break;
        ++provider->statistics.retries;
    }
    if (result != FVIZ_OK)
    {
        if (result != FVIZ_ERROR_CANCELLED) ++provider->statistics.failures;
        fviz_release(data);
        return result;
    }
    if (data == NULL || fviz_data_object_is_data_object(data) == FVIZ_FALSE)
    {
        fviz_release(data);
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "data provider returned success without a data object");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (request->allow_cache != FVIZ_FALSE && provider->options.cache_entry_capacity != 0u)
    {
        const FVizSize bytes = fviz_data_object_memory_size(data);
        if (provider->options.cache_byte_capacity == 0u || bytes <= provider->options.cache_byte_capacity)
        {
            while (provider->entry_count >= provider->options.cache_entry_capacity ||
                   (provider->options.cache_byte_capacity != 0u &&
                    bytes > provider->options.cache_byte_capacity - provider->statistics.resident_bytes))
            {
                FVizSize oldest = 0u;
                for (i = 1u; i < provider->entry_count; ++i)
                    if (provider->entries[i].last_use < provider->entries[oldest].last_use) oldest = i;
                fviz_data_provider_remove(provider, oldest);
                ++provider->statistics.evictions;
            }
            provider->entries[provider->entry_count].resource_key = request->resource_key;
            provider->entries[provider->entry_count].request = request->pipeline;
            provider->entries[provider->entry_count].request.cancellation = NULL;
            provider->entries[provider->entry_count].data = (FVizDataObject*)fviz_retain(data);
            provider->entries[provider->entry_count].bytes = bytes;
            provider->entries[provider->entry_count].last_use = ++provider->use_serial;
            if (provider->entries[provider->entry_count].data == NULL)
            {
                fviz_release(data);
                return fviz_last_error_code();
            }
            ++provider->entry_count;
            provider->statistics.resident_entries = provider->entry_count;
            provider->statistics.resident_bytes += bytes;
        }
    }
    *out_data = data;
    return FVIZ_OK;
}

FVizResult fviz_data_provider_fetch(FVizDataProvider* provider, const FVizDataProviderRequest* request,
                                    FVizDataObject** out_data)
{
    FVizResult result;
    if (provider == NULL)
    {
        if (out_data != NULL) *out_data = NULL;
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_data_provider_lock(provider);
    result = fviz_data_provider_fetch_unlocked(provider, request, out_data);
    fviz_data_provider_unlock(provider);
    return result;
}

static FVizResult fviz_data_provider_async_run(FVizCancellationToken* cancellation, void* user_data, void** out_value)
{
    FVizDataProviderAsyncTask* task = (FVizDataProviderAsyncTask*)user_data;
    FVizDataObject* data = NULL;
    FVizResult result;
    task->request.pipeline.cancellation = cancellation;
    result = fviz_data_provider_fetch(task->provider, &task->request, &data);
    if (result == FVIZ_OK) *out_value = data;
    else
        fviz_release(data);
    return result;
}

static void fviz_data_provider_async_task_destroy(void* user_data)
{
    FVizDataProviderAsyncTask* task = (FVizDataProviderAsyncTask*)user_data;
    if (task == NULL) return;
    fviz_data_provider_release(task->provider);
    fviz_free(task);
}

FVizResult fviz_data_provider_fetch_async(FVizDataProvider* provider, const FVizDataProviderRequest* request,
                                          FVizExecutor* executor, int priority, FVizFuture** out_future)
{
    FVizDataProviderAsyncTask* task;
    FVizResult result;
    if (out_future == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_future = NULL;
    if (provider == NULL || request == NULL || executor == NULL || request->struct_size < sizeof(*request) ||
        request->pipeline.struct_size < sizeof(request->pipeline))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    task = (FVizDataProviderAsyncTask*)fviz_alloc(sizeof(*task));
    if (task == NULL) return fviz_last_error_code();
    task->provider = provider;
    task->request = *request;
    (void)fviz_atomic_u32_fetch_add(&provider->references, 1u);
    result =
        fviz_executor_submit(executor, priority, fviz_data_provider_async_run, task,
                             fviz_data_provider_async_task_destroy, (FVizTaskValueDestroyFn)fviz_release, out_future);
    if (result != FVIZ_OK) fviz_data_provider_async_task_destroy(task);
    return result;
}

void fviz_data_provider_get_statistics(const FVizDataProvider* provider, FVizDataProviderStatistics* out_statistics)
{
    if (out_statistics == NULL) return;
    memset(out_statistics, 0, sizeof(*out_statistics));
    out_statistics->struct_size = (uint32_t)sizeof(*out_statistics);
    if (provider != NULL)
    {
        FVizDataProvider* mutable_provider = (FVizDataProvider*)provider;
        fviz_data_provider_lock(mutable_provider);
        *out_statistics = provider->statistics;
        fviz_data_provider_unlock(mutable_provider);
    }
}
