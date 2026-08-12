#ifndef FVIZ_PIPELINE_FILTER_H
#define FVIZ_PIPELINE_FILTER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizFilter FVizFilter;
#define FVIZ_TYPE_FILTER UINT64_C(0x5C1E8FA30B274D69)
#define FVIZ_TYPE_THRESHOLD_FILTER UINT64_C(0x9A2D4B6C7E8F0192)
#define FVIZ_TYPE_WARP_FILTER UINT64_C(0x6C9E0F1B2D3A5C7E)
#define FVIZ_TYPE_CELL_DATA_TO_POINT_FILTER UINT64_C(0xE1F2A3B4C5D6E7F8)

FVIZ_API FVizResult fviz_threshold_filter_create(
    const char* scalar_name,
    double minimum,
    double maximum,
    FVizFilter** out_filter);
FVIZ_API FVizResult fviz_warp_filter_create(
    const char* vector_name,
    double scale,
    FVizFilter** out_filter);
FVIZ_API FVizResult fviz_cell_data_to_point_filter_create(FVizFilter** out_filter);

FVIZ_API FVizResult fviz_filter_set_input(FVizFilter* filter, const FVizUnstructuredGrid* input);
FVIZ_API const FVizUnstructuredGrid* fviz_filter_const_input(const FVizFilter* filter);
FVIZ_API FVizUnstructuredGrid* fviz_filter_output(FVizFilter* filter);
FVIZ_API FVizResult fviz_filter_update(FVizFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_PIPELINE_FILTER_H */
