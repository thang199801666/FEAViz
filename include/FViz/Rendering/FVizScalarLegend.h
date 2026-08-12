#ifndef FVIZ_RENDERING_SCALAR_LEGEND_H
#define FVIZ_RENDERING_SCALAR_LEGEND_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Rendering/FVizLookupTable.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizScalarLegend FVizScalarLegend;
#define FVIZ_TYPE_SCALAR_LEGEND UINT64_C(0x9F3C7A51E8B2D406)

typedef enum FVizLegendPosition
{
    FVIZ_LEGEND_TOP_RIGHT = 0,
    FVIZ_LEGEND_TOP_LEFT = 1,
    FVIZ_LEGEND_BOTTOM_RIGHT = 2,
    FVIZ_LEGEND_BOTTOM_LEFT = 3
} FVizLegendPosition;

FVIZ_API FVizResult fviz_scalar_legend_create(FVizScalarLegend** out_legend);
FVIZ_API void fviz_scalar_legend_set_lookup_table(FVizScalarLegend* legend, FVizLookupTable* table);
FVIZ_API FVizLookupTable* fviz_scalar_legend_lookup_table(FVizScalarLegend* legend);
FVIZ_API void fviz_scalar_legend_set_range(FVizScalarLegend* legend, float minimum, float maximum);
FVIZ_API void fviz_scalar_legend_get_range(const FVizScalarLegend* legend, float* minimum, float* maximum);
FVIZ_API void fviz_scalar_legend_set_position(FVizScalarLegend* legend, FVizLegendPosition position);
FVIZ_API FVizLegendPosition fviz_scalar_legend_position(const FVizScalarLegend* legend);
FVIZ_API void fviz_scalar_legend_set_visible(FVizScalarLegend* legend, FVizBool visible);
FVIZ_API FVizBool fviz_scalar_legend_is_visible(const FVizScalarLegend* legend);
FVIZ_API void fviz_scalar_legend_set_title(FVizScalarLegend* legend, const char* title);
FVIZ_API const char* fviz_scalar_legend_title(const FVizScalarLegend* legend);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_SCALAR_LEGEND_H */
