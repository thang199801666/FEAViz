#include <stddef.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizCommand.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizObject.h>

#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizCompiler.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

#define FVIZ_OBJECT_MAGIC UINT32_C(0x46564F42)

typedef struct FVizObjectObserver
{
    FVizObserverTag tag;
    FVizEventId event_id;
    float priority;
    uint64_t sequence;
    uint64_t activation_dispatch;
    FVizObserverCallbackFn callback;
    void* client_data;
    FVizCommand* command;
    FVizBool active;
} FVizObjectObserver;

struct FVizObserverList
{
    FVizObjectObserver* observers;
    FVizSize count;
    FVizSize capacity;
    FVizObserverTag next_tag;
    uint64_t next_sequence;
    uint64_t dispatch_serial;
    uint32_t dispatch_depth;
    FVizBool needs_compaction;
    FVizBool needs_sort;
};


static void fviz_base_object_destroy(FVizObject* object);
static void fviz_object_observer_list_destroy(FVizObject* object);

static FVizObserverList* fviz_object_observer_list_ensure(FVizObject* object)
{
    FVizObserverList* list;
    if (object->observer_list != NULL) return object->observer_list;
    list = (FVizObserverList*)fviz_allocator_allocate(
        &object->allocator, sizeof(FVizObserverList), (FVizSize)FVIZ_INTERNAL_MAX_ALIGNMENT);
    if (list == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_OUT_OF_MEMORY, "failed to allocate observer list");
        return NULL;
    }
    (void)memset(list, 0, sizeof(*list));
    list->next_tag = 1u;
    list->next_sequence = 1u;
    object->observer_list = list;
    return list;
}

static FVizResult fviz_object_observer_reserve(FVizObject* object, FVizObserverList* list, FVizSize required)
{
    FVizSize capacity;
    FVizSize bytes;
    FVizObjectObserver* observers;
    if (required <= list->capacity) return FVIZ_OK;
    capacity = list->capacity == 0u ? 1u : list->capacity;
    while (capacity < required)
    {
        if (capacity > SIZE_MAX / 2u)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "observer capacity overflow");
            return FVIZ_ERROR_OVERFLOW;
        }
        capacity *= 2u;
    }
    if (capacity > SIZE_MAX / sizeof(FVizObjectObserver))
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "observer allocation size overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    bytes = capacity * sizeof(FVizObjectObserver);
    observers = (FVizObjectObserver*)fviz_allocator_reallocate(
        &object->allocator, list->observers, list->capacity * sizeof(FVizObjectObserver),
        bytes, (FVizSize)FVIZ_INTERNAL_MAX_ALIGNMENT);
    if (observers == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_OUT_OF_MEMORY, "failed to grow observer list");
        return FVIZ_ERROR_OUT_OF_MEMORY;
    }
    list->observers = observers;
    list->capacity = capacity;
    return FVIZ_OK;
}

