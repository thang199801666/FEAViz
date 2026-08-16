#ifndef FVIZ_INTERNAL_FEA_DEFORMED_SHAPE_PRIVATE_H
#define FVIZ_INTERNAL_FEA_DEFORMED_SHAPE_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/FEA/FVizDeformedShape.h>

struct FVizFEADeformedShapeResult
{
    FVizObject base;
    FVizFEADeformationState state;
    double scale_factor;
    FVizDeformationMetrics metrics;
    FVizSize mapped_point_count;
    FVizSize missing_point_count;
    FVizDataArray* displacements;
    FVizDataArray* coverage_mask;
    FVizUnstructuredGrid* base_grid;
    FVizUnstructuredGrid* grid;
};

struct FVizFEADeformedShapeController
{
    FVizObject base;
    const FVizFEAFrame* cached_frame;
    const FVizUnstructuredGrid* cached_grid;
    FVizMTime cached_frame_mtime;
    FVizMTime cached_grid_mtime;
    FVizString* cached_field_name;
    FVizString* cached_instance_name;
    FVizFEADeformedShapeOptions cached_options;
    FVizFEADeformedShapeResult* cached_result;
    uint64_t hits;
    uint64_t misses;
    uint64_t clears;
};

#endif /* FVIZ_INTERNAL_FEA_DEFORMED_SHAPE_PRIVATE_H */
