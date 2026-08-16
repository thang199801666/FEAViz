#ifndef FVIZ_INTERNAL_DATA_ATTRIBUTE_SET_PRIVATE_H
#define FVIZ_INTERNAL_DATA_ATTRIBUTE_SET_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizAttributeSet.h>

typedef struct FVizAttributeEntry
{
    FVizString* name;
    uint64_t name_hash;
    FVizDataArray* array;
    FVizObserverTag array_modified_tag;
} FVizAttributeEntry;

struct FVizAttributeSet
{
    FVizObject base;
    FVizArray* entries;
    FVizHashMap* name_index;
    FVizString* active[FVIZ_ATTRIBUTE_ROLE_COUNT];
};

#endif /* FVIZ_INTERNAL_DATA_ATTRIBUTE_SET_PRIVATE_H */
