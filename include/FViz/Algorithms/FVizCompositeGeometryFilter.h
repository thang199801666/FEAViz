#ifndef FVIZ_ALGORITHMS_COMPOSITE_GEOMETRY_FILTER_H
#define FVIZ_ALGORITHMS_COMPOSITE_GEOMETRY_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizMultiBlockDataSet.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizCompositeGeometryFilter FVizCompositeGeometryFilter;
#define FVIZ_TYPE_COMPOSITE_GEOMETRY_FILTER UINT64_C(0xBF208A7DE193465C)

typedef struct FVizCompositeGeometryCacheStatistics
{
    FVizSize entries;
    uint64_t hits;
    uint64_t misses;
    uint64_t pruned;
    FVizSize byte_capacity;
    FVizSize bytes;
    uint64_t evictions;
    uint64_t oversize_skips;
    uint64_t parallel_batches;
    uint64_t parallel_leaf_conversions;
} FVizCompositeGeometryCacheStatistics;

FVIZ_API FVizResult fviz_composite_geometry_filter_create(FVizCompositeGeometryFilter** out_filter);
FVIZ_API FVizResult fviz_composite_geometry_filter_set_input_data(FVizCompositeGeometryFilter* filter,
                                                                  FVizMultiBlockDataSet* input);
FVIZ_API FVizResult fviz_composite_geometry_filter_set_input_connection(FVizCompositeGeometryFilter* filter,
                                                                        FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_composite_geometry_filter_algorithm(FVizCompositeGeometryFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_composite_geometry_filter_output_port(FVizCompositeGeometryFilter* filter);
FVIZ_API FVizMultiBlockDataSet* fviz_composite_geometry_filter_output(FVizCompositeGeometryFilter* filter);
FVIZ_API FVizResult fviz_composite_geometry_filter_update(FVizCompositeGeometryFilter* filter);
/* Leaf conversion cache. Unchanged PolyData/structured/unstructured leaves are
 * reused across composite updates; entries removed from the hierarchy are
 * pruned after a successful execution. */
FVIZ_API void fviz_composite_geometry_filter_clear_cache(FVizCompositeGeometryFilter* filter);
/* Unlimited by default. A non-zero byte capacity applies LRU eviction to leaf
 * conversion cache entries while the current output tree continues to retain
 * its assigned geometry. Oversize single leaves are returned but not cached. */
FVIZ_API FVizResult fviz_composite_geometry_filter_set_cache_byte_capacity(FVizCompositeGeometryFilter* filter,
                                                                           FVizSize byte_capacity);
FVIZ_API FVizSize fviz_composite_geometry_filter_cache_byte_capacity(const FVizCompositeGeometryFilter* filter);
/* Dirty/new leaf conversion is parallel by default once at least four leaves
 * need work. Hierarchy construction, cache mutation, and output assignment are
 * serialized to preserve deterministic object/observer semantics. */
FVIZ_API FVizResult fviz_composite_geometry_filter_set_parallel_enabled(FVizCompositeGeometryFilter* filter,
                                                                        FVizBool enabled);
FVIZ_API FVizBool fviz_composite_geometry_filter_parallel_enabled(const FVizCompositeGeometryFilter* filter);
FVIZ_API FVizResult fviz_composite_geometry_filter_set_parallel_threshold(FVizCompositeGeometryFilter* filter,
                                                                          FVizSize leaf_count);
FVIZ_API FVizSize fviz_composite_geometry_filter_parallel_threshold(const FVizCompositeGeometryFilter* filter);
FVIZ_API FVizCompositeGeometryCacheStatistics
fviz_composite_geometry_filter_cache_statistics(const FVizCompositeGeometryFilter* filter);

FVIZ_EXTERN_C_END

#endif
