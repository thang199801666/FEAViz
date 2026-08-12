#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int test_bit_array_basic(void)
{
    FVizBitArray* bits = NULL;
    FVizSize i;
    CHECK(fviz_bit_array_create(100u, &bits) == FVIZ_OK);
    CHECK(bits != NULL);
    CHECK(fviz_bit_array_count(bits) == 100u);
    for (i = 0u; i < 100u; ++i)
    {
        CHECK(fviz_bit_array_test(bits, i) == FVIZ_FALSE);
    }
    for (i = 0u; i < 100u; i += 2u)
    {
        CHECK(fviz_bit_array_set(bits, i, FVIZ_TRUE) == FVIZ_OK);
    }
    for (i = 0u; i < 100u; ++i)
    {
        CHECK(fviz_bit_array_test(bits, i) == ((i % 2u) == 0u ? FVIZ_TRUE : FVIZ_FALSE));
    }
    CHECK(fviz_bit_array_pop_count(bits) == 50u);
    CHECK(fviz_bit_array_set(bits, 100u, FVIZ_TRUE) == FVIZ_ERROR_INVALID_ARGUMENT);
    CHECK(fviz_bit_array_test(bits, 100u) == FVIZ_FALSE);
    fviz_bit_array_clear(bits);
    CHECK(fviz_bit_array_pop_count(bits) == 0u);
    fviz_bit_array_set_all(bits, FVIZ_TRUE);
    CHECK(fviz_bit_array_pop_count(bits) == 100u);
    fviz_bit_array_set_all(bits, FVIZ_FALSE);
    CHECK(fviz_bit_array_pop_count(bits) == 0u);
    fviz_release(bits);
    return 0;
}

static int test_bit_array_resize(void)
{
    FVizBitArray* bits = NULL;
    FVizSize i;
    CHECK(fviz_bit_array_create(64u, &bits) == FVIZ_OK);
    for (i = 0u; i < 64u; ++i)
    {
        CHECK(fviz_bit_array_set(bits, i, FVIZ_TRUE) == FVIZ_OK);
    }
    CHECK(fviz_bit_array_resize(bits, 130u) == FVIZ_OK);
    CHECK(fviz_bit_array_count(bits) == 130u);
    for (i = 64u; i < 130u; ++i)
    {
        CHECK(fviz_bit_array_test(bits, i) == FVIZ_FALSE);
    }
    CHECK(fviz_bit_array_pop_count(bits) == 64u);
    for (i = 64u; i < 130u; ++i)
    {
        CHECK(fviz_bit_array_set(bits, i, FVIZ_TRUE) == FVIZ_OK);
    }
    CHECK(fviz_bit_array_pop_count(bits) == 130u);
    CHECK(fviz_bit_array_resize(bits, 100u) == FVIZ_OK);
    CHECK(fviz_bit_array_count(bits) == 100u);
    CHECK(fviz_bit_array_pop_count(bits) == 100u);
    fviz_release(bits);
    return 0;
}

static int test_hash_map_basic(void)
{
    FVizHashMap* map = NULL;
    void* value = NULL;
    FVizSize cursor = 0u;
    FVizSize entries = 0u;
    FVizId key;
    CHECK(fviz_hash_map_create(&map) == FVIZ_OK);
    CHECK(map != NULL);
    CHECK(fviz_hash_map_count(map) == 0u);
    CHECK(fviz_hash_map_set(map, 42u, (void*)0x1000) == FVIZ_OK);
    CHECK(fviz_hash_map_set(map, 7u, (void*)0x2000) == FVIZ_OK);
    CHECK(fviz_hash_map_count(map) == 2u);
    CHECK(fviz_hash_map_get(map, 42u, &value) == FVIZ_TRUE);
    CHECK(value == (void*)0x1000);
    CHECK(fviz_hash_map_get(map, 7u, &value) == FVIZ_TRUE);
    CHECK(value == (void*)0x2000);
    CHECK(fviz_hash_map_get(map, 99u, &value) == FVIZ_FALSE);
    CHECK(fviz_hash_map_contains(map, 42u) == FVIZ_TRUE);
    CHECK(fviz_hash_map_set(map, 42u, (void*)0x3000) == FVIZ_OK);
    CHECK(fviz_hash_map_count(map) == 2u);
    CHECK(fviz_hash_map_get(map, 42u, &value) == FVIZ_TRUE);
    CHECK(value == (void*)0x3000);
    CHECK(fviz_hash_map_erase(map, 7u) == FVIZ_TRUE);
    CHECK(fviz_hash_map_count(map) == 1u);
    CHECK(fviz_hash_map_contains(map, 7u) == FVIZ_FALSE);
    CHECK(fviz_hash_map_contains(map, 42u) == FVIZ_TRUE);
    CHECK(fviz_hash_map_erase(map, 7u) == FVIZ_FALSE);
    while (fviz_hash_map_iterate(map, &cursor, &key, &value) != FVIZ_FALSE)
    {
        ++entries;
    }
    CHECK(entries == 1u);
    fviz_hash_map_clear(map);
    CHECK(fviz_hash_map_count(map) == 0u);
    fviz_release(map);
    return 0;
}

static int test_hash_map_grow(void)
{
    FVizHashMap* map = NULL;
    void* value = NULL;
    FVizId i;
    CHECK(fviz_hash_map_create(&map) == FVIZ_OK);
    for (i = 0; i < 1000; ++i)
    {
        CHECK(fviz_hash_map_set(map, i * 3u + 1u, (void*)(i + 1u)) == FVIZ_OK);
    }
    CHECK(fviz_hash_map_count(map) == 1000u);
    for (i = 0; i < 1000; ++i)
    {
        CHECK(fviz_hash_map_get(map, i * 3u + 1u, &value) == FVIZ_TRUE);
        CHECK(value == (void*)(i + 1u));
    }
    for (i = 0; i < 1000; i += 2u)
    {
        CHECK(fviz_hash_map_erase(map, i * 3u + 1u) == FVIZ_TRUE);
    }
    CHECK(fviz_hash_map_count(map) == 500u);
    for (i = 1; i < 1000; i += 2u)
    {
        CHECK(fviz_hash_map_contains(map, i * 3u + 1u) == FVIZ_TRUE);
    }
    fviz_release(map);
    return 0;
}

int main(void)
{
    CHECK(test_bit_array_basic() == 0);
    CHECK(test_bit_array_resize() == 0);
    CHECK(test_hash_map_basic() == 0);
    CHECK(test_hash_map_grow() == 0);
    return 0;
}
