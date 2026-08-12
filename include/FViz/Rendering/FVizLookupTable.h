#ifndef FVIZ_RENDERING_LOOKUP_TABLE_H
#define FVIZ_RENDERING_LOOKUP_TABLE_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizLookupTable FVizLookupTable;
#define FVIZ_TYPE_LOOKUP_TABLE UINT64_C(0x4B9E27C1A3F80D64)

FVIZ_API FVizResult fviz_lookup_table_create(FVizSize table_size, FVizLookupTable** out_table);
FVIZ_API FVizSize fviz_lookup_table_size(const FVizLookupTable* table);
FVIZ_API void fviz_lookup_table_set_range(FVizLookupTable* table, float minimum, float maximum);
FVIZ_API void fviz_lookup_table_get_range(const FVizLookupTable* table, float* minimum, float* maximum);
FVIZ_API FVizResult fviz_lookup_table_set_color(FVizLookupTable* table, FVizSize index, float red, float green, float blue);
FVIZ_API void fviz_lookup_table_get_color(const FVizLookupTable* table, FVizSize index, float* red, float* green, float* blue);
FVIZ_API void fviz_lookup_table_build(FVizLookupTable* table);
FVIZ_API void fviz_lookup_table_map_scalar(const FVizLookupTable* table, float value, float* red, float* green, float* blue);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_LOOKUP_TABLE_H */
