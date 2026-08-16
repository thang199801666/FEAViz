#ifndef FVIZ_RENDERING_RENDER_GRAPH_H
#define FVIZ_RENDERING_RENDER_GRAPH_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Rendering/FVizRenderPass.h>
#include <FViz/Rendering/FVizRenderTarget.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRenderGraph FVizRenderGraph;

#define FVIZ_TYPE_RENDER_GRAPH UINT64_C(0x3A04F33DFE8B2197)

typedef FVizId FVizRenderGraphPassId;
typedef FVizId FVizRenderGraphResourceId;

#define FVIZ_RENDER_GRAPH_PASS_ID_INVALID UINT64_C(0)
#define FVIZ_RENDER_GRAPH_RESOURCE_ID_INVALID UINT64_C(0)
#define FVIZ_RENDER_GRAPH_PHYSICAL_SLOT_INVALID UINT32_MAX

typedef enum FVizRenderGraphAccess
{
    FVIZ_RENDER_GRAPH_READ = 1,
    FVIZ_RENDER_GRAPH_WRITE = 2,
    FVIZ_RENDER_GRAPH_READ_WRITE = 3
} FVizRenderGraphAccess;

typedef struct FVizRenderGraphResourceDesc
{
    uint32_t struct_size;
    const char* name;
    FVizRenderTargetDesc target;
    /* External resources are supplied by the backend and are not allocated or
     * aliased by the graph. They may be read before a graph pass writes them. */
    FVizBool external;
    FVizBool transient_resource;
} FVizRenderGraphResourceDesc;

typedef struct FVizRenderGraphStatistics
{
    uint32_t struct_size;
    uint32_t pass_count;
    uint32_t resource_count;
    uint32_t dependency_edge_count;
    uint32_t physical_target_count;
    uint64_t logical_transient_bytes;
    uint64_t peak_physical_target_bytes;
    uint64_t compile_generation;
    FVizBool compiled;
} FVizRenderGraphStatistics;

FVIZ_RENDERING_API void fviz_render_graph_resource_desc_initialize(FVizRenderGraphResourceDesc* desc);
FVIZ_RENDERING_API void fviz_render_graph_statistics_initialize(FVizRenderGraphStatistics* statistics);
FVIZ_RENDERING_API FVizResult fviz_render_graph_create(FVizRenderGraph** out_graph);
FVIZ_RENDERING_API void fviz_render_graph_clear(FVizRenderGraph* graph);

FVIZ_RENDERING_API FVizResult fviz_render_graph_add_resource(FVizRenderGraph* graph, const FVizRenderGraphResourceDesc* desc,
                                                   FVizRenderGraphResourceId* out_resource_id);
FVIZ_RENDERING_API FVizResult fviz_render_graph_add_pass(FVizRenderGraph* graph, const char* name, FVizRenderPass* pass,
                                               FVizRenderGraphPassId* out_pass_id);
FVIZ_RENDERING_API FVizResult fviz_render_graph_add_dependency(FVizRenderGraph* graph, FVizRenderGraphPassId before,
                                                     FVizRenderGraphPassId after);
FVIZ_RENDERING_API FVizResult fviz_render_graph_use_resource(FVizRenderGraph* graph, FVizRenderGraphPassId pass_id,
                                                   FVizRenderGraphResourceId resource_id, FVizRenderGraphAccess access);

/* Compilation validates references and hazards, rejects cycles and reads of
 * uninitialized internal resources, computes a stable execution order, and
 * aliases compatible transient targets whose live ranges do not overlap. */
FVIZ_RENDERING_API FVizResult fviz_render_graph_compile(FVizRenderGraph* graph);
FVIZ_RENDERING_API FVizBool fviz_render_graph_is_compiled(const FVizRenderGraph* graph);
FVIZ_RENDERING_API FVizSize fviz_render_graph_execution_count(const FVizRenderGraph* graph);
FVIZ_RENDERING_API FVizRenderGraphPassId fviz_render_graph_execution_pass_id(const FVizRenderGraph* graph,
                                                                   FVizSize execution_index);
FVIZ_RENDERING_API FVizRenderPass* fviz_render_graph_execution_pass(const FVizRenderGraph* graph, FVizSize execution_index);
FVIZ_RENDERING_API const char* fviz_render_graph_pass_name(const FVizRenderGraph* graph, FVizRenderGraphPassId pass_id);
FVIZ_RENDERING_API const char* fviz_render_graph_resource_name(const FVizRenderGraph* graph,
                                                     FVizRenderGraphResourceId resource_id);
FVIZ_RENDERING_API FVizResult fviz_render_graph_resource_lifetime(const FVizRenderGraph* graph,
                                                        FVizRenderGraphResourceId resource_id,
                                                        FVizSize* out_first_execution, FVizSize* out_last_execution);
FVIZ_RENDERING_API uint32_t fviz_render_graph_resource_physical_slot(const FVizRenderGraph* graph,
                                                           FVizRenderGraphResourceId resource_id);
FVIZ_RENDERING_API FVizRenderTarget* fviz_render_graph_physical_target(FVizRenderGraph* graph, uint32_t physical_slot);
FVIZ_RENDERING_API void fviz_render_graph_get_statistics(const FVizRenderGraph* graph, FVizRenderGraphStatistics* out_statistics);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_RENDER_GRAPH_H */
