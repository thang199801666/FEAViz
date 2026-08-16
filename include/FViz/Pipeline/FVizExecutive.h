#ifndef FVIZ_PIPELINE_EXECUTIVE_H
#define FVIZ_PIPELINE_EXECUTIVE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizAlgorithm FVizAlgorithm;
typedef struct FVizExecutive FVizExecutive;
typedef struct FVizCancellationToken FVizCancellationToken;

#define FVIZ_TYPE_EXECUTIVE UINT64_C(0xB9D3416E2A705CF8)

typedef enum FVizPipelineRequest
{
    FVIZ_PIPELINE_REQUEST_NONE = 0,
    FVIZ_PIPELINE_REQUEST_INFORMATION = 1,
    FVIZ_PIPELINE_REQUEST_DATA_OBJECT = 2,
    FVIZ_PIPELINE_REQUEST_UPDATE_EXTENT = 3,
    FVIZ_PIPELINE_REQUEST_DATA = 4
} FVizPipelineRequest;

typedef enum FVizPipelineRequestFlags
{
    FVIZ_PIPELINE_REQUEST_FLAG_NONE = 0,
    FVIZ_PIPELINE_REQUEST_FLAG_EXACT_EXTENT = 1 << 0,
    FVIZ_PIPELINE_REQUEST_FLAG_RELEASE_DATA = 1 << 1
} FVizPipelineRequestFlags;

/* Versioned value descriptor. Initialize with fviz_pipeline_request_initialize(). */
typedef struct FVizPipelineRequestInfo
{
    uint32_t struct_size;
    FVizPipelineRequest type;
    uint32_t requested_output_port;
    uint32_t piece;
    uint32_t number_of_pieces;
    uint32_t ghost_levels;
    FVizBool has_extent;
    int64_t extent[6];
    FVizBool has_time;
    double time;
    uint32_t flags;
    uint64_t transaction_id;
    FVizCancellationToken* cancellation;
} FVizPipelineRequestInfo;

FVIZ_FILTERS_API void fviz_pipeline_request_initialize(FVizPipelineRequestInfo* request);
FVIZ_FILTERS_API FVizResult fviz_pipeline_request_set_time(FVizPipelineRequestInfo* request, double time);
FVIZ_FILTERS_API void fviz_pipeline_request_clear_time(FVizPipelineRequestInfo* request);
FVIZ_FILTERS_API FVizResult fviz_pipeline_request_set_piece(FVizPipelineRequestInfo* request, uint32_t piece,
                                                    uint32_t number_of_pieces, uint32_t ghost_levels);
FVIZ_FILTERS_API FVizResult fviz_pipeline_request_set_extent(FVizPipelineRequestInfo* request, const int64_t extent[6]);
FVIZ_FILTERS_API void fviz_pipeline_request_clear_extent(FVizPipelineRequestInfo* request);

FVIZ_FILTERS_API FVizAlgorithm* fviz_executive_algorithm(FVizExecutive* executive);
FVIZ_FILTERS_API FVizResult fviz_executive_update(FVizExecutive* executive, uint32_t output_port);
FVIZ_FILTERS_API FVizResult fviz_executive_update_request(FVizExecutive* executive, const FVizPipelineRequestInfo* request);
/* Convenience demand-driven update helpers. These build a versioned request
 * and route it through the same cache/transaction path as update_request(). */
FVIZ_FILTERS_API FVizResult fviz_executive_update_piece(FVizExecutive* executive, uint32_t output_port, uint32_t piece,
                                                uint32_t number_of_pieces, uint32_t ghost_levels);
FVIZ_FILTERS_API FVizResult fviz_executive_update_extent(FVizExecutive* executive, uint32_t output_port,
                                                 const int64_t extent[6], uint32_t ghost_levels);
FVIZ_FILTERS_API FVizResult fviz_executive_update_time(FVizExecutive* executive, uint32_t output_port, double time);
FVIZ_FILTERS_API uint64_t fviz_executive_last_transaction_id(const FVizExecutive* executive);
/* Query mode: pass NULL/0 for text/capacity and read out_required_size. */
FVIZ_FILTERS_API FVizResult fviz_executive_write_dot(const FVizExecutive* executive, char* text, FVizSize capacity,
                                             FVizSize* out_required_size);
FVIZ_FILTERS_API FVizPipelineRequest fviz_executive_last_request(const FVizExecutive* executive);
FVIZ_FILTERS_API uint64_t fviz_executive_execution_count(const FVizExecutive* executive);
FVIZ_FILTERS_API uint64_t fviz_executive_cache_hit_count(const FVizExecutive* executive);
FVIZ_FILTERS_API FVizResult fviz_executive_last_result(const FVizExecutive* executive);
FVIZ_FILTERS_API void fviz_executive_reset_statistics(FVizExecutive* executive);

FVIZ_EXTERN_C_END

#endif /* FVIZ_PIPELINE_EXECUTIVE_H */
