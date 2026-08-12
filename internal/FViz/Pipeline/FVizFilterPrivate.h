#ifndef FVIZ_INTERNAL_PIPELINE_FILTER_PRIVATE_H
#define FVIZ_INTERNAL_PIPELINE_FILTER_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Pipeline/FVizFilter.h>

typedef enum FVizFilterKind
{
    FVIZ_FILTER_THRESHOLD = 1,
    FVIZ_FILTER_WARP = 2,
    FVIZ_FILTER_CELL_TO_POINT = 3
} FVizFilterKind;

struct FVizFilter
{
    FVizObject base;
    FVizFilterKind kind;
    FVizUnstructuredGrid* input;
    FVizUnstructuredGrid* output;
    uint32_t input_generation;
    FVizBool updated;
    char scalar_name[128];
    char vector_name[128];
    double minimum;
    double maximum;
    double scale;
};

#endif /* FVIZ_INTERNAL_PIPELINE_FILTER_PRIVATE_H */
