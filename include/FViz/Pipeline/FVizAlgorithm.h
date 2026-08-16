#ifndef FVIZ_PIPELINE_ALGORITHM_H
#define FVIZ_PIPELINE_ALGORITHM_H

#include <stddef.h>
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
typedef struct FVizExecutor FVizExecutor;
typedef struct FVizFuture FVizFuture;

#define FVIZ_TYPE_ALGORITHM UINT64_C(0xA6213C94D7E850BF)

typedef struct FVizAlgorithmPortInfo
{
    FVizTypeId data_type;
    FVizBool optional;
    FVizBool repeatable;
} FVizAlgorithmPortInfo;

typedef void (*FVizAlgorithmProgressFn)(FVizAlgorithm* algorithm, double progress, void* user_data);

typedef FVizResult (*FVizAlgorithmProcessRequestFn)(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                    void* state);
typedef void (*FVizAlgorithmDestroyStateFn)(void* state);
typedef FVizMTime (*FVizAlgorithmGetStateMTimeFn)(const void* state);

/* Optional streaming hook. It is called before an upstream connection is
 * executed and may remap piece/extent/time/ghost requests for that specific
 * input. The default behavior is an exact copy of downstream_request with the
 * producer output port substituted by the executive. */
typedef FVizResult (*FVizAlgorithmMapInputRequestFn)(FVizAlgorithm* algorithm, uint32_t input_port, uint32_t connection,
                                                     const FVizPipelineRequestInfo* downstream_request,
                                                     FVizPipelineRequestInfo* upstream_request, void* state);

typedef struct FVizAlgorithmCallbacks
{
    uint32_t struct_size;
    FVizAlgorithmProcessRequestFn process_request;
    FVizAlgorithmDestroyStateFn destroy_state;
    FVizAlgorithmGetStateMTimeFn get_state_mtime;
    /* Optional borrowed FVizObject representing algorithm state.  When supplied,
     * its ModifiedEvent is bridged to the algorithm's ModifiedEvent.  DeleteEvent
     * automatically detaches the bridge so external state may safely die first. */
    FVizObject* state_object;
    /* Optional. Present when struct_size is larger than the v0.34 callback
     * layout. Older callers remain binary compatible through struct_size. */
    FVizAlgorithmMapInputRequestFn map_input_request;
} FVizAlgorithmCallbacks;

#define FVIZ_ALGORITHM_CALLBACKS_V1_SIZE ((uint32_t)offsetof(FVizAlgorithmCallbacks, state_object))
#define FVIZ_ALGORITHM_CALLBACKS_V2_SIZE ((uint32_t)offsetof(FVizAlgorithmCallbacks, map_input_request))

FVIZ_API void fviz_algorithm_callbacks_initialize(FVizAlgorithmCallbacks* callbacks);
FVIZ_API FVizResult fviz_algorithm_create(uint32_t input_port_count, uint32_t output_port_count,
                                          const FVizAlgorithmCallbacks* callbacks, void* state,
                                          FVizAlgorithm** out_algorithm);
FVIZ_API void* fviz_algorithm_state(FVizAlgorithm* algorithm);
FVIZ_API const void* fviz_algorithm_const_state(const FVizAlgorithm* algorithm);
FVIZ_API uint64_t fviz_algorithm_diagnostic_id(const FVizAlgorithm* algorithm);
FVIZ_API FVizResult fviz_algorithm_configure_input_port(FVizAlgorithm* algorithm, uint32_t port, FVizTypeId data_type,
                                                        FVizBool optional, FVizBool repeatable);
FVIZ_API FVizResult fviz_algorithm_configure_output_port(FVizAlgorithm* algorithm, uint32_t port, FVizTypeId data_type);
FVIZ_API FVizDataObject* fviz_algorithm_resolved_input(FVizAlgorithm* algorithm, uint32_t port, uint32_t connection);
FVIZ_API FVizResult fviz_algorithm_set_output_data(FVizAlgorithm* algorithm, uint32_t port,
                                                   FVizDataObject* data_object);
FVIZ_API FVizResult fviz_algorithm_report_progress(FVizAlgorithm* algorithm, double progress);

FVIZ_API uint32_t fviz_algorithm_input_port_count(const FVizAlgorithm* algorithm);
FVIZ_API uint32_t fviz_algorithm_output_port_count(const FVizAlgorithm* algorithm);
FVIZ_API FVizResult fviz_algorithm_input_port_info(const FVizAlgorithm* algorithm, uint32_t port,
                                                   FVizAlgorithmPortInfo* out_info);
FVIZ_API FVizResult fviz_algorithm_output_port_info(const FVizAlgorithm* algorithm, uint32_t port,
                                                    FVizAlgorithmPortInfo* out_info);

/* Output-port proxies are borrowed and remain valid while their producer lives. */
FVIZ_API FVizAlgorithmOutput* fviz_algorithm_output_port(FVizAlgorithm* algorithm, uint32_t port);
FVIZ_API FVizAlgorithm* fviz_algorithm_output_producer(const FVizAlgorithmOutput* output);
FVIZ_API uint32_t fviz_algorithm_output_index(const FVizAlgorithmOutput* output);

