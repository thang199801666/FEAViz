#include <stddef.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Rendering/FVizRenderGraph.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRenderGraphPrivate.h>

typedef struct FVizRenderGraphPhysicalSlot
{
    FVizRenderTargetDesc desc;
    FVizSize last_execution;
    uint32_t slot;
} FVizRenderGraphPhysicalSlot;

static void fviz_render_graph_invalidate(FVizRenderGraph* graph);

static void fviz_render_graph_release_passes(FVizRenderGraph* graph)
{
    FVizSize i;
    if (graph == NULL || graph->passes == NULL) return;
    for (i = 0u; i < fviz_array_count(graph->passes); ++i)
    {
        FVizRenderGraphPassRecord* record = (FVizRenderGraphPassRecord*)fviz_array_at(graph->passes, i);
        fviz_release(record->name);
        fviz_release(record->pass);
        fviz_release(record->dependencies);
        fviz_release(record->uses);
    }
    fviz_array_clear(graph->passes);
}

static void fviz_render_graph_release_resources(FVizRenderGraph* graph)
{
    FVizSize i;
    if (graph == NULL || graph->resources == NULL) return;
    for (i = 0u; i < fviz_array_count(graph->resources); ++i)
    {
        FVizRenderGraphResourceRecord* record = (FVizRenderGraphResourceRecord*)fviz_array_at(graph->resources, i);
        fviz_release(record->name);
    }
    fviz_array_clear(graph->resources);
}

static void fviz_render_graph_release_physical_targets(FVizRenderGraph* graph)
{
    FVizSize i;
    if (graph == NULL || graph->physical_targets == NULL) return;
    for (i = 0u; i < fviz_array_count(graph->physical_targets); ++i)
        fviz_release(*(FVizRenderTarget**)fviz_array_at(graph->physical_targets, i));
    fviz_array_clear(graph->physical_targets);
}

static void fviz_render_graph_destroy(FVizObject* object)
{
    FVizRenderGraph* graph = (FVizRenderGraph*)object;
    fviz_render_graph_release_passes(graph);
    fviz_render_graph_release_resources(graph);
    fviz_render_graph_release_physical_targets(graph);
    fviz_release(graph->passes);
    fviz_release(graph->resources);
    fviz_release(graph->execution_order);
    fviz_release(graph->physical_targets);
}

static const FVizObjectClass g_fviz_render_graph_class = {FVIZ_TYPE_RENDER_GRAPH, "FVizRenderGraph",
                                                          &g_fviz_object_class, fviz_render_graph_destroy, NULL};

void fviz_render_graph_resource_desc_initialize(FVizRenderGraphResourceDesc* desc)
{
    if (desc == NULL) return;
    (void)memset(desc, 0, sizeof(*desc));
    desc->struct_size = (uint32_t)sizeof(*desc);
    desc->name = "resource";
    fviz_render_target_desc_initialize(&desc->target);
    desc->transient_resource = FVIZ_TRUE;
}

void fviz_render_graph_statistics_initialize(FVizRenderGraphStatistics* statistics)
{
    if (statistics == NULL) return;
    (void)memset(statistics, 0, sizeof(*statistics));
    statistics->struct_size = (uint32_t)sizeof(*statistics);
}

FVizResult fviz_render_graph_create(FVizRenderGraph** out_graph)
{
    FVizRenderGraph* graph;
    if (out_graph == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_graph = NULL;
    graph = (FVizRenderGraph*)fviz_internal_object_allocate(sizeof(*graph), &g_fviz_render_graph_class, NULL);
    if (graph == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizRenderGraphPassRecord), &graph->passes) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizRenderGraphResourceRecord), &graph->resources) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizRenderGraphPassId), &graph->execution_order) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizRenderTarget*), &graph->physical_targets) != FVIZ_OK)
    {
        fviz_release(graph);
        return fviz_last_error_code();
    }
    fviz_render_graph_statistics_initialize(&graph->statistics);
    *out_graph = graph;
    return FVIZ_OK;
}

