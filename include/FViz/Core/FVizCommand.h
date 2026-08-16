#ifndef FVIZ_CORE_COMMAND_H
#define FVIZ_CORE_COMMAND_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizObject FVizObject;
typedef struct FVizCommand FVizCommand;
typedef uint32_t FVizEventId;
typedef uint64_t FVizObserverTag;

#define FVIZ_TYPE_COMMAND UINT64_C(0x0B75E8E7BDA3C733)
#define FVIZ_OBSERVER_TAG_INVALID UINT64_C(0)

/* Core event identifiers intentionally mirror the vtkCommand model: event 0 is
 * a wildcard, small IDs are library-defined, and user events start at a stable
 * high range. Existing IDs are never renumbered once published. */
typedef enum FVizEvent
{
    FVIZ_EVENT_ANY = 0,
    FVIZ_EVENT_DELETE = 1,
    FVIZ_EVENT_START = 2,
    FVIZ_EVENT_END = 3,
    FVIZ_EVENT_MODIFIED = 4,
    FVIZ_EVENT_RENDER_START = 5,
    FVIZ_EVENT_RENDER_END = 6,
    FVIZ_EVENT_WINDOW_RESIZE = 7,
    FVIZ_EVENT_WINDOW_CLOSE = 8,
    FVIZ_EVENT_WINDOW_FOCUS_IN = 9,
    FVIZ_EVENT_WINDOW_FOCUS_OUT = 10,
    FVIZ_EVENT_WINDOW_DPI_CHANGED = 11,
    FVIZ_EVENT_WINDOW_REPARENTED = 12,
    FVIZ_EVENT_START_INTERACTION = 13,
    FVIZ_EVENT_INTERACTION = 14,
    FVIZ_EVENT_END_INTERACTION = 15,
    FVIZ_EVENT_ENABLE = 16,
    FVIZ_EVENT_DISABLE = 17,
    FVIZ_EVENT_START_PICK = 18,
    FVIZ_EVENT_PICK = 19,
    FVIZ_EVENT_END_PICK = 20,
    FVIZ_EVENT_PROGRESS = 21,
    FVIZ_EVENT_ABORT_CHECK = 22,
    FVIZ_EVENT_RENDER_REQUESTED = 23,

    /* Low-level interaction events carry const FVizInteractionEvent* in call_data. */
    FVIZ_EVENT_INTERACTION_ANY = 1000,
    FVIZ_EVENT_MOUSE_BUTTON_DOWN = 1001,
    FVIZ_EVENT_MOUSE_BUTTON_UP = 1002,
    FVIZ_EVENT_MOUSE_MOVE = 1003,
    FVIZ_EVENT_MOUSE_WHEEL = 1004,
    FVIZ_EVENT_KEY_DOWN = 1005,
    FVIZ_EVENT_KEY_UP = 1006,
    FVIZ_EVENT_RESIZE = 1007,
    FVIZ_EVENT_ENTER = 1008,
    FVIZ_EVENT_LEAVE = 1009,
    FVIZ_EVENT_EXPOSE = 1010,
    FVIZ_EVENT_FOCUS_IN = 1011,
    FVIZ_EVENT_FOCUS_OUT = 1012,
    FVIZ_EVENT_TIMER = 1013,
    FVIZ_EVENT_DOUBLE_CLICK = 1014,
    FVIZ_EVENT_CHAR = 1015,

    FVIZ_EVENT_USER = 10000
} FVizEvent;

/* Returning FVIZ_TRUE aborts propagation to lower-priority observers, just as
 * vtkCommand::AbortFlag stops later observers. client_data is registration-time
 * state; call_data belongs to the event source and is valid only for the call. */
typedef FVizBool (*FVizObserverCallbackFn)(FVizObject* caller, FVizEventId event_id, void* call_data,
                                           void* client_data);

/* Reusable command objects provide the vtkCommand-style path. A command can be
 * registered on multiple objects/events. The observer retains the command until
 * its tag is removed or the observed object is destroyed. Returning FVIZ_TRUE,
 * or setting the command abort flag from inside the callback, stops propagation. */
typedef FVizBool (*FVizCommandExecuteFn)(FVizCommand* command, FVizObject* caller, FVizEventId event_id,
                                         void* call_data, void* client_data);

FVIZ_CORE_API FVizResult fviz_command_create(FVizCommandExecuteFn execute, void* client_data, FVizCommand** out_command);
FVIZ_CORE_API void fviz_command_set_execute(FVizCommand* command, FVizCommandExecuteFn execute);
FVIZ_CORE_API FVizCommandExecuteFn fviz_command_execute_function(const FVizCommand* command);
FVIZ_CORE_API void fviz_command_set_client_data(FVizCommand* command, void* client_data);
FVIZ_CORE_API void* fviz_command_client_data(FVizCommand* command);
FVIZ_CORE_API const void* fviz_command_const_client_data(const FVizCommand* command);
FVIZ_CORE_API void fviz_command_set_abort_flag(FVizCommand* command, FVizBool abort_flag);
FVIZ_CORE_API FVizBool fviz_command_abort_flag(const FVizCommand* command);
FVIZ_CORE_API FVizBool fviz_command_execute(FVizCommand* command, FVizObject* caller, FVizEventId event_id, void* call_data);

FVIZ_CORE_API const char* fviz_event_name(FVizEventId event_id);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_COMMAND_H */
