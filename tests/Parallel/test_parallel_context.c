#include <FViz/FViz.h>

#include <string.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <pthread.h>
    #include <sched.h>
#endif

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

typedef struct ContextFill
{
    uint64_t* values;
    uint64_t increment;
} ContextFill;

typedef struct ConcurrentDispatch
{
    FVizParallelContext* context;
    volatile long* arrivals;
    FVizResult result;
} ConcurrentDispatch;

static FVizResult fill_result(FVizSize begin, FVizSize end, void* user_data)
{
    ContextFill* fill = (ContextFill*)user_data;
    void* scratch = fviz_parallel_scratch_allocate(128u, 64u);
    FVizSize i;
    if (scratch == NULL || ((uintptr_t)scratch & 63u) != 0u) return FVIZ_ERROR_INTERNAL;
    memset(scratch, 0x5a, 128u);
    for (i = begin; i < end; ++i) fill->values[i] += fill->increment;
    return FVIZ_OK;
}

static FVizResult fail_second_chunk(FVizSize begin, FVizSize end, void* user_data)
{
    (void)end;
    (void)user_data;
    return begin >= 16u ? FVIZ_ERROR_INTERNAL : FVIZ_OK;
}

static FVizResult cancellation_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizCancellationToken* token = (FVizCancellationToken*)user_data;
    (void)begin;
    (void)end;
    fviz_cancellation_token_cancel(token);
    return FVIZ_OK;
}

static long concurrent_increment(volatile long* value)
{
#if defined(_WIN32)
    return InterlockedIncrement(value);
#else
    return __atomic_add_fetch(value, 1, __ATOMIC_ACQ_REL);
#endif
}

static long concurrent_load(volatile long* value)
{
#if defined(_WIN32)
    return InterlockedCompareExchange(value, 0, 0);
#else
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#endif
}

static FVizResult rendezvous_range(FVizSize begin, FVizSize end, void* user_data)
{
    volatile long* arrivals = (volatile long*)user_data;
    uint32_t spins;
    (void)begin;
    (void)end;
    (void)concurrent_increment(arrivals);
    for (spins = 0u; spins < 1000000u && concurrent_load(arrivals) < 2; ++spins)
    {
#if defined(_WIN32)
        SwitchToThread();
#else
        sched_yield();
#endif
    }
    return concurrent_load(arrivals) >= 2 ? FVIZ_OK : FVIZ_ERROR_BUSY;
}

#if defined(_WIN32)
static DWORD WINAPI dispatch_thread(LPVOID user_data)
#else
static void* dispatch_thread(void* user_data)
#endif
{
    ConcurrentDispatch* dispatch = (ConcurrentDispatch*)user_data;
    dispatch->result = fviz_parallel_context_for(
        dispatch->context, 0u, 1u, 1u, rendezvous_range,
        (void*)dispatch->arrivals, NULL);
#if defined(_WIN32)
    return 0u;
#else
    return NULL;
#endif
}

static int test_independent_contexts(FVizParallelContext* first, FVizParallelContext* second)
{
    volatile long arrivals = 0;
    ConcurrentDispatch dispatches[2];
    dispatches[0].context = first;
    dispatches[0].arrivals = &arrivals;
    dispatches[0].result = FVIZ_ERROR_INTERNAL;
    dispatches[1].context = second;
    dispatches[1].arrivals = &arrivals;
    dispatches[1].result = FVIZ_ERROR_INTERNAL;
#if defined(_WIN32)
    {
        HANDLE threads[2];
        threads[0] = CreateThread(NULL, 0u, dispatch_thread, &dispatches[0], 0u, NULL);
        threads[1] = CreateThread(NULL, 0u, dispatch_thread, &dispatches[1], 0u, NULL);
        CHECK(threads[0] != NULL && threads[1] != NULL);
        CHECK(WaitForMultipleObjects(2u, threads, TRUE, 10000u) == WAIT_OBJECT_0);
        CloseHandle(threads[0]);
        CloseHandle(threads[1]);
    }
#else
    {
        pthread_t threads[2];
        CHECK(pthread_create(&threads[0], NULL, dispatch_thread, &dispatches[0]) == 0);
        CHECK(pthread_create(&threads[1], NULL, dispatch_thread, &dispatches[1]) == 0);
        CHECK(pthread_join(threads[0], NULL) == 0);
        CHECK(pthread_join(threads[1], NULL) == 0);
    }
#endif
    CHECK(dispatches[0].result == FVIZ_OK);
    CHECK(dispatches[1].result == FVIZ_OK);
    return 0;
}