static void fviz_render_graph_invalidate(FVizRenderGraph* graph)
{
    if (graph == NULL) return;
    fviz_array_clear(graph->execution_order);
    fviz_render_graph_release_physical_targets(graph);
    graph->statistics.compiled = FVIZ_FALSE;
    graph->statistics.dependency_edge_count = 0u;
    graph->statistics.physical_target_count = 0u;
    graph->statistics.logical_transient_bytes = 0u;
    graph->statistics.peak_physical_target_bytes = 0u;
    fviz_object_modified((FVizObject*)graph);
}

void fviz_render_graph_clear(FVizRenderGraph* graph)
{
    if (graph == NULL) return;
    fviz_render_graph_release_passes(graph);
    fviz_render_graph_release_resources(graph);
    fviz_render_graph_invalidate(graph);
    graph->statistics.pass_count = 0u;
    graph->statistics.resource_count = 0u;
}

static FVizBool fviz_render_graph_valid_pass_id(const FVizRenderGraph* graph, FVizRenderGraphPassId id)
{
    return graph != NULL && id != FVIZ_RENDER_GRAPH_PASS_ID_INVALID &&
                   id <= (FVizRenderGraphPassId)fviz_array_count(graph->passes)
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

static FVizBool fviz_render_graph_valid_resource_id(const FVizRenderGraph* graph, FVizRenderGraphResourceId id)
{
    return graph != NULL && id != FVIZ_RENDER_GRAPH_RESOURCE_ID_INVALID &&
                   id <= (FVizRenderGraphResourceId)fviz_array_count(graph->resources)
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizResult fviz_render_graph_add_resource(FVizRenderGraph* graph, const FVizRenderGraphResourceDesc* desc,
                                          FVizRenderGraphResourceId* out_resource_id)
{
    FVizRenderGraphResourceRecord record;
    FVizResult result;
    if (graph == NULL || desc == NULL || out_resource_id == NULL || desc->struct_size < sizeof(*desc) ||
        desc->name == NULL || desc->name[0] == '\0')
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_resource_id = FVIZ_RENDER_GRAPH_RESOURCE_ID_INVALID;
    result = fviz_render_target_desc_validate(&desc->target);
    if (result != FVIZ_OK) return result;
    (void)memset(&record, 0, sizeof(record));
    result = fviz_string_create_from(desc->name, &record.name);
    if (result != FVIZ_OK) return result;
    record.target = desc->target;
    record.target.struct_size = (uint32_t)sizeof(record.target);
    record.external = desc->external != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    record.transient_resource = desc->transient_resource != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    record.first_execution = SIZE_MAX;
    record.last_execution = SIZE_MAX;
    record.physical_slot = FVIZ_RENDER_GRAPH_PHYSICAL_SLOT_INVALID;
    result = fviz_array_push(graph->resources, &record);
    if (result != FVIZ_OK)
    {
        fviz_release(record.name);
        return result;
    }
    *out_resource_id = (FVizRenderGraphResourceId)fviz_array_count(graph->resources);
    graph->statistics.resource_count = (uint32_t)fviz_array_count(graph->resources);
    fviz_render_graph_invalidate(graph);
    return FVIZ_OK;
}

FVizResult fviz_render_graph_add_pass(FVizRenderGraph* graph, const char* name, FVizRenderPass* pass,
                                      FVizRenderGraphPassId* out_pass_id)
{
    FVizRenderGraphPassRecord record;
    FVizResult result;
    if (graph == NULL || name == NULL || name[0] == '\0' || pass == NULL || out_pass_id == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_pass_id = FVIZ_RENDER_GRAPH_PASS_ID_INVALID;
    (void)memset(&record, 0, sizeof(record));
    if (fviz_string_create_from(name, &record.name) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizRenderGraphPassId), &record.dependencies) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizRenderGraphUseRecord), &record.uses) != FVIZ_OK || fviz_retain(pass) == NULL)
    {
        fviz_release(record.name);
        fviz_release(record.dependencies);
        fviz_release(record.uses);
        return fviz_last_error_code();
    }
    record.pass = pass;
    result = fviz_array_push(graph->passes, &record);
    if (result != FVIZ_OK)
    {
        fviz_release(record.name);
        fviz_release(record.pass);
        fviz_release(record.dependencies);
        fviz_release(record.uses);
        return result;
    }
    *out_pass_id = (FVizRenderGraphPassId)fviz_array_count(graph->passes);
    graph->statistics.pass_count = (uint32_t)fviz_array_count(graph->passes);
    fviz_render_graph_invalidate(graph);
    return FVIZ_OK;
}

