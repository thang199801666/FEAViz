#ifndef FVIZ_INTERNAL_DATA_RECTILINEAR_GRID_PRIVATE_H
#define FVIZ_INTERNAL_DATA_RECTILINEAR_GRID_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataSet.h>
#include <FViz/Data/FVizRectilinearGrid.h>

struct FVizRectilinearGrid
{
    FVizObject base;
    FVizDataSet* data_set;
    FVizDataArray* coordinates[3];
    FVizObserverTag coordinate_modified_tags[3];
    FVizObserverTag data_set_modified_tag;
    uint32_t dependency_suppression;
    int64_t extent[6];
};

#endif /* FVIZ_INTERNAL_DATA_RECTILINEAR_GRID_PRIVATE_H */
