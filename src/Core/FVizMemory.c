#include <stdint.h>

#include <FViz/Core/FVizAllocator.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>

#include <FViz/Core/FVizErrorInternal.h>

void* fviz_alloc(FVizSize size)
{
    const FVizAllocator allocator = fviz_allocator_default();
    return fviz_allocator_allocate(&allocator, size, FVIZ_DEFAULT_ALIGNMENT);
}

void* fviz_alloc_aligned(FVizSize size, FVizSize alignment)
{
    const FVizAllocator allocator = fviz_allocator_default();
    return fviz_allocator_allocate(&allocator, size, alignment);
}

void* fviz_realloc(void* memory, FVizSize new_size)
{
    const FVizAllocator allocator = fviz_allocator_default();
    return fviz_allocator_reallocate(&allocator, memory, 0u, new_size, FVIZ_DEFAULT_ALIGNMENT);
}

void* fviz_realloc_aligned(void* memory, FVizSize new_size, FVizSize alignment)
{
    const FVizAllocator allocator = fviz_allocator_default();
    return fviz_allocator_reallocate(&allocator, memory, 0u, new_size, alignment);
}

void fviz_free(void* memory)
{
    const FVizAllocator allocator = fviz_allocator_default();
    fviz_allocator_deallocate(&allocator, memory, 0u, FVIZ_DEFAULT_ALIGNMENT);
}

FVizResult fviz_size_add(FVizSize a, FVizSize b, FVizSize* out_value)
{
    if (out_value == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_value must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }

    if (a > SIZE_MAX - b)
    {
        *out_value = 0u;
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "size addition overflow");
        return FVIZ_ERROR_OVERFLOW;
    }

    *out_value = a + b;
    return FVIZ_OK;
}

FVizResult fviz_size_multiply(FVizSize a, FVizSize b, FVizSize* out_value)
{
    if (out_value == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_value must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }

    if (a != 0u && b > SIZE_MAX / a)
    {
        *out_value = 0u;
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "size multiplication overflow");
        return FVIZ_ERROR_OVERFLOW;
    }

    *out_value = a * b;
    return FVIZ_OK;
}