static FVizBool fviz_object_observer_precedes(
    const FVizObjectObserver* left, const FVizObjectObserver* right)
{
    if (left->priority != right->priority)
        return left->priority > right->priority ? FVIZ_TRUE : FVIZ_FALSE;
    return left->sequence < right->sequence ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_object_observer_matches(FVizEventId observer_event, FVizEventId event_id)
{
    if (observer_event == FVIZ_EVENT_ANY || observer_event == event_id) return FVIZ_TRUE;
    if (observer_event == FVIZ_EVENT_INTERACTION_ANY &&
        event_id >= FVIZ_EVENT_MOUSE_BUTTON_DOWN && event_id <= FVIZ_EVENT_CHAR)
        return FVIZ_TRUE;
    return FVIZ_FALSE;
}

static FVizBool fviz_object_command_observer_callback(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    return fviz_command_execute((FVizCommand*)client_data, caller, event_id, call_data);
}

static void fviz_object_observers_maintain(FVizObject* object)
{
    FVizObserverList* list = object != NULL ? object->observer_list : NULL;
    FVizSize source;
    FVizSize destination = 0u;
    if (list == NULL || list->dispatch_depth != 0u) return;
    if (list->needs_compaction != FVIZ_FALSE)
    {
        for (source = 0u; source < list->count; ++source)
        {
            if (list->observers[source].active != FVIZ_FALSE)
            {
                if (source != destination) list->observers[destination] = list->observers[source];
                ++destination;
            }
            else if (list->observers[source].command != NULL)
            {
                fviz_release(list->observers[source].command);
                list->observers[source].command = NULL;
            }
        }
        list->count = destination;
        list->needs_compaction = FVIZ_FALSE;
        list->needs_sort = FVIZ_TRUE;
    }
    if (list->needs_sort != FVIZ_FALSE)
    {
        for (source = 1u; source < list->count; ++source)
        {
            const FVizObjectObserver value = list->observers[source];
            FVizSize position = source;
            while (position > 0u &&
                fviz_object_observer_precedes(&value, &list->observers[position - 1u]) != FVIZ_FALSE)
            {
                list->observers[position] = list->observers[position - 1u];
                --position;
            }
            list->observers[position] = value;
        }
        list->needs_sort = FVIZ_FALSE;
    }
}

static void fviz_object_observer_list_destroy(FVizObject* object)
{
    FVizObserverList* list;
    if (object == NULL || object->observer_list == NULL) return;
    list = object->observer_list;
    if (list->observers != NULL)
    {
        FVizSize i;
        for (i = 0u; i < list->count; ++i)
            if (list->observers[i].command != NULL)
                fviz_release(list->observers[i].command);
        fviz_allocator_deallocate(
            &object->allocator, list->observers, list->capacity * sizeof(FVizObjectObserver),
            (FVizSize)FVIZ_INTERNAL_MAX_ALIGNMENT);
    }
    fviz_allocator_deallocate(
        &object->allocator, list, sizeof(FVizObserverList),
        (FVizSize)FVIZ_INTERNAL_MAX_ALIGNMENT);
    object->observer_list = NULL;
}

static FVizAtomicU64 g_fviz_next_mtime = {0};

const FVizObjectClass g_fviz_object_class = {
    FVIZ_TYPE_OBJECT,
    "FVizObject",
    NULL,
    fviz_base_object_destroy,
    NULL
};

static FVizMTime fviz_object_next_mtime(void)
{
    return fviz_atomic_u64_fetch_add(&g_fviz_next_mtime, 1u) + 1u;
}

static FVizBool fviz_object_is_valid(const FVizObject* object)
{
    return (object != NULL && object->magic == FVIZ_OBJECT_MAGIC && object->object_class != NULL)
        ? FVIZ_TRUE
        : FVIZ_FALSE;
}

static void fviz_base_object_destroy(FVizObject* object)
{
    FVIZ_UNUSED(object);
}

const FVizObjectClass* fviz_internal_object_base_class(void)
{
    return &g_fviz_object_class;
}

FVizObject* fviz_internal_object_allocate(
    FVizSize object_size,
    const FVizObjectClass* object_class,
    const FVizAllocator* allocator)
{
    FVizAllocator selected_allocator;
    FVizObject* object;

    if (object_class == NULL || object_class->type_id == 0u || object_class->type_name == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object class is invalid");
        return NULL;
    }

    if (object_size < sizeof(FVizObject))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object allocation size is smaller than FVizObject");
        return NULL;
    }

    if (allocator == NULL)
    {
        selected_allocator = fviz_allocator_default();
    }
    else
    {
        if (fviz_allocator_is_valid(allocator) == FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object allocator is invalid");
            return NULL;
        }
        selected_allocator = *allocator;
    }

    object = (FVizObject*)fviz_allocator_allocate(
        &selected_allocator,
        object_size,
        (FVizSize)FVIZ_INTERNAL_MAX_ALIGNMENT);
    if (object == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_OUT_OF_MEMORY, "failed to allocate FEAViz object");
        return NULL;
    }

    (void)memset(object, 0, object_size);
    object->magic = FVIZ_OBJECT_MAGIC;
    object->ref_count.value = 1;
    object->mtime.value = (int64_t)fviz_object_next_mtime();
    object->object_class = object_class;
    object->allocator = selected_allocator;
    object->allocation_size = object_size;
    return object;
}


