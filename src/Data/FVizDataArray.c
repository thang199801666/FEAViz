#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataArray.h>

#include <FViz/Core/FVizArrayPrivate.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Data/FVizDataArrayPrivate.h>

static void fviz_data_array_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_data_array_class = {FVIZ_TYPE_DATA_ARRAY, "FVizDataArray", &g_fviz_object_class,
                                                        fviz_data_array_destroy, NULL};

static void fviz_data_array_record_dirty(FVizDataArray* array, FVizSize first, FVizSize count, FVizBool full)
{
    uint32_t slot;
    FVizDataArrayDirtyRecord* record;
    fviz_object_modified((FVizObject*)array);
    if (array->dirty_history_count < FVIZ_DATA_ARRAY_DIRTY_HISTORY_CAPACITY)
    {
        slot = (array->dirty_history_begin + array->dirty_history_count) % FVIZ_DATA_ARRAY_DIRTY_HISTORY_CAPACITY;
        ++array->dirty_history_count;
    }
    else
    {
        slot = array->dirty_history_begin;
        array->dirty_history_begin = (array->dirty_history_begin + 1u) % FVIZ_DATA_ARRAY_DIRTY_HISTORY_CAPACITY;
    }
    record = &array->dirty_history[slot];
    record->mtime = fviz_internal_object_local_mtime((const FVizObject*)array);
    record->first = first;
    record->count = count;
    record->full = full != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

static void fviz_data_array_destroy(FVizObject* object)
{
    FVizDataArray* array = (FVizDataArray*)object;
    if (array->external != FVIZ_FALSE && array->external_release_callback != NULL)
        array->external_release_callback(array->external_data, array->external_release_user_data);
    fviz_release(array->storage);
    array->storage = NULL;
    array->external_data = NULL;
}

static FVizResult fviz_data_array_allocate(FVizDataType type, uint32_t components, FVizDataArray** out_array)
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
        fviz_internal_set_error(type_size == 0u ? FVIZ_ERROR_INVALID_ARGUMENT : FVIZ_ERROR_OVERFLOW,
                                "invalid data array type or tuple stride");
        return type_size == 0u ? FVIZ_ERROR_INVALID_ARGUMENT : FVIZ_ERROR_OVERFLOW;
    }
    array = (FVizDataArray*)fviz_internal_object_allocate(sizeof(FVizDataArray), &g_fviz_data_array_class, NULL);
    if (array == NULL) return fviz_last_error_code();
    array->type = type;
    array->components = components;
    array->tuple_stride = stride;
    array->mutable_data = FVIZ_TRUE;
    *out_array = array;
    return FVIZ_OK;
}

