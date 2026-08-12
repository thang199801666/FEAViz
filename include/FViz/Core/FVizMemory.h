#ifndef FVIZ_CORE_MEMORY_H
#define FVIZ_CORE_MEMORY_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

#define FVIZ_DEFAULT_ALIGNMENT ((FVizSize)0)

FVIZ_API void* fviz_alloc(FVizSize size);
FVIZ_API void* fviz_alloc_aligned(FVizSize size, FVizSize alignment);
FVIZ_API void* fviz_realloc(void* memory, FVizSize new_size);
FVIZ_API void* fviz_realloc_aligned(void* memory, FVizSize new_size, FVizSize alignment);
FVIZ_API void fviz_free(void* memory);

FVIZ_API FVizResult fviz_size_add(FVizSize a, FVizSize b, FVizSize* out_value);
FVIZ_API FVizResult fviz_size_multiply(FVizSize a, FVizSize b, FVizSize* out_value);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_MEMORY_H */
