#ifndef FVIZ_CORE_BIT_ARRAY_H
#define FVIZ_CORE_BIT_ARRAY_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizBitArray FVizBitArray;
#define FVIZ_TYPE_BIT_ARRAY UINT64_C(0x3A7C1E4B9F62D581)

FVIZ_CORE_API FVizResult fviz_bit_array_create(FVizSize bit_count, FVizBitArray** out_bit_array);
FVIZ_CORE_API FVizSize fviz_bit_array_count(const FVizBitArray* bit_array);
FVIZ_CORE_API FVizResult fviz_bit_array_set(FVizBitArray* bit_array, FVizSize index, FVizBool value);
FVIZ_CORE_API FVizBool fviz_bit_array_test(const FVizBitArray* bit_array, FVizSize index);
FVIZ_CORE_API FVizResult fviz_bit_array_resize(FVizBitArray* bit_array, FVizSize bit_count);
FVIZ_CORE_API void fviz_bit_array_clear(FVizBitArray* bit_array);
FVIZ_CORE_API void fviz_bit_array_set_all(FVizBitArray* bit_array, FVizBool value);
FVIZ_CORE_API FVizSize fviz_bit_array_pop_count(const FVizBitArray* bit_array);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_BIT_ARRAY_H */
