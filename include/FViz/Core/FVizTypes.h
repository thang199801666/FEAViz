#ifndef FVIZ_CORE_TYPES_H
#define FVIZ_CORE_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include <FViz/Core/FVizApi.h>

FVIZ_EXTERN_C_BEGIN

typedef uint8_t FVizBool;

enum
{
    FVIZ_FALSE = 0,
    FVIZ_TRUE = 1
};

typedef uint64_t FVizId;
typedef size_t FVizSize;

typedef struct FVizDirtyRange
{
    FVizSize first;
    FVizSize count;
    FVizBool full;
} FVizDirtyRange;

#define FVIZ_INVALID_ID UINT64_MAX

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_TYPES_H */