FVizResult fviz_render_graph_add_dependency(FVizRenderGraph* graph, FVizRenderGraphPassId before,
                                            FVizRenderGraphPassId after)
{
    FVizRenderGraphPassRecord* record;
    FVizSize i;
    if (fviz_render_graph_valid_pass_id(graph, before) == FVIZ_FALSE ||
        fviz_render_graph_valid_pass_id(graph, after) == FVIZ_FALSE || before == after)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    record = (FVizRenderGraphPassRecord*)fviz_array_at(graph->passes, (FVizSize)(after - 1u));
    for (i = 0u; i < fviz_array_count(record->dependencies); ++i)
        if (*(const FVizRenderGraphPassId*)fviz_array_const_at(record->dependencies, i) == before) return FVIZ_OK;
    if (fviz_array_push(record->dependencies, &before) != FVIZ_OK) return fviz_last_error_code();
    fviz_render_graph_invalidate(graph);
    return FVIZ_OK;
}

FVizResult fviz_render_graph_use_resource(FVizRenderGraph* graph, FVizRenderGraphPassId pass_id,
                                          FVizRenderGraphResourceId resource_id, FVizRenderGraphAccess access)
{
    FVizRenderGraphPassRecord* record;
    FVizSize i;
    FVizRenderGraphUseRecord use;
    if (fviz_render_graph_valid_pass_id(graph, pass_id) == FVIZ_FALSE ||
        fviz_render_graph_valid_resource_id(graph, resource_id) == FVIZ_FALSE ||
        (access != FVIZ_RENDER_GRAPH_READ && access != FVIZ_RENDER_GRAPH_WRITE &&
         access != FVIZ_RENDER_GRAPH_READ_WRITE))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    record = (FVizRenderGraphPassRecord*)fviz_array_at(graph->passes, (FVizSize)(pass_id - 1u));
    for (i = 0u; i < fviz_array_count(record->uses); ++i)
    {
        FVizRenderGraphUseRecord* existing = (FVizRenderGraphUseRecord*)fviz_array_at(record->uses, i);
        if (existing->resource_id == resource_id)
        {
            existing->access = (FVizRenderGraphAccess)(existing->access | access);
            fviz_render_graph_invalidate(graph);
            return FVIZ_OK;
        }
    }
    use.resource_id = resource_id;
    use.access = access;
    if (fviz_array_push(record->uses, &use) != FVIZ_OK) return fviz_last_error_code();
    fviz_render_graph_invalidate(graph);
    return FVIZ_OK;
}

static FVizBool fviz_render_target_desc_equal(const FVizRenderTargetDesc* a, const FVizRenderTargetDesc* b)
{
    uint32_t i;
    if (a->width != b->width || a->height != b->height || a->samples != b->samples ||
        a->attachment_count != b->attachment_count)
        return FVIZ_FALSE;
    for (i = 0u; i < a->attachment_count; ++i)
    {
        if (a->attachments[i].point != b->attachments[i].point ||
            a->attachments[i].format != b->attachments[i].format ||
            a->attachments[i].sampled != b->attachments[i].sampled)
            return FVIZ_FALSE;
    }
    return FVIZ_TRUE;
}

static FVizResult fviz_render_graph_add_edge(uint8_t* edges, uint32_t* indegrees, FVizSize count, FVizSize before,
                                             FVizSize after, uint32_t* edge_count)
{
    if (before >= count || after >= count || before == after) return FVIZ_ERROR_INVALID_STATE;
    if (edges[before * count + after] == 0u)
    {
        edges[before * count + after] = 1u;
        if (indegrees[after] == UINT32_MAX || *edge_count == UINT32_MAX) return FVIZ_ERROR_OVERFLOW;
        ++indegrees[after];
        ++(*edge_count);
    }
    return FVIZ_OK;
}

