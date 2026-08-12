#ifndef FVIZ_INTERNAL_ALGORITHMS_CONTOUR_FILTER_PRIVATE_H
#define FVIZ_INTERNAL_ALGORITHMS_CONTOUR_FILTER_PRIVATE_H

#include <FViz/Algorithms/FVizContourFilter.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizContourFilter
{
    FVizObject base;
    FVizPolyData* input;
    FVizPolyData* output;
    char scalar_name[128];
    float* levels;
    FVizSize level_count;
    FVizMTime input_mtime;
    FVizBool updated;
};

#endif /* FVIZ_INTERNAL_ALGORITHMS_CONTOUR_FILTER_PRIVATE_H */
