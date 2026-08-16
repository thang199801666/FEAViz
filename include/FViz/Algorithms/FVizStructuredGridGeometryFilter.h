#ifndef FVIZ_ALGORITHMS_STRUCTURED_GRID_GEOMETRY_FILTER_H
#define FVIZ_ALGORITHMS_STRUCTURED_GRID_GEOMETRY_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizStructuredGrid.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizStructuredGridGeometryFilter FVizStructuredGridGeometryFilter;
#define FVIZ_TYPE_STRUCTURED_GRID_GEOMETRY_FILTER UINT64_C(0xF1936C28A45DE70B)

FVIZ_API FVizResult fviz_structured_grid_geometry_filter_create(
    FVizStructuredGridGeometryFilter** out_filter);
FVIZ_API FVizResult fviz_structured_grid_geometry_filter_set_input_data(
    FVizStructuredGridGeometryFilter* filter, FVizStructuredGrid* input);
FVIZ_API FVizResult fviz_structured_grid_geometry_filter_set_input_connection(
    FVizStructuredGridGeometryFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_structured_grid_geometry_filter_algorithm(
    FVizStructuredGridGeometryFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_structured_grid_geometry_filter_output_port(
    FVizStructuredGridGeometryFilter* filter);
FVIZ_API FVizPolyData* fviz_structured_grid_geometry_filter_output(
    FVizStructuredGridGeometryFilter* filter);
FVIZ_API FVizResult fviz_structured_grid_geometry_filter_update(
    FVizStructuredGridGeometryFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_STRUCTURED_GRID_GEOMETRY_FILTER_H */
