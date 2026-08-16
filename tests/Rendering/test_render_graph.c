#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static FVizResult add_test_pass(
    FVizRenderGraph* graph,
    const char* name,
    FVizRenderPassStage stage,
    FVizRenderGraphPassId* out_id)
{
    FVizRenderPass* pass = NULL;
    FVizResult result = fviz_render_pass_create(stage, NULL, NULL, NULL, &pass);
    if (result == FVIZ_OK) result = fviz_render_graph_add_pass(graph, name, pass, out_id);
    fviz_release(pass);
    return result;
}

static FVizResult add_color_resource(
    FVizRenderGraph* graph,
    const char* name,
    FVizBool external,
    FVizRenderGraphResourceId* out_id)
{
    FVizRenderGraphResourceDesc desc;
    fviz_render_graph_resource_desc_initialize(&desc);
    desc.name = name;
    desc.external = external;
    desc.target.width = 320u;
    desc.target.height = 200u;
    desc.target.samples = 1u;
    CHECK(fviz_render_target_desc_add_attachment(
        &desc.target, FVIZ_RENDER_ATTACHMENT_COLOR0,
        FVIZ_RENDER_FORMAT_RGBA8_UNORM, FVIZ_TRUE) == FVIZ_OK);
    return fviz_render_graph_add_resource(graph, &desc, out_id);
}

int main(void)
{
    FVizRenderGraph* graph = NULL;
    FVizRenderGraph* invalid = NULL;
    FVizRenderGraphPassId clear_pass;
    FVizRenderGraphPassId opaque_pass;
    FVizRenderGraphPassId post_pass;
    FVizRenderGraphPassId overlay_pass;
    FVizRenderGraphResourceId scene_color;
    FVizRenderGraphResourceId post_color;
    FVizRenderGraphResourceId overlay_color;
    FVizRenderGraphStatistics statistics;
    FVizSize first;
    FVizSize last;
    uint32_t scene_slot;
    uint32_t overlay_slot;

    CHECK(fviz_render_graph_create(&graph) == FVIZ_OK);
    CHECK(add_color_resource(graph, "scene-color", FVIZ_FALSE, &scene_color) == FVIZ_OK);
    CHECK(add_color_resource(graph, "post-color", FVIZ_FALSE, &post_color) == FVIZ_OK);
    CHECK(add_color_resource(graph, "overlay-color", FVIZ_FALSE, &overlay_color) == FVIZ_OK);
    CHECK(add_test_pass(graph, "clear", FVIZ_RENDER_PASS_CLEAR, &clear_pass) == FVIZ_OK);
    CHECK(add_test_pass(graph, "opaque", FVIZ_RENDER_PASS_OPAQUE, &opaque_pass) == FVIZ_OK);
    CHECK(add_test_pass(graph, "post", FVIZ_RENDER_PASS_TRANSLUCENT, &post_pass) == FVIZ_OK);
    CHECK(add_test_pass(graph, "overlay", FVIZ_RENDER_PASS_OVERLAY, &overlay_pass) == FVIZ_OK);

    CHECK(fviz_render_graph_use_resource(
        graph, clear_pass, scene_color, FVIZ_RENDER_GRAPH_WRITE) == FVIZ_OK);
    CHECK(fviz_render_graph_use_resource(
        graph, opaque_pass, scene_color, FVIZ_RENDER_GRAPH_READ_WRITE) == FVIZ_OK);
    CHECK(fviz_render_graph_use_resource(
        graph, post_pass, scene_color, FVIZ_RENDER_GRAPH_READ) == FVIZ_OK);
    CHECK(fviz_render_graph_use_resource(
        graph, post_pass, post_color, FVIZ_RENDER_GRAPH_WRITE) == FVIZ_OK);
    CHECK(fviz_render_graph_use_resource(
        graph, overlay_pass, overlay_color, FVIZ_RENDER_GRAPH_WRITE) == FVIZ_OK);
    CHECK(fviz_render_graph_add_dependency(graph, post_pass, overlay_pass) == FVIZ_OK);
    CHECK(fviz_render_graph_compile(graph) == FVIZ_OK);
    CHECK(fviz_render_graph_is_compiled(graph) == FVIZ_TRUE);
    CHECK(fviz_render_graph_execution_count(graph) == 4u);
    CHECK(strcmp(fviz_render_graph_pass_name(graph,
        fviz_render_graph_execution_pass_id(graph, 0u)), "clear") == 0);
    CHECK(fviz_render_graph_execution_pass(graph, 3u) != NULL);
    CHECK(fviz_render_graph_resource_lifetime(
        graph, scene_color, &first, &last) == FVIZ_OK);
    CHECK(first == 0u && last == 2u);
    scene_slot = fviz_render_graph_resource_physical_slot(graph, scene_color);
    overlay_slot = fviz_render_graph_resource_physical_slot(graph, overlay_color);
    CHECK(scene_slot != FVIZ_RENDER_GRAPH_PHYSICAL_SLOT_INVALID);
    CHECK(scene_slot == overlay_slot);
    CHECK(fviz_render_graph_physical_target(graph, scene_slot) != NULL);
    fviz_render_graph_get_statistics(graph, &statistics);
    CHECK(statistics.compiled == FVIZ_TRUE);
    CHECK(statistics.pass_count == 4u && statistics.resource_count == 3u);
    CHECK(statistics.dependency_edge_count >= 3u);
    CHECK(statistics.physical_target_count == 2u);
    CHECK(statistics.logical_transient_bytes == 3u * 320u * 200u * 4u);
    CHECK(statistics.peak_physical_target_bytes == 2u * 320u * 200u * 4u);

    CHECK(fviz_render_graph_add_dependency(graph, overlay_pass, clear_pass) == FVIZ_OK);
    CHECK(fviz_render_graph_is_compiled(graph) == FVIZ_FALSE);
    CHECK(fviz_render_graph_compile(graph) == FVIZ_ERROR_INVALID_STATE);

    CHECK(fviz_render_graph_create(&invalid) == FVIZ_OK);
    CHECK(add_color_resource(invalid, "uninitialized", FVIZ_FALSE, &scene_color) == FVIZ_OK);
    CHECK(add_test_pass(invalid, "reader", FVIZ_RENDER_PASS_OPAQUE, &opaque_pass) == FVIZ_OK);
    CHECK(fviz_render_graph_use_resource(
        invalid, opaque_pass, scene_color, FVIZ_RENDER_GRAPH_READ) == FVIZ_OK);
    CHECK(fviz_render_graph_compile(invalid) == FVIZ_ERROR_INVALID_STATE);

    fviz_render_graph_clear(graph);
    CHECK(fviz_render_graph_execution_count(graph) == 0u);
    fviz_release(invalid);
    fviz_release(graph);
    return 0;
}
