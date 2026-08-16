#ifndef FVIZ_ALGORITHMS_RECTILINEAR_GRID_GEOMETRY_FILTER_H
#define FVIZ_ALGORITHMS_RECTILINEAR_GRID_GEOMETRY_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizRectilinearGrid.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRectilinearGridGeometryFilter FVizRectilinearGridGeometryFilter;
#define FVIZ_TYPE_RECTILINEAR_GRID_GEOMETRY_FILTER UINT64_C(0xEA492C7BD186350F)

FVIZ_API FVizResult fviz_rectilinear_grid_geometry_filter_create(
    FVizRectilinearGridGeometryFilter** out_filter);
FVIZ_API FVizResult fviz_rectilinear_grid_geometry_filter_set_input_data(
    FVizRectilinearGridGeometryFilter* filter, FVizRectilinearGrid* input);
FVIZ_API FVizResult fviz_rectilinear_grid_geometry_filter_set_input_connection(
    FVizRectilinearGridGeometryFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_rectilinear_grid_geometry_filter_algorithm(
    FVizRectilinearGridGeometryFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_rectilinear_grid_geometry_filter_output_port(
    FVizRectilinearGridGeometryFilter* filter);
FVIZ_API FVizPolyData* fviz_rectilinear_grid_geometry_filter_output(
    FVizRectilinearGridGeometryFilter* filter);
FVIZ_API FVizResult fviz_rectilinear_grid_geometry_filter_update(
    FVizRectilinearGridGeometryFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_RECTILINEAR_GRID_GEOMETRY_FILTER_H */
