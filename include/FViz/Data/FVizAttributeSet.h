#ifndef FVIZ_DATA_ATTRIBUTE_SET_H
#define FVIZ_DATA_ATTRIBUTE_SET_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataArray.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizAttributeSet FVizAttributeSet;
#define FVIZ_TYPE_ATTRIBUTE_SET UINT64_C(0xB9A7A4C8E6D14F21)

FVIZ_API FVizResult fviz_attribute_set_create(FVizAttributeSet** out_set);
FVIZ_API void fviz_attribute_set_clear(FVizAttributeSet* set);
FVIZ_API FVizSize fviz_attribute_set_count(const FVizAttributeSet* set);
FVIZ_API const char* fviz_attribute_set_name_at(const FVizAttributeSet* set, FVizSize index);
FVIZ_API FVizDataArray* fviz_attribute_set_array_at(FVizAttributeSet* set, FVizSize index);
FVIZ_API const FVizDataArray* fviz_attribute_set_const_array_at(const FVizAttributeSet* set, FVizSize index);
FVIZ_API FVizDataArray* fviz_attribute_set_get(FVizAttributeSet* set, const char* name);
FVIZ_API const FVizDataArray* fviz_attribute_set_const_get(const FVizAttributeSet* set, const char* name);
FVIZ_API FVizResult fviz_attribute_set_add(FVizAttributeSet* set, const char* name, FVizDataArray* array);
FVIZ_API FVizResult fviz_attribute_set_remove(FVizAttributeSet* set, const char* name);

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_ATTRIBUTE_SET_H */
