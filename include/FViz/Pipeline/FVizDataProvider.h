#ifndef FVIZ_PIPELINE_DATA_PROVIDER_H
#define FVIZ_PIPELINE_DATA_PROVIDER_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataObject.h>
#include <FViz/Pipeline/FVizExecutive.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizDataProvider FVizDataProvider;
typedef struct FVizExecutor FVizExecutor;
typedef struct FVizFuture FVizFuture;

typedef struct FVizDataProviderRequest
{
    uint32_t struct_size;
    /* Provider-defined stable identity, e.g. file/chunk/dataset key. */
    uint64_t resource_key;
    FVizPipelineRequestInfo pipeline;
    FVizBool allow_cache;
} FVizDataProviderRequest;

typedef FVizResult (*FVizDataProviderFetchFn)(const FVizDataProviderRequest* request, void* user_data,
                                              FVizDataObject** out_data);
typedef void (*FVizDataProviderDestroyFn)(void* user_data);

typedef struct FVizDataProviderCallbacks
{
    uint32_t struct_size;
    FVizDataProviderFetchFn fetch;
    FVizDataProviderDestroyFn destroy;
} FVizDataProviderCallbacks;

typedef struct FVizDataProviderOptions
{
    uint32_t struct_size;
    FVizSize cache_entry_capacity;
    FVizSize cache_byte_capacity;
    uint32_t retry_count;
} FVizDataProviderOptions;

typedef struct FVizDataProviderStatistics
{
    uint32_t struct_size;
    uint64_t requests;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t fetch_attempts;
    uint64_t retries;
    uint64_t failures;
    uint64_t evictions;
    FVizSize resident_entries;
    FVizSize resident_bytes;
} FVizDataProviderStatistics;

FVIZ_FILTERS_API void fviz_data_provider_request_initialize(FVizDataProviderRequest* request);
FVIZ_FILTERS_API void fviz_data_provider_callbacks_initialize(FVizDataProviderCallbacks* callbacks);
FVIZ_FILTERS_API void fviz_data_provider_options_initialize(FVizDataProviderOptions* options);
FVIZ_FILTERS_API FVizResult fviz_data_provider_create(const FVizDataProviderCallbacks* callbacks, void* user_data,
                                              const FVizDataProviderOptions* options, FVizDataProvider** out_provider);
FVIZ_FILTERS_API void fviz_data_provider_destroy(FVizDataProvider* provider);
/* Returns an owned data reference. Provider mutation/fetch is externally synchronized. */
FVIZ_FILTERS_API FVizResult fviz_data_provider_fetch(FVizDataProvider* provider, const FVizDataProviderRequest* request,
                                             FVizDataObject** out_data);
/* The provider and request are retained/copied through completion. The future
 * value is an owned FVizDataObject reference transferable with take_value(). */
FVIZ_FILTERS_API FVizResult fviz_data_provider_fetch_async(FVizDataProvider* provider, const FVizDataProviderRequest* request,
                                                   FVizExecutor* executor, int priority, FVizFuture** out_future);
FVIZ_FILTERS_API void fviz_data_provider_clear_cache(FVizDataProvider* provider);
FVIZ_FILTERS_API void fviz_data_provider_get_statistics(const FVizDataProvider* provider,
                                                FVizDataProviderStatistics* out_statistics);

FVIZ_EXTERN_C_END

#endif /* FVIZ_PIPELINE_DATA_PROVIDER_H */
