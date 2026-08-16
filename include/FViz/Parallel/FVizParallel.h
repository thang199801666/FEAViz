#ifndef FVIZ_PARALLEL_PARALLEL_H
#define FVIZ_PARALLEL_PARALLEL_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef void (*FVizParallelRangeFn)(FVizSize begin, FVizSize end, void* user_data);
typedef FVizResult (*FVizParallelRangeResultFn)(FVizSize begin, FVizSize end, void* user_data);

typedef struct FVizParallelContext FVizParallelContext;
typedef struct FVizCancellationToken FVizCancellationToken;
typedef struct FVizTaskGroup FVizTaskGroup;

typedef enum FVizParallelAffinityPolicy
{
    FVIZ_PARALLEL_AFFINITY_NONE = 0,
    FVIZ_PARALLEL_AFFINITY_COMPACT = 1,
    FVIZ_PARALLEL_AFFINITY_SPREAD = 2
} FVizParallelAffinityPolicy;

#define FVIZ_PARALLEL_CONTEXT_OPTIONS_VERSION 1u

typedef struct FVizParallelContextOptions
{
    uint32_t struct_size;
    uint32_t version;
    uint32_t thread_count;
    FVizParallelAffinityPolicy affinity_policy;
} FVizParallelContextOptions;

typedef struct FVizParallelStatistics
{
    uint64_t dispatch_count;
    uint64_t chunk_count;
    uint64_t completed_chunk_count;
    uint64_t cancelled_dispatch_count;
    uint64_t failed_dispatch_count;
} FVizParallelStatistics;

FVIZ_PARALLEL_API void fviz_parallel_context_options_initialize(FVizParallelContextOptions* options);
FVIZ_PARALLEL_API FVizResult fviz_parallel_context_create(const FVizParallelContextOptions* options,
                                                 FVizParallelContext** out_context);
FVIZ_PARALLEL_API void fviz_parallel_context_destroy(FVizParallelContext* context);
FVIZ_PARALLEL_API FVizParallelContext* fviz_parallel_default_context(void);
FVIZ_PARALLEL_API uint32_t fviz_parallel_context_thread_count(const FVizParallelContext* context);
FVIZ_PARALLEL_API uint32_t fviz_parallel_context_worker_count(const FVizParallelContext* context);
FVIZ_PARALLEL_API void fviz_parallel_context_get_statistics(const FVizParallelContext* context,
                                                   FVizParallelStatistics* out_statistics);

FVIZ_PARALLEL_API FVizResult fviz_cancellation_token_create(FVizCancellationToken** out_token);
FVIZ_PARALLEL_API void fviz_cancellation_token_destroy(FVizCancellationToken* token);
FVIZ_PARALLEL_API void fviz_cancellation_token_cancel(FVizCancellationToken* token);
FVIZ_PARALLEL_API void fviz_cancellation_token_reset(FVizCancellationToken* token);
FVIZ_PARALLEL_API FVizBool fviz_cancellation_token_is_cancelled(const FVizCancellationToken* token);

FVIZ_PARALLEL_API FVizResult fviz_parallel_context_for(FVizParallelContext* context, FVizSize begin, FVizSize end,
                                              FVizSize grain_size, FVizParallelRangeResultFn function, void* user_data,
                                              FVizCancellationToken* cancellation);

FVIZ_PARALLEL_API FVizResult fviz_task_group_create(FVizParallelContext* context, FVizCancellationToken* cancellation,
                                           FVizTaskGroup** out_group);
FVIZ_PARALLEL_API void fviz_task_group_destroy(FVizTaskGroup* group);
/* Queued task ranges are independent work items and may execute concurrently
 * during wait(). Callers must partition writes or provide their own synchronization
 * when multiple tasks access the same mutable memory. */
FVIZ_PARALLEL_API FVizResult fviz_task_group_run(FVizTaskGroup* group, FVizSize begin, FVizSize end, FVizSize grain_size,
                                        FVizParallelRangeResultFn function, void* user_data);
FVIZ_PARALLEL_API FVizResult fviz_task_group_wait(FVizTaskGroup* group);

FVIZ_PARALLEL_API FVizResult fviz_parallel_sum_f64(FVizParallelContext* context, const double* values, FVizSize count,
                                          double* out_sum, FVizCancellationToken* cancellation);
/* input and output may alias for in-place scans. */
FVIZ_PARALLEL_API FVizResult fviz_parallel_exclusive_scan_u64(FVizParallelContext* context, const uint64_t* input,
                                                     uint64_t* output, FVizSize count, uint64_t initial_value,
                                                     uint64_t* out_total);
FVIZ_PARALLEL_API FVizResult fviz_parallel_inclusive_scan_u64(FVizParallelContext* context, const uint64_t* input,
                                                     uint64_t* output, FVizSize count, uint64_t* out_total);
FVIZ_PARALLEL_API FVizResult fviz_parallel_stable_sort_u64_indices(FVizParallelContext* context, const uint64_t* keys,
                                                          FVizSize* indices, FVizSize count);

/* Callback-local scratch is reset before every scheduled chunk. */
FVIZ_PARALLEL_API void* fviz_parallel_scratch_allocate(FVizSize size, FVizSize alignment);
FVIZ_PARALLEL_API FVizSize fviz_parallel_scratch_capacity(void);

FVIZ_PARALLEL_API uint32_t fviz_parallel_hardware_thread_count(void);
FVIZ_PARALLEL_API void fviz_parallel_set_thread_limit(uint32_t thread_limit);
FVIZ_PARALLEL_API uint32_t fviz_parallel_thread_limit(void);
FVIZ_PARALLEL_API uint32_t fviz_parallel_worker_count(void);
FVIZ_PARALLEL_API uint64_t fviz_parallel_dispatch_count(void);
FVIZ_PARALLEL_API FVizResult fviz_parallel_for(FVizSize begin, FVizSize end, FVizSize grain_size, FVizParallelRangeFn function,
                                      void* user_data);

FVIZ_EXTERN_C_END

#endif /* FVIZ_PARALLEL_PARALLEL_H */
