#ifndef FVIZ_INTERNAL_DATA_DATA_ARRAY_PRIVATE_H
#define FVIZ_INTERNAL_DATA_DATA_ARRAY_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataArray.h>

struct FVizDataArray
{
    FVizObject base;
    FVizDataType type;
    uint32_t components;
    FVizSize tuple_stride;
    FVizArray* storage;
};

#endif /* FVIZ_INTERNAL_DATA_DATA_ARRAY_PRIVATE_H */
