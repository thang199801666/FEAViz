#ifndef FVIZ_INTERNAL_FEA_UNSTRUCTURED_GRID_PRIVATE_H
#define FVIZ_INTERNAL_FEA_UNSTRUCTURED_GRID_PRIVATE_H

#include <stdint.h>

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataSet.h>
#include <FViz/Data/FVizUnstructuredGrid.h>

struct FVizUnstructuredGrid
{
    FVizObject base;
    FVizPoints* points;
    FVizCellArray* cells;
    FVizDataSet* data_set;
    FVizObserverTag points_modified_tag;
    FVizObserverTag cells_modified_tag;
    FVizObserverTag data_set_modified_tag;
    uint32_t dependency_suppression;
};

#endif /* FVIZ_INTERNAL_FEA_UNSTRUCTURED_GRID_PRIVATE_H */
