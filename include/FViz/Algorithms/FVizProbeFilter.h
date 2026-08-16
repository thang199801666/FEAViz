#ifndef FVIZ_ALGORITHMS_PROBE_FILTER_H
#define FVIZ_ALGORITHMS_PROBE_FILTER_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizProbeFilter FVizProbeFilter;
#define FVIZ_TYPE_PROBE_FILTER UINT64_C(0x73B5E81CA4D2069F)

/*
 * Samples point-data arrays from an UnstructuredGrid onto PolyData points.
 * Invalid samples are zero-filled and marked by the uint8 "FVizValidPointMask" array.
 */
FVIZ_API FVizResult fviz_probe_filter_create(FVizProbeFilter** out_filter);
FVIZ_API FVizResult fviz_probe_filter_set_input_data(FVizProbeFilter* filter, FVizPolyData* input);
FVIZ_API FVizResult fviz_probe_filter_set_input_connection(FVizProbeFilter* filter, FVizAlgorithmOutput* input);
FVIZ_API FVizResult fviz_probe_filter_set_source_data(FVizProbeFilter* filter, FVizUnstructuredGrid* source);
FVIZ_API FVizResult fviz_probe_filter_set_source_connection(FVizProbeFilter* filter, FVizAlgorithmOutput* source);
FVIZ_API FVizAlgorithm* fviz_probe_filter_algorithm(FVizProbeFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_probe_filter_output_port(FVizProbeFilter* filter);
FVIZ_API FVizPolyData* fviz_probe_filter_output(FVizProbeFilter* filter);
FVIZ_API FVizResult fviz_probe_filter_update(FVizProbeFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_PROBE_FILTER_H */
