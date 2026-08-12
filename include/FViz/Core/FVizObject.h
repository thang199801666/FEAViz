#ifndef FVIZ_CORE_OBJECT_H
#define FVIZ_CORE_OBJECT_H

#include <stdint.h>

#include <FViz/Core/FVizAllocator.h>
#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef uint64_t FVizTypeId;
typedef struct FVizObject FVizObject;

#define FVIZ_TYPE_OBJECT UINT64_C(0x87547DE558BA7349)

FVIZ_API FVizTypeId fviz_type_id_from_name(const char* type_name);

FVIZ_API FVizResult fviz_object_create(FVizObject** out_object);
FVIZ_API FVizResult fviz_object_create_with_allocator(
    const FVizAllocator* allocator,
    FVizObject** out_object);

FVIZ_API void* fviz_retain(void* object);
FVIZ_API void fviz_release(void* object);

FVIZ_API FVizTypeId fviz_object_type_id(const FVizObject* object);
FVIZ_API const char* fviz_object_type_name(const FVizObject* object);
FVIZ_API FVizBool fviz_object_is_type(const FVizObject* object, FVizTypeId type_id);
FVIZ_API uint32_t fviz_object_ref_count(const FVizObject* object);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_OBJECT_H */
