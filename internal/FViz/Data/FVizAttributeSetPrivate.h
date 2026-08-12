#ifndef FVIZ_INTERNAL_DATA_ATTRIBUTE_SET_PRIVATE_H
#define FVIZ_INTERNAL_DATA_ATTRIBUTE_SET_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizAttributeSet.h>

typedef struct FVizAttributeEntry
{
    FVizString* name;
    FVizDataArray* array;
} FVizAttributeEntry;

struct FVizAttributeSet
{
    FVizObject base;
    FVizArray* entries;
};

#endif /* FVIZ_INTERNAL_DATA_ATTRIBUTE_SET_PRIVATE_H */
