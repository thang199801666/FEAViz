#include <math.h>
#include <string.h>

#include <FViz/Data/FVizFieldStatistics.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizImageData.h>
#include <FViz/Data/FVizGhost.h>
#include <FViz/Data/FVizMultiBlockDataSet.h>
#include <FViz/Data/FVizPartitionedDataSet.h>
#include <FViz/Data/FVizRectilinearGrid.h>
#include <FViz/Data/FVizStructuredGrid.h>
#include <FViz/Data/FVizTemporalDataSet.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Parallel/FVizParallel.h>
#include <FViz/Core/FVizMemory.h>

#include <FViz/Core/FVizErrorInternal.h>

typedef struct FVizFieldStatsTraversal
{
    const char* array_name;
    FVizFieldStatisticsOptions options;
    FVizFieldStatistics* stats;
    FVizBool has_partition;
    FVizSize partition_index;
    FVizBool has_time;
    FVizSize temporal_index;
    double time;
    double aggregate_mean;
    double aggregate_m2;
    double aggregate_mean_square;
    FVizBool compute_moments;
} FVizFieldStatsTraversal;

static const FVizAttributeSet* fviz_field_stats_attributes(const FVizDataObject* data, FVizFieldAssociation association)
{
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_POLY_DATA))
        return association == FVIZ_FIELD_POINT_DATA ? fviz_poly_data_const_point_data((const FVizPolyData*)data)
                                                    : fviz_poly_data_const_cell_data((const FVizPolyData*)data);
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_UNSTRUCTURED_GRID))
        return association == FVIZ_FIELD_POINT_DATA ? fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)data)
                                                    : fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)data);
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_IMAGE_DATA))
        return association == FVIZ_FIELD_POINT_DATA ? fviz_image_data_const_point_data((const FVizImageData*)data)
                                                    : fviz_image_data_const_cell_data((const FVizImageData*)data);
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_STRUCTURED_GRID))
        return association == FVIZ_FIELD_POINT_DATA
                   ? fviz_structured_grid_const_point_data((const FVizStructuredGrid*)data)
                   : fviz_structured_grid_const_cell_data((const FVizStructuredGrid*)data);
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_RECTILINEAR_GRID))
        return association == FVIZ_FIELD_POINT_DATA
                   ? fviz_rectilinear_grid_const_point_data((const FVizRectilinearGrid*)data)
                   : fviz_rectilinear_grid_const_cell_data((const FVizRectilinearGrid*)data);
    return NULL;
}

static FVizBool fviz_field_stats_poly_cell_position(const FVizPolyData* poly, FVizSize cell_id, FVizVec3* out)
{
    const FVizCellArray* groups[4] = {fviz_poly_data_verts(poly), fviz_poly_data_lines(poly),
                                      fviz_poly_data_polys(poly), fviz_poly_data_strips(poly)};
    FVizSize group;
    const FVizVec3* points = fviz_poly_data_points(poly);
    for (group = 0u; group < 4u; ++group)
    {
        const FVizSize count = fviz_cell_array_count(groups[group]);
        if (cell_id < count)
        {
            FVizCellView view;
            FVizVec3 sum = fviz_vec3(0, 0, 0);
            FVizSize i;
            if (fviz_cell_array_cell_view(groups[group], cell_id, &view) != FVIZ_OK || view.point_count == 0u)
                return FVIZ_FALSE;
            for (i = 0u; i < view.point_count; ++i)
            {
                const FVizId id = fviz_cell_view_point_id(&view, i);
                if (id >= (FVizId)fviz_poly_data_point_count(poly)) return FVIZ_FALSE;
                sum = fviz_vec3_add(sum, points[(FVizSize)id]);
            }
            *out = fviz_vec3_scale(sum, 1.0f / (float)view.point_count);
            return FVIZ_TRUE;
        }
        cell_id -= count;
    }
    return FVIZ_FALSE;
}

