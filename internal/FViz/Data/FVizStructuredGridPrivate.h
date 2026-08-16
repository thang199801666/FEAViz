#ifndef FVIZ_INTERNAL_DATA_STRUCTURED_GRID_PRIVATE_H
#define FVIZ_INTERNAL_DATA_STRUCTURED_GRID_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataSet.h>
#include <FViz/Data/FVizStructuredGrid.h>
#include <FViz/Mesh/FVizPoints.h>

struct FVizStructuredGrid
{
    FVizObject base;
    FVizPoints* points;
    FVizDataSet* data_set;
    FVizObserverTag points_modified_tag;
    FVizObserverTag data_set_modified_tag;
    uint32_t dependency_suppression;
    int64_t extent[6];
};

#endif /* FVIZ_INTERNAL_DATA_STRUCTURED_GRID_PRIVATE_H */
