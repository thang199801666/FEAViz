#ifndef FVIZ_DATA_DATA_ARRAY_H
#define FVIZ_DATA_DATA_ARRAY_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataType.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizDataArray FVizDataArray;
#define FVIZ_TYPE_DATA_ARRAY UINT64_C(0x9DBB30DBD2611D25)

FVIZ_API FVizResult fviz_data_array_create(FVizDataType type, uint32_t components, FVizDataArray** out_array);
FVIZ_API FVizDataType fviz_data_array_type(const FVizDataArray* array);
FVIZ_API uint32_t fviz_data_array_components(const FVizDataArray* array);
FVIZ_API FVizSize fviz_data_array_tuple_count(const FVizDataArray* array);
FVIZ_API FVizSize fviz_data_array_tuple_stride(const FVizDataArray* array);
FVIZ_API void* fviz_data_array_data(FVizDataArray* array);
FVIZ_API const void* fviz_data_array_const_data(const FVizDataArray* array);
FVIZ_API FVizResult fviz_data_array_resize(FVizDataArray* array, FVizSize tuple_count);
FVIZ_API FVizResult fviz_data_array_reserve(FVizDataArray* array, FVizSize tuple_capacity);
FVIZ_API FVizResult fviz_data_array_append_tuple(FVizDataArray* array, const void* tuple);
FVIZ_API FVizResult fviz_data_array_set_tuple(FVizDataArray* array, FVizSize index, const void* tuple);
FVIZ_API void* fviz_data_array_tuple(FVizDataArray* array, FVizSize index);
FVIZ_API const void* fviz_data_array_const_tuple(const FVizDataArray* array, FVizSize index);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_DATA_ARRAY_H */
