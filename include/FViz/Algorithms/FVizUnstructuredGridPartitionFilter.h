#ifndef FVIZ_ALGORITHMS_UNSTRUCTURED_GRID_PARTITION_FILTER_H
#define FVIZ_ALGORITHMS_UNSTRUCTURED_GRID_PARTITION_FILTER_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizPartitionedDataSet.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizUnstructuredGridPartitionFilter FVizUnstructuredGridPartitionFilter;
#define FVIZ_TYPE_UNSTRUCTURED_GRID_PARTITION_FILTER UINT64_C(0x7C06A1E5B3924FD8)

/* Materializes a complete partition set from one unstructured grid. Each
 * partition owns a balanced contiguous cell range and optional topology-aware
 * ghost layers. The output is a PartitionedDataSet whose children retain
 * original point/cell provenance arrays. */
FVIZ_API FVizResult fviz_unstructured_grid_partition_filter_create(
    FVizUnstructuredGridPartitionFilter** out_filter);
FVIZ_API FVizResult fviz_unstructured_grid_partition_filter_set_input_data(
    FVizUnstructuredGridPartitionFilter* filter,
    FVizUnstructuredGrid* input);
FVIZ_API FVizResult fviz_unstructured_grid_partition_filter_set_input_connection(
    FVizUnstructuredGridPartitionFilter* filter,
    FVizAlgorithmOutput* input);
FVIZ_API FVizResult fviz_unstructured_grid_partition_filter_set_partition_count(
    FVizUnstructuredGridPartitionFilter* filter,
    uint32_t partition_count);
FVIZ_API uint32_t fviz_unstructured_grid_partition_filter_partition_count(
    const FVizUnstructuredGridPartitionFilter* filter);
FVIZ_API FVizResult fviz_unstructured_grid_partition_filter_set_ghost_levels(
    FVizUnstructuredGridPartitionFilter* filter,
    uint32_t ghost_levels);
FVIZ_API uint32_t fviz_unstructured_grid_partition_filter_ghost_levels(
    const FVizUnstructuredGridPartitionFilter* filter);
FVIZ_API FVizAlgorithm* fviz_unstructured_grid_partition_filter_algorithm(
    FVizUnstructuredGridPartitionFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_unstructured_grid_partition_filter_output_port(
    FVizUnstructuredGridPartitionFilter* filter);
FVIZ_API FVizPartitionedDataSet* fviz_unstructured_grid_partition_filter_output(
    FVizUnstructuredGridPartitionFilter* filter);
FVIZ_API FVizResult fviz_unstructured_grid_partition_filter_update(
    FVizUnstructuredGridPartitionFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_UNSTRUCTURED_GRID_PARTITION_FILTER_H */
