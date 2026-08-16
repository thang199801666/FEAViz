#ifndef FVIZ_DATA_FIELD_STATISTICS_H
#define FVIZ_DATA_FIELD_STATISTICS_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataObject.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizFieldAssociation
{
    FVIZ_FIELD_POINT_DATA = 0,
    FVIZ_FIELD_CELL_DATA = 1
} FVizFieldAssociation;

typedef struct FVizFieldStatisticsOptions
{
    uint32_t struct_size;
    FVizFieldAssociation association;
    uint32_t component;
    FVizBool magnitude;
    /* Ignore tuples marked FVIZ_GHOST_DUPLICATE/FVIZ_GHOST_HIDDEN when a
     * standard FVIZ_GHOST_ARRAY_NAME array is present. Defaults to true. */
    FVizBool ignore_ghosts;
} FVizFieldStatisticsOptions;

typedef struct FVizFieldExtremum
{
    double value;
    FVizSize tuple_id;
    FVizSize partition_index;
    FVizSize temporal_index;
    double time;
    FVizVec3 world_position;
    FVizBool has_partition;
    FVizBool has_time;
    FVizBool has_world_position;
    const FVizDataObject* leaf; /* Borrowed; valid while the queried hierarchy is alive. */
} FVizFieldExtremum;

typedef struct FVizFieldStatistics
{
    FVizBool valid;
    FVizSize finite_tuple_count;
    FVizFieldExtremum minimum;
    FVizFieldExtremum maximum;
} FVizFieldStatistics;

typedef struct FVizFieldMoments
{
    FVizBool valid;
    FVizSize finite_tuple_count;
    double mean;
    double root_mean_square;
    /* Population variance (M2 / N), not sample variance. */
    double variance;
    double standard_deviation;
} FVizFieldMoments;

FVIZ_API void fviz_field_statistics_options_initialize(FVizFieldStatisticsOptions* options);
/* Searches PolyData, UnstructuredGrid, ImageData, MultiBlockDataSet,
 * PartitionedDataSet and TemporalDataSet recursively. Non-finite values and
 * leaves missing array_name are skipped. Standard duplicate/hidden ghost
 * tuples are skipped by default. */
FVIZ_API FVizResult fviz_field_statistics_compute(
    const FVizDataObject* data,
    const char* array_name,
    const FVizFieldStatisticsOptions* options,
    FVizFieldStatistics* out_statistics);
/* Computes deterministic parallel moments using the same traversal and ghost
 * policy as fviz_field_statistics_compute(). */
FVIZ_API FVizResult fviz_field_statistics_compute_moments(
    const FVizDataObject* data,
    const char* array_name,
    const FVizFieldStatisticsOptions* options,
    FVizFieldMoments* out_moments);

FVIZ_EXTERN_C_END

#endif
