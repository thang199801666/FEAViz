#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

typedef struct TaskData
{
    int id;
    int delay_ms;
    int* order;
    volatile int* count;
} TaskData;

static FVizResult run_task(FVizCancellationToken* cancellation, void* user_data, void** out_value)
{
    TaskData* task = (TaskData*)user_data;
    int* value;
    if (task->delay_ms > 0)
    {
#if defined(_WIN32)
        Sleep((DWORD)task->delay_ms);
#else
        usleep((useconds_t)task->delay_ms * 1000u);
#endif
    }
    if (fviz_cancellation_token_is_cancelled(cancellation) != FVIZ_FALSE)
        return FVIZ_ERROR_CANCELLED;
    task->order[*task->count] = task->id;
    ++(*task->count);
    value = (int*)fviz_alloc(sizeof(*value));
    if (value == NULL) return FVIZ_ERROR_OUT_OF_MEMORY;
    *value = task->id;
    *out_value = value;
    return FVIZ_OK;
}

static FVizResult run_progress_task(FVizTaskContext* context, void* user_data, void** out_value)
{
    int* calls = (int*)user_data;
    (void)out_value;
#if defined(_WIN32)
    Sleep(20u);
#else
    usleep(20000u);
#endif
    CHECK(fviz_task_context_cancellation(context) != NULL);
    CHECK(fviz_task_context_report_progress(context, 0.25) == FVIZ_OK);
    CHECK(fviz_task_context_report_progress(context, 0.75) == FVIZ_OK);
    ++*calls;
    return FVIZ_OK;
}

static void observe_progress(FVizFuture* future, double progress, void* user_data)
{
    int* calls = (int*)user_data;
    (void)future;
    if (progress == 0.25 || progress == 0.75) ++*calls;
}

static FVizResult run_continuation(
    FVizResult antecedent_result, FVizCancellationToken* cancellation,
    void* user_data, void** out_value)
{
    int* calls = (int*)user_data;
    (void)cancellation;
    (void)out_value;
    if (antecedent_result != FVIZ_OK) return antecedent_result;
    ++*calls;
    return FVIZ_OK;
}

int main(void)
{
    FVizExecutorOptions options;
    FVizExecutor* executor = NULL;
    FVizFuture* first = NULL;
    FVizFuture* low = NULL;
    FVizFuture* high = NULL;
    FVizFuture* cancelled = NULL;
    FVizFuture* progress_future = NULL;
    FVizFuture* continuation = NULL;
    FVizExecutorStatistics statistics;
    int order[4] = {0,0,0,0};
    volatile int count = 0;
    TaskData tasks[4] = {
        {1, 80, order, &count}, {2, 0, order, &count},
        {3, 0, order, &count}, {4, 0, order, &count}};
    void* value = NULL;
    int progress_calls = 0;
    int continuation_calls = 0;
    fviz_executor_options_initialize(&options);
    options.thread_count = 1u;
    options.queue_capacity = 8u;
    CHECK(fviz_executor_create(&options, &executor) == FVIZ_OK);
    CHECK(fviz_executor_submit(executor, 0, run_task, &tasks[0], NULL, fviz_free, &first) == FVIZ_OK);
#if defined(_WIN32)
    Sleep(10u);
#else
    usleep(10000u);
#endif
    CHECK(fviz_executor_submit(executor, -1, run_task, &tasks[1], NULL, fviz_free, &low) == FVIZ_OK);
    CHECK(fviz_future_then(low, executor, 100, run_continuation,
        &continuation_calls, NULL, NULL, &continuation) == FVIZ_OK);
    CHECK(fviz_executor_submit(executor, 10, run_task, &tasks[2], NULL, fviz_free, &high) == FVIZ_OK);
    CHECK(fviz_executor_submit(executor, 5, run_task, &tasks[3], NULL, fviz_free, &cancelled) == FVIZ_OK);
    fviz_future_cancel(cancelled);
    CHECK(fviz_future_wait(first) == FVIZ_OK);
    CHECK(fviz_future_wait(high) == FVIZ_OK);
    CHECK(fviz_future_wait(cancelled) == FVIZ_ERROR_CANCELLED);
    CHECK(fviz_future_wait(low) == FVIZ_OK);
    CHECK(fviz_future_wait(continuation) == FVIZ_OK);
    CHECK(continuation_calls == 1);
    CHECK(fviz_executor_submit_context(executor, 0, run_progress_task,
        &progress_calls, NULL, NULL, &progress_future) == FVIZ_OK);
    fviz_future_set_progress_callback(progress_future, observe_progress, &progress_calls);
    CHECK(fviz_future_wait(progress_future) == FVIZ_OK);
    CHECK(fviz_future_progress(progress_future) == 0.75);
    CHECK(progress_calls == 3);
    CHECK(count == 3);
    CHECK(order[0] == 1 && order[1] == 3 && order[2] == 2);
    CHECK(fviz_future_take_value(high, &value) == FVIZ_OK);
    CHECK(*(int*)value == 3);
    fviz_free(value);
    CHECK(fviz_future_take_value(high, &value) == FVIZ_ERROR_NOT_FOUND);
    fviz_executor_get_statistics(executor, &statistics);
    CHECK(statistics.submitted == 6u && statistics.completed == 6u);
    CHECK(statistics.cancelled == 1u && statistics.failed == 0u);
    fviz_future_destroy(cancelled);
    fviz_future_destroy(progress_future);
    fviz_future_destroy(continuation);
    fviz_future_destroy(high);
    fviz_future_destroy(low);
    fviz_future_destroy(first);
    fviz_executor_destroy(executor);
    return 0;
}