FVizTypeId fviz_type_id_from_name(const char* type_name)
{
    const unsigned char* current;
    uint64_t hash = UINT64_C(14695981039346656037);

    if (type_name == NULL || type_name[0] == '\0')
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "type_name must not be NULL or empty");
        return 0u;
    }

    current = (const unsigned char*)type_name;
    while (*current != 0u)
    {
        hash ^= (uint64_t)*current;
        hash *= UINT64_C(1099511628211);
        ++current;
    }

    if (hash == 0u)
    {
        hash = UINT64_C(1);
    }
    return hash;
}

FVizResult fviz_object_create(FVizObject** out_object)
{
    return fviz_object_create_with_allocator(NULL, out_object);
}

FVizResult fviz_object_create_with_allocator(
    const FVizAllocator* allocator,
    FVizObject** out_object)
{
    FVizObject* object;

    if (out_object == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_object must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }

    *out_object = NULL;
    object = fviz_internal_object_allocate(sizeof(FVizObject), &g_fviz_object_class, allocator);
    if (object == NULL)
    {
        return fviz_last_error_code();
    }

    *out_object = object;
    return FVIZ_OK;
}

void* fviz_retain(void* object_pointer)
{
    FVizObject* object = (FVizObject*)object_pointer;
    uint32_t expected;

    if (object == NULL)
    {
        return NULL;
    }

    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        return NULL;
    }

    expected = fviz_atomic_u32_load(&object->ref_count);
    for (;;)
    {
        if (expected == 0u)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cannot retain an object whose reference count is zero");
            return NULL;
        }

        if (expected == UINT32_MAX)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "object reference count overflow");
            return NULL;
        }

        if (fviz_atomic_u32_compare_exchange(&object->ref_count, &expected, expected + 1u))
        {
            return object_pointer;
        }
    }
}

void fviz_release(void* object_pointer)
{
    FVizObject* object = (FVizObject*)object_pointer;
    uint32_t previous;
    FVizAllocator allocator;
    FVizSize allocation_size;
    const FVizObjectClass* object_class;

    if (object == NULL)
    {
        return;
    }

    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        return;
    }

    previous = fviz_atomic_u32_fetch_sub(&object->ref_count, 1u);
    if (previous == 0u)
    {
        (void)fviz_atomic_u32_fetch_add(&object->ref_count, 1u);
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "object reference count underflow");
        return;
    }

    if (previous != 1u)
    {
        return;
    }

    allocator = object->allocator;
    allocation_size = object->allocation_size;
    object_class = object->object_class;

    (void)fviz_object_invoke_event(object, FVIZ_EVENT_DELETE, NULL);
    if (object_class->destroy != NULL)
    {
        object_class->destroy(object);
    }
    fviz_object_observer_list_destroy(object);

    object->magic = 0u;
    object->object_class = NULL;
    fviz_allocator_deallocate(
        &allocator,
        object,
        allocation_size,
        (FVizSize)FVIZ_INTERNAL_MAX_ALIGNMENT);
}

FVizTypeId fviz_object_type_id(const FVizObject* object)
{
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        }
        return 0u;
    }

    return object->object_class->type_id;
}

const char* fviz_object_type_name(const FVizObject* object)
{
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        }
        return NULL;
    }

    return object->object_class->type_name;
}

FVizBool fviz_object_is_type(const FVizObject* object, FVizTypeId type_id)
{
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        }
        return FVIZ_FALSE;
    }

    {
        const FVizObjectClass* object_class = object->object_class;
        while (object_class != NULL)
        {
            if (object_class->type_id == type_id)
            {
                return FVIZ_TRUE;
            }
            object_class = object_class->parent;
        }
    }

    return FVIZ_FALSE;
}

uint32_t fviz_object_ref_count(const FVizObject* object)
{
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        }
        return 0u;
    }

    return fviz_atomic_u32_load(&object->ref_count);
}

