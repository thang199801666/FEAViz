#include <stdio.h>
#include <stdlib.h>

#include <FViz/FViz.h>

typedef struct TrackingAllocatorState
{
    FVizAllocator backing;
    FVizSize allocation_count;
    FVizSize deallocation_count;
    FVizSize active_allocations;
} TrackingAllocatorState;

static int require_true(int condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "FAILED: %s\n", message);
        return 0;
    }
    return 1;
}

static void* tracking_allocate(void* user_data, FVizSize size, FVizSize alignment)
{
    TrackingAllocatorState* state = (TrackingAllocatorState*)user_data;
    void* memory = state->backing.allocate(state->backing.user_data, size, alignment);
    if (memory != NULL)
    {
        state->allocation_count += 1u;
        state->active_allocations += 1u;
    }
    return memory;
}

static void* tracking_reallocate(
    void* user_data,
    void* memory,
    FVizSize old_size,
    FVizSize new_size,
    FVizSize alignment)
{
    TrackingAllocatorState* state = (TrackingAllocatorState*)user_data;
    void* result = state->backing.reallocate(
        state->backing.user_data,
        memory,
        old_size,
        new_size,
        alignment);

    if (memory == NULL && result != NULL)
    {
        state->allocation_count += 1u;
        state->active_allocations += 1u;
    }
    else if (memory != NULL && new_size == 0u)
    {
        state->deallocation_count += 1u;
        state->active_allocations -= 1u;
    }
    return result;
}

static void tracking_deallocate(
    void* user_data,
    void* memory,
    FVizSize size,
    FVizSize alignment)
{
    TrackingAllocatorState* state = (TrackingAllocatorState*)user_data;
    if (memory != NULL)
    {
        state->deallocation_count += 1u;
        state->active_allocations -= 1u;
    }
    state->backing.deallocate(state->backing.user_data, memory, size, alignment);
}

int main(void)
{
    TrackingAllocatorState state;
    FVizAllocator allocator;
    FVizObject* object = NULL;
    FVizMTime initial_mtime;
    FVizMTime modified_mtime;
    FVizSize i;

    state.backing = fviz_allocator_default();
    state.allocation_count = 0u;
    state.deallocation_count = 0u;
    state.active_allocations = 0u;

    allocator.allocate = tracking_allocate;
    allocator.reallocate = tracking_reallocate;
    allocator.deallocate = tracking_deallocate;
    allocator.user_data = &state;

    if (!require_true(fviz_allocator_is_valid(&allocator) == FVIZ_TRUE, "tracking allocator should be valid")) return 1;
    if (!require_true(fviz_object_create_with_allocator(&allocator, &object) == FVIZ_OK, "object creation failed")) return 1;
    if (!require_true(object != NULL, "object is NULL")) return 1;
    if (!require_true(state.active_allocations == 1u, "object allocation was not tracked")) return 1;
    if (!require_true(fviz_type_id_from_name("FVizObject") == FVIZ_TYPE_OBJECT, "stable type-id hash mismatch")) return 1;
    if (!require_true(fviz_object_type_id(object) == FVIZ_TYPE_OBJECT, "object type id mismatch")) return 1;
    if (!require_true(fviz_object_is_type(object, FVIZ_TYPE_OBJECT) == FVIZ_TRUE, "object type query failed")) return 1;
    if (!require_true(fviz_object_type_name(object) != NULL, "object type name is NULL")) return 1;
    if (!require_true(fviz_object_ref_count(object) == 1u, "initial ref count should be one")) return 1;
    initial_mtime = fviz_object_mtime(object);
    if (!require_true(initial_mtime != 0u, "initial modification time should be non-zero")) return 1;
    fviz_object_modified(object);
    modified_mtime = fviz_object_mtime(object);
    if (!require_true(modified_mtime > initial_mtime, "Modified should advance modification time")) return 1;
    if (!require_true(fviz_object_mtime(NULL) == 0u, "NULL modification time should be zero")) return 1;

    for (i = 0u; i < 100000u; ++i)
    {
        if (!require_true(fviz_retain(object) == object, "retain failed")) return 1;
    }
    if (!require_true(fviz_object_ref_count(object) == 100001u, "ref count after retain stress mismatch")) return 1;

    for (i = 0u; i < 100000u; ++i)
    {
        fviz_release(object);
    }
    if (!require_true(fviz_object_ref_count(object) == 1u, "ref count after release stress mismatch")) return 1;
    if (!require_true(fviz_object_mtime(object) == modified_mtime, "retain/release should not modify MTime")) return 1;

    fviz_release(object);
    object = NULL;

    if (!require_true(state.active_allocations == 0u, "object allocator leaked memory")) return 1;
    if (!require_true(state.allocation_count == state.deallocation_count, "allocation/deallocation counts differ")) return 1;

    return 0;
}
