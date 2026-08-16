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


typedef struct ObserverState
{
    int calls[16];
    int call_count;
    int modified_count;
    int delete_count;
    FVizObserverTag target_tag;
    FVizObserverTag added_tag;
} ObserverState;

static FVizBool observer_high(FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    ObserverState* state = (ObserverState*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    state->calls[state->call_count++] = 1;
    return FVIZ_FALSE;
}

static FVizBool observer_low(FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    ObserverState* state = (ObserverState*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    state->calls[state->call_count++] = 2;
    return FVIZ_FALSE;
}

static FVizBool observer_abort(FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    ObserverState* state = (ObserverState*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    state->calls[state->call_count++] = 3;
    return FVIZ_TRUE;
}

static FVizBool observer_modified(FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    ObserverState* state = (ObserverState*)client_data;
    (void)caller;
    (void)call_data;
    if (event_id == FVIZ_EVENT_MODIFIED) state->modified_count += 1;
    return FVIZ_FALSE;
}

static FVizBool observer_delete(FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    ObserverState* state = (ObserverState*)client_data;
    (void)caller;
    (void)call_data;
    if (event_id == FVIZ_EVENT_DELETE) state->delete_count += 1;
    return FVIZ_FALSE;
}


static FVizBool observer_remove_target(FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    ObserverState* state = (ObserverState*)client_data;
    (void)event_id;
    (void)call_data;
    state->calls[state->call_count++] = 4;
    if (state->target_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer(caller, state->target_tag);
    return FVIZ_FALSE;
}

static FVizBool observer_add_late(FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    ObserverState* state = (ObserverState*)client_data;
    (void)event_id;
    (void)call_data;
    state->calls[state->call_count++] = 5;
    if (state->added_tag == FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_add_observer(caller, FVIZ_EVENT_USER + 2u, -10.0f,
            observer_low, state, &state->added_tag);
    return FVIZ_FALSE;
}

static FVizBool observer_interaction_any(FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    ObserverState* state = (ObserverState*)client_data;
    (void)caller;
    (void)call_data;
    if (event_id == FVIZ_EVENT_MOUSE_MOVE) state->calls[state->call_count++] = 6;
    return FVIZ_FALSE;
}

static FVizBool reusable_command_execute(
    FVizCommand* command,
    FVizObject* caller,
    FVizEventId event_id,
    void* call_data,
    void* client_data)
{
    ObserverState* state = (ObserverState*)client_data;
    (void)caller;
    (void)call_data;
    if (event_id == FVIZ_EVENT_USER + 3u)
    {
        state->calls[state->call_count++] = 7;
        fviz_command_set_abort_flag(command, FVIZ_TRUE);
    }
    return FVIZ_FALSE;
}

int main(void)
{
    TrackingAllocatorState state;
    FVizAllocator allocator;
    FVizObject* object = NULL;
    FVizMTime initial_mtime;
    FVizMTime modified_mtime;
    FVizSize i;
    ObserverState observer_state = {{0}, 0, 0, 0, FVIZ_OBSERVER_TAG_INVALID, FVIZ_OBSERVER_TAG_INVALID};
    FVizObserverTag high_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag low_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag abort_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag delete_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag remover_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag adder_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag interaction_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag command_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag command_low_tag = FVIZ_OBSERVER_TAG_INVALID;
    FVizCommand* command = NULL;

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

    if (!require_true(fviz_event_name(FVIZ_EVENT_MODIFIED) != NULL, "event name should be available")) return 1;
    if (!require_true(fviz_interaction_event_id(FVIZ_INTERACTION_MOUSE_MOVE) == FVIZ_EVENT_MOUSE_MOVE,
            "interaction-to-object event mapping mismatch")) return 1;
    if (!require_true(fviz_interaction_event_id(FVIZ_INTERACTION_EVENT_ANY) == FVIZ_EVENT_INTERACTION_ANY,
            "none interaction event should not map to a concrete object event")) return 1;
    if (!require_true(fviz_event_name(FVIZ_EVENT_WINDOW_DPI_CHANGED) != NULL,
            "window DPI event name should be available")) return 1;
    if (!require_true(fviz_object_add_observer(object, FVIZ_EVENT_USER, 10.0f, observer_high,
            &observer_state, &high_tag) == FVIZ_OK, "high-priority observer registration failed")) return 1;
    if (!require_true(fviz_object_add_observer(object, FVIZ_EVENT_USER, -2.0f, observer_low,
            &observer_state, &low_tag) == FVIZ_OK, "low-priority observer registration failed")) return 1;
    if (!require_true(fviz_object_observer_count(object) == 2u, "observer count mismatch")) return 1;
    if (!require_true(fviz_object_invoke_event(object, FVIZ_EVENT_USER, NULL) == FVIZ_FALSE,
            "non-aborting event should return false")) return 1;
    if (!require_true(observer_state.call_count == 2 && observer_state.calls[0] == 1 &&
            observer_state.calls[1] == 2, "observer priority/order mismatch")) return 1;

    observer_state.call_count = 0;
    if (!require_true(fviz_object_add_observer(object, FVIZ_EVENT_USER, 100.0f, observer_abort,
            &observer_state, &abort_tag) == FVIZ_OK, "abort observer registration failed")) return 1;
    if (!require_true(fviz_object_invoke_event(object, FVIZ_EVENT_USER, NULL) == FVIZ_TRUE,
            "aborted event should return true")) return 1;
    if (!require_true(observer_state.call_count == 1 && observer_state.calls[0] == 3,
            "abort should stop lower-priority observers")) return 1;
    if (!require_true(fviz_object_remove_observer(object, abort_tag) == FVIZ_OK,
            "abort observer removal failed")) return 1;

    if (!require_true(fviz_object_add_observer(object, FVIZ_EVENT_MODIFIED, 0.0f, observer_modified,
            &observer_state, &modified_tag) == FVIZ_OK, "ModifiedEvent observer registration failed")) return 1;
    fviz_object_modified(object);
    if (!require_true(observer_state.modified_count == 1, "Modified() should invoke ModifiedEvent")) return 1;
    modified_mtime = fviz_object_mtime(object);

    observer_state.call_count = 0;
    if (!require_true(fviz_object_add_observer(object, FVIZ_EVENT_USER + 1u, -20.0f, observer_low,
            &observer_state, &observer_state.target_tag) == FVIZ_OK, "mutation target registration failed")) return 1;
    if (!require_true(fviz_object_add_observer(object, FVIZ_EVENT_USER + 1u, 20.0f, observer_remove_target,
            &observer_state, &remover_tag) == FVIZ_OK, "remover registration failed")) return 1;
    (void)fviz_object_invoke_event(object, FVIZ_EVENT_USER + 1u, NULL);
    if (!require_true(observer_state.call_count == 1 && observer_state.calls[0] == 4,
            "removal during dispatch should suppress not-yet-run observer")) return 1;
    if (!require_true(fviz_object_remove_observer(object, remover_tag) == FVIZ_OK,
            "remover cleanup failed")) return 1;

    observer_state.call_count = 0;
    observer_state.added_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (!require_true(fviz_object_add_observer(object, FVIZ_EVENT_USER + 2u, 20.0f, observer_add_late,
            &observer_state, &adder_tag) == FVIZ_OK, "adder registration failed")) return 1;
    (void)fviz_object_invoke_event(object, FVIZ_EVENT_USER + 2u, NULL);
    if (!require_true(observer_state.call_count == 1 && observer_state.calls[0] == 5,
            "observer added during dispatch must wait until next dispatch")) return 1;
    observer_state.call_count = 0;
    (void)fviz_object_invoke_event(object, FVIZ_EVENT_USER + 2u, NULL);
    if (!require_true(observer_state.call_count == 2 && observer_state.calls[0] == 5 && observer_state.calls[1] == 2,
            "late observer should participate in next dispatch")) return 1;
    (void)fviz_object_remove_observers(object, FVIZ_EVENT_USER + 2u);

    observer_state.call_count = 0;
    if (!require_true(fviz_object_add_observer(object, FVIZ_EVENT_INTERACTION_ANY, 0.0f,
            observer_interaction_any, &observer_state, &interaction_tag) == FVIZ_OK,
            "interaction category observer registration failed")) return 1;
    if (!require_true(fviz_object_has_observer(object, FVIZ_EVENT_MOUSE_MOVE) == FVIZ_TRUE,
            "interaction category should satisfy concrete has-observer query")) return 1;
    (void)fviz_object_invoke_event(object, FVIZ_EVENT_MOUSE_MOVE, NULL);
    if (!require_true(observer_state.call_count == 1 && observer_state.calls[0] == 6,
            "InteractionEvent category should observe concrete interaction events")) return 1;
    if (!require_true(fviz_object_remove_observer(object, interaction_tag) == FVIZ_OK,
            "interaction category observer cleanup failed")) return 1;

    observer_state.call_count = 0;
    if (!require_true(fviz_command_create(reusable_command_execute, &observer_state, &command) == FVIZ_OK,
            "command creation failed")) return 1;
    if (!require_true(fviz_object_is_type((const FVizObject*)command, FVIZ_TYPE_COMMAND) == FVIZ_TRUE,
            "command should be an FVIZ_TYPE_COMMAND object")) return 1;
    if (!require_true(fviz_object_add_command_observer(object, FVIZ_EVENT_USER + 3u, 20.0f,
            command, &command_tag) == FVIZ_OK, "command observer registration failed")) return 1;
    if (!require_true(fviz_object_ref_count((const FVizObject*)command) == 2u,
            "observer should retain reusable command")) return 1;
    if (!require_true(fviz_object_add_observer(object, FVIZ_EVENT_USER + 3u, -20.0f, observer_low,
            &observer_state, &command_low_tag) == FVIZ_OK, "command trailing observer failed")) return 1;
    if (!require_true(fviz_object_invoke_event(object, FVIZ_EVENT_USER + 3u, NULL) == FVIZ_TRUE,
            "command abort flag should abort observer propagation")) return 1;
    if (!require_true(observer_state.call_count == 1 && observer_state.calls[0] == 7,
            "reusable command should run before and suppress lower priority observer")) return 1;
    if (!require_true(fviz_command_abort_flag(command) == FVIZ_TRUE,
            "command abort flag should remain visible after Execute")) return 1;
    if (!require_true(fviz_object_remove_observer(object, command_tag) == FVIZ_OK,
            "command observer removal failed")) return 1;
    if (!require_true(fviz_object_ref_count((const FVizObject*)command) == 1u,
            "removing command observer should release retained command")) return 1;
    if (!require_true(fviz_object_remove_observer(object, command_low_tag) == FVIZ_OK,
            "command trailing observer cleanup failed")) return 1;
    fviz_release(command);
    command = NULL;

    if (!require_true(fviz_object_add_observer(object, FVIZ_EVENT_DELETE, 0.0f, observer_delete,
            &observer_state, &delete_tag) == FVIZ_OK, "DeleteEvent observer registration failed")) return 1;
    if (!require_true(fviz_object_has_observer(object, FVIZ_EVENT_DELETE) == FVIZ_TRUE,
            "DeleteEvent observer should be discoverable")) return 1;
    if (!require_true(fviz_object_remove_observer(object, low_tag) == FVIZ_OK,
            "low-priority observer removal failed")) return 1;
    if (!require_true(fviz_object_remove_observers(object, FVIZ_EVENT_USER) == 1u,
            "event-specific observer removal mismatch")) return 1;

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

    if (!require_true(observer_state.delete_count == 1, "final release should invoke DeleteEvent")) return 1;
    if (!require_true(state.active_allocations == 0u, "object allocator leaked memory")) return 1;
    if (!require_true(state.allocation_count == state.deallocation_count, "allocation/deallocation counts differ")) return 1;

    return 0;
}