static FVizBool fviz_render_graph_pass_uses_resource(const FVizRenderGraphPassRecord* pass,
                                                     FVizRenderGraphResourceId resource_id,
                                                     FVizRenderGraphAccess* out_access)
{
    FVizSize i;
    for (i = 0u; i < fviz_array_count(pass->uses); ++i)
    {
        const FVizRenderGraphUseRecord* use = (const FVizRenderGraphUseRecord*)fviz_array_const_at(pass->uses, i);
        if (use->resource_id == resource_id)
        {
            if (out_access != NULL) *out_access = use->access;
            return FVIZ_TRUE;
        }
    }
    return FVIZ_FALSE;
}

static FVizResult fviz_render_graph_compile_edges(FVizRenderGraph* graph, uint8_t* edges, uint32_t* indegrees,
                                                  uint32_t* edge_count)
{
    const FVizSize pass_count = fviz_array_count(graph->passes);
    const FVizSize resource_count = fviz_array_count(graph->resources);
    FVizSize i;
    FVizSize j;
    for (i = 0u; i < pass_count; ++i)
    {
        const FVizRenderGraphPassRecord* pass = (const FVizRenderGraphPassRecord*)fviz_array_const_at(graph->passes, i);
        for (j = 0u; j < fviz_array_count(pass->dependencies); ++j)
        {
            const FVizRenderGraphPassId dependency =
                *(const FVizRenderGraphPassId*)fviz_array_const_at(pass->dependencies, j);
            FVizResult result;
            if (fviz_render_graph_valid_pass_id(graph, dependency) == FVIZ_FALSE) return FVIZ_ERROR_INVALID_STATE;
            result =
                fviz_render_graph_add_edge(edges, indegrees, pass_count, (FVizSize)(dependency - 1u), i, edge_count);
            if (result != FVIZ_OK) return result;
        }
    }
    for (i = 0u; i < resource_count; ++i)
    {
        const FVizRenderGraphResourceId resource_id = (FVizRenderGraphResourceId)(i + 1u);
        const FVizRenderGraphResourceRecord* resource =
            (const FVizRenderGraphResourceRecord*)fviz_array_const_at(graph->resources, i);
        FVizBool initialized = resource->external;
        for (j = 0u; j < pass_count; ++j)
        {
            const FVizRenderGraphPassRecord* current =
                (const FVizRenderGraphPassRecord*)fviz_array_const_at(graph->passes, j);
            FVizRenderGraphAccess current_access;
            FVizSize previous;
            if (fviz_render_graph_pass_uses_resource(current, resource_id, &current_access) == FVIZ_FALSE) continue;
            if ((current_access & FVIZ_RENDER_GRAPH_READ) != 0 && initialized == FVIZ_FALSE)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                        "render graph reads an internal resource before its first write");
                return FVIZ_ERROR_INVALID_STATE;
            }
            for (previous = 0u; previous < j; ++previous)
            {
                const FVizRenderGraphPassRecord* prior =
                    (const FVizRenderGraphPassRecord*)fviz_array_const_at(graph->passes, previous);
                FVizRenderGraphAccess prior_access;
                if (fviz_render_graph_pass_uses_resource(prior, resource_id, &prior_access) != FVIZ_FALSE &&
                    (((prior_access & FVIZ_RENDER_GRAPH_WRITE) != 0) ||
                     ((current_access & FVIZ_RENDER_GRAPH_WRITE) != 0)))
                {
                    FVizResult result =
                        fviz_render_graph_add_edge(edges, indegrees, pass_count, previous, j, edge_count);
                    if (result != FVIZ_OK) return result;
                }
            }
            if ((current_access & FVIZ_RENDER_GRAPH_WRITE) != 0) initialized = FVIZ_TRUE;
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_render_graph_sort(FVizRenderGraph* graph, const uint8_t* edges, uint32_t* indegrees)
{
    const FVizSize count = fviz_array_count(graph->passes);
    uint8_t* emitted;
    FVizSize output;
    emitted = count != 0u ? (uint8_t*)fviz_alloc(count) : NULL;
    if (count != 0u && emitted == NULL) return fviz_last_error_code();
    if (emitted != NULL) (void)memset(emitted, 0, (size_t)count);
    for (output = 0u; output < count; ++output)
    {
        FVizSize candidate;
        FVizSize next;
        FVizRenderGraphPassId id;
        for (candidate = 0u; candidate < count; ++candidate)
            if (emitted[candidate] == 0u && indegrees[candidate] == 0u) break;
        if (candidate == count)
        {
            fviz_free(emitted);
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "render graph contains a dependency cycle");
            return FVIZ_ERROR_INVALID_STATE;
        }
        emitted[candidate] = 1u;
        id = (FVizRenderGraphPassId)(candidate + 1u);
        if (fviz_array_push(graph->execution_order, &id) != FVIZ_OK)
        {
            fviz_free(emitted);
            return fviz_last_error_code();
        }
        for (next = 0u; next < count; ++next)
        {
            if (edges[candidate * count + next] != 0u)
            {
                if (indegrees[next] == 0u)
                {
                    fviz_free(emitted);
                    return FVIZ_ERROR_INTERNAL;
                }
                --indegrees[next];
            }
        }
    }
    fviz_free(emitted);
    return FVIZ_OK;
}

