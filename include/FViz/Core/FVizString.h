#ifndef FVIZ_CORE_STRING_H
#define FVIZ_CORE_STRING_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizString FVizString;
#define FVIZ_TYPE_STRING UINT64_C(0x7731F0E85F7AE8D0)

FVIZ_API FVizResult fviz_string_create(FVizString** out_string);
FVIZ_API FVizResult fviz_string_create_from(const char* text, FVizString** out_string);
FVIZ_API const char* fviz_string_c_str(const FVizString* string);
FVIZ_API FVizSize fviz_string_length(const FVizString* string);
FVIZ_API FVizResult fviz_string_set(FVizString* string, const char* text);
FVIZ_API FVizResult fviz_string_append(FVizString* string, const char* text);
FVIZ_API void fviz_string_clear(FVizString* string);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_STRING_H */
