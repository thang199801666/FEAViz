#ifndef FVIZ_ALGORITHMS_MESH_PROCESSING_FILTERS_H
#define FVIZ_ALGORITHMS_MESH_PROCESSING_FILTERS_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Math/FVizPlane.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizSmoothPolyDataFilter FVizSmoothPolyDataFilter;
typedef struct FVizDecimatePolyDataFilter FVizDecimatePolyDataFilter;
typedef struct FVizClipPolyDataFilter FVizClipPolyDataFilter;
#define FVIZ_TYPE_SMOOTH_POLY_DATA_FILTER UINT64_C(0x5D2F0A8BC193E764)
#define FVIZ_TYPE_DECIMATE_POLY_DATA_FILTER UINT64_C(0x8A16D4F329B07CE1)
#define FVIZ_TYPE_CLIP_POLY_DATA_FILTER UINT64_C(0x4C7A91E5B20FD638)

FVIZ_FILTERS_API FVizResult fviz_smooth_poly_data_filter_create(FVizSmoothPolyDataFilter** out_filter);
FVIZ_FILTERS_API FVizResult fviz_smooth_poly_data_filter_set_iterations(FVizSmoothPolyDataFilter* filter, uint32_t iterations);
FVIZ_FILTERS_API uint32_t fviz_smooth_poly_data_filter_iterations(const FVizSmoothPolyDataFilter* filter);
FVIZ_FILTERS_API FVizResult fviz_smooth_poly_data_filter_set_relaxation_factor(FVizSmoothPolyDataFilter* filter, double factor);
FVIZ_FILTERS_API double fviz_smooth_poly_data_filter_relaxation_factor(const FVizSmoothPolyDataFilter* filter);
FVIZ_FILTERS_API void fviz_smooth_poly_data_filter_set_boundary_smoothing(FVizSmoothPolyDataFilter* filter, FVizBool enabled);
FVIZ_FILTERS_API FVizBool fviz_smooth_poly_data_filter_boundary_smoothing(const FVizSmoothPolyDataFilter* filter);
FVIZ_FILTERS_API FVizResult fviz_smooth_poly_data_filter_set_input_data(FVizSmoothPolyDataFilter* filter, FVizPolyData* input);
FVIZ_FILTERS_API FVizResult fviz_smooth_poly_data_filter_set_input_connection(FVizSmoothPolyDataFilter* filter,
                                                                      FVizAlgorithmOutput* input);
FVIZ_FILTERS_API FVizAlgorithm* fviz_smooth_poly_data_filter_algorithm(FVizSmoothPolyDataFilter* filter);
FVIZ_FILTERS_API FVizAlgorithmOutput* fviz_smooth_poly_data_filter_output_port(FVizSmoothPolyDataFilter* filter);
FVIZ_FILTERS_API FVizPolyData* fviz_smooth_poly_data_filter_output(FVizSmoothPolyDataFilter* filter);
FVIZ_FILTERS_API FVizResult fviz_smooth_poly_data_filter_update(FVizSmoothPolyDataFilter* filter);

/* Fast deterministic vertex-clustering decimator. Target reduction is in [0, 0.99]. */
FVIZ_FILTERS_API FVizResult fviz_decimate_poly_data_filter_create(FVizDecimatePolyDataFilter** out_filter);
FVIZ_FILTERS_API FVizResult fviz_decimate_poly_data_filter_set_target_reduction(FVizDecimatePolyDataFilter* filter,
                                                                        double reduction);
FVIZ_FILTERS_API double fviz_decimate_poly_data_filter_target_reduction(const FVizDecimatePolyDataFilter* filter);
FVIZ_FILTERS_API FVizResult fviz_decimate_poly_data_filter_set_input_data(FVizDecimatePolyDataFilter* filter,
                                                                  FVizPolyData* input);
FVIZ_FILTERS_API FVizResult fviz_decimate_poly_data_filter_set_input_connection(FVizDecimatePolyDataFilter* filter,
                                                                        FVizAlgorithmOutput* input);
FVIZ_FILTERS_API FVizAlgorithm* fviz_decimate_poly_data_filter_algorithm(FVizDecimatePolyDataFilter* filter);
FVIZ_FILTERS_API FVizAlgorithmOutput* fviz_decimate_poly_data_filter_output_port(FVizDecimatePolyDataFilter* filter);
FVIZ_FILTERS_API FVizPolyData* fviz_decimate_poly_data_filter_output(FVizDecimatePolyDataFilter* filter);
FVIZ_FILTERS_API FVizResult fviz_decimate_poly_data_filter_update(FVizDecimatePolyDataFilter* filter);

/* Plane clip for render-ready triangle surfaces. Point attributes are interpolated.
 * Optional caps require closed manifold cut loops. Cap cells are identified by the
 * uint8 cell array "FVizClipCap" and have UINT64_MAX original-cell provenance. */
FVIZ_FILTERS_API FVizResult fviz_clip_poly_data_filter_create(FVizClipPolyDataFilter** out_filter);
FVIZ_FILTERS_API void fviz_clip_poly_data_filter_set_plane(FVizClipPolyDataFilter* filter, FVizPlane plane);
FVIZ_FILTERS_API FVizPlane fviz_clip_poly_data_filter_plane(const FVizClipPolyDataFilter* filter);
FVIZ_FILTERS_API void fviz_clip_poly_data_filter_set_inside_out(FVizClipPolyDataFilter* filter, FVizBool inside_out);
FVIZ_FILTERS_API FVizBool fviz_clip_poly_data_filter_inside_out(const FVizClipPolyDataFilter* filter);
FVIZ_FILTERS_API void fviz_clip_poly_data_filter_set_generate_cap(FVizClipPolyDataFilter* filter, FVizBool enabled);
FVIZ_FILTERS_API FVizBool fviz_clip_poly_data_filter_generate_cap(const FVizClipPolyDataFilter* filter);
FVIZ_FILTERS_API FVizResult fviz_clip_poly_data_filter_set_input_data(FVizClipPolyDataFilter* filter, FVizPolyData* input);
FVIZ_FILTERS_API FVizResult fviz_clip_poly_data_filter_set_input_connection(FVizClipPolyDataFilter* filter,
                                                                    FVizAlgorithmOutput* input);
FVIZ_FILTERS_API FVizAlgorithm* fviz_clip_poly_data_filter_algorithm(FVizClipPolyDataFilter* filter);
FVIZ_FILTERS_API FVizAlgorithmOutput* fviz_clip_poly_data_filter_output_port(FVizClipPolyDataFilter* filter);
FVIZ_FILTERS_API FVizPolyData* fviz_clip_poly_data_filter_output(FVizClipPolyDataFilter* filter);
FVIZ_FILTERS_API FVizResult fviz_clip_poly_data_filter_update(FVizClipPolyDataFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_MESH_PROCESSING_FILTERS_H */
