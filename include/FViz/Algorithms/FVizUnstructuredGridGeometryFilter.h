#ifndef FVIZ_ALGORITHMS_UNSTRUCTURED_GRID_GEOMETRY_FILTER_H
#define FVIZ_ALGORITHMS_UNSTRUCTURED_GRID_GEOMETRY_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizUnstructuredGridGeometryFilter FVizUnstructuredGridGeometryFilter;
#define FVIZ_TYPE_UNSTRUCTURED_GRID_GEOMETRY_FILTER UINT64_C(0xA4E9D3C81B7265F0)

FVIZ_API FVizResult fviz_unstructured_grid_geometry_filter_create(FVizUnstructuredGridGeometryFilter** out_filter);
FVIZ_API FVizResult fviz_unstructured_grid_geometry_filter_set_input_data(FVizUnstructuredGridGeometryFilter* filter,
                                                                          FVizUnstructuredGrid* input);
FVIZ_API FVizResult fviz_unstructured_grid_geometry_filter_set_input_connection(
    FVizUnstructuredGridGeometryFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_unstructured_grid_geometry_filter_algorithm(FVizUnstructuredGridGeometryFilter* filter);
FVIZ_API FVizAlgorithmOutput*
fviz_unstructured_grid_geometry_filter_output_port(FVizUnstructuredGridGeometryFilter* filter);
FVIZ_API FVizPolyData* fviz_unstructured_grid_geometry_filter_output(FVizUnstructuredGridGeometryFilter* filter);
FVIZ_API FVizResult fviz_unstructured_grid_geometry_filter_update(FVizUnstructuredGridGeometryFilter* filter);

FVIZ_EXTERN_C_END
#endif
