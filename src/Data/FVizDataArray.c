#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataArray.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Data/FVizDataArrayPrivate.h>

static void fviz_data_array_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_data_array_class = {
    FVIZ_TYPE_DATA_ARRAY,
    "FVizDataArray",
    &g_fviz_object_class,
    fviz_data_array_destroy
};

static void fviz_data_array_destroy(FVizObject* object)
{
    FVizDataArray* array = (FVizDataArray*)object;
    fviz_release(array->storage);
    array->storage = NULL;
}

FVizResult fviz_data_array_create(FVizDataType type, uint32_t components, FVizDataArray** out_array)
{
    FVizDataArray* array;
    FVizSize type_size;
    FVizSize stride;
    if (out_array == NULL || components == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array requires components and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_array = NULL;
    type_size = fviz_data_type_size(type);
    if (type_size == 0u || fviz_size_multiply(type_size, components, &stride) != FVIZ_OK)
    {
        fviz_internal_set_error(type_size == 0u ? FVIZ_ERROR_INVALID_ARGUMENT : FVIZ_ERROR_OVERFLOW, "invalid data array type or tuple stride");
        return type_size == 0u ? FVIZ_ERROR_INVALID_ARGUMENT : FVIZ_ERROR_OVERFLOW;
    }
    array = (FVizDataArray*)fviz_internal_object_allocate(sizeof(FVizDataArray), &g_fviz_data_array_class, NULL);
    if (array == NULL) return fviz_last_error_code();
    array->type = type;
    array->components = components;
    array->tuple_stride = stride;
    if (fviz_array_create(stride, &array->storage) != FVIZ_OK)
    {
        fviz_release(array);
        return fviz_last_error_code();
    }
    *out_array = array;
    return FVIZ_OK;
}

FVizDataType fviz_data_array_type(const FVizDataArray* array) { return array != NULL ? array->type : (FVizDataType)0; }
uint32_t fviz_data_array_components(const FVizDataArray* array) { return array != NULL ? array->components : 0u; }
FVizSize fviz_data_array_tuple_count(const FVizDataArray* array) { return array != NULL ? fviz_array_count(array->storage) : 0u; }
FVizSize fviz_data_array_tuple_stride(const FVizDataArray* array) { return array != NULL ? array->tuple_stride : 0u; }
void* fviz_data_array_data(FVizDataArray* array) { return array != NULL ? fviz_array_data(array->storage) : NULL; }
const void* fviz_data_array_const_data(const FVizDataArray* array) { return array != NULL ? fviz_array_const_data(array->storage) : NULL; }
FVizResult fviz_data_array_resize(FVizDataArray* array, FVizSize tuple_count) { return array != NULL ? fviz_array_resize(array->storage, tuple_count) : FVIZ_ERROR_INVALID_ARGUMENT; }
FVizResult fviz_data_array_reserve(FVizDataArray* array, FVizSize tuple_capacity) { return array != NULL ? fviz_array_reserve(array->storage, tuple_capacity) : FVIZ_ERROR_INVALID_ARGUMENT; }
FVizResult fviz_data_array_append_tuple(FVizDataArray* array, const void* tuple) { return array != NULL ? fviz_array_push(array->storage, tuple) : FVIZ_ERROR_INVALID_ARGUMENT; }

FVizResult fviz_data_array_set_tuple(FVizDataArray* array, FVizSize index, const void* tuple)
{
    void* destination;
    if (array == NULL || tuple == NULL || index >= fviz_data_array_tuple_count(array))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array tuple assignment is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    destination = fviz_array_at(array->storage, index);
    (void)memcpy(destination, tuple, array->tuple_stride);
    return FVIZ_OK;
}
void* fviz_data_array_tuple(FVizDataArray* array, FVizSize index) { return array != NULL ? fviz_array_at(array->storage, index) : NULL; }
const void* fviz_data_array_const_tuple(const FVizDataArray* array, FVizSize index) { return array != NULL ? fviz_array_const_at(array->storage, index) : NULL; }
