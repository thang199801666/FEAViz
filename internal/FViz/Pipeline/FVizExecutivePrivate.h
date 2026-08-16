#ifndef FVIZ_INTERNAL_PIPELINE_EXECUTIVE_PRIVATE_H
#define FVIZ_INTERNAL_PIPELINE_EXECUTIVE_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizArena.h>
#include <FViz/Pipeline/FVizExecutive.h>

struct FVizExecutive
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizPipelineRequest last_request;
    FVizResult last_result;
    uint64_t execution_count;
    uint64_t cache_hit_count;
    uint64_t last_transaction_id;
    FVizArena* scratch_arena;
};

FVizResult fviz_internal_executive_create(FVizAlgorithm* algorithm, FVizExecutive** out_executive);

#endif /* FVIZ_INTERNAL_PIPELINE_EXECUTIVE_PRIVATE_H */
