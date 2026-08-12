#ifndef FVIZ_INTERNAL_FEA_UNSTRUCTURED_GRID_PRIVATE_H
#define FVIZ_INTERNAL_FEA_UNSTRUCTURED_GRID_PRIVATE_H

#include <stdint.h>

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataSet.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>

struct FVizUnstructuredGrid
{
    FVizObject base;
    FVizPoints* points;
    FVizCellArray* cells;
    FVizDataSet* data_set;
    uint32_t generation;
};

uint32_t fviz_internal_unstructured_grid_generation(const FVizUnstructuredGrid* grid);

#endif /* FVIZ_INTERNAL_FEA_UNSTRUCTURED_GRID_PRIVATE_H */
