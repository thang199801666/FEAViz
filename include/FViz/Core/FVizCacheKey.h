#ifndef FVIZ_CORE_CACHE_KEY_H
#define FVIZ_CORE_CACHE_KEY_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef uint64_t FVizCacheKey;

FVIZ_API FVizCacheKey fviz_cache_key_initialize(void);
FVIZ_API FVizCacheKey fviz_cache_key_append_bytes(FVizCacheKey key, const void* data, FVizSize size);
FVIZ_API FVizCacheKey fviz_cache_key_append_u64(FVizCacheKey key, uint64_t value);
FVIZ_API FVizCacheKey fviz_cache_key_append_string(FVizCacheKey key, const char* value);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_CACHE_KEY_H */
