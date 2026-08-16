#ifndef FVIZ_CORE_BUFFER_H
#define FVIZ_CORE_BUFFER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizBuffer FVizBuffer;
typedef void (*FVizBufferReleaseFn)(void* data, FVizSize size, void* user_data);

#define FVIZ_TYPE_BUFFER UINT64_C(0x5BC0B26C9EB4546B)

FVIZ_API FVizResult fviz_buffer_create(FVizSize size, FVizBuffer** out_buffer);
FVIZ_API FVizResult fviz_buffer_create_copy(const void* data, FVizSize size, FVizBuffer** out_buffer);
FVIZ_API FVizResult fviz_buffer_wrap(void* data, FVizSize size, FVizBufferReleaseFn release_fn, void* user_data,
                                     FVizBuffer** out_buffer);
FVIZ_API void* fviz_buffer_data(FVizBuffer* buffer);
FVIZ_API const void* fviz_buffer_const_data(const FVizBuffer* buffer);
FVIZ_API FVizSize fviz_buffer_size(const FVizBuffer* buffer);
FVIZ_API FVizBool fviz_buffer_is_external(const FVizBuffer* buffer);
FVIZ_API FVizResult fviz_buffer_resize(FVizBuffer* buffer, FVizSize new_size);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_BUFFER_H */
