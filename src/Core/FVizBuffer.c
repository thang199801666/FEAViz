#include <string.h>

#include <FViz/Core/FVizBuffer.h>
#include <FViz/Core/FVizError.h>

#include <FViz/Core/FVizBufferPrivate.h>
#include <FViz/Core/FVizErrorInternal.h>

static void fviz_buffer_destroy(FVizObject* object);

static const FVizObjectClass g_fviz_buffer_class = {
    FVIZ_TYPE_BUFFER,
    "FVizBuffer",
    &g_fviz_object_class,
    fviz_buffer_destroy,
    NULL
};

static void fviz_buffer_destroy(FVizObject* object)
{
    FVizBuffer* buffer = (FVizBuffer*)object;
    if (buffer->data == NULL)
    {
        return;
    }

    if (buffer->external == FVIZ_TRUE)
    {
        if (buffer->release_fn != NULL)
        {
            buffer->release_fn(buffer->data, buffer->size, buffer->release_user_data);
        }
    }
    else
    {
        fviz_allocator_deallocate(&buffer->base.allocator, buffer->data, buffer->size, 0u);
    }
    buffer->data = NULL;
    buffer->size = 0u;
}

static FVizResult fviz_buffer_allocate_object(FVizBuffer** out_buffer)
{
    FVizBuffer* buffer;
    if (out_buffer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_buffer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = NULL;

    buffer = (FVizBuffer*)fviz_internal_object_allocate(sizeof(FVizBuffer), &g_fviz_buffer_class, NULL);
    if (buffer == NULL)
    {
        return fviz_last_error_code();
    }
    *out_buffer = buffer;
    return FVIZ_OK;
}

FVizResult fviz_buffer_create(FVizSize size, FVizBuffer** out_buffer)
{
    FVizBuffer* buffer;
    FVizResult result = fviz_buffer_allocate_object(&buffer);
    if (result != FVIZ_OK)
    {
        return result;
    }

    if (size != 0u)
    {
        buffer->data = (unsigned char*)fviz_allocator_allocate(&buffer->base.allocator, size, 0u);
        if (buffer->data == NULL)
        {
            fviz_release(buffer);
            return fviz_last_error_code();
        }
        (void)memset(buffer->data, 0, size);
    }
    buffer->size = size;
    *out_buffer = buffer;
    return FVIZ_OK;
}

FVizResult fviz_buffer_create_copy(const void* data, FVizSize size, FVizBuffer** out_buffer)
{
    FVizResult result;
    if (size != 0u && data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "data must not be NULL when size is non-zero");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result = fviz_buffer_create(size, out_buffer);
    if (result == FVIZ_OK && size != 0u)
    {
        (void)memcpy((*out_buffer)->data, data, size);
    }
    return result;
}

FVizResult fviz_buffer_wrap(
    void* data,
    FVizSize size,
    FVizBufferReleaseFn release_fn,
    void* user_data,
    FVizBuffer** out_buffer)
{
    FVizBuffer* buffer;
    FVizResult result;
    if (size != 0u && data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "external data must not be NULL when size is non-zero");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result = fviz_buffer_allocate_object(&buffer);
    if (result != FVIZ_OK)
    {
        return result;
    }
    buffer->data = (unsigned char*)data;
    buffer->size = size;
    buffer->release_fn = release_fn;
    buffer->release_user_data = user_data;
    buffer->external = FVIZ_TRUE;
    *out_buffer = buffer;
    return FVIZ_OK;
}

void* fviz_buffer_data(FVizBuffer* buffer)
{
    return buffer != NULL ? buffer->data : NULL;
}

const void* fviz_buffer_const_data(const FVizBuffer* buffer)
{
    return buffer != NULL ? buffer->data : NULL;
}

FVizSize fviz_buffer_size(const FVizBuffer* buffer)
{
    return buffer != NULL ? buffer->size : 0u;
}

FVizBool fviz_buffer_is_external(const FVizBuffer* buffer)
{
    return buffer != NULL ? buffer->external : FVIZ_FALSE;
}

FVizResult fviz_buffer_resize(FVizBuffer* buffer, FVizSize new_size)
{
    void* memory;
    if (buffer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "buffer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->external == FVIZ_TRUE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "an external buffer cannot be resized");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (new_size == buffer->size)
    {
        return FVIZ_OK;
    }
    if (new_size == 0u)
    {
        fviz_allocator_deallocate(&buffer->base.allocator, buffer->data, buffer->size, 0u);
        buffer->data = NULL;
        buffer->size = 0u;
        fviz_object_modified((FVizObject*)buffer);
        return FVIZ_OK;
    }

    memory = fviz_allocator_reallocate(&buffer->base.allocator, buffer->data, buffer->size, new_size, 0u);
    if (memory == NULL)
    {
        return fviz_last_error_code();
    }
    if (new_size > buffer->size)
    {
        (void)memset((unsigned char*)memory + buffer->size, 0, new_size - buffer->size);
    }
    buffer->data = (unsigned char*)memory;
    buffer->size = new_size;
    fviz_object_modified((FVizObject*)buffer);
    return FVIZ_OK;
}
