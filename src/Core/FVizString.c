#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizString.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizStringPrivate.h>

static void fviz_string_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_string_class = {FVIZ_TYPE_STRING, "FVizString", &g_fviz_object_class,
                                                    fviz_string_destroy, NULL};

static void fviz_string_destroy(FVizObject* object)
{
    FVizString* string = (FVizString*)object;
    fviz_allocator_deallocate(&string->base.allocator, string->data, string->capacity, 0u);
    string->data = NULL;
    string->length = 0u;
    string->capacity = 0u;
}

static FVizResult fviz_string_reserve(FVizString* string, FVizSize capacity)
{
    void* memory;
    if (capacity <= string->capacity)
    {
        return FVIZ_OK;
    }
    memory = fviz_allocator_reallocate(&string->base.allocator, string->data, string->capacity, capacity, 0u);
    if (memory == NULL)
    {
        return fviz_last_error_code();
    }
    string->data = (char*)memory;
    string->capacity = capacity;
    return FVIZ_OK;
}

FVizResult fviz_string_create(FVizString** out_string)
{
    FVizString* string;
    if (out_string == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_string must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_string = NULL;
    string = (FVizString*)fviz_internal_object_allocate(sizeof(FVizString), &g_fviz_string_class, NULL);
    if (string == NULL)
    {
        return fviz_last_error_code();
    }
    if (fviz_string_reserve(string, 1u) != FVIZ_OK)
    {
        fviz_release(string);
        return fviz_last_error_code();
    }
    string->data[0] = '\0';
    *out_string = string;
    return FVIZ_OK;
}

FVizResult fviz_string_create_from(const char* text, FVizString** out_string)
{
    FVizResult result = fviz_string_create(out_string);
    if (result == FVIZ_OK)
    {
        result = fviz_string_set(*out_string, text);
        if (result != FVIZ_OK)
        {
            fviz_release(*out_string);
            *out_string = NULL;
        }
    }
    return result;
}

const char* fviz_string_c_str(const FVizString* string)
{
    return string != NULL && string->data != NULL ? string->data : "";
}

FVizSize fviz_string_length(const FVizString* string)
{
    return string != NULL ? string->length : 0u;
}

static FVizBool fviz_string_source_offset(const FVizString* string, const char* text, FVizSize* out_offset)
{
    uintptr_t base;
    uintptr_t source;
    uintptr_t end;
    if (string == NULL || string->data == NULL || text == NULL || out_offset == NULL) return FVIZ_FALSE;
    base = (uintptr_t)(const void*)string->data;
    source = (uintptr_t)(const void*)text;
    end = base + (uintptr_t)string->length;
    if (source < base || source > end) return FVIZ_FALSE;
    *out_offset = (FVizSize)(source - base);
    return FVIZ_TRUE;
}

FVizResult fviz_string_set(FVizString* string, const char* text)
{
    FVizSize length;
    FVizSize source_offset = 0u;
    FVizBool source_is_internal;
    if (string == NULL || text == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "string and text must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    source_is_internal = fviz_string_source_offset(string, text, &source_offset);
    length = (FVizSize)strlen(text);
    if (fviz_string_reserve(string, length + 1u) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    if (source_is_internal != FVIZ_FALSE) text = string->data + source_offset;
    (void)memmove(string->data, text, length + 1u);
    string->length = length;
    fviz_object_modified((FVizObject*)string);
    return FVIZ_OK;
}

FVizResult fviz_string_append(FVizString* string, const char* text)
{
    FVizSize extra;
    FVizSize required;
    FVizSize source_offset = 0u;
    FVizSize old_length;
    FVizBool source_is_internal;
    if (string == NULL || text == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "string and text must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    source_is_internal = fviz_string_source_offset(string, text, &source_offset);
    extra = (FVizSize)strlen(text);
    old_length = string->length;
    required = old_length + extra + 1u;
    if (required < old_length || fviz_string_reserve(string, required) != FVIZ_OK)
    {
        if (required < old_length)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "string append overflow");
        }
        return fviz_last_error_code();
    }
    if (source_is_internal != FVIZ_FALSE) text = string->data + source_offset;
    (void)memmove(string->data + old_length, text, extra + 1u);
    string->length = old_length + extra;
    if (extra != 0u) fviz_object_modified((FVizObject*)string);
    return FVIZ_OK;
}

void fviz_string_clear(FVizString* string)
{
    if (string != NULL && string->data != NULL)
    {
        const FVizBool changed = string->length != 0u ? FVIZ_TRUE : FVIZ_FALSE;
        string->data[0] = '\0';
        string->length = 0u;
        if (changed == FVIZ_TRUE) fviz_object_modified((FVizObject*)string);
    }
}
