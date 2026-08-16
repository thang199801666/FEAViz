#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizArena.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>

#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_ARENA_DEFAULT_BLOCK_SIZE ((FVizSize)65536u)

typedef struct FVizArenaBlock
{
    struct FVizArenaBlock* next;
    FVizSize storage_bytes;
    FVizSize used;
} FVizArenaBlock;

struct FVizArena
{
    FVizArenaBlock* first;
    FVizArenaBlock* last;
    FVizArenaBlock* current;
    FVizSize block_size;
    FVizSize block_count;
    FVizSize reserved_bytes;
    FVizSize used_bytes;
    FVizSize peak_used_bytes;
    uint64_t allocation_count;
};

static FVizBool fviz_arena_power_of_two(FVizSize value)
{
    return value != 0u && (value & (value - 1u)) == 0u ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_arena_new_block(FVizArena* arena, FVizSize minimum_payload, FVizSize alignment,
                                       FVizArenaBlock** out_block)
{
    FVizSize storage = arena->block_size;
    FVizSize padding = alignment > 1u ? alignment - 1u : 0u;
    FVizSize required;
    FVizSize total;
    FVizArenaBlock* block;
    if (fviz_size_add(minimum_payload, padding, &required) != FVIZ_OK) return fviz_last_error_code();
    if (storage < required) storage = required;
    if (fviz_size_add(sizeof(*block), storage, &total) != FVIZ_OK) return fviz_last_error_code();
    block = (FVizArenaBlock*)fviz_alloc(total);
    if (block == NULL) return fviz_last_error_code();
    block->next = NULL;
    block->storage_bytes = storage;
    block->used = 0u;
    if (arena->first == NULL) arena->first = block;
    else
        arena->last->next = block;
    arena->last = block;
    arena->current = block;
    ++arena->block_count;
    arena->reserved_bytes += storage;
    *out_block = block;
    return FVIZ_OK;
}

FVizResult fviz_arena_create(FVizSize block_size, FVizArena** out_arena)
{
    FVizArena* arena;
    if (out_arena == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_arena must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_arena = NULL;
    if (block_size == 0u) block_size = FVIZ_ARENA_DEFAULT_BLOCK_SIZE;
    arena = (FVizArena*)fviz_alloc(sizeof(*arena));
    if (arena == NULL) return fviz_last_error_code();
    (void)memset(arena, 0, sizeof(*arena));
    arena->block_size = block_size;
    *out_arena = arena;
    return FVIZ_OK;
}

void fviz_arena_destroy(FVizArena* arena)
{
    FVizArenaBlock* block;
    if (arena == NULL) return;
    block = arena->first;
    while (block != NULL)
    {
        FVizArenaBlock* next = block->next;
        fviz_free(block);
        block = next;
    }
    fviz_free(arena);
}

static void* fviz_arena_try_allocate(FVizArenaBlock* block, FVizSize size, FVizSize alignment)
{
    const uintptr_t base = (uintptr_t)(block + 1);
    const uintptr_t cursor = base + (uintptr_t)block->used;
    const uintptr_t aligned = (cursor + (uintptr_t)alignment - 1u) & ~((uintptr_t)alignment - 1u);
    const FVizSize offset = (FVizSize)(aligned - base);
    if (offset > block->storage_bytes || size > block->storage_bytes - offset) return NULL;
    block->used = offset + size;
    return (void*)aligned;
}

void* fviz_arena_allocate(FVizArena* arena, FVizSize size, FVizSize alignment)
{
    FVizArenaBlock* block;
    void* memory;
    if (arena == NULL || size == 0u)
    {
        if (arena == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "arena must not be NULL");
        return NULL;
    }
    if (alignment == 0u) alignment = sizeof(void*);
    if (fviz_arena_power_of_two(alignment) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "arena alignment must be a power of two");
        return NULL;
    }
    if (arena->used_bytes > (FVizSize)-1 - size)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "arena usage counter overflow");
        return NULL;
    }
    block = arena->current != NULL ? arena->current : arena->first;
    while (block != NULL)
    {
        memory = fviz_arena_try_allocate(block, size, alignment);
        if (memory != NULL) goto allocated;
        block = block->next;
        arena->current = block;
    }
    if (fviz_arena_new_block(arena, size, alignment, &block) != FVIZ_OK) return NULL;
    memory = fviz_arena_try_allocate(block, size, alignment);
    if (memory == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "new arena block could not satisfy allocation");
        return NULL;
    }
allocated:
    arena->used_bytes += size;
    if (arena->used_bytes > arena->peak_used_bytes) arena->peak_used_bytes = arena->used_bytes;
    ++arena->allocation_count;
    arena->current = block;
    return memory;
}

void fviz_arena_reset(FVizArena* arena)
{
    FVizArenaBlock* block;
    if (arena == NULL) return;
    for (block = arena->first; block != NULL; block = block->next)
        block->used = 0u;
    arena->current = arena->first;
    arena->used_bytes = 0u;
}

void fviz_arena_trim(FVizArena* arena)
{
    FVizArenaBlock* block;
    if (arena == NULL || arena->first == NULL) return;
    block = arena->first->next;
    arena->first->next = NULL;
    arena->last = arena->first;
    while (block != NULL)
    {
        FVizArenaBlock* next = block->next;
        arena->reserved_bytes -= block->storage_bytes;
        --arena->block_count;
        fviz_free(block);
        block = next;
    }
    arena->first->used = 0u;
    arena->current = arena->first;
    arena->used_bytes = 0u;
}

FVizArenaStatistics fviz_arena_statistics(const FVizArena* arena)
{
    FVizArenaStatistics statistics;
    (void)memset(&statistics, 0, sizeof(statistics));
    if (arena != NULL)
    {
        statistics.block_count = arena->block_count;
        statistics.reserved_bytes = arena->reserved_bytes;
        statistics.used_bytes = arena->used_bytes;
        statistics.peak_used_bytes = arena->peak_used_bytes;
        statistics.allocation_count = arena->allocation_count;
    }
    return statistics;
}

static void* fviz_arena_allocator_allocate(void* user_data, FVizSize size, FVizSize alignment)
{
    return fviz_arena_allocate((FVizArena*)user_data, size, alignment);
}

static void* fviz_arena_allocator_reallocate(void* user_data, void* memory, FVizSize old_size, FVizSize new_size,
                                             FVizSize alignment)
{
    void* replacement;
    if (memory == NULL) return fviz_arena_allocate((FVizArena*)user_data, new_size, alignment);
    if (new_size == 0u) return NULL;
    replacement = fviz_arena_allocate((FVizArena*)user_data, new_size, alignment);
    if (replacement != NULL) (void)memcpy(replacement, memory, old_size < new_size ? old_size : new_size);
    return replacement;
}

static void fviz_arena_allocator_deallocate(void* user_data, void* memory, FVizSize size, FVizSize alignment)
{
    (void)user_data;
    (void)memory;
    (void)size;
    (void)alignment;
}

FVizAllocator fviz_arena_allocator(FVizArena* arena)
{
    FVizAllocator allocator;
    allocator.allocate = fviz_arena_allocator_allocate;
    allocator.reallocate = fviz_arena_allocator_reallocate;
    allocator.deallocate = fviz_arena_allocator_deallocate;
    allocator.user_data = arena;
    return allocator;
}
