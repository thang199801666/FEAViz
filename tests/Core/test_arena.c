#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <FViz/FViz.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while(0)

int main(void)
{
    FVizArena* arena = NULL;
    FVizArenaStatistics stats;
    FVizAllocator allocator;
    unsigned char* a;
    uint64_t* b;
    unsigned char* large;
    void* grown;
    CHECK(fviz_arena_create(128u, &arena) == FVIZ_OK);
    CHECK(arena != NULL);
    a = (unsigned char*)fviz_arena_allocate(arena, 31u, 16u);
    b = (uint64_t*)fviz_arena_allocate(arena, 4u * sizeof(uint64_t), 32u);
    CHECK(a != NULL && b != NULL);
    CHECK(((uintptr_t)a & 15u) == 0u && ((uintptr_t)b & 31u) == 0u);
    memset(a, 0x5a, 31u);
    large = (unsigned char*)fviz_arena_allocate(arena, 512u, 64u);
    CHECK(large != NULL && ((uintptr_t)large & 63u) == 0u);
    stats = fviz_arena_statistics(arena);
    CHECK(stats.block_count >= 2u && stats.used_bytes == 31u + 4u * sizeof(uint64_t) + 512u);
    CHECK(stats.peak_used_bytes == stats.used_bytes && stats.allocation_count == 3u);

    allocator = fviz_arena_allocator(arena);
    CHECK(fviz_allocator_is_valid(&allocator) == FVIZ_TRUE);
    grown = fviz_allocator_reallocate(&allocator, a, 31u, 64u, 16u);
    CHECK(grown != NULL && memcmp(grown, a, 31u) == 0);

    fviz_arena_reset(arena);
    stats = fviz_arena_statistics(arena);
    CHECK(stats.used_bytes == 0u && stats.block_count >= 2u && stats.peak_used_bytes > 0u);
    CHECK(fviz_arena_allocate(arena, 32u, 8u) != NULL);
    fviz_arena_trim(arena);
    stats = fviz_arena_statistics(arena);
    CHECK(stats.block_count == 1u && stats.used_bytes == 0u);
    CHECK(fviz_arena_allocate(arena, 1u, 3u) == NULL);
    fviz_arena_destroy(arena);
    return 0;
}
