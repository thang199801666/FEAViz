#ifndef FVIZ_ALGORITHMS_IMAGE_DATA_GEOMETRY_FILTER_H
#define FVIZ_ALGORITHMS_IMAGE_DATA_GEOMETRY_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizImageData.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizImageDataGeometryFilter FVizImageDataGeometryFilter;
#define FVIZ_TYPE_IMAGE_DATA_GEOMETRY_FILTER UINT64_C(0xE9B50A14C72D8136)

FVIZ_API FVizResult fviz_image_data_geometry_filter_create(FVizImageDataGeometryFilter** out_filter);
FVIZ_API FVizResult fviz_image_data_geometry_filter_set_input_data(
    FVizImageDataGeometryFilter* filter, FVizImageData* input);
FVIZ_API FVizResult fviz_image_data_geometry_filter_set_input_connection(
    FVizImageDataGeometryFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_image_data_geometry_filter_algorithm(FVizImageDataGeometryFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_image_data_geometry_filter_output_port(FVizImageDataGeometryFilter* filter);
FVIZ_API FVizPolyData* fviz_image_data_geometry_filter_output(FVizImageDataGeometryFilter* filter);
FVIZ_API FVizResult fviz_image_data_geometry_filter_update(FVizImageDataGeometryFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_IMAGE_DATA_GEOMETRY_FILTER_H */
