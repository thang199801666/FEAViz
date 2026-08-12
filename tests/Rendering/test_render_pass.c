#include <math.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

typedef struct PassState
{
    uint32_t executions;
    uint32_t destructions;
} PassState;

static FVizResult execute_pass(
    FVizRenderPass* pass,
    FVizRenderer* renderer,
    const FVizRenderPassContext* context,
    void* user_data)
{
    PassState* state = (PassState*)user_data;
    CHECK(pass != NULL && renderer != NULL);
    CHECK(context->viewport_width == 400 && context->viewport_height == 600);
    ++state->executions;
    return FVIZ_OK;
}

static void destroy_pass_state(void* user_data)
{
    PassState* state = (PassState*)user_data;
    ++state->destructions;
}

int main(void)
{
    FVizRenderer* renderer = NULL;
    FVizRenderPass* custom = NULL;
    FVizRenderPassContext context;
    FVizRenderPassStage previous = FVIZ_RENDER_PASS_CLEAR;
    PassState state = {0u, 0u};
    FVizVec3 view;
    FVizVec3 ndc;
    FVizVec3 display;
    FVizRay ray;
    FVizSize i;

    CHECK(fviz_renderer_create(&renderer) == FVIZ_OK);
    CHECK(fviz_renderer_pass_count(renderer) == 6u);
    for (i = 0u; i < fviz_renderer_pass_count(renderer); ++i)
    {
        FVizRenderPass* pass = fviz_renderer_pass_at(renderer, i);
        CHECK(pass != NULL);
        CHECK(i == 0u || fviz_render_pass_stage(pass) >= previous);
        CHECK(fviz_render_pass_is_custom(pass) == FVIZ_FALSE);
        previous = fviz_render_pass_stage(pass);
    }
    CHECK(fviz_render_pass_create(
        FVIZ_RENDER_PASS_TRANSLUCENT, execute_pass, &state,
        destroy_pass_state, &custom) == FVIZ_OK);
    CHECK(fviz_renderer_add_pass(renderer, custom) == FVIZ_OK);
    CHECK(fviz_renderer_pass_count(renderer) == 7u);
    (void)memset(&context, 0, sizeof(context));
    context.struct_size = (uint32_t)sizeof(context);
    context.viewport_width = 400;
    context.viewport_height = 600;
    context.aspect_ratio = 2.0f / 3.0f;
    CHECK(fviz_render_pass_execute(custom, renderer, &context) == FVIZ_OK);
    CHECK(state.executions == 1u);

    fviz_camera_set_position(fviz_renderer_camera(renderer), fviz_vec3(0.0f, 0.0f, 10.0f));
    fviz_camera_set_target(fviz_renderer_camera(renderer), fviz_vec3(0.0f, 0.0f, 0.0f));
    CHECK(fviz_renderer_set_viewport(renderer, 0.5f, 0.0f, 1.0f, 1.0f) == FVIZ_OK);
    CHECK(fviz_renderer_world_to_view(renderer, fviz_vec3(0.0f, 0.0f, 0.0f), &view) == FVIZ_OK);
    CHECK(fabsf(view.x) < 1.0e-6f && fabsf(view.y) < 1.0e-6f && fabsf(view.z + 10.0f) < 1.0e-5f);
    CHECK(fviz_renderer_view_to_ndc(renderer, view, 2.0f / 3.0f, &ndc) == FVIZ_OK);
    CHECK(fabsf(ndc.x) < 1.0e-6f && fabsf(ndc.y) < 1.0e-6f);
    CHECK(fviz_renderer_ndc_to_display(renderer, ndc, 800, 600, &display) == FVIZ_OK);
    CHECK(fabsf(display.x - 600.0f) < 1.0e-4f);
    CHECK(fabsf(display.y - 300.0f) < 1.0e-4f);
    CHECK(fviz_renderer_display_to_world_ray(renderer, 600.0f, 300.0f, 800, 600, &ray) == FVIZ_OK);
    CHECK(fabsf(ray.direction.x) < 0.01f && fabsf(ray.direction.y) < 0.01f && ray.direction.z < -0.99f);
    CHECK(fviz_renderer_display_to_world_ray(renderer, 200.0f, 300.0f, 800, 600, &ray) == FVIZ_ERROR_NOT_FOUND);

    CHECK(fviz_renderer_remove_pass(renderer, custom) == FVIZ_OK);
    CHECK(state.destructions == 0u);
    fviz_release(custom);
    CHECK(state.destructions == 1u);
    fviz_release(renderer);

    if (fviz_render_window_supported() != FVIZ_FALSE)
    {
        FVizRenderWindow* window = NULL;
        FVizRenderCapabilities capabilities;
        FVizPolyData* triangle = NULL;
        FVizActor* actor = NULL;
        FVizActor* front_actor = NULL;
        FVizDataArray* original_cell_ids = NULL;
        FVizHardwarePick pick;
        uint64_t original_cell_id = 42u;
        uint32_t a;
        uint32_t b;
        uint32_t c;
        uint8_t* pixels;
        float* depth;
        const FVizSize pixel_count = 96u * 64u;
        CHECK(fviz_render_window_create_offscreen(96, 64, &window) == FVIZ_OK);
        CHECK(fviz_render_window_state(window) == FVIZ_RENDER_WINDOW_OFFSCREEN);
        fviz_render_window_get_capabilities(window, &capabilities);
        CHECK(capabilities.struct_size == sizeof(capabilities));
        CHECK(capabilities.color_readback_supported == FVIZ_TRUE);
        CHECK(fviz_poly_data_create(&triangle) == FVIZ_OK);
        CHECK(fviz_poly_data_add_point(triangle, fviz_vec3(-1.0f, -1.0f, 0.0f), &a) == FVIZ_OK);
        CHECK(fviz_poly_data_add_point(triangle, fviz_vec3(1.0f, -1.0f, 0.0f), &b) == FVIZ_OK);
        CHECK(fviz_poly_data_add_point(triangle, fviz_vec3(0.0f, 1.0f, 0.0f), &c) == FVIZ_OK);
        CHECK(fviz_poly_data_add_triangle(triangle, a, b, c) == FVIZ_OK);
        CHECK(fviz_poly_data_compute_normals(triangle) == FVIZ_OK);
        CHECK(fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &original_cell_ids) == FVIZ_OK);
        CHECK(fviz_data_array_append_tuple(original_cell_ids, &original_cell_id) == FVIZ_OK);
        CHECK(fviz_attribute_set_add(
            fviz_poly_data_cell_data(triangle), "FVizOriginalCellIds", original_cell_ids) == FVIZ_OK);
        CHECK(fviz_actor_create(&actor) == FVIZ_OK);
        CHECK(fviz_actor_set_poly_data(actor, triangle) == FVIZ_OK);
        CHECK(fviz_scene_add_actor(
            fviz_renderer_scene(fviz_render_window_renderer(window)), actor) == FVIZ_OK);
        CHECK(fviz_actor_create(&front_actor) == FVIZ_OK);
        CHECK(fviz_actor_set_poly_data(front_actor, triangle) == FVIZ_OK);
        fviz_actor_set_position(front_actor, fviz_vec3(0.0f, 0.0f, 0.5f));
        CHECK(fviz_scene_add_actor(
            fviz_renderer_scene(fviz_render_window_renderer(window)), front_actor) == FVIZ_OK);
        fviz_camera_set_position(
            fviz_renderer_camera(fviz_render_window_renderer(window)),
            fviz_vec3(0.0f, 0.0f, 5.0f));
        fviz_camera_set_target(
            fviz_renderer_camera(fviz_render_window_renderer(window)),
            fviz_vec3(0.0f, 0.0f, 0.0f));
        fviz_renderer_fit_camera(fviz_render_window_renderer(window), 1.2f);
        CHECK(fviz_render_window_resize(window, 96, 64) == FVIZ_OK);
        CHECK(fviz_render_window_render(window) == FVIZ_OK);
        pixels = (uint8_t*)fviz_alloc(pixel_count * 4u);
        depth = (float*)fviz_alloc(pixel_count * sizeof(*depth));
        CHECK(pixels != NULL && depth != NULL);
        CHECK(fviz_render_window_read_rgba8(window, pixels, pixel_count * 4u) == FVIZ_OK);
        CHECK(fviz_render_window_read_depth_f32(window, depth, pixel_count) == FVIZ_OK);
        if (capabilities.modern_pipeline != FVIZ_FALSE)
        {
            CHECK(fviz_render_window_hardware_pick(window, 48, 32, &pick) == FVIZ_OK);
            CHECK(pick.actor == front_actor && pick.rendered_primitive_id == 0u);
            CHECK(pick.original_cell_id == 42u && pick.depth < 1.0f);
        }
        fviz_render_window_finalize(window);
        CHECK(fviz_render_window_state(window) == FVIZ_RENDER_WINDOW_FINALIZED);
        CHECK(fviz_render_window_render(window) == FVIZ_ERROR_INVALID_STATE);
        CHECK(fviz_render_window_initialize(window) == FVIZ_OK);
        CHECK(fviz_render_window_state(window) == FVIZ_RENDER_WINDOW_OFFSCREEN);
        CHECK(fviz_render_window_render(window) == FVIZ_OK);
        fviz_release(front_actor);
        fviz_release(actor);
        fviz_release(original_cell_ids);
        fviz_release(triangle);
        fviz_free(depth);
        fviz_free(pixels);
        fviz_release(window);
    }
    return 0;
}
