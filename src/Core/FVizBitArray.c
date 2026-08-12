#include <string.h>

#include <FViz/Core/FVizBitArray.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>

#include <FViz/Core/FVizBitArrayPrivate.h>
#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_BITS_PER_WORD 64u
#define FVIZ_BIT_ARRAY_INITIAL_WORDS 4u

static void fviz_bit_array_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_bit_array_class = {
    FVIZ_TYPE_BIT_ARRAY,
    "FVizBitArray",
    &g_fviz_object_class,
    fviz_bit_array_destroy,
    NULL
};

static void fviz_bit_array_destroy(FVizObject* object)
{
    FVizBitArray* bit_array = (FVizBitArray*)object;
    FVizSize bytes = 0u;
    (void)fviz_size_multiply(bit_array->capacity_words, sizeof(uint64_t), &bytes);
    fviz_allocator_deallocate(&bit_array->base.allocator, bit_array->words, bytes, 0u);
    bit_array->words = NULL;
    bit_array->bit_count = 0u;
    bit_array->capacity_words = 0u;
}

static FVizSize fviz_bit_array_word_count(FVizSize bit_count)
{
    return (bit_count + FVIZ_BITS_PER_WORD - 1u) / FVIZ_BITS_PER_WORD;
}

static FVizResult fviz_bit_array_reserve_words(FVizBitArray* bit_array, FVizSize word_capacity)
{
    FVizSize old_bytes;
    FVizSize new_bytes;
    void* memory;
    if (word_capacity <= bit_array->capacity_words)
    {
        return FVIZ_OK;
    }
    if (fviz_size_multiply(bit_array->capacity_words, sizeof(uint64_t), &old_bytes) != FVIZ_OK ||
        fviz_size_multiply(word_capacity, sizeof(uint64_t), &new_bytes) != FVIZ_OK)
    {
        return FVIZ_ERROR_OVERFLOW;
    }
    memory = fviz_allocator_reallocate(
        &bit_array->base.allocator, bit_array->words, old_bytes, new_bytes, 0u);
    if (memory == NULL)
    {
        return fviz_last_error_code();
    }
    bit_array->words = (uint64_t*)memory;
    bit_array->capacity_words = word_capacity;
    return FVIZ_OK;
}

