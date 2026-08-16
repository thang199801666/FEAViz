#include <stdio.h>

#include <FViz/Algorithms/FVizUnstructuredGridPartitionFilter.h>
#include <FViz/Algorithms/FVizUnstructuredGridPieceFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizUnstructuredGridPartitionFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizUnstructuredGridPieceFilter* piece_filter;
    uint32_t partition_count;
    uint32_t ghost_levels;
};

static void fviz_unstructured_partition_destroy(FVizObject* object)
{
    FVizUnstructuredGridPartitionFilter* filter = (FVizUnstructuredGridPartitionFilter*)object;
    fviz_release(filter->piece_filter);
    filter->piece_filter = NULL;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_unstructured_partition_class = {
    FVIZ_TYPE_UNSTRUCTURED_GRID_PARTITION_FILTER,
    "FVizUnstructuredGridPartitionFilter",
    &g_fviz_object_class,
    fviz_unstructured_partition_destroy,
    NULL
};

static FVizMTime fviz_unstructured_partition_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static FVizResult fviz_unstructured_partition_map_request(
    FVizAlgorithm* algorithm,
    uint32_t input_port,
    uint32_t connection,
    const FVizPipelineRequestInfo* downstream,
    FVizPipelineRequestInfo* upstream,
    void* state)
{
    (void)algorithm;
    (void)connection;
    (void)downstream;
    (void)state;
    if (input_port != 0u) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_pipeline_request_set_piece(upstream, 0u, 1u, 0u);
}

static FVizResult fviz_unstructured_partition_process_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* state)
{
    FVizUnstructuredGridPartitionFilter* filter = (FVizUnstructuredGridPartitionFilter*)state;
    FVizUnstructuredGrid* input;
    FVizPartitionedDataSet* output = NULL;
    uint32_t piece;
    FVizResult result = FVIZ_OK;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizUnstructuredGrid*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "partition filter has no UnstructuredGrid input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (filter->partition_count == 0u) return FVIZ_ERROR_INVALID_STATE;
    if (fviz_partitioned_data_set_create(&output) != FVIZ_OK ||
        fviz_partitioned_data_set_reserve(output, (FVizSize)filter->partition_count) != FVIZ_OK ||
        fviz_unstructured_grid_piece_filter_set_input_data(filter->piece_filter, input) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto done;
    }
    for (piece = 0u; piece < filter->partition_count; ++piece)
    {
        FVizUnstructuredGrid* piece_output;
        char name[48];
        int written;
        if (fviz_unstructured_grid_piece_filter_update_piece(
                filter->piece_filter, piece, filter->partition_count, filter->ghost_levels) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
        piece_output = fviz_unstructured_grid_piece_filter_output(filter->piece_filter);
        if (piece_output == NULL)
        {
            result = FVIZ_ERROR_INVALID_STATE;
            goto done;
        }
        written = snprintf(name, sizeof(name), "Piece %u", piece);
        if (written < 0 || (size_t)written >= sizeof(name))
        {
            result = FVIZ_ERROR_INTERNAL;
            goto done;
        }
        if (fviz_partitioned_data_set_add_partition(
                output, (FVizDataObject*)piece_output, name, NULL) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
    }
    if (fviz_algorithm_set_output_data(
            algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        result = fviz_last_error_code();

done:
    fviz_release(output);
    return result;
}

FVizResult fviz_unstructured_grid_partition_filter_create(
    FVizUnstructuredGridPartitionFilter** out_filter)
{
    FVizUnstructuredGridPartitionFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizUnstructuredGridPartitionFilter*)fviz_internal_object_allocate(
        sizeof(*filter), &g_fviz_unstructured_partition_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->partition_count = 1u;
    filter->ghost_levels = 0u;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_unstructured_partition_process_request;
    callbacks.get_state_mtime = fviz_unstructured_partition_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    callbacks.map_input_request = fviz_unstructured_partition_map_request;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_unstructured_grid_piece_filter_create(&filter->piece_filter) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(
            filter->algorithm, 0u, FVIZ_TYPE_UNSTRUCTURED_GRID, FVIZ_FALSE, FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(
            filter->algorithm, 0u, FVIZ_TYPE_PARTITIONED_DATA_SET) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_unstructured_grid_partition_filter_set_input_data(
    FVizUnstructuredGridPartitionFilter* filter,
    FVizUnstructuredGrid* input)
{
    return filter != NULL && input != NULL
        ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
        : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_unstructured_grid_partition_filter_set_input_connection(
    FVizUnstructuredGridPartitionFilter* filter,
    FVizAlgorithmOutput* input)
{
    return filter != NULL && input != NULL
        ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
        : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_unstructured_grid_partition_filter_set_partition_count(
    FVizUnstructuredGridPartitionFilter* filter,
    uint32_t partition_count)
{
    if (filter == NULL || partition_count == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "partition count must be greater than zero");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (filter->partition_count == partition_count) return FVIZ_OK;
    filter->partition_count = partition_count;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

uint32_t fviz_unstructured_grid_partition_filter_partition_count(
    const FVizUnstructuredGridPartitionFilter* filter)
{
    return filter != NULL ? filter->partition_count : 0u;
}

FVizResult fviz_unstructured_grid_partition_filter_set_ghost_levels(
    FVizUnstructuredGridPartitionFilter* filter,
    uint32_t ghost_levels)
{
    if (filter == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (ghost_levels > (uint32_t)UINT16_MAX) return FVIZ_ERROR_OVERFLOW;
    if (filter->ghost_levels == ghost_levels) return FVIZ_OK;
    filter->ghost_levels = ghost_levels;
    fviz_object_modified((FVizObject*)filter);
    return FVIZ_OK;
}

uint32_t fviz_unstructured_grid_partition_filter_ghost_levels(
    const FVizUnstructuredGridPartitionFilter* filter)
{
    return filter != NULL ? filter->ghost_levels : 0u;
}

FVizAlgorithm* fviz_unstructured_grid_partition_filter_algorithm(
    FVizUnstructuredGridPartitionFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_unstructured_grid_partition_filter_output_port(
    FVizUnstructuredGridPartitionFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPartitionedDataSet* fviz_unstructured_grid_partition_filter_output(
    FVizUnstructuredGridPartitionFilter* filter)
{
    return filter != NULL
        ? (FVizPartitionedDataSet*)fviz_algorithm_output_data(filter->algorithm, 0u)
        : NULL;
}

FVizResult fviz_unstructured_grid_partition_filter_update(
    FVizUnstructuredGridPartitionFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
