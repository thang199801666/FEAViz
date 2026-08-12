#ifndef FVIZ_INTERNAL_CORE_ARRAY_PRIVATE_H
#define FVIZ_INTERNAL_CORE_ARRAY_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizArray
{
    FVizObject base;
    unsigned char* data;
    FVizSize count;
    FVizSize capacity;
    FVizSize stride;
};

#endif /* FVIZ_INTERNAL_CORE_ARRAY_PRIVATE_H */