FVizResult fviz_bit_array_create(FVizSize bit_count, FVizBitArray** out_bit_array)
{
    FVizBitArray* bit_array;
    FVizSize words;
    FVizSize capacity;
    FVizResult result;
    if (out_bit_array == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_bit_array must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_bit_array = NULL;
    bit_array = (FVizBitArray*)fviz_internal_object_allocate(sizeof(FVizBitArray), &g_fviz_bit_array_class, NULL);
    if (bit_array == NULL)
    {
        return fviz_last_error_code();
    }
    words = fviz_bit_array_word_count(bit_count);
    capacity = words < FVIZ_BIT_ARRAY_INITIAL_WORDS ? FVIZ_BIT_ARRAY_INITIAL_WORDS : words;
    result = fviz_bit_array_reserve_words(bit_array, capacity);
    if (result != FVIZ_OK)
    {
        fviz_release(bit_array);
        return result;
    }
    (void)memset(bit_array->words, 0, capacity * sizeof(uint64_t));
    bit_array->bit_count = bit_count;
    *out_bit_array = bit_array;
    return FVIZ_OK;
}

FVizSize fviz_bit_array_count(const FVizBitArray* bit_array)
{
    return bit_array != NULL ? bit_array->bit_count : 0u;
}

static void fviz_bit_array_set_bit(FVizBitArray* bit_array, FVizSize index, FVizBool value)
{
    const FVizSize word = index / FVIZ_BITS_PER_WORD;
    const uint64_t mask = UINT64_C(1) << (index % FVIZ_BITS_PER_WORD);
    if (value != FVIZ_FALSE)
    {
        bit_array->words[word] |= mask;
    }
    else
    {
        bit_array->words[word] &= ~mask;
    }
}

FVizResult fviz_bit_array_set(FVizBitArray* bit_array, FVizSize index, FVizBool value)
{
    if (bit_array == NULL || index >= bit_array->bit_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "bit index out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_bit_array_set_bit(bit_array, index, value);
    fviz_object_modified((FVizObject*)bit_array);
    return FVIZ_OK;
}

FVizBool fviz_bit_array_test(const FVizBitArray* bit_array, FVizSize index)
{
    FVizSize word;
    uint64_t mask;
    if (bit_array == NULL || index >= bit_array->bit_count)
    {
        return FVIZ_FALSE;
    }
    word = index / FVIZ_BITS_PER_WORD;
    mask = UINT64_C(1) << (index % FVIZ_BITS_PER_WORD);
    return (bit_array->words[word] & mask) != 0u ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizResult fviz_bit_array_resize(FVizBitArray* bit_array, FVizSize bit_count)
{
    FVizSize old_word_count;
    FVizSize new_word_count;
    FVizSize old_bit_count;
    FVizResult result;
    if (bit_array == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "bit_array must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    old_bit_count = bit_array->bit_count;
    old_word_count = fviz_bit_array_word_count(old_bit_count);
    new_word_count = fviz_bit_array_word_count(bit_count);
    result = fviz_bit_array_reserve_words(bit_array, new_word_count);
    if (result != FVIZ_OK)
    {
        return result;
    }
    if (new_word_count > old_word_count)
    {
        (void)memset(bit_array->words + old_word_count, 0, (new_word_count - old_word_count) * sizeof(uint64_t));
    }
    bit_array->bit_count = bit_count;
    if (bit_count > 0u)
    {
        const FVizSize last_word_bits = bit_count % FVIZ_BITS_PER_WORD;
        if (last_word_bits != 0u)
        {
            bit_array->words[new_word_count - 1u] &= (UINT64_C(1) << last_word_bits) - UINT64_C(1);
        }
    }
    if (old_bit_count != bit_count) fviz_object_modified((FVizObject*)bit_array);
    return FVIZ_OK;
}

void fviz_bit_array_clear(FVizBitArray* bit_array)
{
    if (bit_array == NULL) return;
    if (bit_array->capacity_words > 0u)
    {
        (void)memset(bit_array->words, 0, bit_array->capacity_words * sizeof(uint64_t));
        fviz_object_modified((FVizObject*)bit_array);
    }
}

void fviz_bit_array_set_all(FVizBitArray* bit_array, FVizBool value)
{
    FVizSize word_count;
    if (bit_array == NULL || bit_array->bit_count == 0u) return;
    word_count = fviz_bit_array_word_count(bit_array->bit_count);
    if (value != FVIZ_FALSE)
    {
        (void)memset(bit_array->words, 0xFF, word_count * sizeof(uint64_t));
        if (bit_array->bit_count % FVIZ_BITS_PER_WORD != 0u)
        {
            const FVizSize last_word_bits = bit_array->bit_count % FVIZ_BITS_PER_WORD;
            bit_array->words[word_count - 1u] &= (UINT64_C(1) << last_word_bits) - UINT64_C(1);
        }
    }
    else
    {
        (void)memset(bit_array->words, 0, word_count * sizeof(uint64_t));
    }
    fviz_object_modified((FVizObject*)bit_array);
}

static FVizSize fviz_bit_array_word_pop_count(uint64_t word)
{
#if defined(_MSC_VER)
    return (FVizSize)__popcnt64(word);
#else
    return (FVizSize)__builtin_popcountll(word);
#endif
}

FVizSize fviz_bit_array_pop_count(const FVizBitArray* bit_array)
{
    FVizSize word_count;
    FVizSize total = 0u;
    FVizSize i;
    if (bit_array == NULL) return 0u;
    word_count = fviz_bit_array_word_count(bit_array->bit_count);
    for (i = 0u; i < word_count; ++i)
    {
        total += fviz_bit_array_word_pop_count(bit_array->words[i]);
    }
    return total;
}
