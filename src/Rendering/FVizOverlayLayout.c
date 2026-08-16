#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizOverlayLayout.h>

#include <FViz/Core/FVizErrorInternal.h>

static float fviz_overlay_clamp(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static FVizBool fviz_overlay_intersects(
    const FVizOverlayLayoutResult* a, const FVizOverlayLayoutResult* b)
{
    return a->x < b->x + b->width && a->x + a->width > b->x &&
           a->y < b->y + b->height && a->y + a->height > b->y
        ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_overlay_layout_context_initialize(FVizOverlayLayoutContext* context)
{
    if (context == NULL) return;
    memset(context, 0, sizeof(*context));
    context->struct_size = sizeof(*context);
    context->content_scale = 1.0f;
}

void fviz_overlay_layout_item_initialize(FVizOverlayLayoutItem* item)
{
    if (item == NULL) return;
    memset(item, 0, sizeof(*item));
    item->struct_size = sizeof(*item);
    item->anchor_space = FVIZ_OVERLAY_NORMALIZED_VIEWPORT;
    item->horizontal_alignment = FVIZ_OVERLAY_ALIGN_LEFT;
    item->vertical_alignment = FVIZ_OVERLAY_ALIGN_BOTTOM;
    item->stack_direction = FVIZ_OVERLAY_STACK_DOWN;
    item->visible = FVIZ_TRUE;
}

FVizResult fviz_overlay_layout_resolve(
    const FVizOverlayLayoutContext* context,
    const FVizOverlayLayoutItem* items,
    FVizSize item_count,
    FVizOverlayLayoutResult* results)
{
    FVizSize i;
    float safe_left;
    float safe_bottom;
    float safe_right;
    float safe_top;
    if (context == NULL || context->struct_size < sizeof(*context) ||
        (item_count != 0u && (items == NULL || results == NULL)) ||
        !isfinite(context->window_width) || !isfinite(context->window_height) ||
        !isfinite(context->viewport_width) || !isfinite(context->viewport_height) ||
        !isfinite(context->content_scale) || context->window_width <= 0.0f ||
        context->window_height <= 0.0f || context->viewport_width <= 0.0f ||
        context->viewport_height <= 0.0f || context->content_scale <= 0.0f)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid overlay layout context or output storage");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    safe_left = context->viewport_x + fmaxf(0.0f, context->safe_area[0]);
    safe_bottom = context->viewport_y + fmaxf(0.0f, context->safe_area[1]);
    safe_right = context->viewport_x + context->viewport_width - fmaxf(0.0f, context->safe_area[2]);
    safe_top = context->viewport_y + context->viewport_height - fmaxf(0.0f, context->safe_area[3]);
    for (i = 0u; i < item_count; ++i)
    {
        const FVizOverlayLayoutItem* item = &items[i];
        FVizOverlayLayoutResult* result = &results[i];
        float anchor_x = 0.0f;
        float anchor_y = 0.0f;
        float left_padding;
        float bottom_padding;
        float right_padding;
        float top_padding;
        FVizSize previous;
        if (item->struct_size < sizeof(*item) || !isfinite(item->width) ||
            !isfinite(item->height) || item->width < 0.0f || item->height < 0.0f)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid overlay layout item");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        memset(result, 0, sizeof(*result));
        result->struct_size = sizeof(*result);
        result->id = item->id;
        result->width = item->width;
        result->height = item->height;
        result->visible = item->visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        if (result->visible == FVIZ_FALSE) continue;
        switch (item->anchor_space)
        {
            case FVIZ_OVERLAY_DISPLAY_PIXELS:
                anchor_x = item->anchor.x;
                anchor_y = item->anchor.y;
                break;
            case FVIZ_OVERLAY_VIEWPORT_PIXELS:
                anchor_x = context->viewport_x + item->anchor.x;
                anchor_y = context->viewport_y + item->anchor.y;
                break;
            case FVIZ_OVERLAY_NORMALIZED_VIEWPORT:
                anchor_x = context->viewport_x + item->anchor.x * context->viewport_width;
                anchor_y = context->viewport_y + item->anchor.y * context->viewport_height;
                break;
            case FVIZ_OVERLAY_NORMALIZED_WINDOW:
                anchor_x = item->anchor.x * context->window_width;
                anchor_y = item->anchor.y * context->window_height;
                break;
            case FVIZ_OVERLAY_WORLD:
                if (context->world_to_display == NULL ||
                    context->world_to_display(
                        item->anchor, &anchor_x, &anchor_y,
                        context->world_to_display_user_data) == FVIZ_FALSE)
                {
                    result->visible = FVIZ_FALSE;
                    result->flags |= FVIZ_OVERLAY_LAYOUT_PROJECTION_FAILED;
                    continue;
                }
                break;
            default:
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "unknown overlay anchor space");
                return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        left_padding = fmaxf(0.0f, item->padding[0]) * context->content_scale;
        bottom_padding = fmaxf(0.0f, item->padding[1]) * context->content_scale;
        right_padding = fmaxf(0.0f, item->padding[2]) * context->content_scale;
        top_padding = fmaxf(0.0f, item->padding[3]) * context->content_scale;
        result->x = anchor_x;
        result->y = anchor_y;
        if (item->horizontal_alignment == FVIZ_OVERLAY_ALIGN_CENTER) result->x -= 0.5f * result->width;
        else if (item->horizontal_alignment == FVIZ_OVERLAY_ALIGN_RIGHT) result->x -= result->width;
        if (item->vertical_alignment == FVIZ_OVERLAY_ALIGN_MIDDLE) result->y -= 0.5f * result->height;
        else if (item->vertical_alignment == FVIZ_OVERLAY_ALIGN_TOP) result->y -= result->height;
        result->x += item->horizontal_alignment == FVIZ_OVERLAY_ALIGN_RIGHT ? -right_padding : left_padding;
        result->y += item->vertical_alignment == FVIZ_OVERLAY_ALIGN_TOP ? -top_padding : bottom_padding;
        for (previous = 0u; previous < i; ++previous)
        {
            const FVizOverlayLayoutItem* other_item = &items[previous];
            const FVizOverlayLayoutResult* other = &results[previous];
            const float gap = fmaxf(0.0f, item->stack_gap) * context->content_scale;
            if (item->collision_group == 0u || item->collision_group != other_item->collision_group ||
                other->visible == FVIZ_FALSE || fviz_overlay_intersects(result, other) == FVIZ_FALSE)
                continue;
            if (item->stack_direction == FVIZ_OVERLAY_STACK_UP) result->y = other->y + other->height + gap;
            else if (item->stack_direction == FVIZ_OVERLAY_STACK_LEFT) result->x = other->x - result->width - gap;
            else if (item->stack_direction == FVIZ_OVERLAY_STACK_RIGHT) result->x = other->x + other->width + gap;
            else result->y = other->y - result->height - gap;
            result->flags |= FVIZ_OVERLAY_LAYOUT_MOVED_FOR_COLLISION;
        }
        if (result->width > safe_right - safe_left || result->height > safe_top - safe_bottom)
            result->flags |= FVIZ_OVERLAY_LAYOUT_OVERFLOW;
        else
        {
            const float clamped_x = fviz_overlay_clamp(result->x, safe_left, safe_right - result->width);
            const float clamped_y = fviz_overlay_clamp(result->y, safe_bottom, safe_top - result->height);
            if (clamped_x != result->x || clamped_y != result->y)
                result->flags |= FVIZ_OVERLAY_LAYOUT_CLAMPED_TO_SAFE_AREA;
            result->x = clamped_x;
            result->y = clamped_y;
        }
    }
    return FVIZ_OK;
}