FVizResult fviz_data_array_create(FVizDataType type, uint32_t components, FVizDataArray** out_array)
{
    FVizDataArray* array = NULL;
    if (out_array == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_array must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    FVizResult result = fviz_data_array_allocate(type, components, &array);
    if (result != FVIZ_OK) return result;
    if (fviz_array_create(array->tuple_stride, &array->storage) != FVIZ_OK)
    {
        fviz_release(array);
        return fviz_last_error_code();
    }
    *out_array = array;
    return FVIZ_OK;
}

FVizResult fviz_data_array_create_external(FVizDataType type, uint32_t components, void* data, FVizSize tuple_count,
                                           FVizDataArrayExternalFlags flags,
                                           FVizDataArrayExternalReleaseCallback release_callback,
                                           void* release_user_data, FVizDataArray** out_array)
{
    FVizDataArray* array = NULL;
    FVizResult result;
    FVizSize ignored_bytes;
    if ((data == NULL && tuple_count != 0u) ||
        (((unsigned int)flags & ~(unsigned int)FVIZ_DATA_ARRAY_EXTERNAL_MUTABLE) != 0u))
    {
        if (out_array != NULL) *out_array = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "external data array storage or flags are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result = fviz_data_array_allocate(type, components, &array);
    if (result != FVIZ_OK) return result;
    if (fviz_size_multiply(tuple_count, array->tuple_stride, &ignored_bytes) != FVIZ_OK)
    {
        fviz_release(array);
        return FVIZ_ERROR_OVERFLOW;
    }
    array->external = FVIZ_TRUE;
    array->mutable_data = (flags & FVIZ_DATA_ARRAY_EXTERNAL_MUTABLE) != 0 ? FVIZ_TRUE : FVIZ_FALSE;
    array->external_data = data;
    array->external_tuple_count = tuple_count;
    array->external_release_callback = release_callback;
    array->external_release_user_data = release_user_data;
    *out_array = array;
    return FVIZ_OK;
}

FVizBool fviz_data_array_is_external(const FVizDataArray* array)
{
    return array != NULL ? array->external : FVIZ_FALSE;
}

FVizBool fviz_data_array_is_mutable(const FVizDataArray* array)
{
    return array != NULL ? array->mutable_data : FVIZ_FALSE;
}

FVizDataType fviz_data_array_type(const FVizDataArray* array)
{
    return array != NULL ? array->type : (FVizDataType)0;
}

uint32_t fviz_data_array_components(const FVizDataArray* array)
{
    return array != NULL ? array->components : 0u;
}

FVizSize fviz_data_array_tuple_count(const FVizDataArray* array)
{
    return array != NULL
               ? (array->external != FVIZ_FALSE ? array->external_tuple_count : fviz_array_count(array->storage))
               : 0u;
}

FVizSize fviz_data_array_tuple_stride(const FVizDataArray* array)
{
    return array != NULL ? array->tuple_stride : 0u;
}

void* fviz_data_array_data(FVizDataArray* array)
{
    return array != NULL && array->mutable_data != FVIZ_FALSE
               ? (array->external != FVIZ_FALSE ? array->external_data : fviz_array_data(array->storage))
               : NULL;
}

const void* fviz_data_array_const_data(const FVizDataArray* array)
{
    return array != NULL
               ? (array->external != FVIZ_FALSE ? array->external_data : fviz_array_const_data(array->storage))
               : NULL;
}

FVizResult fviz_data_array_resize(FVizDataArray* array, FVizSize tuple_count)
{
    FVizResult result;
    FVizSize old_count;
    if (array == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    old_count = fviz_data_array_tuple_count(array);
    if (array->external != FVIZ_FALSE)
    {
        if (tuple_count == old_count) return FVIZ_OK;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "external data array shape is fixed");
        return FVIZ_ERROR_INVALID_STATE;
    }
    result = fviz_internal_array_resize_untracked(array->storage, tuple_count);
    if (result == FVIZ_OK && old_count != tuple_count) fviz_data_array_record_dirty(array, 0u, tuple_count, FVIZ_TRUE);
    return result;
}

FVizResult fviz_data_array_reserve(FVizDataArray* array, FVizSize tuple_capacity)
{
    if (array == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (array->external != FVIZ_FALSE)
    {
        if (tuple_capacity <= array->external_tuple_count) return FVIZ_OK;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "external data array capacity is fixed");
        return FVIZ_ERROR_INVALID_STATE;
    }
    return fviz_array_reserve(array->storage, tuple_capacity);
}

FVizResult fviz_data_array_append_tuples(FVizDataArray* array, const void* tuples, FVizSize tuple_count)
{
    FVizResult result;
    FVizSize first;
    if (array == NULL || (tuples == NULL && tuple_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array append requires valid tuples");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (tuple_count == 0u) return FVIZ_OK;
    if (array->external != FVIZ_FALSE && tuple_count != 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "cannot append to an external data array");
        return FVIZ_ERROR_INVALID_STATE;
    }
    first = fviz_data_array_tuple_count(array);
    result = fviz_internal_array_append(array->storage, tuples, tuple_count);
    if (result == FVIZ_OK && tuple_count != 0u) fviz_data_array_record_dirty(array, first, tuple_count, FVIZ_FALSE);
    return result;
}

FVizResult fviz_data_array_append_tuple(FVizDataArray* array, const void* tuple)
{
    return fviz_data_array_append_tuples(array, tuple, 1u);
}

FVizResult fviz_data_array_set_tuples(FVizDataArray* array, FVizSize first, const void* tuples, FVizSize tuple_count)
{
    FVizSize count;
    FVizSize bytes;
    unsigned char* destination;
    if (array == NULL || (tuples == NULL && tuple_count != 0u))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array tuple assignment requires valid input");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (array->mutable_data == FVIZ_FALSE && tuple_count != 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "data array is immutable");
        return FVIZ_ERROR_INVALID_STATE;
    }
    count = fviz_data_array_tuple_count(array);
    if (first > count || tuple_count > count - first)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array tuple assignment is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (tuple_count == 0u) return FVIZ_OK;
    if (fviz_size_multiply(tuple_count, array->tuple_stride, &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    destination = (unsigned char*)fviz_data_array_data(array) + first * array->tuple_stride;
    if (memcmp(destination, tuples, (size_t)bytes) == 0) return FVIZ_OK;
    (void)memcpy(destination, tuples, (size_t)bytes);
    fviz_data_array_record_dirty(array, first, tuple_count, FVIZ_FALSE);
    return FVIZ_OK;
}

FVizResult fviz_data_array_mark_dirty(FVizDataArray* array, FVizSize first, FVizSize tuple_count)
{
    const FVizSize count = array != NULL ? fviz_data_array_tuple_count(array) : 0u;
    if (array == NULL || first > count || tuple_count > count - first)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array dirty range is out of bounds");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (array->mutable_data == FVIZ_FALSE && tuple_count != 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "data array is immutable");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (tuple_count != 0u) fviz_data_array_record_dirty(array, first, tuple_count, FVIZ_FALSE);
    return FVIZ_OK;
}

FVizResult fviz_data_array_dirty_range_since(const FVizDataArray* array, FVizMTime since_mtime,
                                             FVizDirtyRange* out_range)
{
    FVizMTime current_mtime;
    uint32_t offset;
    FVizBool found = FVIZ_FALSE;
    if (array == NULL || out_range == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array dirty range query is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    out_range->first = 0u;
    out_range->count = 0u;
    out_range->full = FVIZ_FALSE;
    current_mtime = fviz_internal_object_local_mtime((const FVizObject*)array);
    if (since_mtime >= current_mtime) return FVIZ_OK;
    if (since_mtime == 0u || array->dirty_history_count == 0u) goto full;
    {
        const FVizDataArrayDirtyRecord* oldest = &array->dirty_history[array->dirty_history_begin];
        const uint32_t newest_slot =
            (array->dirty_history_begin + array->dirty_history_count - 1u) % FVIZ_DATA_ARRAY_DIRTY_HISTORY_CAPACITY;
        const FVizDataArrayDirtyRecord* newest = &array->dirty_history[newest_slot];
        if (newest->mtime != current_mtime ||
            (array->dirty_history_count == FVIZ_DATA_ARRAY_DIRTY_HISTORY_CAPACITY && since_mtime < oldest->mtime))
            goto full;
    }
    for (offset = 0u; offset < array->dirty_history_count; ++offset)
    {
        const uint32_t slot = (array->dirty_history_begin + offset) % FVIZ_DATA_ARRAY_DIRTY_HISTORY_CAPACITY;
        const FVizDataArrayDirtyRecord* record = &array->dirty_history[slot];
        FVizSize end;
        FVizSize current_end;
        if (record->mtime <= since_mtime) continue;
        if (record->full != FVIZ_FALSE) goto full;
        end = record->first + record->count;
        if (found == FVIZ_FALSE)
        {
            out_range->first = record->first;
            out_range->count = record->count;
            found = FVIZ_TRUE;
        }
        else
        {
            current_end = out_range->first + out_range->count;
            if (record->first < out_range->first) out_range->first = record->first;
            if (end > current_end) current_end = end;
            out_range->count = current_end - out_range->first;
        }
    }
    if (found == FVIZ_FALSE) goto full;
    return FVIZ_OK;
full:
    out_range->first = 0u;
    out_range->count = fviz_data_array_tuple_count(array);
    out_range->full = FVIZ_TRUE;
    return FVIZ_OK;
}

FVizResult fviz_data_array_set_tuple(FVizDataArray* array, FVizSize index, const void* tuple)
{
    return fviz_data_array_set_tuples(array, index, tuple, 1u);
}

void* fviz_data_array_tuple(FVizDataArray* array, FVizSize index)
{
    return array != NULL && array->mutable_data != FVIZ_FALSE && index < fviz_data_array_tuple_count(array)
               ? (unsigned char*)fviz_data_array_data(array) + index * array->tuple_stride
               : NULL;
}

const void* fviz_data_array_const_tuple(const FVizDataArray* array, FVizSize index)
{
    return array != NULL && index < fviz_data_array_tuple_count(array)
               ? (const unsigned char*)fviz_data_array_const_data(array) + index * array->tuple_stride
               : NULL;
}

FVizResult fviz_data_array_deep_copy(const FVizDataArray* source, FVizDataArray** out_copy)
{
    FVizDataArray* copy = NULL;
    FVizSize bytes = 0u;
    if (source == NULL || out_copy == NULL)
    {
        if (out_copy != NULL) *out_copy = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array deep copy requires source and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_copy = NULL;
    if (fviz_data_array_create(source->type, source->components, &copy) != FVIZ_OK ||
        fviz_data_array_resize(copy, fviz_data_array_tuple_count(source)) != FVIZ_OK)
    {
        fviz_release(copy);
        return fviz_last_error_code();
    }
    if (fviz_size_multiply(fviz_data_array_tuple_count(source), source->tuple_stride, &bytes) != FVIZ_OK)
    {
        fviz_release(copy);
        return FVIZ_ERROR_OVERFLOW;
    }
    if (bytes != 0u) (void)memcpy(fviz_data_array_data(copy), fviz_data_array_const_data(source), bytes);
    *out_copy = copy;
    return FVIZ_OK;
}

static double fviz_data_array_read_numeric(const FVizDataArray* array, FVizSize scalar_index)
{
    const unsigned char* bytes = (const unsigned char*)fviz_data_array_const_data(array);
    const FVizSize type_size = fviz_data_type_size(array->type);
    const void* value = bytes + scalar_index * type_size;
    switch (array->type)
    {
        case FVIZ_DATA_INT8:
            return (double)*(const int8_t*)value;
        case FVIZ_DATA_UINT8:
            return (double)*(const uint8_t*)value;
        case FVIZ_DATA_INT16:
            return (double)*(const int16_t*)value;
        case FVIZ_DATA_UINT16:
            return (double)*(const uint16_t*)value;
        case FVIZ_DATA_INT32:
            return (double)*(const int32_t*)value;
        case FVIZ_DATA_UINT32:
            return (double)*(const uint32_t*)value;
        case FVIZ_DATA_INT64:
            return (double)*(const int64_t*)value;
        case FVIZ_DATA_UINT64:
            return (double)*(const uint64_t*)value;
        case FVIZ_DATA_FLOAT32:
            return (double)*(const float*)value;
        case FVIZ_DATA_FLOAT64:
            return *(const double*)value;
        default:
            return 0.0;
    }
}

static void fviz_data_array_write_numeric(FVizDataArray* array, FVizSize scalar_index, double input)
{
    unsigned char* bytes = (unsigned char*)fviz_data_array_data(array);
    const FVizSize type_size = fviz_data_type_size(array->type);
    void* value = bytes + scalar_index * type_size;
    if (isnan(input)) input = 0.0;
    switch (array->type)
    {
        case FVIZ_DATA_INT8:
            if (input < INT8_MIN) input = INT8_MIN;
            else if (input > INT8_MAX)
                input = INT8_MAX;
            *(int8_t*)value = (int8_t)input;
            break;
        case FVIZ_DATA_UINT8:
            if (input < 0.0) input = 0.0;
            else if (input > UINT8_MAX)
                input = UINT8_MAX;
            *(uint8_t*)value = (uint8_t)input;
            break;
        case FVIZ_DATA_INT16:
            if (input < INT16_MIN) input = INT16_MIN;
            else if (input > INT16_MAX)
                input = INT16_MAX;
            *(int16_t*)value = (int16_t)input;
            break;
        case FVIZ_DATA_UINT16:
            if (input < 0.0) input = 0.0;
            else if (input > UINT16_MAX)
                input = UINT16_MAX;
            *(uint16_t*)value = (uint16_t)input;
            break;
        case FVIZ_DATA_INT32:
            if (input < (double)INT32_MIN) input = (double)INT32_MIN;
            else if (input > (double)INT32_MAX)
                input = (double)INT32_MAX;
            *(int32_t*)value = (int32_t)input;
            break;
        case FVIZ_DATA_UINT32:
            if (input < 0.0) input = 0.0;
            else if (input > (double)UINT32_MAX)
                input = (double)UINT32_MAX;
            *(uint32_t*)value = (uint32_t)input;
            break;
        case FVIZ_DATA_INT64:
            if (input <= -9223372036854775808.0) *(int64_t*)value = INT64_MIN;
            else if (input >= 9223372036854775807.0)
                *(int64_t*)value = INT64_MAX;
            else
                *(int64_t*)value = (int64_t)input;
            break;
        case FVIZ_DATA_UINT64:
            if (input <= 0.0) *(uint64_t*)value = 0u;
            else if (input >= 18446744073709551615.0)
                *(uint64_t*)value = UINT64_MAX;
            else
                *(uint64_t*)value = (uint64_t)input;
            break;
        case FVIZ_DATA_FLOAT32:
            *(float*)value = (float)input;
            break;
        case FVIZ_DATA_FLOAT64:
            *(double*)value = input;
            break;
        default:
            break;
    }
}

FVizResult fviz_data_array_get_component(const FVizDataArray* array, FVizSize tuple_index, uint32_t component,
                                         double* out_value)
{
    if (array == NULL || out_value == NULL || tuple_index >= fviz_data_array_tuple_count(array) ||
        component >= array->components)
    {
        if (out_value != NULL) *out_value = 0.0;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array component access is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_value = fviz_data_array_read_numeric(array, tuple_index * array->components + component);
    return FVIZ_OK;
}

FVizResult fviz_data_array_set_component(FVizDataArray* array, FVizSize tuple_index, uint32_t component, double value)
{
    if (array == NULL || tuple_index >= fviz_data_array_tuple_count(array) || component >= array->components)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array component assignment is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (array->mutable_data == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "data array is immutable");
        return FVIZ_ERROR_INVALID_STATE;
    }
    {
        const FVizSize scalar_index = tuple_index * array->components + component;
        if (fviz_data_array_read_numeric(array, scalar_index) == value) return FVIZ_OK;
        fviz_data_array_write_numeric(array, scalar_index, value);
    }
    fviz_data_array_record_dirty(array, tuple_index, 1u, FVIZ_FALSE);
    return FVIZ_OK;
}

static void fviz_data_array_store_range_cache(const FVizDataArray* array, FVizMTime mtime, int32_t component,
                                              FVizBool ignore_non_finite, double minimum, double maximum)
{
    FVizDataArray* mutable_array = (FVizDataArray*)array;
    mutable_array->range_cache_valid = FVIZ_TRUE;
    mutable_array->range_cache_ignore_non_finite = ignore_non_finite;
    mutable_array->range_cache_component = component;
    mutable_array->range_cache_mtime = mtime;
    mutable_array->range_cache_minimum = minimum;
    mutable_array->range_cache_maximum = maximum;
}

FVizResult fviz_data_array_get_range(const FVizDataArray* array, int32_t component, FVizBool ignore_non_finite,
                                     double* out_minimum, double* out_maximum)
{
    FVizSize tuple;
    FVizBool found = FVIZ_FALSE;
    FVizBool normalized_ignore;
    FVizMTime current_mtime;
    double minimum = 0.0;
    double maximum = 0.0;
    if (array == NULL || out_minimum == NULL || out_maximum == NULL || component < -1 ||
        (component >= 0 && (uint32_t)component >= array->components))
    {
        if (out_minimum != NULL) *out_minimum = 0.0;
        if (out_maximum != NULL) *out_maximum = 0.0;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array range request is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_minimum = 0.0;
    *out_maximum = 0.0;
    normalized_ignore = ignore_non_finite != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    current_mtime = fviz_internal_object_local_mtime((const FVizObject*)array);
    if (array->range_cache_valid != FVIZ_FALSE && array->range_cache_mtime == current_mtime &&
        array->range_cache_component == component && array->range_cache_ignore_non_finite == normalized_ignore)
    {
        *out_minimum = array->range_cache_minimum;
        *out_maximum = array->range_cache_maximum;
        return FVIZ_OK;
    }

    /* Scalar float arrays dominate FEA result visualization.  Keep the generic
       numeric path below for all other types/components, but avoid the per-value
       type switch and address reconstruction for the two native floating types. */
    if (component >= 0 && (array->type == FVIZ_DATA_FLOAT32 || array->type == FVIZ_DATA_FLOAT64))
    {
        const FVizSize tuple_count = fviz_data_array_tuple_count(array);
        const FVizSize stride = (FVizSize)array->components;
        const FVizSize offset = (FVizSize)(uint32_t)component;
        if (array->type == FVIZ_DATA_FLOAT32)
        {
            const float* values = (const float*)fviz_data_array_const_data(array);
            for (tuple = 0u; tuple < tuple_count; ++tuple)
            {
                const double value = (double)values[tuple * stride + offset];
                if (normalized_ignore != FVIZ_FALSE && !isfinite(value)) continue;
                if (found == FVIZ_FALSE)
                {
                    minimum = maximum = value;
                    found = FVIZ_TRUE;
                }
                else
                {
                    if (value < minimum) minimum = value;
                    if (value > maximum) maximum = value;
                }
            }
        }
        else
        {
            const double* values = (const double*)fviz_data_array_const_data(array);
            for (tuple = 0u; tuple < tuple_count; ++tuple)
            {
                const double value = values[tuple * stride + offset];
                if (normalized_ignore != FVIZ_FALSE && !isfinite(value)) continue;
                if (found == FVIZ_FALSE)
                {
                    minimum = maximum = value;
                    found = FVIZ_TRUE;
                }
                else
                {
                    if (value < minimum) minimum = value;
                    if (value > maximum) maximum = value;
                }
            }
        }
        if (found == FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "data array range has no finite values");
            return FVIZ_ERROR_NOT_FOUND;
        }
        *out_minimum = minimum;
        *out_maximum = maximum;
        fviz_data_array_store_range_cache(array, current_mtime, component, normalized_ignore, minimum, maximum);
        return FVIZ_OK;
    }

    for (tuple = 0u; tuple < fviz_data_array_tuple_count(array); ++tuple)
    {
        double value;
        if (component >= 0)
        {
            value = fviz_data_array_read_numeric(array, tuple * array->components + (uint32_t)component);
        }
        else
        {
            uint32_t c;
            double magnitude = 0.0;
            for (c = 0u; c < array->components; ++c)
            {
                const double v = fviz_data_array_read_numeric(array, tuple * array->components + c);
                magnitude = hypot(magnitude, v);
            }
            value = magnitude;
        }
        if (normalized_ignore != FVIZ_FALSE && !isfinite(value)) continue;
        if (found == FVIZ_FALSE)
        {
            minimum = maximum = value;
            found = FVIZ_TRUE;
        }
        else
        {
            if (value < minimum) minimum = value;
            if (value > maximum) maximum = value;
        }
    }
    if (found == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "data array range has no finite values");
        return FVIZ_ERROR_NOT_FOUND;
    }
    *out_minimum = minimum;
    *out_maximum = maximum;
    fviz_data_array_store_range_cache(array, current_mtime, component, normalized_ignore, minimum, maximum);
    return FVIZ_OK;
}

FVizResult fviz_data_array_iter_begin(const FVizDataArray* array, FVizDataArrayTupleIterator* out_iter)
{
    if (out_iter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array iterator output is required");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    out_iter->array = array;
    out_iter->tuple_index = 0u;
    if (array == NULL || fviz_data_array_tuple_count(array) == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "data array iterator has no tuples");
        return FVIZ_ERROR_NOT_FOUND;
    }
    return FVIZ_OK;
}

FVizBool fviz_data_array_iter_next(FVizDataArrayTupleIterator* iter)
{
    if (iter == NULL || iter->array == NULL) return FVIZ_FALSE;
    if (iter->tuple_index >= fviz_data_array_tuple_count(iter->array)) return FVIZ_FALSE;
    ++iter->tuple_index;
    return iter->tuple_index < fviz_data_array_tuple_count(iter->array) ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_data_array_iter_valid(const FVizDataArrayTupleIterator* iter)
{
    return iter != NULL && iter->array != NULL && iter->tuple_index < fviz_data_array_tuple_count(iter->array)
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizSize fviz_data_array_iter_index(const FVizDataArrayTupleIterator* iter)
{
    return iter != NULL ? iter->tuple_index : 0u;
}

const void* fviz_data_array_iter_tuple(const FVizDataArrayTupleIterator* iter)
{
    const unsigned char* base;
    if (!fviz_data_array_iter_valid(iter)) return NULL;
    base = (const unsigned char*)fviz_data_array_const_data(iter->array);
    return base != NULL ? (const void*)(base + iter->tuple_index * iter->array->tuple_stride) : NULL;
}

FVizResult fviz_data_array_mut_iter_begin(FVizDataArray* array, FVizDataArrayMutableIterator* out_iter)
{
    if (out_iter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data array iterator output is required");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    out_iter->array = array;
    out_iter->tuple_index = 0u;
    if (array == NULL || fviz_data_array_tuple_count(array) == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "data array iterator has no tuples");
        return FVIZ_ERROR_NOT_FOUND;
    }
    if (array->mutable_data == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "data array storage is not writable");
        return FVIZ_ERROR_INVALID_STATE;
    }
    return FVIZ_OK;
}

FVizBool fviz_data_array_mut_iter_next(FVizDataArrayMutableIterator* iter)
{
    if (iter == NULL || iter->array == NULL) return FVIZ_FALSE;
    if (iter->tuple_index >= fviz_data_array_tuple_count(iter->array)) return FVIZ_FALSE;
    ++iter->tuple_index;
    return iter->tuple_index < fviz_data_array_tuple_count(iter->array) ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_data_array_mut_iter_valid(const FVizDataArrayMutableIterator* iter)
{
    return iter != NULL && iter->array != NULL && iter->tuple_index < fviz_data_array_tuple_count(iter->array)
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizSize fviz_data_array_mut_iter_index(const FVizDataArrayMutableIterator* iter)
{
    return iter != NULL ? iter->tuple_index : 0u;
}

void* fviz_data_array_mut_iter_tuple(FVizDataArrayMutableIterator* iter)
{
    unsigned char* base;
    if (!fviz_data_array_mut_iter_valid(iter)) return NULL;
    base = (unsigned char*)fviz_data_array_data(iter->array);
    return base != NULL ? (void*)(base + iter->tuple_index * iter->array->tuple_stride) : NULL;
}
