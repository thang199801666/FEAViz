#ifndef FVIZ_INTERACTION_INTERACTOR_STYLE_H
#define FVIZ_INTERACTION_INTERACTOR_STYLE_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Interaction/FVizEvent.h>
#include <FViz/Rendering/FVizRenderer.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizInteractorStyle FVizInteractorStyle;
typedef struct FVizActor FVizActor;

typedef enum FVizInteractionState
{
    FVIZ_INTERACTION_STATE_NONE = 0,
    FVIZ_INTERACTION_STATE_ROTATE = 1,
    FVIZ_INTERACTION_STATE_PAN = 2,
    FVIZ_INTERACTION_STATE_DOLLY = 3,
    FVIZ_INTERACTION_STATE_SPIN = 4,
    FVIZ_INTERACTION_STATE_RUBBER_BAND = 5,
    FVIZ_INTERACTION_STATE_ACTOR_ROTATE = 6,
    FVIZ_INTERACTION_STATE_ACTOR_PAN = 7,
    FVIZ_INTERACTION_STATE_ACTOR_DOLLY = 8
} FVizInteractionState;

#define FVIZ_TYPE_INTERACTOR_STYLE UINT64_C(0x71B932DE4A6C850F)
#define FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_CAMERA UINT64_C(0x2F8C4D71A9E630B5)
#define FVIZ_TYPE_INTERACTOR_STYLE_RUBBER_BAND UINT64_C(0xC5087D31E9A642BF)
#define FVIZ_TYPE_INTERACTOR_STYLE_TRACKBALL_ACTOR UINT64_C(0x8DC5F3A1274BE690)

FVIZ_API FVizResult fviz_interactor_style_trackball_camera_create(FVizInteractorStyle** out_style);
FVIZ_API FVizResult fviz_interactor_style_trackball_actor_create(FVizInteractorStyle** out_style);
FVIZ_API FVizResult fviz_interactor_style_trackball_actor_set_actor(
    FVizInteractorStyle* style,
    FVizActor* actor);
FVIZ_API FVizActor* fviz_interactor_style_trackball_actor_actor(FVizInteractorStyle* style);
FVIZ_API FVizResult fviz_interactor_style_rubber_band_create(FVizInteractorStyle** out_style);
FVIZ_API FVizBool fviz_interactor_style_rubber_band_active(const FVizInteractorStyle* style);
FVIZ_API FVizBool fviz_interactor_style_rubber_band_completed(const FVizInteractorStyle* style);
FVIZ_API FVizResult fviz_interactor_style_rubber_band_rectangle(
    const FVizInteractorStyle* style,
    int* minimum_x,
    int* minimum_y,
    int* maximum_x,
    int* maximum_y);
FVIZ_API void fviz_interactor_style_rubber_band_reset(FVizInteractorStyle* style);
FVIZ_API void fviz_interactor_style_set_orbit_sensitivity(FVizInteractorStyle* style, float radians_per_pixel);
FVIZ_API float fviz_interactor_style_orbit_sensitivity(const FVizInteractorStyle* style);
FVIZ_API void fviz_interactor_style_set_pan_sensitivity(FVizInteractorStyle* style, float fraction_per_pixel);
FVIZ_API float fviz_interactor_style_pan_sensitivity(const FVizInteractorStyle* style);
FVIZ_API void fviz_interactor_style_set_dolly_factor(FVizInteractorStyle* style, float factor);
FVIZ_API float fviz_interactor_style_dolly_factor(const FVizInteractorStyle* style);
FVIZ_API FVizInteractionState fviz_interactor_style_state(const FVizInteractorStyle* style);
FVIZ_API void fviz_interactor_style_cancel_interaction(FVizInteractorStyle* style);
FVIZ_API FVizBool fviz_interactor_style_process_event(
    FVizInteractorStyle* style,
    FVizRenderer* renderer,
    const FVizInteractionEvent* event);

FVIZ_EXTERN_C_END

#endif /* FVIZ_INTERACTION_INTERACTOR_STYLE_H */
