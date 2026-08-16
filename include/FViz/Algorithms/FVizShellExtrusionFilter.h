#ifndef FVIZ_ALGORITHMS_SHELL_EXTRUSION_FILTER_H
#define FVIZ_ALGORITHMS_SHELL_EXTRUSION_FILTER_H

#include <stdint.h>
#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizShellExtrusionFilter FVizShellExtrusionFilter;
#define FVIZ_TYPE_SHELL_EXTRUSION_FILTER UINT64_C(0x71C40EA5962BF8D3)

FVIZ_FILTERS_API FVizResult fviz_shell_extrusion_filter_create(FVizShellExtrusionFilter** out_filter);
FVIZ_FILTERS_API FVizResult fviz_shell_extrusion_filter_set_input_data(FVizShellExtrusionFilter* filter, FVizPolyData* input);
FVIZ_FILTERS_API FVizResult fviz_shell_extrusion_filter_set_input_connection(FVizShellExtrusionFilter* filter,
                                                                     FVizAlgorithmOutput* input);
FVIZ_FILTERS_API void fviz_shell_extrusion_filter_set_thickness(FVizShellExtrusionFilter* filter, double thickness);
FVIZ_FILTERS_API double fviz_shell_extrusion_filter_thickness(const FVizShellExtrusionFilter* filter);
FVIZ_FILTERS_API FVizAlgorithm* fviz_shell_extrusion_filter_algorithm(FVizShellExtrusionFilter* filter);
FVIZ_FILTERS_API FVizAlgorithmOutput* fviz_shell_extrusion_filter_output_port(FVizShellExtrusionFilter* filter);
FVIZ_FILTERS_API FVizPolyData* fviz_shell_extrusion_filter_output(FVizShellExtrusionFilter* filter);
FVIZ_FILTERS_API FVizResult fviz_shell_extrusion_filter_update(FVizShellExtrusionFilter* filter);

FVIZ_EXTERN_C_END
#endif
