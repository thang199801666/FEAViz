#ifndef FVIZ_INTERNAL_CORE_STRING_PRIVATE_H
#define FVIZ_INTERNAL_CORE_STRING_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>

struct FVizString
{
    FVizObject base;
    char* data;
    FVizSize length;
    FVizSize capacity;
};

#endif /* FVIZ_INTERNAL_CORE_STRING_PRIVATE_H */
