#ifndef FVIZ_ALGORITHMS_GEOMETRY_FILTERS_H
#define FVIZ_ALGORITHMS_GEOMETRY_FILTERS_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizTriangleFilter FVizTriangleFilter;
typedef struct FVizPolyDataNormalsFilter FVizPolyDataNormalsFilter;
typedef struct FVizFeatureEdgesFilter FVizFeatureEdgesFilter;
typedef struct FVizPolyDataConnectivityFilter FVizPolyDataConnectivityFilter;

#define FVIZ_TYPE_TRIANGLE_FILTER UINT64_C(0xE31A62C40B9D57F1)
#define FVIZ_TYPE_POLY_DATA_NORMALS_FILTER UINT64_C(0x7C914E2AB603D85F)
#define FVIZ_TYPE_FEATURE_EDGES_FILTER UINT64_C(0xD9F4271C6A30BE85)
#define FVIZ_TYPE_POLY_DATA_CONNECTIVITY_FILTER UINT64_C(0xA58D31E7C2946BF0)

typedef enum FVizConnectivityExtractionMode
{
    FVIZ_CONNECTIVITY_ALL_REGIONS = 0,
    FVIZ_CONNECTIVITY_LARGEST_REGION = 1
} FVizConnectivityExtractionMode;

typedef enum FVizFeatureEdgeType
{
    FVIZ_FEATURE_EDGE_BOUNDARY = 1,
    FVIZ_FEATURE_EDGE_FEATURE = 2,
    FVIZ_FEATURE_EDGE_NON_MANIFOLD = 3,
    FVIZ_FEATURE_EDGE_MANIFOLD = 4
} FVizFeatureEdgeType;

FVIZ_API FVizResult fviz_triangle_filter_create(FVizTriangleFilter** out_filter);
FVIZ_API void fviz_triangle_filter_set_pass_verts(FVizTriangleFilter* filter, FVizBool enabled);
FVIZ_API void fviz_triangle_filter_set_pass_lines(FVizTriangleFilter* filter, FVizBool enabled);
FVIZ_API FVizBool fviz_triangle_filter_pass_verts(const FVizTriangleFilter* filter);
FVIZ_API FVizBool fviz_triangle_filter_pass_lines(const FVizTriangleFilter* filter);
FVIZ_API FVizResult fviz_triangle_filter_set_input_data(FVizTriangleFilter* filter, FVizPolyData* input);
FVIZ_API FVizResult fviz_triangle_filter_set_input_connection(FVizTriangleFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_triangle_filter_algorithm(FVizTriangleFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_triangle_filter_output_port(FVizTriangleFilter* filter);
FVIZ_API FVizPolyData* fviz_triangle_filter_output(FVizTriangleFilter* filter);
FVIZ_API FVizResult fviz_triangle_filter_update(FVizTriangleFilter* filter);

FVIZ_API FVizResult fviz_poly_data_normals_filter_create(FVizPolyDataNormalsFilter** out_filter);
FVIZ_API FVizResult fviz_poly_data_normals_filter_set_array_name(FVizPolyDataNormalsFilter* filter, const char* name);
FVIZ_API const char* fviz_poly_data_normals_filter_array_name(const FVizPolyDataNormalsFilter* filter);
FVIZ_API FVizResult fviz_poly_data_normals_filter_set_input_data(FVizPolyDataNormalsFilter* filter,
                                                                 FVizPolyData* input);
FVIZ_API FVizResult fviz_poly_data_normals_filter_set_input_connection(FVizPolyDataNormalsFilter* filter,
                                                                       FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_poly_data_normals_filter_algorithm(FVizPolyDataNormalsFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_poly_data_normals_filter_output_port(FVizPolyDataNormalsFilter* filter);
FVIZ_API FVizPolyData* fviz_poly_data_normals_filter_output(FVizPolyDataNormalsFilter* filter);
FVIZ_API FVizResult fviz_poly_data_normals_filter_update(FVizPolyDataNormalsFilter* filter);

FVIZ_API FVizResult fviz_poly_data_connectivity_filter_create(FVizPolyDataConnectivityFilter** out_filter);
FVIZ_API void fviz_poly_data_connectivity_filter_set_extraction_mode(FVizPolyDataConnectivityFilter* filter,
                                                                     FVizConnectivityExtractionMode mode);
FVIZ_API FVizConnectivityExtractionMode
fviz_poly_data_connectivity_filter_extraction_mode(const FVizPolyDataConnectivityFilter* filter);
FVIZ_API FVizResult fviz_poly_data_connectivity_filter_set_array_name(FVizPolyDataConnectivityFilter* filter,
                                                                      const char* name);
FVIZ_API const char* fviz_poly_data_connectivity_filter_array_name(const FVizPolyDataConnectivityFilter* filter);
FVIZ_API uint32_t fviz_poly_data_connectivity_filter_region_count(const FVizPolyDataConnectivityFilter* filter);
FVIZ_API FVizResult fviz_poly_data_connectivity_filter_set_input_data(FVizPolyDataConnectivityFilter* filter,
                                                                      FVizPolyData* input);
FVIZ_API FVizResult fviz_poly_data_connectivity_filter_set_input_connection(FVizPolyDataConnectivityFilter* filter,
                                                                            FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_poly_data_connectivity_filter_algorithm(FVizPolyDataConnectivityFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_poly_data_connectivity_filter_output_port(FVizPolyDataConnectivityFilter* filter);
FVIZ_API FVizPolyData* fviz_poly_data_connectivity_filter_output(FVizPolyDataConnectivityFilter* filter);
FVIZ_API FVizResult fviz_poly_data_connectivity_filter_update(FVizPolyDataConnectivityFilter* filter);

FVIZ_API FVizResult fviz_feature_edges_filter_create(FVizFeatureEdgesFilter** out_filter);
FVIZ_API FVizResult fviz_feature_edges_filter_set_feature_angle(FVizFeatureEdgesFilter* filter, double degrees);
FVIZ_API double fviz_feature_edges_filter_feature_angle(const FVizFeatureEdgesFilter* filter);
FVIZ_API void fviz_feature_edges_filter_set_boundary_edges(FVizFeatureEdgesFilter* filter, FVizBool enabled);
FVIZ_API void fviz_feature_edges_filter_set_feature_edges(FVizFeatureEdgesFilter* filter, FVizBool enabled);
FVIZ_API void fviz_feature_edges_filter_set_non_manifold_edges(FVizFeatureEdgesFilter* filter, FVizBool enabled);
FVIZ_API void fviz_feature_edges_filter_set_manifold_edges(FVizFeatureEdgesFilter* filter, FVizBool enabled);
FVIZ_API FVizResult fviz_feature_edges_filter_set_input_data(FVizFeatureEdgesFilter* filter, FVizPolyData* input);
FVIZ_API FVizResult fviz_feature_edges_filter_set_input_connection(FVizFeatureEdgesFilter* filter,
                                                                   FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_feature_edges_filter_algorithm(FVizFeatureEdgesFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_feature_edges_filter_output_port(FVizFeatureEdgesFilter* filter);
FVIZ_API FVizPolyData* fviz_feature_edges_filter_output(FVizFeatureEdgesFilter* filter);
FVIZ_API FVizResult fviz_feature_edges_filter_update(FVizFeatureEdgesFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_GEOMETRY_FILTERS_H */
