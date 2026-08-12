#ifndef FVIZ_INTERNAL_RENDERING_SCALAR_LEGEND_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_SCALAR_LEGEND_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Rendering/FVizScalarLegend.h>

struct FVizScalarLegend
{
    FVizObject base;
    FVizLookupTable* lookup_table;
    FVizString* title;
    float range_min;
    float range_max;
    FVizLegendPosition position;
    FVizBool visible;
};

#endif /* FVIZ_INTERNAL_RENDERING_SCALAR_LEGEND_PRIVATE_H */
