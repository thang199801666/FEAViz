#ifndef FVIZ_DATA_DATA_OBJECT_H
#define FVIZ_DATA_DATA_OBJECT_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizDataObject FVizDataObject;
#define FVIZ_TYPE_DATA_OBJECT UINT64_C(0x7D10B5A9E4C263F1)

FVIZ_API FVizBool fviz_data_object_is_data_object(const FVizDataObject* data_object);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_DATA_OBJECT_H */
