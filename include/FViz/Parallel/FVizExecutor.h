#ifndef FVIZ_PARALLEL_EXECUTOR_H
#define FVIZ_PARALLEL_EXECUTOR_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Parallel/FVizParallel.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizExecutor FVizExecutor;
typedef struct FVizFuture FVizFuture;
typedef struct FVizTaskContext FVizTaskContext;

typedef FVizResult (*FVizTaskFn)(FVizCancellationToken* cancellation, void* user_data, void** out_value);
typedef void (*FVizTaskDataDestroyFn)(void* user_data);
typedef void (*FVizTaskValueDestroyFn)(void* value);
typedef FVizResult (*FVizTaskContextFn)(FVizTaskContext* context, void* user_data, void** out_value);
typedef FVizResult (*FVizContinuationFn)(FVizResult antecedent_result, FVizCancellationToken* cancellation,
                                         void* user_data, void** out_value);
typedef void (*FVizFutureProgressFn)(FVizFuture* future, double progress, void* user_data);

typedef struct FVizExecutorOptions
{
    uint32_t struct_size;
    /* Zero chooses the current FViz parallel thread limit. */
    uint32_t thread_count;
    /* Zero chooses 1024 pending tasks. Running tasks do not consume queue slots. */
    FVizSize queue_capacity;
} FVizExecutorOptions;

typedef struct FVizExecutorStatistics
{
    uint32_t struct_size;
    uint64_t submitted;
    uint64_t completed;
    uint64_t cancelled;
    uint64_t failed;
    FVizSize queued;
    uint32_t running;
    uint64_t queue_wait_ns;
    uint64_t execution_ns;
} FVizExecutorStatistics;

FVIZ_API void fviz_executor_options_initialize(FVizExecutorOptions* options);
FVIZ_API FVizResult fviz_executor_create(const FVizExecutorOptions* options, FVizExecutor** out_executor);
/* Cancels queued/running work and joins all workers. Futures remain valid and
 * ready after executor destruction. Submit/destroy require external synchronization. */
FVIZ_API void fviz_executor_destroy(FVizExecutor* executor);
FVIZ_API FVizResult fviz_executor_submit(FVizExecutor* executor, int priority, FVizTaskFn function, void* user_data,
                                         FVizTaskDataDestroyFn user_data_destroy, FVizTaskValueDestroyFn value_destroy,
                                         FVizFuture** out_future);
/* Context tasks can publish monotonic progress. The callback, when installed,
 * runs on the reporting worker thread and must not destroy the future/executor. */
FVIZ_API FVizResult fviz_executor_submit_context(FVizExecutor* executor, int priority, FVizTaskContextFn function,
                                                 void* user_data, FVizTaskDataDestroyFn user_data_destroy,
                                                 FVizTaskValueDestroyFn value_destroy, FVizFuture** out_future);
/* Queues a non-blocking DAG successor on the same executor. It becomes runnable
 * only after antecedent completes, and receives the antecedent result. */
FVIZ_API FVizResult fviz_future_then(FVizFuture* antecedent, FVizExecutor* executor, int priority,
                                     FVizContinuationFn function, void* user_data,
                                     FVizTaskDataDestroyFn user_data_destroy, FVizTaskValueDestroyFn value_destroy,
                                     FVizFuture** out_future);
FVIZ_API void fviz_executor_get_statistics(const FVizExecutor* executor, FVizExecutorStatistics* out_statistics);

FVIZ_API void fviz_future_cancel(FVizFuture* future);
FVIZ_API FVizBool fviz_future_ready(const FVizFuture* future);
FVIZ_API FVizCancellationToken* fviz_task_context_cancellation(FVizTaskContext* context);
FVIZ_API FVizResult fviz_task_context_report_progress(FVizTaskContext* context, double progress);
FVIZ_API double fviz_future_progress(const FVizFuture* future);
FVIZ_API void fviz_future_set_progress_callback(FVizFuture* future, FVizFutureProgressFn callback, void* user_data);
FVIZ_API FVizResult fviz_future_wait(FVizFuture* future);
/* Transfers the task value exactly once after successful completion. */
FVIZ_API FVizResult fviz_future_take_value(FVizFuture* future, void** out_value);
FVIZ_API void fviz_future_destroy(FVizFuture* future);

FVIZ_EXTERN_C_END

#endif /* FVIZ_PARALLEL_EXECUTOR_H */