static FVizBool fviz_field_stats_position(const FVizDataObject* data, FVizFieldAssociation association, FVizSize tuple,
                                          FVizVec3* out)
{
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_POLY_DATA))
    {
        const FVizPolyData* poly = (const FVizPolyData*)data;
        if (association == FVIZ_FIELD_POINT_DATA)
        {
            if (tuple >= fviz_poly_data_point_count(poly)) return FVIZ_FALSE;
            *out = fviz_poly_data_points(poly)[tuple];
            return FVIZ_TRUE;
        }
        return fviz_field_stats_poly_cell_position(poly, tuple, out);
    }
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_UNSTRUCTURED_GRID))
    {
        const FVizUnstructuredGrid* grid = (const FVizUnstructuredGrid*)data;
        const FVizVec3* points = fviz_points_data(fviz_unstructured_grid_points((FVizUnstructuredGrid*)grid));
        if (association == FVIZ_FIELD_POINT_DATA)
        {
            if (tuple >= fviz_unstructured_grid_point_count(grid)) return FVIZ_FALSE;
            *out = points[tuple];
            return FVIZ_TRUE;
        }
        else
        {
            FVizCellView view;
            FVizVec3 sum = fviz_vec3(0, 0, 0);
            FVizSize i;
            if (tuple >= fviz_unstructured_grid_cell_count(grid) ||
                fviz_cell_array_cell_view(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), tuple, &view) !=
                    FVIZ_OK ||
                view.point_count == 0u)
                return FVIZ_FALSE;
            for (i = 0u; i < view.point_count; ++i)
            {
                const FVizId id = fviz_cell_view_point_id(&view, i);
                if (id >= (FVizId)fviz_unstructured_grid_point_count(grid)) return FVIZ_FALSE;
                sum = fviz_vec3_add(sum, points[(FVizSize)id]);
            }
            *out = fviz_vec3_scale(sum, 1.0f / (float)view.point_count);
            return FVIZ_TRUE;
        }
    }
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_STRUCTURED_GRID))
    {
        const FVizStructuredGrid* grid = (const FVizStructuredGrid*)data;
        if (association == FVIZ_FIELD_POINT_DATA)
            return fviz_structured_grid_point(grid, (FVizId)tuple, out) == FVIZ_OK ? FVIZ_TRUE : FVIZ_FALSE;
        else
        {
            FVizId ids[8];
            uint32_t count = 0u, i;
            FVizVec3 sum = fviz_vec3(0, 0, 0);
            if (fviz_structured_grid_cell_point_ids(grid, (FVizId)tuple, ids, &count) != FVIZ_OK || count == 0u)
                return FVIZ_FALSE;
            for (i = 0u; i < count; ++i)
            {
                FVizVec3 p;
                if (fviz_structured_grid_point(grid, ids[i], &p) != FVIZ_OK) return FVIZ_FALSE;
                sum = fviz_vec3_add(sum, p);
            }
            *out = fviz_vec3_scale(sum, 1.0f / (float)count);
            return FVIZ_TRUE;
        }
    }
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_RECTILINEAR_GRID))
    {
        const FVizRectilinearGrid* grid = (const FVizRectilinearGrid*)data;
        if (association == FVIZ_FIELD_POINT_DATA)
            return fviz_rectilinear_grid_point(grid, (FVizId)tuple, out) == FVIZ_OK ? FVIZ_TRUE : FVIZ_FALSE;
        else
        {
            FVizId ids[8];
            uint32_t count = 0u, i;
            FVizVec3 sum = fviz_vec3(0, 0, 0);
            if (fviz_rectilinear_grid_cell_point_ids(grid, (FVizId)tuple, ids, &count) != FVIZ_OK || count == 0u)
                return FVIZ_FALSE;
            for (i = 0u; i < count; ++i)
            {
                FVizVec3 p;
                if (fviz_rectilinear_grid_point(grid, ids[i], &p) != FVIZ_OK) return FVIZ_FALSE;
                sum = fviz_vec3_add(sum, p);
            }
            *out = fviz_vec3_scale(sum, 1.0f / (float)count);
            return FVIZ_TRUE;
        }
    }
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_IMAGE_DATA))
    {
        const FVizImageData* image = (const FVizImageData*)data;
        if (association == FVIZ_FIELD_POINT_DATA)
            return fviz_image_data_point(image, (FVizId)tuple, out) == FVIZ_OK ? FVIZ_TRUE : FVIZ_FALSE;
        else
        {
            FVizId ids[8];
            uint32_t count = 0u, i;
            FVizVec3 sum = fviz_vec3(0, 0, 0);
            if (fviz_image_data_cell_point_ids(image, (FVizId)tuple, ids, &count) != FVIZ_OK || count == 0u)
                return FVIZ_FALSE;
            for (i = 0u; i < count; ++i)
            {
                FVizVec3 p;
                if (fviz_image_data_point(image, ids[i], &p) != FVIZ_OK) return FVIZ_FALSE;
                sum = fviz_vec3_add(sum, p);
            }
            *out = fviz_vec3_scale(sum, 1.0f / (float)count);
            return FVIZ_TRUE;
        }
    }
    return FVIZ_FALSE;
}

