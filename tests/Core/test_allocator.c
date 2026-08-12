#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

static int require_true(int condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "FAILED: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    static const FVizSize alignments[] = { 8u, 16u, 32u, 64u, 128u };
    FVizSize i;

    for (i = 0u; i < FVIZ_ARRAY_COUNT(alignments); ++i)
    {
        const FVizSize alignment = alignments[i];
        unsigned char* memory = (unsigned char*)fviz_alloc_aligned(257u, alignment);
        FVizSize j;

        if (!require_true(memory != NULL, "aligned allocation returned NULL")) return 1;
        if (!require_true(((uintptr_t)memory % alignment) == 0u, "allocation is not correctly aligned")) return 1;

        for (j = 0u; j < 257u; ++j)
        {
            memory[j] = (unsigned char)(j & 0xffu);
        }

        memory = (unsigned char*)fviz_realloc_aligned(memory, 1025u, alignment);
        if (!require_true(memory != NULL, "aligned reallocation returned NULL")) return 1;
        if (!require_true(((uintptr_t)memory % alignment) == 0u, "reallocation lost alignment")) return 1;

        for (j = 0u; j < 257u; ++j)
        {
            if (!require_true(memory[j] == (unsigned char)(j & 0xffu), "reallocation did not preserve data")) return 1;
        }

        fviz_free(memory);
    }

    fviz_clear_last_error();
    if (!require_true(fviz_alloc_aligned(64u, 3u) == NULL, "non-power-of-two alignment should fail")) return 1;
    if (!require_true(fviz_last_error_code() == FVIZ_ERROR_INVALID_ARGUMENT, "invalid alignment error code mismatch")) return 1;

    {
        FVizSize value = 0u;
        if (!require_true(fviz_size_add(100u, 23u, &value) == FVIZ_OK && value == 123u, "checked size addition failed")) return 1;
        if (!require_true(fviz_size_multiply(12u, 11u, &value) == FVIZ_OK && value == 132u, "checked size multiplication failed")) return 1;
        if (!require_true(fviz_size_add(SIZE_MAX, 1u, &value) == FVIZ_ERROR_OVERFLOW, "size addition overflow was not detected")) return 1;
        if (!require_true(fviz_size_multiply(SIZE_MAX, 2u, &value) == FVIZ_ERROR_OVERFLOW, "size multiplication overflow was not detected")) return 1;
    }

    return 0;
}
