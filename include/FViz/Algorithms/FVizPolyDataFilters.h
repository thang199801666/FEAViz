#ifndef FVIZ_ALGORITHMS_POLY_DATA_FILTERS_H
#define FVIZ_ALGORITHMS_POLY_DATA_FILTERS_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Math/FVizTransform.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizTransformPolyDataFilter FVizTransformPolyDataFilter;
typedef struct FVizElevationFilter FVizElevationFilter;
typedef struct FVizAppendPolyDataFilter FVizAppendPolyDataFilter;
typedef struct FVizCleanPolyDataFilter FVizCleanPolyDataFilter;

#define FVIZ_TYPE_TRANSFORM_POLY_DATA_FILTER UINT64_C(0x4A08C7D1E36B59F2)
#define FVIZ_TYPE_ELEVATION_FILTER UINT64_C(0x91D26B5E4C70A83F)
#define FVIZ_TYPE_APPEND_POLY_DATA_FILTER UINT64_C(0xB38F20C64E9A175D)
#define FVIZ_TYPE_CLEAN_POLY_DATA_FILTER UINT64_C(0x6D42F8B31A90CE57)

FVIZ_API FVizResult fviz_transform_poly_data_filter_create(FVizTransform* transform,
                                                           FVizTransformPolyDataFilter** out_filter);
FVIZ_API FVizResult fviz_transform_poly_data_filter_set_transform(FVizTransformPolyDataFilter* filter,
                                                                  FVizTransform* transform);
FVIZ_API FVizTransform* fviz_transform_poly_data_filter_transform(FVizTransformPolyDataFilter* filter);
FVIZ_API FVizResult fviz_transform_poly_data_filter_set_input_data(FVizTransformPolyDataFilter* filter,
                                                                   FVizPolyData* input);
FVIZ_API FVizResult fviz_transform_poly_data_filter_set_input_connection(FVizTransformPolyDataFilter* filter,
                                                                         FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_transform_poly_data_filter_algorithm(FVizTransformPolyDataFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_transform_poly_data_filter_output_port(FVizTransformPolyDataFilter* filter);
FVIZ_API FVizPolyData* fviz_transform_poly_data_filter_output(FVizTransformPolyDataFilter* filter);
FVIZ_API FVizResult fviz_transform_poly_data_filter_update(FVizTransformPolyDataFilter* filter);

FVIZ_API FVizResult fviz_elevation_filter_create(FVizElevationFilter** out_filter);
FVIZ_API void fviz_elevation_filter_set_low_point(FVizElevationFilter* filter, FVizVec3 low_point);
FVIZ_API void fviz_elevation_filter_set_high_point(FVizElevationFilter* filter, FVizVec3 high_point);
FVIZ_API void fviz_elevation_filter_set_scalar_range(FVizElevationFilter* filter, double low, double high);
FVIZ_API FVizResult fviz_elevation_filter_set_array_name(FVizElevationFilter* filter, const char* name);
FVIZ_API FVizResult fviz_elevation_filter_set_input_data(FVizElevationFilter* filter, FVizPolyData* input);
FVIZ_API FVizResult fviz_elevation_filter_set_input_connection(FVizElevationFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_elevation_filter_algorithm(FVizElevationFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_elevation_filter_output_port(FVizElevationFilter* filter);
FVIZ_API FVizPolyData* fviz_elevation_filter_output(FVizElevationFilter* filter);
FVIZ_API FVizResult fviz_elevation_filter_update(FVizElevationFilter* filter);

FVIZ_API FVizResult fviz_append_poly_data_filter_create(FVizAppendPolyDataFilter** out_filter);
FVIZ_API FVizResult fviz_append_poly_data_filter_set_input_data(FVizAppendPolyDataFilter* filter, FVizPolyData* input);
FVIZ_API FVizResult fviz_append_poly_data_filter_set_input_connection(FVizAppendPolyDataFilter* filter,
                                                                      FVizAlgorithmOutput* input);
FVIZ_API FVizResult fviz_append_poly_data_filter_add_input_connection(FVizAppendPolyDataFilter* filter,
                                                                      FVizAlgorithmOutput* input);
FVIZ_API void fviz_append_poly_data_filter_remove_all_inputs(FVizAppendPolyDataFilter* filter);
FVIZ_API uint32_t fviz_append_poly_data_filter_input_count(const FVizAppendPolyDataFilter* filter);
FVIZ_API FVizAlgorithm* fviz_append_poly_data_filter_algorithm(FVizAppendPolyDataFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_append_poly_data_filter_output_port(FVizAppendPolyDataFilter* filter);
FVIZ_API FVizPolyData* fviz_append_poly_data_filter_output(FVizAppendPolyDataFilter* filter);
FVIZ_API FVizResult fviz_append_poly_data_filter_update(FVizAppendPolyDataFilter* filter);

FVIZ_API FVizResult fviz_clean_poly_data_filter_create(FVizCleanPolyDataFilter** out_filter);
FVIZ_API FVizResult fviz_clean_poly_data_filter_set_tolerance(FVizCleanPolyDataFilter* filter, double tolerance);
FVIZ_API double fviz_clean_poly_data_filter_tolerance(const FVizCleanPolyDataFilter* filter);
FVIZ_API void fviz_clean_poly_data_filter_set_remove_degenerate(FVizCleanPolyDataFilter* filter,
                                                                FVizBool remove_degenerate);
FVIZ_API FVizBool fviz_clean_poly_data_filter_remove_degenerate(const FVizCleanPolyDataFilter* filter);
FVIZ_API FVizResult fviz_clean_poly_data_filter_set_input_data(FVizCleanPolyDataFilter* filter, FVizPolyData* input);
FVIZ_API FVizResult fviz_clean_poly_data_filter_set_input_connection(FVizCleanPolyDataFilter* filter,
                                                                     FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_clean_poly_data_filter_algorithm(FVizCleanPolyDataFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_clean_poly_data_filter_output_port(FVizCleanPolyDataFilter* filter);
FVIZ_API FVizPolyData* fviz_clean_poly_data_filter_output(FVizCleanPolyDataFilter* filter);
FVIZ_API FVizResult fviz_clean_poly_data_filter_update(FVizCleanPolyDataFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_POLY_DATA_FILTERS_H */
