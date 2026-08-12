#ifndef FVIZ_INTERNAL_FEA_UNSTRUCTURED_GRID_PRIVATE_H
#define FVIZ_INTERNAL_FEA_UNSTRUCTURED_GRID_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataSet.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>

struct FVizUnstructuredGrid
{
    FVizObject base;
    FVizPoints* points;
    FVizCellArray* cells;
    FVizDataSet* data_set;
};

#endif /* FVIZ_INTERNAL_FEA_UNSTRUCTURED_GRID_PRIVATE_H */