static void fviz_field_stats_set_extremum(FVizFieldExtremum* e, double value, FVizSize tuple,
                                          const FVizDataObject* leaf, const FVizFieldStatsTraversal* t)
{
    FVizVec3 position;
    memset(e, 0, sizeof(*e));
    e->value = value;
    e->tuple_id = tuple;
    e->leaf = leaf;
    e->has_partition = t->has_partition;
    e->partition_index = t->partition_index;
    e->has_time = t->has_time;
    e->temporal_index = t->temporal_index;
    e->time = t->time;
    if (fviz_field_stats_position(leaf, t->options.association, tuple, &position) != FVIZ_FALSE)
    {
        e->world_position = position;
        e->has_world_position = FVIZ_TRUE;
    }
}

#define FVIZ_FIELD_STATS_PARALLEL_GRAIN ((FVizSize)8192u)

typedef struct FVizFieldStatsPartial
{
    FVizBool valid;
    FVizSize finite_count;
    FVizSize minimum_tuple;
    FVizSize maximum_tuple;
    double minimum;
    double maximum;
    double mean;
    double m2;
    double mean_square;
} FVizFieldStatsPartial;

typedef struct FVizFieldStatsRangeContext
{
    const void* values;
    FVizDataType type;
    uint32_t components;
    uint32_t component;
    FVizBool magnitude;
    FVizBool compute_moments;
    const uint8_t* ghost_flags;
    FVizSize grain;
    FVizFieldStatsPartial* partials;
} FVizFieldStatsRangeContext;

static double fviz_field_stats_raw_component(const FVizFieldStatsRangeContext* context, FVizSize tuple,
                                             uint32_t component)
{
    const FVizSize index = tuple * (FVizSize)context->components + (FVizSize)component;
    switch (context->type)
    {
        case FVIZ_DATA_INT8:
            return ((const int8_t*)context->values)[index];
        case FVIZ_DATA_UINT8:
            return ((const uint8_t*)context->values)[index];
        case FVIZ_DATA_INT16:
            return ((const int16_t*)context->values)[index];
        case FVIZ_DATA_UINT16:
            return ((const uint16_t*)context->values)[index];
        case FVIZ_DATA_INT32:
            return ((const int32_t*)context->values)[index];
        case FVIZ_DATA_UINT32:
            return ((const uint32_t*)context->values)[index];
        case FVIZ_DATA_INT64:
            return (double)((const int64_t*)context->values)[index];
        case FVIZ_DATA_UINT64:
            return (double)((const uint64_t*)context->values)[index];
        case FVIZ_DATA_FLOAT32:
            return ((const float*)context->values)[index];
        case FVIZ_DATA_FLOAT64:
            return ((const double*)context->values)[index];
        default:
            return NAN;
    }
}

static double fviz_field_stats_raw_value(const FVizFieldStatsRangeContext* context, FVizSize tuple)
{
    if (context->magnitude != FVIZ_FALSE)
    {
        double sum = 0.0;
        uint32_t component;
        for (component = 0u; component < context->components; ++component)
        {
            const double value = fviz_field_stats_raw_component(context, tuple, component);
            sum += value * value;
        }
        return sqrt(sum);
    }
    return fviz_field_stats_raw_component(context, tuple, context->component);
}

