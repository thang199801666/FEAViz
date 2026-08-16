#ifndef FVIZ_INTERNAL_DATA_IMAGE_DATA_PRIVATE_H
#define FVIZ_INTERNAL_DATA_IMAGE_DATA_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataSet.h>
#include <FViz/Data/FVizImageData.h>

struct FVizImageData
{
    FVizObject base;
    FVizDataSet* data_set;
    FVizObserverTag data_set_modified_tag;
    uint32_t dependency_suppression;
    int64_t extent[6];
    double origin[3];
    double spacing[3];
    double direction[9];
    double inverse_direction[9];
};

#endif /* FVIZ_INTERNAL_DATA_IMAGE_DATA_PRIVATE_H */
