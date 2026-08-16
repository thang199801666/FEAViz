#ifndef FVIZ_PIPELINE_FILTER_H
#define FVIZ_PIPELINE_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizFilter FVizFilter;
#define FVIZ_TYPE_FILTER UINT64_C(0x5C1E8FA30B274D69)
#define FVIZ_TYPE_THRESHOLD_FILTER UINT64_C(0x9A2D4B6C7E8F0192)
#define FVIZ_TYPE_WARP_FILTER UINT64_C(0x6C9E0F1B2D3A5C7E)
#define FVIZ_TYPE_CELL_DATA_TO_POINT_FILTER UINT64_C(0xE1F2A3B4C5D6E7F8)
#define FVIZ_TYPE_SURFACE_FILTER UINT64_C(0x3B8F61D4A2C709E5)
#define FVIZ_TYPE_SLICE_FILTER UINT64_C(0xD5A7083E1C9F426B)
#define FVIZ_TYPE_TRANSFORM_FILTER UINT64_C(0xF2379A4D6E105BC8)

typedef enum FVizFilterOutputType
{
    FVIZ_FILTER_OUTPUT_NONE = 0,
    FVIZ_FILTER_OUTPUT_UNSTRUCTURED_GRID = 1,
    FVIZ_FILTER_OUTPUT_POLY_DATA = 2
} FVizFilterOutputType;

FVIZ_API FVizResult fviz_threshold_filter_create(
    const char* scalar_name,
    double minimum,
    double maximum,
    FVizFilter** out_filter);
FVIZ_API FVizResult fviz_warp_filter_create(
    const char* vector_name,
    double scale,
    FVizFilter** out_filter);
FVIZ_API FVizResult fviz_cell_data_to_point_filter_create(FVizFilter** out_filter);
FVIZ_API FVizResult fviz_surface_filter_create(FVizBool transfer_scalars, FVizFilter** out_filter);
FVIZ_API FVizResult fviz_slice_filter_create(FVizPlane plane, FVizFilter** out_filter);
FVIZ_API FVizResult fviz_transform_filter_create(FVizTransform* transform, FVizFilter** out_filter);

FVIZ_API FVizResult fviz_threshold_filter_set_scalar_name(FVizFilter* filter, const char* scalar_name);
FVIZ_API FVizResult fviz_threshold_filter_set_range(FVizFilter* filter, double minimum, double maximum);
FVIZ_API FVizResult fviz_warp_filter_set_vector_name(FVizFilter* filter, const char* vector_name);
FVIZ_API FVizResult fviz_warp_filter_set_scale(FVizFilter* filter, double scale);
FVIZ_API FVizResult fviz_surface_filter_set_transfer_scalars(FVizFilter* filter, FVizBool transfer_scalars);
FVIZ_API FVizResult fviz_slice_filter_set_plane(FVizFilter* filter, FVizPlane plane);
FVIZ_API FVizResult fviz_transform_filter_set_transform(FVizFilter* filter, FVizTransform* transform);
FVIZ_API FVizTransform* fviz_transform_filter_transform(FVizFilter* filter);

FVIZ_API FVizResult fviz_filter_set_input(FVizFilter* filter, const FVizUnstructuredGrid* input);
FVIZ_API FVizResult fviz_filter_set_input_connection(FVizFilter* filter, FVizFilter* upstream);
FVIZ_API FVizFilter* fviz_filter_input_connection(FVizFilter* filter);
FVIZ_API FVizAlgorithm* fviz_filter_algorithm(FVizFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_filter_output_port(FVizFilter* filter);
FVIZ_API const FVizUnstructuredGrid* fviz_filter_const_input(const FVizFilter* filter);
FVIZ_API FVizFilterOutputType fviz_filter_output_type(const FVizFilter* filter);
FVIZ_API FVizUnstructuredGrid* fviz_filter_output(FVizFilter* filter);
FVIZ_API FVizPolyData* fviz_filter_poly_data_output(FVizFilter* filter);
FVIZ_API FVizResult fviz_filter_update(FVizFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_PIPELINE_FILTER_H */
