#ifndef FVIZ_INTERNAL_PIPELINE_ALGORITHM_PRIVATE_H
#define FVIZ_INTERNAL_PIPELINE_ALGORITHM_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

typedef struct FVizTaskContext FVizTaskContext;

typedef FVizResult (*FVizAlgorithmExecuteFn)(FVizAlgorithm* algorithm);

typedef struct FVizAlgorithmConnection
{
    FVizAlgorithm* producer;
    uint32_t output_port;
    FVizObserverTag producer_modified_tag;
} FVizAlgorithmConnection;

typedef struct FVizAlgorithmInputPort
{
    FVizAlgorithmPortInfo info;
    FVizDataObject* direct_data;
    FVizObserverTag direct_data_modified_tag;
    FVizArray* connections;
} FVizAlgorithmInputPort;

typedef struct FVizAlgorithmOutputPort
{
    FVizAlgorithmPortInfo info;
    FVizDataObject* data;
    FVizArray* time_steps;
    int64_t whole_extent[6];
    FVizBool has_whole_extent;
    FVizMTime last_input_mtime;
    FVizMTime last_algorithm_mtime;
    uint64_t last_request_key;
    FVizBool updated;
} FVizAlgorithmOutputPort;

struct FVizAlgorithmOutput
{
    FVizAlgorithm* producer;
    uint32_t port;
};

struct FVizAlgorithm
{
    FVizObject base;
    uint32_t input_port_count;
    uint32_t output_port_count;
    FVizAlgorithmInputPort* input_ports;
    FVizAlgorithmOutputPort* output_ports;
    FVizAlgorithmOutput* output_proxies;
    FVizAlgorithmExecuteFn execute;
    FVizAlgorithmCallbacks callbacks;
    void* state;
    FVizObject* observed_state_object;
    FVizObserverTag observed_state_modified_tag;
    FVizObserverTag observed_state_delete_tag;
    FVizBool custom;
    FVizExecutive* executive;
    FVizAlgorithmProgressFn progress_callback;
    void* progress_user_data;
    double progress;
    /* While a context-based async update is running on a worker, report_progress
     * forwards monotonic values to this task context so the owning future exposes
     * live pipeline progress. Set/cleared by the async worker under the documented
     * externally-synchronized pipeline mutation contract. */
    FVizTaskContext* async_progress_context;
    double async_progress_last;
    FVizAtomicU32 abort_requested;
    FVizMTime last_input_mtime;
    FVizMTime last_algorithm_mtime;
    FVizBool updated;
    FVizBool updating;
    FVizBool last_update_executed;
    uint64_t diagnostic_id;
};

extern const FVizObjectClass g_fviz_algorithm_class;

FVizResult fviz_internal_algorithm_initialize(
    FVizAlgorithm* algorithm,
    uint32_t input_port_count,
    uint32_t output_port_count,
    FVizAlgorithmExecuteFn execute);
void fviz_internal_algorithm_deinitialize(FVizAlgorithm* algorithm);
FVizResult fviz_internal_algorithm_configure_input_port(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizTypeId data_type,
    FVizBool optional,
    FVizBool repeatable);
FVizResult fviz_internal_algorithm_configure_output_port(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizTypeId data_type);
FVizDataObject* fviz_internal_algorithm_resolved_input(
    FVizAlgorithm* algorithm,
    uint32_t port,
    uint32_t connection);
FVizResult fviz_internal_algorithm_set_output_data(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizDataObject* data_object);
FVizResult fviz_internal_algorithm_update_now(FVizAlgorithm* algorithm);
FVizResult fviz_internal_algorithm_map_input_request(
    FVizAlgorithm* algorithm,
    uint32_t input_port,
    uint32_t connection,
    const FVizPipelineRequestInfo* downstream_request,
    FVizPipelineRequestInfo* upstream_request);
FVizResult fviz_internal_algorithm_process_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    FVizMTime input_mtime,
    uint64_t request_key,
    FVizBool* out_executed);

#endif /* FVIZ_INTERNAL_PIPELINE_ALGORITHM_PRIVATE_H */
