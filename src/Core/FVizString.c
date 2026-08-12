#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizString.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizStringPrivate.h>

static void fviz_string_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_string_class = {
    FVIZ_TYPE_STRING,
    "FVizString",
    &g_fviz_object_class,
    fviz_string_destroy,
    NULL
};

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

FVizResult fviz_string_set(FVizString* string, const char* text)
{
    FVizSize length;
    if (string == NULL || text == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "string and text must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    length = (FVizSize)strlen(text);
    if (fviz_string_reserve(string, length + 1u) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    (void)memcpy(string->data, text, length + 1u);
    string->length = length;
    fviz_object_modified((FVizObject*)string);
    return FVIZ_OK;
}

FVizResult fviz_string_append(FVizString* string, const char* text)
{
    FVizSize extra;
    FVizSize required;
    if (string == NULL || text == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "string and text must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    extra = (FVizSize)strlen(text);
    required = string->length + extra + 1u;
    if (required < string->length || fviz_string_reserve(string, required) != FVIZ_OK)
    {
        if (required < string->length)
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "string append overflow");
        }
        return fviz_last_error_code();
    }
    (void)memcpy(string->data + string->length, text, extra + 1u);
    string->length += extra;
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
