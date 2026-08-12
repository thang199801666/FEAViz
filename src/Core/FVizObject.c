#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizObject.h>

#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizCompiler.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

#define FVIZ_OBJECT_MAGIC UINT32_C(0x46564F42)

static void fviz_base_object_destroy(FVizObject* object);
static FVizAtomicU64 g_fviz_next_mtime = {0};

const FVizObjectClass g_fviz_object_class = {
    FVIZ_TYPE_OBJECT,
    "FVizObject",
    NULL,
    fviz_base_object_destroy,
    NULL
};

static FVizMTime fviz_object_next_mtime(void)
{
    return fviz_atomic_u64_fetch_add(&g_fviz_next_mtime, 1u) + 1u;
}

static FVizBool fviz_object_is_valid(const FVizObject* object)
{
    return (object != NULL && object->magic == FVIZ_OBJECT_MAGIC && object->object_class != NULL)
        ? FVIZ_TRUE
        : FVIZ_FALSE;
}

static void fviz_base_object_destroy(FVizObject* object)
{
    FVIZ_UNUSED(object);
}

const FVizObjectClass* fviz_internal_object_base_class(void)
{
    return &g_fviz_object_class;
}

FVizObject* fviz_internal_object_allocate(
    FVizSize object_size,
    const FVizObjectClass* object_class,
    const FVizAllocator* allocator)
{
    FVizAllocator selected_allocator;
    FVizObject* object;

    if (object_class == NULL || object_class->type_id == 0u || object_class->type_name == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object class is invalid");
        return NULL;
    }

    if (object_size < sizeof(FVizObject))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object allocation size is smaller than FVizObject");
        return NULL;
    }

    if (allocator == NULL)
    {
        selected_allocator = fviz_allocator_default();
    }
    else
    {
        if (fviz_allocator_is_valid(allocator) == FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object allocator is invalid");
            return NULL;
        }
        selected_allocator = *allocator;
    }

    object = (FVizObject*)fviz_allocator_allocate(
        &selected_allocator,
        object_size,
        (FVizSize)FVIZ_INTERNAL_MAX_ALIGNMENT);
    if (object == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_OUT_OF_MEMORY, "failed to allocate FEAViz object");
        return NULL;
    }

    (void)memset(object, 0, object_size);
    object->magic = FVIZ_OBJECT_MAGIC;
    object->ref_count.value = 1;
    object->mtime.value = (int64_t)fviz_object_next_mtime();
    object->object_class = object_class;
    object->allocator = selected_allocator;
    object->allocation_size = object_size;
    return object;
}


FVizTypeId fviz_type_id_from_name(const char* type_name)
{
    const unsigned char* current;
    uint64_t hash = UINT64_C(14695981039346656037);

    if (type_name == NULL || type_name[0] == '\0')
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "type_name must not be NULL or empty");
        return 0u;
    }

    current = (const unsigned char*)type_name;
    while (*current != 0u)
    {
        hash ^= (uint64_t)*current;
        hash *= UINT64_C(1099511628211);
        ++current;
    }

    if (hash == 0u)
    {
        hash = UINT64_C(1);
    }
    return hash;
}

FVizResult fviz_object_create(FVizObject** out_object)
{
    return fviz_object_create_with_allocator(NULL, out_object);
}

FVizResult fviz_object_create_with_allocator(
    const FVizAllocator* allocator,
    FVizObject** out_object)
{
    FVizObject* object;

    if (out_object == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_object must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }

    *out_object = NULL;
    object = fviz_internal_object_allocate(sizeof(FVizObject), &g_fviz_object_class, allocator);
    if (object == NULL)
    {
        return fviz_last_error_code();
    }

    *out_object = object;
    return FVIZ_OK;
}

void* fviz_retain(void* object_pointer)
{
    FVizObject* object = (FVizObject*)object_pointer;
    uint32_t expected;

    if (object == NULL)
    {
        return NULL;
    }

    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        return NULL;
    }

    expected = fviz_atomic_u32_load(&object->ref_count);
    for (;;)
    {
        if (expected == 0u)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cannot retain an object whose reference count is zero");
            return NULL;
        }

        if (expected == UINT32_MAX)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "object reference count overflow");
            return NULL;
        }

        if (fviz_atomic_u32_compare_exchange(&object->ref_count, &expected, expected + 1u))
        {
            return object_pointer;
        }
    }
}

void fviz_release(void* object_pointer)
{
    FVizObject* object = (FVizObject*)object_pointer;
    uint32_t previous;
    FVizAllocator allocator;
    FVizSize allocation_size;
    const FVizObjectClass* object_class;

    if (object == NULL)
    {
        return;
    }

    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        return;
    }

    previous = fviz_atomic_u32_fetch_sub(&object->ref_count, 1u);
    if (previous == 0u)
    {
        (void)fviz_atomic_u32_fetch_add(&object->ref_count, 1u);
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "object reference count underflow");
        return;
    }

    if (previous != 1u)
    {
        return;
    }

    allocator = object->allocator;
    allocation_size = object->allocation_size;
    object_class = object->object_class;

    if (object_class->destroy != NULL)
    {
        object_class->destroy(object);
    }

    object->magic = 0u;
    object->object_class = NULL;
    fviz_allocator_deallocate(
        &allocator,
        object,
        allocation_size,
        (FVizSize)FVIZ_INTERNAL_MAX_ALIGNMENT);
}

FVizTypeId fviz_object_type_id(const FVizObject* object)
{
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        }
        return 0u;
    }

    return object->object_class->type_id;
}

const char* fviz_object_type_name(const FVizObject* object)
{
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        }
        return NULL;
    }

    return object->object_class->type_name;
}

FVizBool fviz_object_is_type(const FVizObject* object, FVizTypeId type_id)
{
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        }
        return FVIZ_FALSE;
    }

    {
        const FVizObjectClass* object_class = object->object_class;
        while (object_class != NULL)
        {
            if (object_class->type_id == type_id)
            {
                return FVIZ_TRUE;
            }
            object_class = object_class->parent;
        }
    }

    return FVIZ_FALSE;
}

uint32_t fviz_object_ref_count(const FVizObject* object)
{
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        }
        return 0u;
    }

    return fviz_atomic_u32_load(&object->ref_count);
}

FVizMTime fviz_internal_object_local_mtime(const FVizObject* object)
{
    return object != NULL ? fviz_atomic_u64_load(&object->mtime) : 0u;
}

void fviz_object_modified(FVizObject* object)
{
    FVizMTime expected;
    FVizMTime modified;
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        return;
    }
    modified = fviz_object_next_mtime();
    expected = fviz_atomic_u64_load(&object->mtime);
    while (expected < modified &&
        !fviz_atomic_u64_compare_exchange(&object->mtime, &expected, modified))
    {
    }
}

FVizMTime fviz_object_mtime(const FVizObject* object)
{
    if (fviz_object_is_valid(object) == FVIZ_FALSE)
    {
        if (object != NULL)
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "object is not a valid FEAViz object");
        return 0u;
    }
    return object->object_class->get_mtime != NULL
        ? object->object_class->get_mtime(object)
        : fviz_internal_object_local_mtime(object);
}
