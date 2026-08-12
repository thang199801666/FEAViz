#ifndef FVIZ_PIPELINE_ALGORITHM_H
#define FVIZ_PIPELINE_ALGORITHM_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataObject.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizAlgorithm FVizAlgorithm;
typedef struct FVizAlgorithmOutput FVizAlgorithmOutput;
typedef struct FVizExecutive FVizExecutive;
typedef struct FVizPipelineRequestInfo FVizPipelineRequestInfo;

#define FVIZ_TYPE_ALGORITHM UINT64_C(0xA6213C94D7E850BF)

typedef struct FVizAlgorithmPortInfo
{
    FVizTypeId data_type;
    FVizBool optional;
    FVizBool repeatable;
} FVizAlgorithmPortInfo;

typedef void (*FVizAlgorithmProgressFn)(
    FVizAlgorithm* algorithm,
    double progress,
    void* user_data);

typedef FVizResult (*FVizAlgorithmProcessRequestFn)(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* state);
typedef void (*FVizAlgorithmDestroyStateFn)(void* state);
typedef FVizMTime (*FVizAlgorithmGetStateMTimeFn)(const void* state);

typedef struct FVizAlgorithmCallbacks
{
    uint32_t struct_size;
    FVizAlgorithmProcessRequestFn process_request;
    FVizAlgorithmDestroyStateFn destroy_state;
    FVizAlgorithmGetStateMTimeFn get_state_mtime;
} FVizAlgorithmCallbacks;

FVIZ_API void fviz_algorithm_callbacks_initialize(FVizAlgorithmCallbacks* callbacks);
FVIZ_API FVizResult fviz_algorithm_create(
    uint32_t input_port_count,
    uint32_t output_port_count,
    const FVizAlgorithmCallbacks* callbacks,
    void* state,
    FVizAlgorithm** out_algorithm);
FVIZ_API void* fviz_algorithm_state(FVizAlgorithm* algorithm);
FVIZ_API const void* fviz_algorithm_const_state(const FVizAlgorithm* algorithm);
FVIZ_API uint64_t fviz_algorithm_diagnostic_id(const FVizAlgorithm* algorithm);
FVIZ_API FVizResult fviz_algorithm_configure_input_port(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizTypeId data_type,
    FVizBool optional,
    FVizBool repeatable);
FVIZ_API FVizResult fviz_algorithm_configure_output_port(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizTypeId data_type);
FVIZ_API FVizDataObject* fviz_algorithm_resolved_input(
    FVizAlgorithm* algorithm,
    uint32_t port,
    uint32_t connection);
FVIZ_API FVizResult fviz_algorithm_set_output_data(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizDataObject* data_object);
FVIZ_API FVizResult fviz_algorithm_report_progress(FVizAlgorithm* algorithm, double progress);

FVIZ_API uint32_t fviz_algorithm_input_port_count(const FVizAlgorithm* algorithm);
FVIZ_API uint32_t fviz_algorithm_output_port_count(const FVizAlgorithm* algorithm);
FVIZ_API FVizResult fviz_algorithm_input_port_info(
    const FVizAlgorithm* algorithm,
    uint32_t port,
    FVizAlgorithmPortInfo* out_info);
FVIZ_API FVizResult fviz_algorithm_output_port_info(
    const FVizAlgorithm* algorithm,
    uint32_t port,
    FVizAlgorithmPortInfo* out_info);

/* Output-port proxies are borrowed and remain valid while their producer lives. */
FVIZ_API FVizAlgorithmOutput* fviz_algorithm_output_port(FVizAlgorithm* algorithm, uint32_t port);
FVIZ_API FVizAlgorithm* fviz_algorithm_output_producer(const FVizAlgorithmOutput* output);
FVIZ_API uint32_t fviz_algorithm_output_index(const FVizAlgorithmOutput* output);

FVIZ_API FVizResult fviz_algorithm_set_input_data(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizDataObject* data_object);
FVIZ_API FVizResult fviz_algorithm_set_input_connection(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizAlgorithmOutput* output);
FVIZ_API FVizResult fviz_algorithm_add_input_connection(
    FVizAlgorithm* algorithm,
    uint32_t port,
    FVizAlgorithmOutput* output);
FVIZ_API FVizResult fviz_algorithm_remove_input_connection(
    FVizAlgorithm* algorithm,
    uint32_t port,
    uint32_t connection);
FVIZ_API void fviz_algorithm_remove_all_input_connections(FVizAlgorithm* algorithm, uint32_t port);
FVIZ_API uint32_t fviz_algorithm_input_connection_count(
    const FVizAlgorithm* algorithm,
    uint32_t port);
FVIZ_API FVizAlgorithmOutput* fviz_algorithm_input_connection(
    FVizAlgorithm* algorithm,
    uint32_t port,
    uint32_t connection);
FVIZ_API const FVizDataObject* fviz_algorithm_input_data(
    const FVizAlgorithm* algorithm,
    uint32_t port);

FVIZ_API FVizResult fviz_algorithm_update(FVizAlgorithm* algorithm);
FVIZ_API FVizExecutive* fviz_algorithm_executive(FVizAlgorithm* algorithm);
FVIZ_API void fviz_algorithm_set_progress_callback(
    FVizAlgorithm* algorithm,
    FVizAlgorithmProgressFn callback,
    void* user_data);
FVIZ_API double fviz_algorithm_progress(const FVizAlgorithm* algorithm);
FVIZ_API void fviz_algorithm_request_abort(FVizAlgorithm* algorithm);
FVIZ_API void fviz_algorithm_clear_abort(FVizAlgorithm* algorithm);
FVIZ_API FVizBool fviz_algorithm_abort_requested(const FVizAlgorithm* algorithm);
FVIZ_API FVizDataObject* fviz_algorithm_output_data(FVizAlgorithm* algorithm, uint32_t port);
FVIZ_API const FVizDataObject* fviz_algorithm_const_output_data(
    const FVizAlgorithm* algorithm,
    uint32_t port);

FVIZ_EXTERN_C_END

#endif /* FVIZ_PIPELINE_ALGORITHM_H */
