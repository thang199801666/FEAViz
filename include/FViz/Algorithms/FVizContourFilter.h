#ifndef FVIZ_ALGORITHMS_CONTOUR_FILTER_H
#define FVIZ_ALGORITHMS_CONTOUR_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizContourFilter FVizContourFilter;
#define FVIZ_TYPE_CONTOUR_FILTER UINT64_C(0x7A3E9F2C4B6D8A15)

FVIZ_API FVizResult fviz_contour_filter_create(const char* scalar_name, const float* levels, FVizSize level_count,
                                               FVizContourFilter** out_filter);
FVIZ_API FVizResult fviz_contour_filter_set_input(FVizContourFilter* filter, const FVizPolyData* poly_data);
FVIZ_API const FVizPolyData* fviz_contour_filter_const_input(const FVizContourFilter* filter);
FVIZ_API FVizPolyData* fviz_contour_filter_output(FVizContourFilter* filter);
FVIZ_API FVizResult fviz_contour_filter_update(FVizContourFilter* filter);
FVIZ_API FVizSize fviz_contour_filter_level_count(const FVizContourFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_CONTOUR_FILTER_H */
