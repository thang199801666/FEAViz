#include <string.h>

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>

#include <FViz/Core/FVizArrayPrivate.h>
#include <FViz/Core/FVizErrorInternal.h>

static void fviz_array_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_array_class = {
    FVIZ_TYPE_ARRAY,
    "FVizArray",
    &g_fviz_object_class,
    fviz_array_destroy,
    NULL
};

static void fviz_array_destroy(FVizObject* object)
{
    FVizArray* array = (FVizArray*)object;
    FVizSize bytes = 0u;
    (void)fviz_size_multiply(array->capacity, array->stride, &bytes);
    fviz_allocator_deallocate(&array->base.allocator, array->data, bytes, 0u);
    array->data = NULL;
    array->count = 0u;
    array->capacity = 0u;
}

FVizResult fviz_array_create(FVizSize stride, FVizArray** out_array)
{
    return fviz_array_create_reserve(stride, 0u, out_array);
}

FVizResult fviz_array_create_reserve(FVizSize stride, FVizSize capacity, FVizArray** out_array)
{
    FVizArray* array;
    FVizResult result;
    if (out_array == NULL || stride == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "array requires non-zero stride and out_array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_array = NULL;
    array = (FVizArray*)fviz_internal_object_allocate(sizeof(FVizArray), &g_fviz_array_class, NULL);
    if (array == NULL)
    {
        return fviz_last_error_code();
    }
    array->stride = stride;
    result = fviz_array_reserve(array, capacity);
    if (result != FVIZ_OK)
    {
        fviz_release(array);
        return result;
    }
    *out_array = array;
    return FVIZ_OK;
}

FVizSize fviz_array_count(const FVizArray* array) { return array != NULL ? array->count : 0u; }
FVizSize fviz_array_capacity(const FVizArray* array) { return array != NULL ? array->capacity : 0u; }
FVizSize fviz_array_stride(const FVizArray* array) { return array != NULL ? array->stride : 0u; }
void* fviz_array_data(FVizArray* array) { return array != NULL ? array->data : NULL; }
const void* fviz_array_const_data(const FVizArray* array) { return array != NULL ? array->data : NULL; }

void* fviz_array_at(FVizArray* array, FVizSize index)
{
    return array != NULL && index < array->count ? array->data + index * array->stride : NULL;
}

const void* fviz_array_const_at(const FVizArray* array, FVizSize index)
{
    return array != NULL && index < array->count ? array->data + index * array->stride : NULL;
}

FVizResult fviz_array_reserve(FVizArray* array, FVizSize capacity)
{
    FVizSize old_bytes;
    FVizSize new_bytes;
    void* memory;
    if (array == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "array must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (capacity <= array->capacity)
    {
        return FVIZ_OK;
    }
    if (fviz_size_multiply(array->capacity, array->stride, &old_bytes) != FVIZ_OK ||
        fviz_size_multiply(capacity, array->stride, &new_bytes) != FVIZ_OK)
    {
        return FVIZ_ERROR_OVERFLOW;
    }
    memory = fviz_allocator_reallocate(&array->base.allocator, array->data, old_bytes, new_bytes, 0u);
    if (memory == NULL)
    {
        return fviz_last_error_code();
    }
    array->data = (unsigned char*)memory;
    array->capacity = capacity;
    return FVIZ_OK;
}

static FVizResult fviz_array_ensure(FVizArray* array, FVizSize required)
{
    FVizSize capacity;
    if (required <= array->capacity)
    {
        return FVIZ_OK;
    }
    capacity = array->capacity == 0u ? 8u : array->capacity;
    while (capacity < required)
    {
        if (capacity > ((FVizSize)-1) / 2u)
        {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    return fviz_array_reserve(array, capacity);
}

FVizResult fviz_array_resize(FVizArray* array, FVizSize count)
{
    FVizSize old_count;
    FVizResult result;
    if (array == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "array must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    old_count = array->count;
    result = fviz_array_ensure(array, count);
    if (result != FVIZ_OK)
    {
        return result;
    }
    if (count > old_count)
    {
        (void)memset(array->data + old_count * array->stride, 0, (count - old_count) * array->stride);
    }
    array->count = count;
    if (count != old_count) fviz_object_modified((FVizObject*)array);
    return FVIZ_OK;
}

FVizResult fviz_array_push_uninitialized(FVizArray* array, void** out_slot)
{
    FVizResult result;
    if (array == NULL || out_slot == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "array and out_slot must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_slot = NULL;
    result = fviz_array_ensure(array, array->count + 1u);
    if (result != FVIZ_OK)
    {
        return result;
    }
    *out_slot = array->data + array->count * array->stride;
    array->count += 1u;
    fviz_object_modified((FVizObject*)array);
    return FVIZ_OK;
}

FVizResult fviz_array_push(FVizArray* array, const void* value)
{
    void* slot;
    FVizResult result;
    if (value == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "array value must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result = fviz_array_push_uninitialized(array, &slot);
    if (result == FVIZ_OK)
    {
        (void)memcpy(slot, value, array->stride);
    }
    return result;
}

void fviz_array_clear(FVizArray* array)
{
    if (array != NULL)
    {
        const FVizBool changed = array->count != 0u ? FVIZ_TRUE : FVIZ_FALSE;
        array->count = 0u;
        if (changed == FVIZ_TRUE) fviz_object_modified((FVizObject*)array);
    }
}
