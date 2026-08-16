#ifndef FVIZ_DATA_DATA_TYPE_H
#define FVIZ_DATA_DATA_TYPE_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizDataType
{
    FVIZ_DATA_INT8 = 1,
    FVIZ_DATA_UINT8 = 2,
    FVIZ_DATA_INT16 = 3,
    FVIZ_DATA_UINT16 = 4,
    FVIZ_DATA_INT32 = 5,
    FVIZ_DATA_UINT32 = 6,
    FVIZ_DATA_INT64 = 7,
    FVIZ_DATA_UINT64 = 8,
    FVIZ_DATA_FLOAT32 = 9,
    FVIZ_DATA_FLOAT64 = 10
} FVizDataType;

FVIZ_DATA_API FVizSize fviz_data_type_size(FVizDataType type);
FVIZ_DATA_API const char* fviz_data_type_name(FVizDataType type);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_DATA_TYPE_H */
