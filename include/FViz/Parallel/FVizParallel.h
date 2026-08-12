#ifndef FVIZ_PARALLEL_PARALLEL_H
#define FVIZ_PARALLEL_PARALLEL_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef void (*FVizParallelRangeFn)(FVizSize begin, FVizSize end, void* user_data);

FVIZ_API uint32_t fviz_parallel_hardware_thread_count(void);
FVIZ_API void fviz_parallel_set_thread_limit(uint32_t thread_limit);
FVIZ_API uint32_t fviz_parallel_thread_limit(void);
FVIZ_API uint32_t fviz_parallel_worker_count(void);
FVIZ_API uint64_t fviz_parallel_dispatch_count(void);
FVIZ_API FVizResult fviz_parallel_for(
    FVizSize begin,
    FVizSize end,
    FVizSize grain_size,
    FVizParallelRangeFn function,
    void* user_data);

FVIZ_EXTERN_C_END

#endif /* FVIZ_PARALLEL_PARALLEL_H */
