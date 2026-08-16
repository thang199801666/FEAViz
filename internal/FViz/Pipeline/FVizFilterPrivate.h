#ifndef FVIZ_INTERNAL_PIPELINE_FILTER_PRIVATE_H
#define FVIZ_INTERNAL_PIPELINE_FILTER_PRIVATE_H

#include <FViz/Pipeline/FVizFilter.h>
#include <FViz/Pipeline/FVizAlgorithmPrivate.h>

typedef enum FVizFilterKind
{
    FVIZ_FILTER_THRESHOLD = 1,
    FVIZ_FILTER_WARP = 2,
    FVIZ_FILTER_CELL_TO_POINT = 3,
    FVIZ_FILTER_SURFACE = 4,
    FVIZ_FILTER_SLICE = 5,
    FVIZ_FILTER_TRANSFORM = 6
} FVizFilterKind;

struct FVizFilter
{
    FVizAlgorithm base;
    FVizFilterKind kind;
    FVizBool transfer_scalars;
    char scalar_name[128];
    char vector_name[128];
    double minimum;
    double maximum;
    double scale;
    FVizPlane plane;
    FVizTransform* transform;
};

#endif /* FVIZ_INTERNAL_PIPELINE_FILTER_PRIVATE_H */
