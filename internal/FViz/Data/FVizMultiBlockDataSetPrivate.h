#ifndef FVIZ_INTERNAL_DATA_MULTI_BLOCK_DATA_SET_PRIVATE_H
#define FVIZ_INTERNAL_DATA_MULTI_BLOCK_DATA_SET_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizMultiBlockDataSet.h>

typedef struct FVizMultiBlockEntry
{
    FVizDataObject* data;
    FVizString* name;
    FVizObserverTag data_modified_tag;
} FVizMultiBlockEntry;

struct FVizMultiBlockDataSet
{
    FVizObject base;
    FVizArray* blocks;
};

#endif /* FVIZ_INTERNAL_DATA_MULTI_BLOCK_DATA_SET_PRIVATE_H */
