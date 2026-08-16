#ifndef FVIZ_INTERNAL_DATA_PARTITIONED_DATA_SET_PRIVATE_H
#define FVIZ_INTERNAL_DATA_PARTITIONED_DATA_SET_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizPartitionedDataSet.h>

typedef struct FVizPartitionEntry
{
    FVizDataObject* data;
    FVizString* name;
    FVizObserverTag data_modified_tag;
} FVizPartitionEntry;

struct FVizPartitionedDataSet
{
    FVizObject base;
    FVizArray* partitions;
};

#endif /* FVIZ_INTERNAL_DATA_PARTITIONED_DATA_SET_PRIVATE_H */
