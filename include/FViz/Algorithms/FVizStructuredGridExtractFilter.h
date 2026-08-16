#ifndef FVIZ_ALGORITHMS_STRUCTURED_GRID_EXTRACT_FILTER_H
#define FVIZ_ALGORITHMS_STRUCTURED_GRID_EXTRACT_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizStructuredGrid.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizStructuredGridExtractFilter FVizStructuredGridExtractFilter;
#define FVIZ_TYPE_STRUCTURED_GRID_EXTRACT_FILTER UINT64_C(0xC38AB74D029E615F)

FVIZ_API FVizResult fviz_structured_grid_extract_filter_create(
    FVizStructuredGridExtractFilter** out_filter);
FVIZ_API FVizResult fviz_structured_grid_extract_filter_set_input_data(
    FVizStructuredGridExtractFilter* filter, FVizStructuredGrid* input);
FVIZ_API FVizResult fviz_structured_grid_extract_filter_set_input_connection(
    FVizStructuredGridExtractFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizAlgorithm* fviz_structured_grid_extract_filter_algorithm(
    FVizStructuredGridExtractFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_structured_grid_extract_filter_output_port(
    FVizStructuredGridExtractFilter* filter);
FVIZ_API FVizStructuredGrid* fviz_structured_grid_extract_filter_output(
    FVizStructuredGridExtractFilter* filter);
FVIZ_API FVizResult fviz_structured_grid_extract_filter_update(
    FVizStructuredGridExtractFilter* filter);
FVIZ_API FVizResult fviz_structured_grid_extract_filter_update_extent(
    FVizStructuredGridExtractFilter* filter, const int64_t extent[6], uint32_t ghost_levels);

FVIZ_EXTERN_C_END

#endif
