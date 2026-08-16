#ifndef FVIZ_INTERNAL_DATA_DATA_SET_PRIVATE_H
#define FVIZ_INTERNAL_DATA_DATA_SET_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataSet.h>

struct FVizDataSet
{
    FVizObject base;
    FVizSize point_count;
    FVizSize cell_count;
    FVizAttributeSet* point_data;
    FVizAttributeSet* cell_data;
    FVizAttributeSet* field_data;
    FVizObserverTag point_data_modified_tag;
    FVizObserverTag cell_data_modified_tag;
    FVizObserverTag field_data_modified_tag;
};

#endif /* FVIZ_INTERNAL_DATA_DATA_SET_PRIVATE_H */