static void fviz_field_stats_range(FVizSize begin, FVizSize end, void* user_data)
{
    FVizFieldStatsRangeContext* context = (FVizFieldStatsRangeContext*)user_data;
    FVizFieldStatsPartial* partial = &context->partials[begin / context->grain];
    FVizSize tuple;
    (void)memset(partial, 0, sizeof(*partial));
    for (tuple = begin; tuple < end; ++tuple)
    {
        double value;
        if (context->ghost_flags != NULL &&
            (context->ghost_flags[tuple] & (uint8_t)(FVIZ_GHOST_DUPLICATE | FVIZ_GHOST_HIDDEN)) != 0u)
            continue;
        value = fviz_field_stats_raw_value(context, tuple);
        if (!isfinite(value)) continue;
        if (context->compute_moments != FVIZ_FALSE)
        {
            const FVizSize next_count = partial->finite_count + 1u;
            const double delta = value - partial->mean;
            const double square = value * value;
            partial->mean += delta / (double)next_count;
            partial->m2 += delta * (value - partial->mean);
            partial->mean_square += (square - partial->mean_square) / (double)next_count;
        }
        if (partial->valid == FVIZ_FALSE)
        {
            partial->valid = FVIZ_TRUE;
            partial->minimum = value;
            partial->maximum = value;
            partial->minimum_tuple = tuple;
            partial->maximum_tuple = tuple;
        }
        else
        {
            if (value < partial->minimum)
            {
                partial->minimum = value;
                partial->minimum_tuple = tuple;
            }
            if (value > partial->maximum)
            {
                partial->maximum = value;
                partial->maximum_tuple = tuple;
            }
        }
        ++partial->finite_count;
    }
}

