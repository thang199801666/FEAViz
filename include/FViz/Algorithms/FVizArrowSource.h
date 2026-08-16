#ifndef FVIZ_ALGORITHMS_ARROW_SOURCE_H
#define FVIZ_ALGORITHMS_ARROW_SOURCE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizArrowSource FVizArrowSource;
#define FVIZ_TYPE_ARROW_SOURCE UINT64_C(0x9A4205B4E6792F13)

/* Unit arrow aligned to +X. Total length is one. */
FVIZ_API FVizResult fviz_arrow_source_create(FVizArrowSource** out_source);
FVIZ_API FVizResult fviz_arrow_source_set_shaft_radius(FVizArrowSource* source, double radius);
FVIZ_API FVizResult fviz_arrow_source_set_tip_radius(FVizArrowSource* source, double radius);
FVIZ_API FVizResult fviz_arrow_source_set_tip_length(FVizArrowSource* source, double length);
FVIZ_API FVizResult fviz_arrow_source_set_radial_resolution(FVizArrowSource* source, uint32_t resolution);
FVIZ_API double fviz_arrow_source_shaft_radius(const FVizArrowSource* source);
FVIZ_API double fviz_arrow_source_tip_radius(const FVizArrowSource* source);
FVIZ_API double fviz_arrow_source_tip_length(const FVizArrowSource* source);
FVIZ_API uint32_t fviz_arrow_source_radial_resolution(const FVizArrowSource* source);
FVIZ_API FVizAlgorithm* fviz_arrow_source_algorithm(FVizArrowSource* source);
FVIZ_API FVizAlgorithmOutput* fviz_arrow_source_output_port(FVizArrowSource* source);
FVIZ_API FVizPolyData* fviz_arrow_source_output(FVizArrowSource* source);
FVIZ_API FVizResult fviz_arrow_source_update(FVizArrowSource* source);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_ARROW_SOURCE_H */
