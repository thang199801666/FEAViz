#include <stdint.h>
#include <stdio.h>

#include <FViz/FViz.h>

#define SLOT_COUNT 512u
#define ROUND_COUNT 64u

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
    void* slots[SLOT_COUNT] = { 0 };
    FVizSize round;
    FVizSize i;

    for (round = 0u; round < ROUND_COUNT; ++round)
    {
        for (i = 0u; i < SLOT_COUNT; ++i)
        {
            const FVizSize size = 1u + ((i * 37u + round * 13u) % 4096u);
            const FVizSize alignment = (FVizSize)1u << (3u + (unsigned int)(i % 5u));
            unsigned char* memory = (unsigned char*)fviz_alloc_aligned(size, alignment);

            if (!require_true(memory != NULL, "stress allocation failed")) return 1;
            if (!require_true(((uintptr_t)memory % alignment) == 0u, "stress allocation alignment mismatch")) return 1;

            memory[0] = (unsigned char)(i & 0xffu);
            if (size > 1u)
            {
                memory[size - 1u] = (unsigned char)(round & 0xffu);
            }
            slots[i] = memory;
        }

        for (i = 0u; i < SLOT_COUNT; ++i)
        {
            const FVizSize new_size = 4097u + ((i * 17u + round) % 1024u);
            const FVizSize alignment = (FVizSize)1u << (3u + (unsigned int)(i % 5u));
            unsigned char* memory = (unsigned char*)fviz_realloc_aligned(slots[i], new_size, alignment);

            if (!require_true(memory != NULL, "stress reallocation failed")) return 1;
            if (!require_true(memory[0] == (unsigned char)(i & 0xffu), "stress reallocation lost data")) return 1;
            slots[i] = memory;
        }

        for (i = 0u; i < SLOT_COUNT; ++i)
        {
            fviz_free(slots[i]);
            slots[i] = NULL;
        }
    }

    return 0;
}
