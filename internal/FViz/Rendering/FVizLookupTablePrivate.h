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
};

#endif /* FVIZ_INTERNAL_RENDERING_LOOKUP_TABLE_PRIVATE_H */