static FVizResult fviz_render_graph_compute_lifetimes(FVizRenderGraph* graph)
{
    FVizSize resource_index;
    FVizSize execution;
    for (resource_index = 0u; resource_index < fviz_array_count(graph->resources); ++resource_index)
    {
        FVizRenderGraphResourceRecord* resource =
            (FVizRenderGraphResourceRecord*)fviz_array_at(graph->resources, resource_index);
        resource->first_execution = SIZE_MAX;
        resource->last_execution = SIZE_MAX;
        resource->physical_slot = FVIZ_RENDER_GRAPH_PHYSICAL_SLOT_INVALID;
    }
    for (execution = 0u; execution < fviz_array_count(graph->execution_order); ++execution)
    {
        const FVizRenderGraphPassId pass_id =
            *(const FVizRenderGraphPassId*)fviz_array_const_at(graph->execution_order, execution);
        const FVizRenderGraphPassRecord* pass =
            (const FVizRenderGraphPassRecord*)fviz_array_const_at(graph->passes, (FVizSize)(pass_id - 1u));
        FVizSize use_index;
        for (use_index = 0u; use_index < fviz_array_count(pass->uses); ++use_index)
        {
            const FVizRenderGraphUseRecord* use =
                (const FVizRenderGraphUseRecord*)fviz_array_const_at(pass->uses, use_index);
            FVizRenderGraphResourceRecord* resource =
                (FVizRenderGraphResourceRecord*)fviz_array_at(graph->resources, (FVizSize)(use->resource_id - 1u));
            if (resource->first_execution == SIZE_MAX) resource->first_execution = execution;
            resource->last_execution = execution;
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_render_graph_allocate_targets(FVizRenderGraph* graph)
{
    FVizArray* slots = NULL;
    FVizSize resource_index;
    FVizResult result = fviz_array_create(sizeof(FVizRenderGraphPhysicalSlot), &slots);
    if (result != FVIZ_OK) return result;
    for (resource_index = 0u; resource_index < fviz_array_count(graph->resources); ++resource_index)
    {
        FVizRenderGraphResourceRecord* resource =
            (FVizRenderGraphResourceRecord*)fviz_array_at(graph->resources, resource_index);
        FVizSize slot_index;
        FVizRenderTarget* logical_target = NULL;
        uint64_t bytes;
        if (resource->external != FVIZ_FALSE || resource->first_execution == SIZE_MAX) continue;
        result = fviz_render_target_create(&resource->target, &logical_target);
        if (result != FVIZ_OK) break;
        bytes = fviz_render_target_estimated_bytes(logical_target);
        fviz_release(logical_target);
        if (resource->transient_resource != FVIZ_FALSE)
        {
            if (UINT64_MAX - graph->statistics.logical_transient_bytes < bytes)
                graph->statistics.logical_transient_bytes = UINT64_MAX;
            else
                graph->statistics.logical_transient_bytes += bytes;
        }
        for (slot_index = 0u; slot_index < fviz_array_count(slots); ++slot_index)
        {
            FVizRenderGraphPhysicalSlot* slot = (FVizRenderGraphPhysicalSlot*)fviz_array_at(slots, slot_index);
            if (resource->transient_resource != FVIZ_FALSE && slot->last_execution < resource->first_execution &&
                fviz_render_target_desc_equal(&slot->desc, &resource->target) != FVIZ_FALSE)
            {
                resource->physical_slot = slot->slot;
                slot->last_execution = resource->last_execution;
                break;
            }
        }
        if (resource->physical_slot == FVIZ_RENDER_GRAPH_PHYSICAL_SLOT_INVALID)
        {
            FVizRenderGraphPhysicalSlot slot;
            FVizRenderTarget* target = NULL;
            result = fviz_render_target_create(&resource->target, &target);
            if (result != FVIZ_OK) break;
            resource->physical_slot = (uint32_t)fviz_array_count(graph->physical_targets);
            result = fviz_array_push(graph->physical_targets, &target);
            if (result != FVIZ_OK)
            {
                fviz_release(target);
                break;
            }
            slot.desc = resource->target;
            slot.last_execution = resource->last_execution;
            slot.slot = resource->physical_slot;
            result = fviz_array_push(slots, &slot);
            if (result != FVIZ_OK) break;
            if (UINT64_MAX - graph->statistics.peak_physical_target_bytes < bytes)
                graph->statistics.peak_physical_target_bytes = UINT64_MAX;
            else
                graph->statistics.peak_physical_target_bytes += bytes;
        }
    }
    fviz_release(slots);
    return result;
}

FVizResult fviz_render_graph_compile(FVizRenderGraph* graph)
{
    FVizSize count;
    uint8_t* edges = NULL;
    uint32_t* indegrees = NULL;
    uint32_t edge_count = 0u;
    FVizResult result;
    if (graph == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_render_graph_invalidate(graph);
    count = fviz_array_count(graph->passes);
    if (count > UINT32_MAX) return FVIZ_ERROR_OVERFLOW;
    if (count != 0u && count > (FVizSize)(SIZE_MAX / count)) return FVIZ_ERROR_OVERFLOW;
    if (count != 0u)
    {
        edges = (uint8_t*)fviz_alloc(count * count);
        indegrees = (uint32_t*)fviz_alloc(count * sizeof(*indegrees));
        if (edges == NULL || indegrees == NULL)
        {
            fviz_free(indegrees);
            fviz_free(edges);
            return fviz_last_error_code();
        }
        (void)memset(edges, 0, (size_t)(count * count));
        (void)memset(indegrees, 0, (size_t)count * sizeof(*indegrees));
    }
    result = fviz_render_graph_compile_edges(graph, edges, indegrees, &edge_count);
    if (result == FVIZ_OK) result = fviz_render_graph_sort(graph, edges, indegrees);
    if (result == FVIZ_OK) result = fviz_render_graph_compute_lifetimes(graph);
    if (result == FVIZ_OK) result = fviz_render_graph_allocate_targets(graph);
    fviz_free(indegrees);
    fviz_free(edges);
    if (result != FVIZ_OK)
    {
        fviz_render_graph_invalidate(graph);
        return result;
    }
    graph->statistics.pass_count = (uint32_t)fviz_array_count(graph->passes);
    graph->statistics.resource_count = (uint32_t)fviz_array_count(graph->resources);
    graph->statistics.dependency_edge_count = edge_count;
    graph->statistics.physical_target_count = (uint32_t)fviz_array_count(graph->physical_targets);
    if (graph->statistics.compile_generation != UINT64_MAX) ++graph->statistics.compile_generation;
    graph->statistics.compiled = FVIZ_TRUE;
    return FVIZ_OK;
}

FVizBool fviz_render_graph_is_compiled(const FVizRenderGraph* graph)
{
    return graph != NULL ? graph->statistics.compiled : FVIZ_FALSE;
}

FVizSize fviz_render_graph_execution_count(const FVizRenderGraph* graph)
{
    return graph != NULL && graph->statistics.compiled != FVIZ_FALSE ? fviz_array_count(graph->execution_order) : 0u;
}

FVizRenderGraphPassId fviz_render_graph_execution_pass_id(const FVizRenderGraph* graph, FVizSize execution_index)
{
    const FVizRenderGraphPassId* id =
        graph != NULL && graph->statistics.compiled != FVIZ_FALSE
            ? (const FVizRenderGraphPassId*)fviz_array_const_at(graph->execution_order, execution_index)
            : NULL;
    return id != NULL ? *id : FVIZ_RENDER_GRAPH_PASS_ID_INVALID;
}

FVizRenderPass* fviz_render_graph_execution_pass(const FVizRenderGraph* graph, FVizSize execution_index)
{
    const FVizRenderGraphPassId id = fviz_render_graph_execution_pass_id(graph, execution_index);
    const FVizRenderGraphPassRecord* record =
        id != FVIZ_RENDER_GRAPH_PASS_ID_INVALID
            ? (const FVizRenderGraphPassRecord*)fviz_array_const_at(graph->passes, (FVizSize)(id - 1u))
            : NULL;
    return record != NULL ? record->pass : NULL;
}

const char* fviz_render_graph_pass_name(const FVizRenderGraph* graph, FVizRenderGraphPassId pass_id)
{
    const FVizRenderGraphPassRecord* record =
        fviz_render_graph_valid_pass_id(graph, pass_id) != FVIZ_FALSE
            ? (const FVizRenderGraphPassRecord*)fviz_array_const_at(graph->passes, (FVizSize)(pass_id - 1u))
            : NULL;
    return record != NULL ? fviz_string_c_str(record->name) : NULL;
}

const char* fviz_render_graph_resource_name(const FVizRenderGraph* graph, FVizRenderGraphResourceId resource_id)
{
    const FVizRenderGraphResourceRecord* record =
        fviz_render_graph_valid_resource_id(graph, resource_id) != FVIZ_FALSE
            ? (const FVizRenderGraphResourceRecord*)fviz_array_const_at(graph->resources, (FVizSize)(resource_id - 1u))
            : NULL;
    return record != NULL ? fviz_string_c_str(record->name) : NULL;
}

FVizResult fviz_render_graph_resource_lifetime(const FVizRenderGraph* graph, FVizRenderGraphResourceId resource_id,
                                               FVizSize* out_first_execution, FVizSize* out_last_execution)
{
    const FVizRenderGraphResourceRecord* resource;
    if (fviz_render_graph_valid_resource_id(graph, resource_id) == FVIZ_FALSE || out_first_execution == NULL ||
        out_last_execution == NULL || graph->statistics.compiled == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    resource =
        (const FVizRenderGraphResourceRecord*)fviz_array_const_at(graph->resources, (FVizSize)(resource_id - 1u));
    if (resource->first_execution == SIZE_MAX) return FVIZ_ERROR_NOT_FOUND;
    *out_first_execution = resource->first_execution;
    *out_last_execution = resource->last_execution;
    return FVIZ_OK;
}

uint32_t fviz_render_graph_resource_physical_slot(const FVizRenderGraph* graph, FVizRenderGraphResourceId resource_id)
{
    const FVizRenderGraphResourceRecord* resource =
        fviz_render_graph_valid_resource_id(graph, resource_id) != FVIZ_FALSE &&
                graph->statistics.compiled != FVIZ_FALSE
            ? (const FVizRenderGraphResourceRecord*)fviz_array_const_at(graph->resources, (FVizSize)(resource_id - 1u))
            : NULL;
    return resource != NULL ? resource->physical_slot : FVIZ_RENDER_GRAPH_PHYSICAL_SLOT_INVALID;
}

FVizRenderTarget* fviz_render_graph_physical_target(FVizRenderGraph* graph, uint32_t physical_slot)
{
    FVizRenderTarget** target =
        graph != NULL && graph->statistics.compiled != FVIZ_FALSE
            ? (FVizRenderTarget**)fviz_array_at(graph->physical_targets, (FVizSize)physical_slot)
            : NULL;
    return target != NULL ? *target : NULL;
}

void fviz_render_graph_get_statistics(const FVizRenderGraph* graph, FVizRenderGraphStatistics* out_statistics)
{
    if (out_statistics == NULL) return;
    fviz_render_graph_statistics_initialize(out_statistics);
    if (graph != NULL) *out_statistics = graph->statistics;
}