static FVizBool fviz_field_stats_supported_type(FVizDataType type)
{
    return type >= FVIZ_DATA_INT8 && type <= FVIZ_DATA_FLOAT64 ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_field_stats_visit_leaf(const FVizDataObject* data, FVizFieldStatsTraversal* t)
{
    const FVizAttributeSet* set = fviz_field_stats_attributes(data, t->options.association);
    const FVizDataArray* array;
    FVizSize tuple_count;
    uint32_t components;
    FVizFieldStatsRangeContext context;
    FVizFieldStatsPartial* partials = NULL;
    FVizSize partial_count;
    FVizSize partial_bytes;
    FVizSize i;
    FVizFieldStatsPartial leaf;
    if (set == NULL) return FVIZ_OK;
    array = fviz_attribute_set_const_get(set, t->array_name);
    if (array == NULL) return FVIZ_OK;
    tuple_count = fviz_data_array_tuple_count(array);
    components = fviz_data_array_components(array);
    if (components == 0u || fviz_field_stats_supported_type(fviz_data_array_type(array)) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "field statistics array type is unsupported");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (t->options.magnitude == FVIZ_FALSE && t->options.component >= components)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "field statistics component is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (tuple_count == 0u) return FVIZ_OK;
    partial_count =
        tuple_count / FVIZ_FIELD_STATS_PARALLEL_GRAIN + (tuple_count % FVIZ_FIELD_STATS_PARALLEL_GRAIN != 0u ? 1u : 0u);
    if (fviz_size_multiply(partial_count, sizeof(*partials), &partial_bytes) != FVIZ_OK) return fviz_last_error_code();
    partials = (FVizFieldStatsPartial*)fviz_alloc(partial_bytes);
    if (partials == NULL) return fviz_last_error_code();
    (void)memset(partials, 0, partial_bytes);
    context.values = fviz_data_array_const_data(array);
    context.ghost_flags = NULL;
    if (t->options.ignore_ghosts != FVIZ_FALSE)
    {
        const FVizDataArray* ghosts = fviz_attribute_set_const_get(set, FVIZ_GHOST_ARRAY_NAME);
        if (ghosts != NULL && fviz_data_array_type(ghosts) == FVIZ_DATA_UINT8 &&
            fviz_data_array_components(ghosts) == 1u && fviz_data_array_tuple_count(ghosts) == tuple_count)
            context.ghost_flags = (const uint8_t*)fviz_data_array_const_data(ghosts);
    }
    context.type = fviz_data_array_type(array);
    context.components = components;
    context.component = t->options.component;
    context.magnitude = t->options.magnitude;
    context.compute_moments = t->compute_moments;
    context.grain = FVIZ_FIELD_STATS_PARALLEL_GRAIN;
    context.partials = partials;
    if (partial_count == 1u) fviz_field_stats_range(0u, tuple_count, &context);
    else if (fviz_parallel_for(0u, tuple_count, FVIZ_FIELD_STATS_PARALLEL_GRAIN, fviz_field_stats_range, &context) !=
             FVIZ_OK)
    {
        fviz_free(partials);
        return fviz_last_error_code();
    }
    (void)memset(&leaf, 0, sizeof(leaf));
    for (i = 0u; i < partial_count; ++i)
    {
        const FVizFieldStatsPartial* partial = &partials[i];
        if (partial->valid == FVIZ_FALSE) continue;
        if (leaf.valid == FVIZ_FALSE)
        {
            leaf = *partial;
        }
        else
        {
            const FVizSize left_count = leaf.finite_count;
            const FVizSize right_count = partial->finite_count;
            const FVizSize merged_count = left_count + right_count;
            if (t->compute_moments != FVIZ_FALSE)
            {
                const double delta = partial->mean - leaf.mean;
                leaf.mean += delta * (double)right_count / (double)merged_count;
                leaf.m2 +=
                    partial->m2 + delta * delta * (double)left_count * (double)right_count / (double)merged_count;
                leaf.mean_square =
                    (leaf.mean_square * (double)left_count + partial->mean_square * (double)right_count) /
                    (double)merged_count;
            }
            leaf.finite_count = merged_count;
            if (partial->minimum < leaf.minimum)
            {
                leaf.minimum = partial->minimum;
                leaf.minimum_tuple = partial->minimum_tuple;
            }
            if (partial->maximum > leaf.maximum)
            {
                leaf.maximum = partial->maximum;
                leaf.maximum_tuple = partial->maximum_tuple;
            }
        }
    }
    fviz_free(partials);
    if (leaf.valid == FVIZ_FALSE) return FVIZ_OK;
    if (t->stats->valid == FVIZ_FALSE || leaf.minimum < t->stats->minimum.value)
        fviz_field_stats_set_extremum(&t->stats->minimum, leaf.minimum, leaf.minimum_tuple, data, t);
    if (t->stats->valid == FVIZ_FALSE || leaf.maximum > t->stats->maximum.value)
        fviz_field_stats_set_extremum(&t->stats->maximum, leaf.maximum, leaf.maximum_tuple, data, t);
    {
        const FVizSize left_count = t->stats->finite_tuple_count;
        const FVizSize right_count = leaf.finite_count;
        const FVizSize merged_count = left_count + right_count;
        if (t->compute_moments != FVIZ_FALSE)
        {
            if (left_count == 0u)
            {
                t->aggregate_mean = leaf.mean;
                t->aggregate_m2 = leaf.m2;
                t->aggregate_mean_square = leaf.mean_square;
            }
            else
            {
                const double delta = leaf.mean - t->aggregate_mean;
                t->aggregate_mean += delta * (double)right_count / (double)merged_count;
                t->aggregate_m2 +=
                    leaf.m2 + delta * delta * (double)left_count * (double)right_count / (double)merged_count;
                t->aggregate_mean_square =
                    (t->aggregate_mean_square * (double)left_count + leaf.mean_square * (double)right_count) /
                    (double)merged_count;
            }
        }
        t->stats->finite_tuple_count = merged_count;
    }
    t->stats->valid = FVIZ_TRUE;
    return FVIZ_OK;
}

static FVizResult fviz_field_stats_visit(const FVizDataObject* data, FVizFieldStatsTraversal* t);

static FVizResult fviz_field_stats_multiblock_leaf(const FVizMultiBlockDataSet* parent, FVizSize index,
                                                   const FVizDataObject* block, const char* name, FVizSize depth,
                                                   void* user_data)
{
    (void)parent;
    (void)index;
    (void)name;
    (void)depth;
    return fviz_field_stats_visit(block, (FVizFieldStatsTraversal*)user_data);
}

static FVizResult fviz_field_stats_visit(const FVizDataObject* data, FVizFieldStatsTraversal* t)
{
    if (data == NULL) return FVIZ_OK;
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_MULTI_BLOCK_DATA_SET))
        return fviz_multi_block_data_set_visit((const FVizMultiBlockDataSet*)data, FVIZ_TRUE, FVIZ_TRUE,
                                               fviz_field_stats_multiblock_leaf, t);
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_PARTITIONED_DATA_SET))
    {
        const FVizPartitionedDataSet* partitions = (const FVizPartitionedDataSet*)data;
        FVizSize i;
        const FVizBool old = t->has_partition;
        const FVizSize old_index = t->partition_index;
        for (i = 0u; i < fviz_partitioned_data_set_count(partitions); ++i)
        {
            t->has_partition = FVIZ_TRUE;
            t->partition_index = i;
            if (fviz_field_stats_visit(fviz_partitioned_data_set_const_partition(partitions, i), t) != FVIZ_OK)
                return fviz_last_error_code();
        }
        t->has_partition = old;
        t->partition_index = old_index;
        return FVIZ_OK;
    }
    if (fviz_object_is_type((const FVizObject*)data, FVIZ_TYPE_TEMPORAL_DATA_SET))
    {
        const FVizTemporalDataSet* temporal = (const FVizTemporalDataSet*)data;
        FVizSize i;
        const FVizBool old = t->has_time;
        const FVizSize old_index = t->temporal_index;
        const double old_time = t->time;
        for (i = 0u; i < fviz_temporal_data_set_step_count(temporal); ++i)
        {
            t->has_time = FVIZ_TRUE;
            t->temporal_index = i;
            t->time = fviz_temporal_data_set_time(temporal, i);
            if (fviz_field_stats_visit(fviz_temporal_data_set_const_data(temporal, i), t) != FVIZ_OK)
                return fviz_last_error_code();
        }
        t->has_time = old;
        t->temporal_index = old_index;
        t->time = old_time;
        return FVIZ_OK;
    }
    return fviz_field_stats_visit_leaf(data, t);
}

