#ifndef FVIZ_INTERNAL_RENDERING_LOOKUP_TABLE_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_LOOKUP_TABLE_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizLookupTable.h>

struct FVizLookupTable
{
    FVizObject base;
    float* colors;
    FVizSize size;
    float range_min;
    float range_max;
    float nan_color[3];
    float below_range_color[3];
    float above_range_color[3];
    FVizBool use_below_range_color;
    FVizBool use_above_range_color;
};

#endif /* FVIZ_INTERNAL_RENDERING_LOOKUP_TABLE_PRIVATE_H */
