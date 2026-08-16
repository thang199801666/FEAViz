#ifndef FVIZ_INTERNAL_RENDERING_RENDER_GRAPH_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_RENDER_GRAPH_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Rendering/FVizRenderGraph.h>

typedef struct FVizRenderGraphUseRecord
{
    FVizRenderGraphResourceId resource_id;
    FVizRenderGraphAccess access;
} FVizRenderGraphUseRecord;

typedef struct FVizRenderGraphPassRecord
{
    FVizString* name;
    FVizRenderPass* pass;
    FVizArray* dependencies;
    FVizArray* uses;
} FVizRenderGraphPassRecord;

typedef struct FVizRenderGraphResourceRecord
{
    FVizString* name;
    FVizRenderTargetDesc target;
    FVizBool external;
    FVizBool transient_resource;
    FVizSize first_execution;
    FVizSize last_execution;
    uint32_t physical_slot;
} FVizRenderGraphResourceRecord;

struct FVizRenderGraph
{
    FVizObject base;
    FVizArray* passes;
    FVizArray* resources;
    FVizArray* execution_order;
    FVizArray* physical_targets;
    FVizRenderGraphStatistics statistics;
};

#endif /* FVIZ_INTERNAL_RENDERING_RENDER_GRAPH_PRIVATE_H */
