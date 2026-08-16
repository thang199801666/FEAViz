#ifndef FVIZ_INTERNAL_CORE_OBJECT_PRIVATE_H
#define FVIZ_INTERNAL_CORE_OBJECT_PRIVATE_H

#include <FViz/Core/FVizAllocator.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizTypes.h>

#include <FViz/Core/FVizAtomic.h>

typedef void (*FVizObjectDestroyFn)(FVizObject* object);
typedef FVizMTime (*FVizObjectMTimeFn)(const FVizObject* object);

typedef struct FVizObserverList FVizObserverList;

typedef struct FVizObjectClass
{
    FVizTypeId type_id;
    const char* type_name;
    const struct FVizObjectClass* parent;
    FVizObjectDestroyFn destroy;
    FVizObjectMTimeFn get_mtime;
} FVizObjectClass;

struct FVizObject
{
    uint32_t magic;
    FVizAtomicU32 ref_count;
    FVizAtomicU64 mtime;
    const FVizObjectClass* object_class;
    FVizAllocator allocator;
    FVizSize allocation_size;
    FVizObserverList* observer_list;
};

extern const FVizObjectClass g_fviz_object_class;
const FVizObjectClass* fviz_internal_object_base_class(void);

FVIZ_API FVizObject* fviz_internal_object_allocate(
    FVizSize object_size,
    const FVizObjectClass* object_class,
    const FVizAllocator* allocator);
FVIZ_API FVizMTime fviz_internal_object_local_mtime(const FVizObject* object);

#endif /* FVIZ_INTERNAL_CORE_OBJECT_PRIVATE_H */
