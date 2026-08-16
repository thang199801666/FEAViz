#ifndef FVIZ_CORE_ALLOCATOR_H
#define FVIZ_CORE_ALLOCATOR_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef void* (*FVizAllocateFn)(void* user_data, FVizSize size, FVizSize alignment);
typedef void* (*FVizReallocateFn)(void* user_data, void* memory, FVizSize old_size, FVizSize new_size,
                                  FVizSize alignment);
typedef void (*FVizDeallocateFn)(void* user_data, void* memory, FVizSize size, FVizSize alignment);

typedef struct FVizAllocator
{
    FVizAllocateFn allocate;
    FVizReallocateFn reallocate;
    FVizDeallocateFn deallocate;
    void* user_data;
} FVizAllocator;

FVIZ_CORE_API FVizAllocator fviz_allocator_default(void);
FVIZ_CORE_API FVizBool fviz_allocator_is_valid(const FVizAllocator* allocator);
FVIZ_CORE_API void* fviz_allocator_allocate(const FVizAllocator* allocator, FVizSize size, FVizSize alignment);
FVIZ_CORE_API void* fviz_allocator_reallocate(const FVizAllocator* allocator, void* memory, FVizSize old_size,
                                         FVizSize new_size, FVizSize alignment);
FVIZ_CORE_API void fviz_allocator_deallocate(const FVizAllocator* allocator, void* memory, FVizSize size,
                                        FVizSize alignment);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_ALLOCATOR_H */
