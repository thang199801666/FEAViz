#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

typedef struct ProviderState { uint32_t calls; FVizBool fail_next; } ProviderState;

static FVizResult fetch_data(
    const FVizDataProviderRequest* request, void* user_data, FVizDataObject** out_data)
{
    ProviderState* state = (ProviderState*)user_data;
    FVizPolyData* data = NULL;
    ++state->calls;
    *out_data = NULL;
    if (state->fail_next != FVIZ_FALSE)
    { state->fail_next = FVIZ_FALSE; return FVIZ_ERROR_IO; }
    if (fviz_poly_data_create(&data) != FVIZ_OK ||
        fviz_poly_data_add_point(data, fviz_vec3((float)request->resource_key, 0.0f, 0.0f), NULL) != FVIZ_OK)
    { fviz_release(data); return fviz_last_error_code(); }
    *out_data = (FVizDataObject*)data;
    return FVIZ_OK;
}

int main(void)
{
    FVizDataProviderCallbacks callbacks;
    FVizDataProviderOptions options;
    FVizDataProviderRequest request;
    FVizDataProvider* provider = NULL;
    FVizDataProviderStatistics statistics;
    FVizDataObject* data = NULL;
    FVizCancellationToken* cancellation = NULL;
    FVizExecutor* executor = NULL;
    FVizFuture* future = NULL;
    ProviderState state = {0u, FVIZ_TRUE};
    fviz_data_provider_callbacks_initialize(&callbacks);
    callbacks.fetch = fetch_data;
    fviz_data_provider_options_initialize(&options);
    options.cache_entry_capacity = 1u;
    options.retry_count = 1u;
    CHECK(fviz_data_provider_create(&callbacks, &state, &options, &provider) == FVIZ_OK);
    fviz_data_provider_request_initialize(&request);
    request.resource_key = 7u;
    request.pipeline.piece = 1u;
    request.pipeline.number_of_pieces = 4u;
    CHECK(fviz_data_provider_fetch(provider, &request, &data) == FVIZ_OK);
    CHECK(state.calls == 2u);
    fviz_release(data); data = NULL;
    CHECK(fviz_data_provider_fetch(provider, &request, &data) == FVIZ_OK);
    CHECK(state.calls == 2u);
    fviz_release(data); data = NULL;
    request.resource_key = 8u;
    CHECK(fviz_data_provider_fetch(provider, &request, &data) == FVIZ_OK);
    fviz_release(data); data = NULL;
    fviz_data_provider_get_statistics(provider, &statistics);
    CHECK(statistics.requests == 3u && statistics.cache_hits == 1u);
    CHECK(statistics.fetch_attempts == 3u && statistics.retries == 1u);
    CHECK(statistics.evictions == 1u && statistics.resident_entries == 1u);
    CHECK(fviz_cancellation_token_create(&cancellation) == FVIZ_OK);
    fviz_cancellation_token_cancel(cancellation);
    request.pipeline.cancellation = cancellation;
    CHECK(fviz_data_provider_fetch(provider, &request, &data) == FVIZ_ERROR_CANCELLED);
    CHECK(data == NULL);
    request.pipeline.cancellation = NULL;
    request.resource_key = 9u;
    CHECK(fviz_executor_create(NULL, &executor) == FVIZ_OK);
    CHECK(fviz_data_provider_fetch_async(provider, &request, executor, 2, &future) == FVIZ_OK);
    CHECK(fviz_future_take_value(future, (void**)&data) == FVIZ_OK);
    CHECK(data != NULL);
    fviz_release(data); data = NULL;
    fviz_future_destroy(future);
    fviz_executor_destroy(executor);
    fviz_cancellation_token_destroy(cancellation);
    fviz_data_provider_destroy(provider);
    return 0;
}