int main(void)
{
    FVizParallelContextOptions options;
    FVizParallelContext* first = NULL;
    FVizParallelContext* second = NULL;
    FVizCancellationToken* cancellation = NULL;
    FVizTaskGroup* group = NULL;
    FVizParallelStatistics statistics;
    uint64_t values[64] = {0};
    double inputs[10000];
    double expected_sum = 0.0;
    double actual_sum = 0.0;
    uint64_t scan_input[] = {3u, 1u, 4u, 1u, 5u};
    uint64_t scan_output[5];
    uint64_t total = 0u;
    uint64_t keys[] = {5u, 2u, 5u, 1u, 2u};
    FVizSize indices[] = {0u, 1u, 2u, 3u, 4u};
    uint64_t* large_scan_input = NULL;
    uint64_t* large_scan_output = NULL;
    uint64_t* large_keys = NULL;
    FVizSize* large_indices = NULL;
    const FVizSize large_count = 20000u;
    ContextFill fill;
    volatile long group_arrivals = 0;
    FVizSize i;
    int result;

    fviz_parallel_context_options_initialize(&options);
    CHECK(options.struct_size == sizeof(options));
    CHECK(fviz_parallel_scratch_capacity() >= 65536u);
    options.thread_count = 4u;
    CHECK(fviz_parallel_context_create(&options, &first) == FVIZ_OK);
    options.thread_count = 2u;
    CHECK(fviz_parallel_context_create(&options, &second) == FVIZ_OK);
    CHECK(fviz_parallel_context_thread_count(first) >= 1u);
    CHECK(fviz_parallel_context_worker_count(first) + 1u == fviz_parallel_context_thread_count(first));

    result = test_independent_contexts(first, second);
    CHECK(result == 0);
    fill.values = values;
    fill.increment = 1u;
    CHECK(fviz_task_group_create(first, NULL, &group) == FVIZ_OK);
    CHECK(fviz_task_group_run(group, 0u, 32u, 8u, fill_result, &fill) == FVIZ_OK);
    CHECK(fviz_task_group_run(group, 32u, 64u, 8u, fill_result, &fill) == FVIZ_OK);
    CHECK(fviz_task_group_wait(group) == FVIZ_OK);
    for (i = 0u; i < 64u; ++i) CHECK(values[i] == 1u);
    /* Queued tasks share one scheduler dispatch and may overlap.  This catches
     * regressions back to the old sequential TaskGroup::wait behavior. */
    if (fviz_parallel_context_thread_count(first) >= 2u)
    {
        CHECK(fviz_task_group_run(group, 0u, 1u, 1u, rendezvous_range, (void*)&group_arrivals) == FVIZ_OK);
        CHECK(fviz_task_group_run(group, 0u, 1u, 1u, rendezvous_range, (void*)&group_arrivals) == FVIZ_OK);
        CHECK(fviz_task_group_wait(group) == FVIZ_OK);
        CHECK(concurrent_load(&group_arrivals) >= 2);
    }

    CHECK(fviz_parallel_context_for(
        first, 0u, 64u, 16u, fail_second_chunk, NULL, NULL) == FVIZ_ERROR_INTERNAL);
    CHECK(fviz_cancellation_token_create(&cancellation) == FVIZ_OK);
    CHECK(fviz_parallel_context_for(
        first, 0u, 64u, 1u, cancellation_range, cancellation, cancellation) == FVIZ_ERROR_CANCELLED);
    CHECK(fviz_cancellation_token_is_cancelled(cancellation) == FVIZ_TRUE);
    fviz_cancellation_token_reset(cancellation);
    CHECK(fviz_cancellation_token_is_cancelled(cancellation) == FVIZ_FALSE);

    for (i = 0u; i < 10000u; ++i)
    {
        inputs[i] = (double)((i % 17u) + 1u) / 17.0;
        expected_sum += inputs[i];
    }
    CHECK(fviz_parallel_sum_f64(first, inputs, 10000u, &actual_sum, NULL) == FVIZ_OK);
    CHECK(actual_sum == expected_sum);
    CHECK(fviz_parallel_exclusive_scan_u64(
        first, scan_input, scan_output, 5u, 2u, &total) == FVIZ_OK);
    CHECK(scan_output[0] == 2u && scan_output[1] == 5u && scan_output[4] == 11u && total == 16u);
    CHECK(fviz_parallel_inclusive_scan_u64(
        first, scan_input, scan_output, 5u, &total) == FVIZ_OK);
    CHECK(scan_output[0] == 3u && scan_output[4] == 14u && total == 14u);
    CHECK(fviz_parallel_stable_sort_u64_indices(first, keys, indices, 5u) == FVIZ_OK);
    CHECK(indices[0] == 3u && indices[1] == 1u && indices[2] == 4u &&
          indices[3] == 0u && indices[4] == 2u);

    /* Exercise the parallel scan/sort paths rather than only their tiny serial
     * fallbacks.  Stable-sort validation deliberately uses many duplicate keys. */
    large_scan_input = (uint64_t*)fviz_alloc(large_count * sizeof(uint64_t));
    large_scan_output = (uint64_t*)fviz_alloc(large_count * sizeof(uint64_t));
    large_keys = (uint64_t*)fviz_alloc(large_count * sizeof(uint64_t));
    large_indices = (FVizSize*)fviz_alloc(large_count * sizeof(FVizSize));
    CHECK(large_scan_input != NULL && large_scan_output != NULL &&
          large_keys != NULL && large_indices != NULL);
    for (i = 0u; i < large_count; ++i)
    {
        large_scan_input[i] = 1u;
        large_keys[i] = (uint64_t)(i % 257u);
        large_indices[i] = i;
    }
    CHECK(fviz_parallel_exclusive_scan_u64(
        first, large_scan_input, large_scan_output, large_count, 7u, &total) == FVIZ_OK);
    CHECK(large_scan_output[0] == 7u);
    CHECK(large_scan_output[large_count - 1u] == 7u + (uint64_t)(large_count - 1u));
    CHECK(total == 7u + (uint64_t)large_count);
    CHECK(fviz_parallel_inclusive_scan_u64(
        first, large_scan_input, large_scan_output, large_count, &total) == FVIZ_OK);
    CHECK(large_scan_output[0] == 1u);
    CHECK(large_scan_output[large_count - 1u] == (uint64_t)large_count);
    CHECK(total == (uint64_t)large_count);
    CHECK(fviz_parallel_stable_sort_u64_indices(first, large_keys, large_indices, large_count) == FVIZ_OK);
    for (i = 1u; i < large_count; ++i)
    {
        const uint64_t previous_key = large_keys[large_indices[i - 1u]];
        const uint64_t current_key = large_keys[large_indices[i]];
        CHECK(previous_key <= current_key);
        if (previous_key == current_key) CHECK(large_indices[i - 1u] < large_indices[i]);
    }
    fviz_free(large_indices);
    fviz_free(large_keys);
    fviz_free(large_scan_output);
    fviz_free(large_scan_input);

    fviz_parallel_context_get_statistics(first, &statistics);
    CHECK(statistics.dispatch_count >= 4u);
    CHECK(statistics.failed_dispatch_count == 1u);
    CHECK(statistics.cancelled_dispatch_count == 1u);
    CHECK(statistics.chunk_count >= statistics.completed_chunk_count);

    fviz_task_group_destroy(group);
    fviz_cancellation_token_destroy(cancellation);
    fviz_parallel_context_destroy(second);
    fviz_parallel_context_destroy(first);

    for (i = 0u; i < 32u; ++i)
    {
        options.thread_count = (uint32_t)(i % 4u) + 1u;
        CHECK(fviz_parallel_context_create(&options, &first) == FVIZ_OK);
        fviz_parallel_context_destroy(first);
    }
    return 0;
}
