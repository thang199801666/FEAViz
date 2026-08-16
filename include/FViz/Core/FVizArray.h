#ifndef FVIZ_CORE_ARRAY_H
#define FVIZ_CORE_ARRAY_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizArray FVizArray;
#define FVIZ_TYPE_ARRAY UINT64_C(0x4D6E516BF218B621)

FVIZ_API FVizResult fviz_array_create(FVizSize stride, FVizArray** out_array);
FVIZ_API FVizResult fviz_array_create_reserve(FVizSize stride, FVizSize capacity, FVizArray** out_array);
FVIZ_API FVizSize fviz_array_count(const FVizArray* array);
FVIZ_API FVizSize fviz_array_capacity(const FVizArray* array);
FVIZ_API FVizSize fviz_array_stride(const FVizArray* array);
FVIZ_API void* fviz_array_data(FVizArray* array);
FVIZ_API const void* fviz_array_const_data(const FVizArray* array);
FVIZ_API void* fviz_array_at(FVizArray* array, FVizSize index);
FVIZ_API const void* fviz_array_const_at(const FVizArray* array, FVizSize index);
FVIZ_API FVizResult fviz_array_reserve(FVizArray* array, FVizSize capacity);
FVIZ_API FVizResult fviz_array_resize(FVizArray* array, FVizSize count);
FVIZ_API FVizResult fviz_array_push(FVizArray* array, const void* value);
/* Bulk append grows geometrically and updates MTime once for the whole range. */
FVIZ_API FVizResult fviz_array_append(FVizArray* array, const void* values, FVizSize count);
FVIZ_API FVizResult fviz_array_push_uninitialized(FVizArray* array, void** out_slot);
FVIZ_API FVizResult fviz_array_append_uninitialized(FVizArray* array, FVizSize count, void** out_first_slot);
FVIZ_API void fviz_array_clear(FVizArray* array);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_ARRAY_H */