FVIZ_API FVizResult fviz_algorithm_set_input_data(FVizAlgorithm* algorithm, uint32_t port, FVizDataObject* data_object);
FVIZ_API FVizResult fviz_algorithm_set_input_connection(FVizAlgorithm* algorithm, uint32_t port,
                                                        FVizAlgorithmOutput* output);
FVIZ_API FVizResult fviz_algorithm_add_input_connection(FVizAlgorithm* algorithm, uint32_t port,
                                                        FVizAlgorithmOutput* output);
FVIZ_API FVizResult fviz_algorithm_remove_input_connection(FVizAlgorithm* algorithm, uint32_t port,
                                                           uint32_t connection);
FVIZ_API void fviz_algorithm_remove_all_input_connections(FVizAlgorithm* algorithm, uint32_t port);
/* Clears both direct input data and all producer connections on a port. */
FVIZ_API FVizResult fviz_algorithm_clear_input(FVizAlgorithm* algorithm, uint32_t port);
FVIZ_API uint32_t fviz_algorithm_input_connection_count(const FVizAlgorithm* algorithm, uint32_t port);
FVIZ_API FVizAlgorithmOutput* fviz_algorithm_input_connection(FVizAlgorithm* algorithm, uint32_t port,
                                                              uint32_t connection);
FVIZ_API const FVizDataObject* fviz_algorithm_input_data(const FVizAlgorithm* algorithm, uint32_t port);

FVIZ_API FVizResult fviz_algorithm_update(FVizAlgorithm* algorithm);
/* Executes the same demand-driven update on an executor worker. The future
 * retains the algorithm through completion and propagates its cancellation
 * token through every executive request stage. Pipeline mutation remains
 * externally synchronized while the future is running. */
FVIZ_API FVizResult fviz_algorithm_update_async(FVizAlgorithm* algorithm, FVizExecutor* executor, int priority,
                                                FVizFuture** out_future);
/* Runs an ordered array of algorithm updates as a dependent continuation chain
 * on one executor: stage i+1 only becomes runnable after stage i completes, so
 * multi-stage pipelines drain through the shared worker pool without
 * one-thread-per-request or caller-side waits. A failed or cancelled stage
 * short-circuits the remaining stages. The returned terminal future reports the
 * aggregate outcome; every algorithm is retained until its stage completes.
 * Pipeline mutation remains externally synchronized while the chain runs. */
FVIZ_API FVizResult fviz_algorithm_update_async_chain(FVizAlgorithm** algorithms, FVizSize count,
                                                      FVizExecutor* executor, int priority, FVizFuture** out_future);
FVIZ_API FVizExecutive* fviz_algorithm_executive(FVizAlgorithm* algorithm);
FVIZ_API void fviz_algorithm_set_progress_callback(FVizAlgorithm* algorithm, FVizAlgorithmProgressFn callback,
                                                   void* user_data);
FVIZ_API double fviz_algorithm_progress(const FVizAlgorithm* algorithm);
FVIZ_API void fviz_algorithm_request_abort(FVizAlgorithm* algorithm);
FVIZ_API void fviz_algorithm_clear_abort(FVizAlgorithm* algorithm);
FVIZ_API FVizBool fviz_algorithm_abort_requested(const FVizAlgorithm* algorithm);
FVIZ_API FVizDataObject* fviz_algorithm_output_data(FVizAlgorithm* algorithm, uint32_t port);
FVIZ_API const FVizDataObject* fviz_algorithm_const_output_data(const FVizAlgorithm* algorithm, uint32_t port);
/* Explicitly drop retained output payloads so demand-driven streaming clients
 * can bound resident memory. A released output is marked stale and will be
 * regenerated on the next matching update request. */
FVIZ_API FVizResult fviz_algorithm_release_output_data(FVizAlgorithm* algorithm, uint32_t port);
FVIZ_API void fviz_algorithm_release_all_output_data(FVizAlgorithm* algorithm);

/* Temporal output metadata. Time steps must be finite and strictly increasing.
 * The returned step pointer is borrowed and remains valid until the metadata changes. */
FVIZ_API FVizResult fviz_algorithm_set_output_time_steps(FVizAlgorithm* algorithm, uint32_t port,
                                                         const double* time_steps, FVizSize count);
FVIZ_API const double* fviz_algorithm_output_time_steps(const FVizAlgorithm* algorithm, uint32_t port,
                                                        FVizSize* out_count);
FVIZ_API FVizResult fviz_algorithm_output_time_range(const FVizAlgorithm* algorithm, uint32_t port, double* out_minimum,
                                                     double* out_maximum);

/* Spatial output metadata for structured/piece-streamed producers. The whole
 * extent is inclusive [xmin,xmax,ymin,ymax,zmin,zmax]. Like time-step
 * metadata, this is informational state and does not itself execute the pipeline. */
FVIZ_API FVizResult fviz_algorithm_set_output_whole_extent(FVizAlgorithm* algorithm, uint32_t port,
                                                           const int64_t extent[6]);
FVIZ_API void fviz_algorithm_clear_output_whole_extent(FVizAlgorithm* algorithm, uint32_t port);
FVIZ_API FVizBool fviz_algorithm_output_whole_extent(const FVizAlgorithm* algorithm, uint32_t port,
                                                     int64_t out_extent[6]);

FVIZ_EXTERN_C_END

#endif /* FVIZ_PIPELINE_ALGORITHM_H */