FVizMTime fviz_internal_object_local_mtime(const FVizObject* object)
{
    return object != NULL ? fviz_atomic_u64_load(&object->mtime) : 0u;
}

void fviz_object_modified(FVizObject* object)
{
    FVizMTime expected;
    FVizMTime modified;
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        return;
    }
    modified = fviz_object_next_mtime();
    expected = fviz_atomic_u64_load(&object->mtime);
    while (expected < modified &&
        !fviz_atomic_u64_compare_exchange(&object->mtime, &expected, modified))
    {
    }
    (void)fviz_object_invoke_event(object, FVIZ_EVENT_MODIFIED, NULL);
}

FVizMTime fviz_object_mtime(const FVizObject* object)
{
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        return 0u;
    }
    return object->object_class->get_mtime != NULL
        ? object->object_class->get_mtime(object)
        : fviz_internal_object_local_mtime(object);
}


const char* fviz_event_name(FVizEventId event_id)
{
    switch (event_id)
    {
        case FVIZ_EVENT_ANY: return "AnyEvent";
        case FVIZ_EVENT_DELETE: return "DeleteEvent";
        case FVIZ_EVENT_START: return "StartEvent";
        case FVIZ_EVENT_END: return "EndEvent";
        case FVIZ_EVENT_MODIFIED: return "ModifiedEvent";
        case FVIZ_EVENT_RENDER_START: return "RenderStartEvent";
        case FVIZ_EVENT_RENDER_END: return "RenderEndEvent";
        case FVIZ_EVENT_WINDOW_RESIZE: return "WindowResizeEvent";
        case FVIZ_EVENT_WINDOW_CLOSE: return "WindowCloseEvent";
        case FVIZ_EVENT_WINDOW_FOCUS_IN: return "WindowFocusInEvent";
        case FVIZ_EVENT_WINDOW_FOCUS_OUT: return "WindowFocusOutEvent";
        case FVIZ_EVENT_WINDOW_DPI_CHANGED: return "WindowDpiChangedEvent";
        case FVIZ_EVENT_WINDOW_REPARENTED: return "WindowReparentedEvent";
        case FVIZ_EVENT_START_INTERACTION: return "StartInteractionEvent";
        case FVIZ_EVENT_INTERACTION: return "InteractionEvent";
        case FVIZ_EVENT_END_INTERACTION: return "EndInteractionEvent";
        case FVIZ_EVENT_ENABLE: return "EnableEvent";
        case FVIZ_EVENT_DISABLE: return "DisableEvent";
        case FVIZ_EVENT_START_PICK: return "StartPickEvent";
        case FVIZ_EVENT_PICK: return "PickEvent";
        case FVIZ_EVENT_END_PICK: return "EndPickEvent";
        case FVIZ_EVENT_PROGRESS: return "ProgressEvent";
        case FVIZ_EVENT_ABORT_CHECK: return "AbortCheckEvent";
        case FVIZ_EVENT_RENDER_REQUESTED: return "RenderRequestedEvent";
        case FVIZ_EVENT_INTERACTION_ANY: return "AnyInteractionEvent";
        case FVIZ_EVENT_MOUSE_BUTTON_DOWN: return "MouseButtonDownEvent";
        case FVIZ_EVENT_MOUSE_BUTTON_UP: return "MouseButtonUpEvent";
        case FVIZ_EVENT_MOUSE_MOVE: return "MouseMoveEvent";
        case FVIZ_EVENT_MOUSE_WHEEL: return "MouseWheelEvent";
        case FVIZ_EVENT_KEY_DOWN: return "KeyDownEvent";
        case FVIZ_EVENT_KEY_UP: return "KeyUpEvent";
        case FVIZ_EVENT_RESIZE: return "ResizeEvent";
        case FVIZ_EVENT_ENTER: return "EnterEvent";
        case FVIZ_EVENT_LEAVE: return "LeaveEvent";
        case FVIZ_EVENT_EXPOSE: return "ExposeEvent";
        case FVIZ_EVENT_FOCUS_IN: return "FocusInEvent";
        case FVIZ_EVENT_FOCUS_OUT: return "FocusOutEvent";
        case FVIZ_EVENT_TIMER: return "TimerEvent";
        case FVIZ_EVENT_DOUBLE_CLICK: return "DoubleClickEvent";
        case FVIZ_EVENT_CHAR: return "CharEvent";
        default: return event_id >= FVIZ_EVENT_USER ? "UserEvent" : "UnknownEvent";
    }
}

