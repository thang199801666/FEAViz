#ifndef FVIZ_INTERACTION_EVENT_H
#define FVIZ_INTERACTION_EVENT_H

#include <FViz/Core/FVizCommand.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizInteractionEventType
{
    FVIZ_INTERACTION_EVENT_ANY = 0,
    FVIZ_INTERACTION_MOUSE_BUTTON_DOWN = 1,
    FVIZ_INTERACTION_MOUSE_BUTTON_UP = 2,
    FVIZ_INTERACTION_MOUSE_MOVE = 3,
    FVIZ_INTERACTION_MOUSE_WHEEL = 4,
    FVIZ_INTERACTION_KEY_DOWN = 5,
    FVIZ_INTERACTION_KEY_UP = 6,
    FVIZ_INTERACTION_RESIZE = 7,
    FVIZ_INTERACTION_ENTER = 8,
    FVIZ_INTERACTION_LEAVE = 9,
    FVIZ_INTERACTION_EXPOSE = 10,
    FVIZ_INTERACTION_FOCUS_IN = 11,
    FVIZ_INTERACTION_FOCUS_OUT = 12,
    FVIZ_INTERACTION_TIMER = 13,
    FVIZ_INTERACTION_DOUBLE_CLICK = 14,
    FVIZ_INTERACTION_CHAR = 15
} FVizInteractionEventType;

typedef enum FVizMouseButton
{
    FVIZ_MOUSE_BUTTON_NONE = 0,
    FVIZ_MOUSE_BUTTON_LEFT = 1,
    FVIZ_MOUSE_BUTTON_MIDDLE = 2,
    FVIZ_MOUSE_BUTTON_RIGHT = 3,
    FVIZ_MOUSE_BUTTON_X1 = 4,
    FVIZ_MOUSE_BUTTON_X2 = 5
} FVizMouseButton;

typedef enum FVizKey
{
    FVIZ_KEY_UNKNOWN = 0,
    FVIZ_KEY_ESCAPE = 27
} FVizKey;

typedef struct FVizInteractionEvent
{
    FVizInteractionEventType type;
    FVizMouseButton button;
    int x;
    int y;
    int width;
    int height;
    int key;
    float wheel_delta;
    FVizId timer_id;
    double timestamp_seconds;
    FVizBool shift;
    FVizBool control;
    FVizBool alt;
    /* Appended fields preserve the ABI prefix of the original event structure. */
    unsigned int character;
    int delta_x;
    int delta_y;
    float content_scale;
} FVizInteractionEvent;

/* Maps native interaction kinds to the corresponding FVIZ_EVENT_* identifier
 * used by the generic FVizObject observer system. */
FVIZ_INTERACTION_API FVizEventId fviz_interaction_event_id(FVizInteractionEventType type);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_EVENT_H */
