#ifndef FVIZ_CORE_HASH_MAP_H
#define FVIZ_CORE_HASH_MAP_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizHashMap FVizHashMap;
#define FVIZ_TYPE_HASH_MAP UINT64_C(0x71E3B9A24D6C05F7)

FVIZ_CORE_API FVizResult fviz_hash_map_create(FVizHashMap** out_map);
FVIZ_CORE_API FVizResult fviz_hash_map_create_reserve(FVizSize initial_capacity, FVizHashMap** out_map);
FVIZ_CORE_API FVizSize fviz_hash_map_count(const FVizHashMap* map);
FVIZ_CORE_API FVizSize fviz_hash_map_capacity(const FVizHashMap* map);
FVIZ_CORE_API FVizResult fviz_hash_map_set(FVizHashMap* map, FVizId key, void* value);
FVIZ_CORE_API FVizBool fviz_hash_map_get(const FVizHashMap* map, FVizId key, void** out_value);
FVIZ_CORE_API FVizBool fviz_hash_map_contains(const FVizHashMap* map, FVizId key);
FVIZ_CORE_API FVizBool fviz_hash_map_erase(FVizHashMap* map, FVizId key);
FVIZ_CORE_API void fviz_hash_map_clear(FVizHashMap* map);
FVIZ_CORE_API FVizBool fviz_hash_map_iterate(FVizHashMap* map, FVizSize* cursor, FVizId* out_key, void** out_value);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_HASH_MAP_H */
