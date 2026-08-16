#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizAllocator.h>
#include <FViz/Core/FVizError.h>

#include <FViz/Core/FVizCompiler.h>
#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_ALLOCATION_MAGIC UINT64_C(0x4656495A414C4C4F)

typedef struct FVizAllocationHeader
{
    uint64_t magic;
    void* raw_memory;
    FVizSize size;
    FVizSize alignment;
} FVizAllocationHeader;

static FVizBool fviz_is_power_of_two(FVizSize value)
{
    return (value != 0u && (value & (value - 1u)) == 0u) ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizSize fviz_default_alignment(void)
{
    return (FVizSize)FVIZ_INTERNAL_MAX_ALIGNMENT;
}

static FVizResult fviz_normalize_alignment(FVizSize alignment, FVizSize* out_alignment)
{
    FVizSize normalized;

    if (out_alignment == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_alignment must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }

    if (alignment != 0u && fviz_is_power_of_two(alignment) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "alignment must be zero or a power of two");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }

    normalized = alignment == 0u ? fviz_default_alignment() : alignment;

    if (normalized < sizeof(void*))
    {
        normalized = sizeof(void*);
    }

    *out_alignment = normalized;
    return FVIZ_OK;
}

static void* fviz_default_allocate(void* user_data, FVizSize size, FVizSize alignment)
{
    FVizSize normalized_alignment;
    FVizSize overhead;
    FVizSize total_size;
    uintptr_t start;
    uintptr_t aligned_address;
    void* raw_memory;
    FVizAllocationHeader* header;

    FVIZ_UNUSED(user_data);

    if (size == 0u)
    {
        return NULL;
    }

    if (fviz_normalize_alignment(alignment, &normalized_alignment) != FVIZ_OK)
    {
        return NULL;
    }

    if (normalized_alignment - 1u > SIZE_MAX - sizeof(FVizAllocationHeader))
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "allocation alignment overhead overflow");
        return NULL;
    }

    overhead = sizeof(FVizAllocationHeader) + normalized_alignment - 1u;
    if (size > SIZE_MAX - overhead)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "allocation size overflow");
        return NULL;
    }

    total_size = size + overhead;
    raw_memory = malloc(total_size);
    if (raw_memory == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_OUT_OF_MEMORY, "default allocator failed to allocate memory");
        return NULL;
    }

    start = (uintptr_t)raw_memory + sizeof(FVizAllocationHeader);
    aligned_address = (start + normalized_alignment - 1u) & ~(uintptr_t)(normalized_alignment - 1u);
    header = (FVizAllocationHeader*)(aligned_address - sizeof(FVizAllocationHeader));

    header->magic = FVIZ_ALLOCATION_MAGIC;
    header->raw_memory = raw_memory;
    header->size = size;
    header->alignment = normalized_alignment;

    return (void*)aligned_address;
}

static void fviz_default_deallocate(void* user_data, void* memory, FVizSize size, FVizSize alignment)
{
    FVizAllocationHeader* header;

    FVIZ_UNUSED(user_data);
    FVIZ_UNUSED(size);
    FVIZ_UNUSED(alignment);

    if (memory == NULL)
    {
        return;
    }

    header = (FVizAllocationHeader*)((uintptr_t)memory - sizeof(FVizAllocationHeader));
    if (header->magic != FVIZ_ALLOCATION_MAGIC)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "memory was not allocated by the FEAViz default allocator");
        return;
    }

    header->magic = 0u;
    free(header->raw_memory);
}

static void* fviz_default_reallocate(void* user_data, void* memory, FVizSize old_size, FVizSize new_size,
                                     FVizSize alignment)
{
    FVizAllocationHeader* old_header;
    FVizSize copy_size;
    void* new_memory;

    FVIZ_UNUSED(user_data);
    FVIZ_UNUSED(old_size);

    if (memory == NULL)
    {
        return fviz_default_allocate(NULL, new_size, alignment);
    }

    if (new_size == 0u)
    {
        fviz_default_deallocate(NULL, memory, 0u, alignment);
        return NULL;
    }

    old_header = (FVizAllocationHeader*)((uintptr_t)memory - sizeof(FVizAllocationHeader));
    if (old_header->magic != FVIZ_ALLOCATION_MAGIC)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "memory was not allocated by the FEAViz default allocator");
        return NULL;
    }

    if (alignment == 0u)
    {
        alignment = old_header->alignment;
    }

    new_memory = fviz_default_allocate(NULL, new_size, alignment);
    if (new_memory == NULL)
    {
        return NULL;
    }

    copy_size = old_header->size < new_size ? old_header->size : new_size;
    (void)memcpy(new_memory, memory, copy_size);
    fviz_default_deallocate(NULL, memory, old_header->size, old_header->alignment);
    return new_memory;
}

FVizAllocator fviz_allocator_default(void)
{
    FVizAllocator allocator;
    allocator.allocate = fviz_default_allocate;
    allocator.reallocate = fviz_default_reallocate;
    allocator.deallocate = fviz_default_deallocate;
    allocator.user_data = NULL;
    return allocator;
}

FVizBool fviz_allocator_is_valid(const FVizAllocator* allocator)
{
    if (allocator == NULL)
    {
        return FVIZ_FALSE;
    }

    return (allocator->allocate != NULL && allocator->reallocate != NULL && allocator->deallocate != NULL) ? FVIZ_TRUE
                                                                                                           : FVIZ_FALSE;
}

void* fviz_allocator_allocate(const FVizAllocator* allocator, FVizSize size, FVizSize alignment)
{
    if (fviz_allocator_is_valid(allocator) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "allocator is NULL or incomplete");
        return NULL;
    }

    {
        void* memory = allocator->allocate(allocator->user_data, size, alignment);
        if (memory == NULL && size != 0u && allocator->allocate != fviz_default_allocate)
        {
            fviz_internal_set_error(FVIZ_ERROR_OUT_OF_MEMORY, "custom allocator failed to allocate memory");
        }
        return memory;
    }
}

void* fviz_allocator_reallocate(const FVizAllocator* allocator, void* memory, FVizSize old_size, FVizSize new_size,
                                FVizSize alignment)
{
    if (fviz_allocator_is_valid(allocator) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "allocator is NULL or incomplete");
        return NULL;
    }

    {
        void* reallocated = allocator->reallocate(allocator->user_data, memory, old_size, new_size, alignment);
        if (reallocated == NULL && new_size != 0u && allocator->reallocate != fviz_default_reallocate)
        {
            fviz_internal_set_error(FVIZ_ERROR_OUT_OF_MEMORY, "custom allocator failed to reallocate memory");
        }
        return reallocated;
    }
}

void fviz_allocator_deallocate(const FVizAllocator* allocator, void* memory, FVizSize size, FVizSize alignment)
{
    if (memory == NULL)
    {
        return;
    }

    if (fviz_allocator_is_valid(allocator) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "allocator is NULL or incomplete");
        return;
    }

    allocator->deallocate(allocator->user_data, memory, size, alignment);
}
