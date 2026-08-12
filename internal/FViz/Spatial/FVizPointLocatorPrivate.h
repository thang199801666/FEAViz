#ifndef FVIZ_INTERNAL_SPATIAL_POINT_LOCATOR_PRIVATE_H
#define FVIZ_INTERNAL_SPATIAL_POINT_LOCATOR_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Spatial/FVizPointLocator.h>

struct FVizPointLocator
{
    FVizObject base;
    FVizUnstructuredGrid* grid;
};

#endif /* FVIZ_INTERNAL_SPATIAL_POINT_LOCATOR_PRIVATE_H */
