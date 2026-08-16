#include <string.h>

#include <FViz/Core/FVizCacheKey.h>

FVizCacheKey fviz_cache_key_initialize(void)
{
    return UINT64_C(14695981039346656037);
}

FVizCacheKey fviz_cache_key_append_bytes(
    FVizCacheKey key, const void* data, FVizSize size)
{
    const unsigned char* bytes = (const unsigned char*)data;
    FVizSize index;
    if (data == NULL && size != 0u) return 0u;
    if (key == 0u) key = fviz_cache_key_initialize();
    for (index = 0u; index < size; ++index)
    {
        key ^= (uint64_t)bytes[index];
        key *= UINT64_C(1099511628211);
    }
    return key != 0u ? key : UINT64_C(1);
}

FVizCacheKey fviz_cache_key_append_u64(FVizCacheKey key, uint64_t value)
{
    unsigned char bytes[8];
    uint32_t index;
    for (index = 0u; index < 8u; ++index)
        bytes[index] = (unsigned char)((value >> (index * 8u)) & UINT64_C(0xff));
    return fviz_cache_key_append_bytes(key, bytes, 8u);
}

FVizCacheKey fviz_cache_key_append_string(FVizCacheKey key, const char* value)
{
    if (value == NULL) return fviz_cache_key_append_u64(key, 0u);
    key = fviz_cache_key_append_bytes(key, value, (FVizSize)strlen(value));
    return fviz_cache_key_append_u64(key, UINT64_C(0xff));
}
