#ifndef FVIZ_RENDERING_OVERLAY_LAYOUT_H
#define FVIZ_RENDERING_OVERLAY_LAYOUT_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizOverlayAnchorSpace
{
    FVIZ_OVERLAY_DISPLAY_PIXELS = 0,
    FVIZ_OVERLAY_VIEWPORT_PIXELS = 1,
    FVIZ_OVERLAY_NORMALIZED_VIEWPORT = 2,
    FVIZ_OVERLAY_NORMALIZED_WINDOW = 3,
    FVIZ_OVERLAY_WORLD = 4
} FVizOverlayAnchorSpace;

typedef enum FVizOverlayHorizontalAlignment
{
    FVIZ_OVERLAY_ALIGN_LEFT = 0,
    FVIZ_OVERLAY_ALIGN_CENTER = 1,
    FVIZ_OVERLAY_ALIGN_RIGHT = 2
} FVizOverlayHorizontalAlignment;

typedef enum FVizOverlayVerticalAlignment
{
    FVIZ_OVERLAY_ALIGN_BOTTOM = 0,
    FVIZ_OVERLAY_ALIGN_MIDDLE = 1,
    FVIZ_OVERLAY_ALIGN_TOP = 2
} FVizOverlayVerticalAlignment;

typedef enum FVizOverlayStackDirection
{
    FVIZ_OVERLAY_STACK_NONE = 0,
    FVIZ_OVERLAY_STACK_UP = 1,
    FVIZ_OVERLAY_STACK_DOWN = 2,
    FVIZ_OVERLAY_STACK_LEFT = 3,
    FVIZ_OVERLAY_STACK_RIGHT = 4
} FVizOverlayStackDirection;

typedef enum FVizOverlayLayoutFlags
{
    FVIZ_OVERLAY_LAYOUT_NONE = 0,
    FVIZ_OVERLAY_LAYOUT_MOVED_FOR_COLLISION = 1u << 0,
    FVIZ_OVERLAY_LAYOUT_CLAMPED_TO_SAFE_AREA = 1u << 1,
    FVIZ_OVERLAY_LAYOUT_OVERFLOW = 1u << 2,
    FVIZ_OVERLAY_LAYOUT_PROJECTION_FAILED = 1u << 3
} FVizOverlayLayoutFlags;

typedef FVizBool (*FVizOverlayWorldToDisplayCallback)(
    FVizVec3 world, float* display_x, float* display_y, void* user_data);

typedef struct FVizOverlayLayoutContext
{
    FVizSize struct_size;
    float window_width;
    float window_height;
    float viewport_x;
    float viewport_y;
    float viewport_width;
    float viewport_height;
    float content_scale;
    /* Physical display-pixel insets in left, bottom, right, top order. */
    float safe_area[4];
    FVizOverlayWorldToDisplayCallback world_to_display;
    void* world_to_display_user_data;
} FVizOverlayLayoutContext;

typedef struct FVizOverlayLayoutItem
{
    FVizSize struct_size;
    uint64_t id;
    FVizOverlayAnchorSpace anchor_space;
    FVizVec3 anchor;
    /* Physical display-pixel content size. */
    float width;
    float height;
    /* Logical-pixel padding in left, bottom, right, top order; DPI-scaled. */
    float padding[4];
    FVizOverlayHorizontalAlignment horizontal_alignment;
    FVizOverlayVerticalAlignment vertical_alignment;
    uint32_t collision_group;
    FVizOverlayStackDirection stack_direction;
    float stack_gap;
    FVizBool visible;
} FVizOverlayLayoutItem;

typedef struct FVizOverlayLayoutResult
{
    FVizSize struct_size;
    uint64_t id;
    float x;
    float y;
    float width;
    float height;
    uint32_t flags;
    FVizBool visible;
} FVizOverlayLayoutResult;

FVIZ_API void fviz_overlay_layout_context_initialize(FVizOverlayLayoutContext* context);
FVIZ_API void fviz_overlay_layout_item_initialize(FVizOverlayLayoutItem* item);
FVIZ_API FVizResult fviz_overlay_layout_resolve(
    const FVizOverlayLayoutContext* context,
    const FVizOverlayLayoutItem* items,
    FVizSize item_count,
    FVizOverlayLayoutResult* results);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_OVERLAY_LAYOUT_H */
