#ifndef FVIZ_ALGORITHMS_MESH_QUALITY_FILTER_H
#define FVIZ_ALGORITHMS_MESH_QUALITY_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizMeshQualityMetric
{
    FVIZ_MESH_QUALITY_MEASURE = 0,
    FVIZ_MESH_QUALITY_EDGE_RATIO = 1,
    FVIZ_MESH_QUALITY_SCALED_JACOBIAN = 2,
    FVIZ_MESH_QUALITY_MIN_CORNER_ANGLE = 3,
    FVIZ_MESH_QUALITY_MAX_CORNER_ANGLE = 4,
    FVIZ_MESH_QUALITY_WARPAGE = 5
} FVizMeshQualityMetric;

typedef struct FVizMeshQualityFilter FVizMeshQualityFilter;
#define FVIZ_TYPE_MESH_QUALITY_FILTER UINT64_C(0xD6C1F98A4B273E50)

/* Computes one Float64 quality value per cell. Unsupported metric/cell combinations
 * produce NaN rather than dropping cells, preserving tuple correspondence. */
FVIZ_FILTERS_API FVizResult fviz_mesh_quality_compute(const FVizUnstructuredGrid* input, FVizMeshQualityMetric metric,
                                              FVizDataArray** out_quality);
FVIZ_FILTERS_API const char* fviz_mesh_quality_metric_name(FVizMeshQualityMetric metric);

FVIZ_FILTERS_API FVizResult fviz_mesh_quality_filter_create(FVizMeshQualityFilter** out_filter);
FVIZ_FILTERS_API FVizResult fviz_mesh_quality_filter_set_input_data(FVizMeshQualityFilter* filter, FVizUnstructuredGrid* input);
FVIZ_FILTERS_API FVizResult fviz_mesh_quality_filter_set_input_connection(FVizMeshQualityFilter* filter,
                                                                  FVizAlgorithmOutput* input);
FVIZ_FILTERS_API void fviz_mesh_quality_filter_set_metric(FVizMeshQualityFilter* filter, FVizMeshQualityMetric metric);
FVIZ_FILTERS_API FVizMeshQualityMetric fviz_mesh_quality_filter_metric(const FVizMeshQualityFilter* filter);
FVIZ_FILTERS_API FVizResult fviz_mesh_quality_filter_set_result_name(FVizMeshQualityFilter* filter, const char* name);
FVIZ_FILTERS_API const char* fviz_mesh_quality_filter_result_name(const FVizMeshQualityFilter* filter);
FVIZ_FILTERS_API void fviz_mesh_quality_filter_set_result_as_active_scalars(FVizMeshQualityFilter* filter, FVizBool enabled);
FVIZ_FILTERS_API FVizAlgorithm* fviz_mesh_quality_filter_algorithm(FVizMeshQualityFilter* filter);
FVIZ_FILTERS_API FVizAlgorithmOutput* fviz_mesh_quality_filter_output_port(FVizMeshQualityFilter* filter);
FVIZ_FILTERS_API FVizUnstructuredGrid* fviz_mesh_quality_filter_output(FVizMeshQualityFilter* filter);
FVIZ_FILTERS_API FVizResult fviz_mesh_quality_filter_update(FVizMeshQualityFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_MESH_QUALITY_FILTER_H */