void fviz_field_statistics_options_initialize(FVizFieldStatisticsOptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->association = FVIZ_FIELD_POINT_DATA;
    options->ignore_ghosts = FVIZ_TRUE;
}

static FVizResult fviz_field_statistics_compute_internal(const FVizDataObject* data, const char* array_name,
                                                         const FVizFieldStatisticsOptions* options,
                                                         FVizFieldStatistics* out_statistics,
                                                         FVizFieldMoments* out_moments)
{
    FVizFieldStatisticsOptions defaults;
    FVizFieldStatsTraversal traversal;
    FVizResult result;
    if (out_statistics == NULL || data == NULL || array_name == NULL || array_name[0] == '\0')
        return FVIZ_ERROR_INVALID_ARGUMENT;
    memset(out_statistics, 0, sizeof(*out_statistics));
    if (out_moments != NULL) memset(out_moments, 0, sizeof(*out_moments));
    fviz_field_statistics_options_initialize(&defaults);
    if (options != NULL)
    {
        FVizSize copy_size = options->struct_size != 0u ? (FVizSize)options->struct_size : sizeof(*options);
        if (copy_size > sizeof(defaults)) copy_size = sizeof(defaults);
        (void)memcpy(&defaults, options, copy_size);
        defaults.struct_size = (uint32_t)sizeof(defaults);
    }
    if (defaults.association < FVIZ_FIELD_POINT_DATA || defaults.association > FVIZ_FIELD_CELL_DATA)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "field statistics association is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    memset(&traversal, 0, sizeof(traversal));
    traversal.array_name = array_name;
    traversal.options = defaults;
    traversal.stats = out_statistics;
    traversal.compute_moments = out_moments != NULL ? FVIZ_TRUE : FVIZ_FALSE;
    result = fviz_field_stats_visit(data, &traversal);
    if (result != FVIZ_OK) return result;
    if (out_moments != NULL && out_statistics->valid != FVIZ_FALSE && out_statistics->finite_tuple_count != 0u)
    {
        double variance = traversal.aggregate_m2 / (double)out_statistics->finite_tuple_count;
        double mean_square = traversal.aggregate_mean_square;
        if (variance < 0.0 && variance > -1.0e-15) variance = 0.0;
        if (mean_square < 0.0 && mean_square > -1.0e-15) mean_square = 0.0;
        out_moments->valid = FVIZ_TRUE;
        out_moments->finite_tuple_count = out_statistics->finite_tuple_count;
        out_moments->mean = traversal.aggregate_mean;
        out_moments->root_mean_square = sqrt(mean_square);
        out_moments->variance = variance;
        out_moments->standard_deviation = sqrt(variance);
    }
    return FVIZ_OK;
}

FVizResult fviz_field_statistics_compute(const FVizDataObject* data, const char* array_name,
                                         const FVizFieldStatisticsOptions* options, FVizFieldStatistics* out_statistics)
{
    return fviz_field_statistics_compute_internal(data, array_name, options, out_statistics, NULL);
}

FVizResult fviz_field_statistics_compute_moments(const FVizDataObject* data, const char* array_name,
                                                 const FVizFieldStatisticsOptions* options,
                                                 FVizFieldMoments* out_moments)
{
    FVizFieldStatistics statistics;
    if (out_moments == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_field_statistics_compute_internal(data, array_name, options, &statistics, out_moments);
}
