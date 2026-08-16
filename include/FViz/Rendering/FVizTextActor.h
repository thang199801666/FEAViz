#ifndef FVIZ_RENDERING_TEXT_ACTOR_H
#define FVIZ_RENDERING_TEXT_ACTOR_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Rendering/FVizTextProperty.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizTextActor2D FVizTextActor2D;
typedef struct FVizBillboardTextActor3D FVizBillboardTextActor3D;
#define FVIZ_TYPE_TEXT_ACTOR_2D UINT64_C(0xE42391AD7B50A104)
#define FVIZ_TYPE_BILLBOARD_TEXT_ACTOR_3D UINT64_C(0xE42391AD7B50A105)

typedef enum FVizTextCoordinateSystem
{
    FVIZ_TEXT_COORDINATE_DISPLAY_PIXELS = 0,
    FVIZ_TEXT_COORDINATE_NORMALIZED_VIEWPORT = 1
} FVizTextCoordinateSystem;

typedef struct FVizTextMetrics
{
    float width;
    float height;
    float line_height;
    FVizSize glyph_count;
    FVizSize line_count;
} FVizTextMetrics;

FVIZ_RENDERING_API FVizResult fviz_text_measure_utf8(const FVizTextProperty* property, const char* utf8,
                                           FVizTextMetrics* out_metrics);

FVIZ_RENDERING_API FVizResult fviz_text_actor_2d_create(FVizTextActor2D** out_actor);
FVIZ_RENDERING_API FVizResult fviz_text_actor_2d_set_text(FVizTextActor2D* actor, const char* utf8);
FVIZ_RENDERING_API const char* fviz_text_actor_2d_text(const FVizTextActor2D* actor);
FVIZ_RENDERING_API FVizResult fviz_text_actor_2d_set_text_property(FVizTextActor2D* actor, FVizTextProperty* property);
FVIZ_RENDERING_API FVizTextProperty* fviz_text_actor_2d_text_property(FVizTextActor2D* actor);
FVIZ_RENDERING_API const FVizTextProperty* fviz_text_actor_2d_const_text_property(const FVizTextActor2D* actor);
FVIZ_RENDERING_API void fviz_text_actor_2d_set_position(FVizTextActor2D* actor, float x, float y);
FVIZ_RENDERING_API void fviz_text_actor_2d_get_position(const FVizTextActor2D* actor, float* x, float* y);
FVIZ_RENDERING_API void fviz_text_actor_2d_set_coordinate_system(FVizTextActor2D* actor, FVizTextCoordinateSystem system);
FVIZ_RENDERING_API FVizTextCoordinateSystem fviz_text_actor_2d_coordinate_system(const FVizTextActor2D* actor);
FVIZ_RENDERING_API void fviz_text_actor_2d_set_visible(FVizTextActor2D* actor, FVizBool visible);
FVIZ_RENDERING_API FVizBool fviz_text_actor_2d_is_visible(const FVizTextActor2D* actor);
FVIZ_RENDERING_API FVizResult fviz_text_actor_2d_measure(const FVizTextActor2D* actor, FVizTextMetrics* out_metrics);

FVIZ_RENDERING_API FVizResult fviz_billboard_text_actor_3d_create(FVizBillboardTextActor3D** out_actor);
FVIZ_RENDERING_API FVizResult fviz_billboard_text_actor_3d_set_text(FVizBillboardTextActor3D* actor, const char* utf8);
FVIZ_RENDERING_API const char* fviz_billboard_text_actor_3d_text(const FVizBillboardTextActor3D* actor);
FVIZ_RENDERING_API FVizResult fviz_billboard_text_actor_3d_set_text_property(FVizBillboardTextActor3D* actor,
                                                                   FVizTextProperty* property);
FVIZ_RENDERING_API FVizTextProperty* fviz_billboard_text_actor_3d_text_property(FVizBillboardTextActor3D* actor);
FVIZ_RENDERING_API const FVizTextProperty*
fviz_billboard_text_actor_3d_const_text_property(const FVizBillboardTextActor3D* actor);
FVIZ_RENDERING_API void fviz_billboard_text_actor_3d_set_world_position(FVizBillboardTextActor3D* actor, FVizVec3 position);
FVIZ_RENDERING_API FVizVec3 fviz_billboard_text_actor_3d_world_position(const FVizBillboardTextActor3D* actor);
FVIZ_RENDERING_API void fviz_billboard_text_actor_3d_set_pixel_offset(FVizBillboardTextActor3D* actor, float x, float y);
FVIZ_RENDERING_API void fviz_billboard_text_actor_3d_get_pixel_offset(const FVizBillboardTextActor3D* actor, float* x, float* y);
FVIZ_RENDERING_API void fviz_billboard_text_actor_3d_set_depth_test(FVizBillboardTextActor3D* actor, FVizBool enabled);
FVIZ_RENDERING_API FVizBool fviz_billboard_text_actor_3d_depth_test(const FVizBillboardTextActor3D* actor);
FVIZ_RENDERING_API void fviz_billboard_text_actor_3d_set_visible(FVizBillboardTextActor3D* actor, FVizBool visible);
FVIZ_RENDERING_API FVizBool fviz_billboard_text_actor_3d_is_visible(const FVizBillboardTextActor3D* actor);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_TEXT_ACTOR_H */
