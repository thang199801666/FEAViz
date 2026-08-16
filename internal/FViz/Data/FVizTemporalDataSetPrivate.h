#ifndef FVIZ_INTERNAL_DATA_TEMPORAL_DATA_SET_PRIVATE_H
#define FVIZ_INTERNAL_DATA_TEMPORAL_DATA_SET_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizTemporalDataSet.h>

typedef struct FVizTemporalEntry
{
    double time;
    FVizDataObject* data;
    FVizObserverTag data_modified_tag;
} FVizTemporalEntry;

struct FVizTemporalDataSet
{
    FVizObject base;
    FVizArray* steps;
};

#endif /* FVIZ_INTERNAL_DATA_TEMPORAL_DATA_SET_PRIVATE_H */
