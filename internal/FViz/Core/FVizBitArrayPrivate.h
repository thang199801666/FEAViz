#ifndef FVIZ_INTERNAL_CORE_BIT_ARRAY_PRIVATE_H
#define FVIZ_INTERNAL_CORE_BIT_ARRAY_PRIVATE_H

#include <stdint.h>

#include <FViz/Core/FVizBitArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizTypes.h>

struct FVizBitArray
{
    FVizObject base;
    uint64_t* words;
    FVizSize bit_count;
    FVizSize capacity_words;
};

#endif /* FVIZ_INTERNAL_CORE_BIT_ARRAY_PRIVATE_H */
