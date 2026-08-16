#ifndef FVIZ_ALGORITHMS_RECTILINEAR_GRID_EXTRACT_FILTER_H
#define FVIZ_ALGORITHMS_RECTILINEAR_GRID_EXTRACT_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizRectilinearGrid.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRectilinearGridExtractFilter FVizRectilinearGridExtractFilter;
#define FVIZ_TYPE_RECTILINEAR_GRID_EXTRACT_FILTER UINT64_C(0xA8D7306F21CB945E)

FVIZ_API FVizResult fviz_rectilinear_grid_extract_filter_create(
    FVizRectilinearGridExtractFilter** out_filter);
FVIZ_API FVizResult fviz_rectilinear_grid_extract_filter_set_input_data(
    FVizRectilinearGridExtractFilter* filter, FVizRectilinearGrid* input);
FVIZ_API FVizResult fviz_rectilinear_grid_extract_filter_set_input_connection(
    FVizRectilinearGridExtractFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_rectilinear_grid_extract_filter_algorithm(
    FVizRectilinearGridExtractFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_rectilinear_grid_extract_filter_output_port(
    FVizRectilinearGridExtractFilter* filter);
FVIZ_API FVizRectilinearGrid* fviz_rectilinear_grid_extract_filter_output(
    FVizRectilinearGridExtractFilter* filter);
FVIZ_API FVizResult fviz_rectilinear_grid_extract_filter_update(
    FVizRectilinearGridExtractFilter* filter);
FVIZ_API FVizResult fviz_rectilinear_grid_extract_filter_update_extent(
    FVizRectilinearGridExtractFilter* filter, const int64_t extent[6], uint32_t ghost_levels);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_RECTILINEAR_GRID_EXTRACT_FILTER_H */
