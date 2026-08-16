#include <math.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

static int near_value(float a, float b) { return fabsf(a - b) < 1.0e-5f; }

static FVizBool project_world(FVizVec3 world, float* x, float* y, void* user_data)
{
    FVIZ_UNUSED(user_data);
    *x = world.x * 10.0f;
    *y = world.y * 10.0f;
    return world.z >= 0.0f ? FVIZ_TRUE : FVIZ_FALSE;
}

int main(void)
{
    FVizOverlayLayoutContext context;
    FVizOverlayLayoutItem items[5];
    FVizOverlayLayoutResult first[5];
    FVizOverlayLayoutResult second[5];
    FVizSize i;
    fviz_overlay_layout_context_initialize(&context);
    context.window_width = 1000.0f;
    context.window_height = 800.0f;
    context.viewport_x = 100.0f;
    context.viewport_y = 200.0f;
    context.viewport_width = 400.0f;
    context.viewport_height = 300.0f;
    context.content_scale = 2.0f;
    context.safe_area[0] = 10.0f;
    context.safe_area[1] = 20.0f;
    context.safe_area[2] = 30.0f;
    context.safe_area[3] = 40.0f;
    context.world_to_display = project_world;
    for (i = 0u; i < 5u; ++i)
    {
        fviz_overlay_layout_item_initialize(&items[i]);
        items[i].id = (uint64_t)(i + 1u);
        items[i].width = 80.0f;
        items[i].height = 40.0f;
    }
    items[0].anchor = fviz_vec3(0.0f, 1.0f, 0.0f);
    items[0].vertical_alignment = FVIZ_OVERLAY_ALIGN_TOP;
    items[0].padding[0] = 5.0f;
    items[0].padding[3] = 10.0f;
    items[0].collision_group = 1u;
    items[0].stack_gap = 4.0f;
    items[1] = items[0];
    items[1].id = 2u;
    items[2].anchor_space = FVIZ_OVERLAY_NORMALIZED_WINDOW;
    items[2].anchor = fviz_vec3(1.0f, 0.0f, 0.0f);
    items[2].horizontal_alignment = FVIZ_OVERLAY_ALIGN_RIGHT;
    items[3].anchor_space = FVIZ_OVERLAY_WORLD;
    items[3].anchor = fviz_vec3(20.0f, 30.0f, 1.0f);
    items[4].anchor_space = FVIZ_OVERLAY_WORLD;
    items[4].anchor = fviz_vec3(20.0f, 30.0f, -1.0f);
    CHECK(fviz_overlay_layout_resolve(&context, items, 5u, first) == FVIZ_OK);
    CHECK(fviz_overlay_layout_resolve(&context, items, 5u, second) == FVIZ_OK);
    CHECK(near_value(first[0].x, 110.0f));
    CHECK(near_value(first[0].y, 420.0f));
    CHECK(first[0].flags & FVIZ_OVERLAY_LAYOUT_CLAMPED_TO_SAFE_AREA);
    CHECK(first[1].flags & FVIZ_OVERLAY_LAYOUT_MOVED_FOR_COLLISION);
    CHECK(first[1].y < first[0].y);
    CHECK(first[2].flags & FVIZ_OVERLAY_LAYOUT_CLAMPED_TO_SAFE_AREA);
    CHECK(near_value(first[2].x, 390.0f));
    CHECK(first[3].visible == FVIZ_TRUE);
    CHECK(first[4].visible == FVIZ_FALSE);
    CHECK(first[4].flags & FVIZ_OVERLAY_LAYOUT_PROJECTION_FAILED);
    for (i = 0u; i < 5u; ++i)
    {
        CHECK(first[i].id == second[i].id);
        CHECK(first[i].x == second[i].x && first[i].y == second[i].y);
        CHECK(first[i].flags == second[i].flags && first[i].visible == second[i].visible);
    }
    return 0;
}