FVizResult fviz_object_add_observer(
    FVizObject* object,
    FVizEventId event_id,
    float priority,
    FVizObserverCallbackFn callback,
    void* client_data,
    FVizObserverTag* out_tag)
{
    FVizObserverList* list;
    FVizObjectObserver* observer;
    if (out_tag != NULL) *out_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (fviz_object_is_valid(object) == FVIZ_FALSE || callback == NULL || out_tag == NULL ||
        !isfinite((double)priority))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid object observer arguments");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    list = fviz_object_observer_list_ensure(object);
    if (list == NULL) return fviz_last_error_code();
    if (list->next_tag == FVIZ_OBSERVER_TAG_INVALID)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "observer tag space exhausted");
        return FVIZ_ERROR_OVERFLOW;
    }
    if (fviz_object_observer_reserve(object, list, list->count + 1u) != FVIZ_OK)
        return fviz_last_error_code();
    observer = &list->observers[list->count++];
    (void)memset(observer, 0, sizeof(*observer));
    observer->tag = list->next_tag++;
    observer->event_id = event_id;
    observer->priority = priority;
    observer->sequence = list->next_sequence++;
    observer->activation_dispatch = list->dispatch_serial;
    observer->callback = callback;
    observer->client_data = client_data;
    observer->command = NULL;
    observer->active = FVIZ_TRUE;
    list->needs_sort = FVIZ_TRUE;
    *out_tag = observer->tag;
    fviz_object_observers_maintain(object);
    return FVIZ_OK;
}

FVizResult fviz_object_add_command_observer(
    FVizObject* object,
    FVizEventId event_id,
    float priority,
    FVizCommand* command,
    FVizObserverTag* out_tag)
{
    FVizObserverList* list;
    FVizObjectObserver* observer;
    if (out_tag != NULL) *out_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (fviz_object_is_valid(object) == FVIZ_FALSE || command == NULL || out_tag == NULL ||
        !isfinite((double)priority) ||
        fviz_object_is_type((const FVizObject*)command, FVIZ_TYPE_COMMAND) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid command observer arguments");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    list = fviz_object_observer_list_ensure(object);
    if (list == NULL) return fviz_last_error_code();
    if (list->next_tag == FVIZ_OBSERVER_TAG_INVALID)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "observer tag space exhausted");
        return FVIZ_ERROR_OVERFLOW;
    }
    if (fviz_object_observer_reserve(object, list, list->count + 1u) != FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_retain(command) == NULL) return fviz_last_error_code();
    observer = &list->observers[list->count++];
    (void)memset(observer, 0, sizeof(*observer));
    observer->tag = list->next_tag++;
    observer->event_id = event_id;
    observer->priority = priority;
    observer->sequence = list->next_sequence++;
    observer->activation_dispatch = list->dispatch_serial;
    observer->callback = fviz_object_command_observer_callback;
    observer->client_data = command;
    observer->command = command;
    observer->active = FVIZ_TRUE;
    list->needs_sort = FVIZ_TRUE;
    *out_tag = observer->tag;
    fviz_object_observers_maintain(object);
    return FVIZ_OK;
}

FVizResult fviz_object_remove_observer(FVizObject* object, FVizObserverTag tag)
{
    FVizObserverList* list;
    FVizSize i;
    if (fviz_object_is_valid(object) == FVIZ_FALSE || tag == FVIZ_OBSERVER_TAG_INVALID)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid object or observer tag");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    list = object->observer_list;
    if (list != NULL)
    {
        for (i = 0u; i < list->count; ++i)
        {
            if (list->observers[i].active != FVIZ_FALSE && list->observers[i].tag == tag)
            {
                list->observers[i].active = FVIZ_FALSE;
                list->needs_compaction = FVIZ_TRUE;
                fviz_object_observers_maintain(object);
                return FVIZ_OK;
            }
        }
    }
    fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "object observer was not found");
    return FVIZ_ERROR_NOT_FOUND;
}

