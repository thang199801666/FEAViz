#include <stdio.h>
#include <time.h>

#include <FViz/FViz.h>

#define PASS_COUNT 64u
#define RESOURCE_COUNT 16u
#define ITERATIONS 200u

static double now_seconds(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

int main(void)
{
    FVizRenderGraph* graph = NULL;
    FVizRenderGraphPassId passes[PASS_COUNT];
    FVizRenderGraphResourceId resources[RESOURCE_COUNT];
    FVizRenderGraphStatistics statistics;
    FVizSize i;
    double start;
    double elapsed;
    if (fviz_render_graph_create(&graph) != FVIZ_OK) return 1;
    for (i = 0u; i < RESOURCE_COUNT; ++i)
    {
        FVizRenderGraphResourceDesc desc;
        char name[32];
        fviz_render_graph_resource_desc_initialize(&desc);
        (void)snprintf(name, sizeof(name), "target-%u", (unsigned)i);
        desc.name = name;
        desc.target.width = 1920u;
        desc.target.height = 1080u;
        if (fviz_render_target_desc_add_attachment(
                &desc.target, FVIZ_RENDER_ATTACHMENT_COLOR0,
                FVIZ_RENDER_FORMAT_RGBA16_FLOAT, FVIZ_TRUE) != FVIZ_OK ||
            fviz_render_graph_add_resource(graph, &desc, &resources[i]) != FVIZ_OK)
            return 2;
    }
    for (i = 0u; i < PASS_COUNT; ++i)
    {
        FVizRenderPass* pass = NULL;
        char name[32];
        (void)snprintf(name, sizeof(name), "pass-%u", (unsigned)i);
        if (fviz_render_pass_create(
                (FVizRenderPassStage)(i % 6u), NULL, NULL, NULL, &pass) != FVIZ_OK ||
            fviz_render_graph_add_pass(graph, name, pass, &passes[i]) != FVIZ_OK)
            return 3;
        fviz_release(pass);
        if (i > 0u && fviz_render_graph_add_dependency(
                graph, passes[i - 1u], passes[i]) != FVIZ_OK) return 4;
        if (fviz_render_graph_use_resource(
                graph, passes[i], resources[i % RESOURCE_COUNT],
                FVIZ_RENDER_GRAPH_WRITE) != FVIZ_OK) return 5;
    }
    start = now_seconds();
    for (i = 0u; i < ITERATIONS; ++i)
        if (fviz_render_graph_compile(graph) != FVIZ_OK) return 6;
    elapsed = now_seconds() - start;
    fviz_render_graph_get_statistics(graph, &statistics);
    printf("render_graph passes=%u resources=%u iterations=%u %.3f us/compile physical=%u logical_bytes=%llu peak_bytes=%llu\n",
        PASS_COUNT, RESOURCE_COUNT, ITERATIONS,
        elapsed * 1000000.0 / (double)ITERATIONS,
        statistics.physical_target_count,
        (unsigned long long)statistics.logical_transient_bytes,
        (unsigned long long)statistics.peak_physical_target_bytes);
    fviz_release(graph);
    return 0;
}
