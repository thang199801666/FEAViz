#ifndef FVIZ_CORE_ARENA_H
#define FVIZ_CORE_ARENA_H

#include <FViz/Core/FVizAllocator.h>
#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizArena FVizArena;

typedef struct FVizArenaStatistics
{
    FVizSize block_count;
    FVizSize reserved_bytes;
    FVizSize used_bytes;
    FVizSize peak_used_bytes;
    uint64_t allocation_count;
} FVizArenaStatistics;

/* A transient monotonic allocator intended for filter/pipeline scratch data.
 * The arena is not internally synchronized; use one arena per worker/context. */
FVIZ_API FVizResult fviz_arena_create(FVizSize block_size, FVizArena** out_arena);
FVIZ_API void fviz_arena_destroy(FVizArena* arena);
FVIZ_API void* fviz_arena_allocate(FVizArena* arena, FVizSize size, FVizSize alignment);
/* Rewinds every retained block. Existing pointers become invalid for subsequent use. */
FVIZ_API void fviz_arena_reset(FVizArena* arena);
/* Releases spare blocks while retaining at most the first reusable block. */
FVIZ_API void fviz_arena_trim(FVizArena* arena);
FVIZ_API FVizArenaStatistics fviz_arena_statistics(const FVizArena* arena);
/* Adapter for APIs accepting FVizAllocator. deallocate() is intentionally a no-op;
 * reallocate() performs allocate+copy. The adapter is valid only while arena lives. */
FVIZ_API FVizAllocator fviz_arena_allocator(FVizArena* arena);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_ARENA_H */
