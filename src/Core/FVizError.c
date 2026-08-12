#include <string.h>

#include <FViz/Core/FVizError.h>

#include <FViz/Core/FVizCompiler.h>
#include <FViz/Core/FVizErrorInternal.h>

#define FVIZ_ERROR_MESSAGE_CAPACITY 512u

typedef struct FVizThreadErrorState
{
    FVizResult code;
    char message[FVIZ_ERROR_MESSAGE_CAPACITY];
} FVizThreadErrorState;

static FVIZ_THREAD_LOCAL FVizThreadErrorState g_fviz_error_state = { FVIZ_OK, { 0 } };

void fviz_internal_set_error(FVizResult result, const char* message)
{
    g_fviz_error_state.code = result;

    if (message == NULL)
    {
        g_fviz_error_state.message[0] = '\0';
        return;
    }

#if defined(_MSC_VER)
    (void)strncpy_s(
        g_fviz_error_state.message,
        FVIZ_ERROR_MESSAGE_CAPACITY,
        message,
        _TRUNCATE);
#else
    (void)strncpy(
        g_fviz_error_state.message,
        message,
        FVIZ_ERROR_MESSAGE_CAPACITY - 1u);
    g_fviz_error_state.message[FVIZ_ERROR_MESSAGE_CAPACITY - 1u] = '\0';
#endif
}

const char* fviz_result_string(FVizResult result)
{
    switch (result)
    {
        case FVIZ_OK: return "success";
        case FVIZ_ERROR_INVALID_ARGUMENT: return "invalid argument";
        case FVIZ_ERROR_OUT_OF_MEMORY: return "out of memory";
        case FVIZ_ERROR_NOT_SUPPORTED: return "not supported";
        case FVIZ_ERROR_IO: return "I/O error";
        case FVIZ_ERROR_INTERNAL: return "internal error";
        case FVIZ_ERROR_OVERFLOW: return "numeric overflow";
        case FVIZ_ERROR_INVALID_STATE: return "invalid state";
        case FVIZ_ERROR_NOT_FOUND: return "not found";
        case FVIZ_ERROR_BUSY: return "resource busy";
        case FVIZ_ERROR_PARSE: return "parse error";
        case FVIZ_ERROR_GRAPHICS: return "graphics error";
        default: return "unknown result";
    }
}

FVizResult fviz_last_error_code(void)
{
    return g_fviz_error_state.code;
}

const char* fviz_last_error_message(void)
{
    return g_fviz_error_state.message;
}

void fviz_clear_last_error(void)
{
    g_fviz_error_state.code = FVIZ_OK;
    g_fviz_error_state.message[0] = '\0';
}