FVizSize fviz_object_remove_observers(FVizObject* object, FVizEventId event_id)
{
    FVizObserverList* list;
    FVizSize i;
    FVizSize removed = 0u;
    if (fviz_object_is_valid(object) == FVIZ_FALSE) return 0u;
    list = object->observer_list;
    if (list == NULL) return 0u;
    for (i = 0u; i < list->count; ++i)
    {
        if (list->observers[i].active != FVIZ_FALSE &&
            (event_id == FVIZ_EVENT_ANY || list->observers[i].event_id == event_id))
        {
            list->observers[i].active = FVIZ_FALSE;
            ++removed;
        }
    }
    if (removed != 0u)
    {
        list->needs_compaction = FVIZ_TRUE;
        fviz_object_observers_maintain(object);
    }
    return removed;
}

void fviz_object_remove_all_observers(FVizObject* object)
{
    (void)fviz_object_remove_observers(object, FVIZ_EVENT_ANY);
}

FVizBool fviz_object_has_observer(const FVizObject* object, FVizEventId event_id)
{
    const FVizObserverList* list;
    FVizSize i;
    if (fviz_object_is_valid(object) == FVIZ_FALSE) return FVIZ_FALSE;
    list = object->observer_list;
    if (list == NULL) return FVIZ_FALSE;
    for (i = 0u; i < list->count; ++i)
    {
        if (list->observers[i].active != FVIZ_FALSE &&
            (event_id == FVIZ_EVENT_ANY ||
             fviz_object_observer_matches(list->observers[i].event_id, event_id) != FVIZ_FALSE ||
             (event_id == FVIZ_EVENT_INTERACTION_ANY &&
              list->observers[i].event_id >= FVIZ_EVENT_MOUSE_BUTTON_DOWN &&
              list->observers[i].event_id <= FVIZ_EVENT_CHAR)))
            return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

FVizSize fviz_object_observer_count(const FVizObject* object)
{
    const FVizObserverList* list;
    FVizSize i;
    FVizSize count = 0u;
    if (fviz_object_is_valid(object) == FVIZ_FALSE) return 0u;
    list = object->observer_list;
    if (list == NULL) return 0u;
    for (i = 0u; i < list->count; ++i)
        if (list->observers[i].active != FVIZ_FALSE) ++count;
    return count;
}

FVizBool fviz_object_invoke_event(FVizObject* object, FVizEventId event_id, void* call_data)
{
    FVizObserverList* list;
    FVizSize dispatch_count;
    FVizSize i;
    FVizBool aborted = FVIZ_FALSE;
    FVizBool top_level;
    if (fviz_object_is_valid(object) == FVIZ_FALSE) return FVIZ_FALSE;
    list = object->observer_list;
    if (list == NULL || list->count == 0u) return FVIZ_FALSE;
    top_level = list->dispatch_depth == 0u ? FVIZ_TRUE : FVIZ_FALSE;
    if (top_level != FVIZ_FALSE)
    {
        fviz_object_observers_maintain(object);
        ++list->dispatch_serial;
    }
    ++list->dispatch_depth;
    dispatch_count = list->count;
    for (i = 0u; i < dispatch_count; ++i)
    {
        FVizObjectObserver* observer = &list->observers[i];
        if (observer->active != FVIZ_FALSE &&
            observer->activation_dispatch < list->dispatch_serial &&
            fviz_object_observer_matches(observer->event_id, event_id) != FVIZ_FALSE &&
            observer->callback(object, event_id, call_data, observer->client_data) != FVIZ_FALSE)
        {
            aborted = FVIZ_TRUE;
            break;
        }
    }
    --list->dispatch_depth;
    if (top_level != FVIZ_FALSE) fviz_object_observers_maintain(object);
    return aborted;
}
