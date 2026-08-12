#ifndef FVIZ_INTERNAL_PIPELINE_ALGORITHM_PRIVATE_H
#define FVIZ_INTERNAL_PIPELINE_ALGORITHM_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

typedef FVizResult (*FVizAlgorithmExecuteFn)(FVizAlgorithm* algorithm);

typedef struct FVizAlgorithmConnection
{
    FVizAlgorithm* producer;
    uint32_t output_port;
} FVizAlgorithmConnection;

typedef struct FVizAlgorithmInputPort
{
    FVizAlgorithmPortInfo info;
    FVizDataObject* direct_data;
    FVizArray* connections;
} FVizAlgorithmInputPort;

typedef struct FVizAlgorithmOutputPort
{
    FVizAlgorithmPortInfo info;
    FVizDataObject* data;
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
    FVizExecutive* executive;
    FVizAlgorithmProgressFn progress_callback;
    void* progress_user_data;
    double progress;
    FVizAtomicU32 abort_requested;
    FVizMTime last_input_mtime;
    FVizMTime last_algorithm_mtime;
    FVizBool updated;
    FVizBool updating;
    FVizBool last_update_executed;
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

#endif /* FVIZ_INTERNAL_PIPELINE_ALGORITHM_PRIVATE_H */
