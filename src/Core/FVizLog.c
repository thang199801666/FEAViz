#include <stdio.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizLog.h>

#include <FViz/Core/FVizAtomic.h>
#include <FViz/Core/FVizErrorInternal.h>

typedef struct FVizLogState
{
    FVizAtomicU32 minimum_level;
    FVizSpinLock callback_lock;
    FVizLogCallback callback;
    void* user_data;
} FVizLogState;

static FVizLogState g_fviz_log_state = {{FVIZ_LOG_INFO}, {0u}, NULL, NULL};

static void fviz_default_log_callback(FVizLogLevel level, const char* category, const char* message, void* user_data)
{
    FVIZ_UNUSED(user_data);

    if (category != NULL && category[0] != '\0')
    {
        (void)fprintf(stderr, "[FEAViz][%s][%s] %s\n", fviz_log_level_string(level), category, message);
    }
    else
    {
        (void)fprintf(stderr, "[FEAViz][%s] %s\n", fviz_log_level_string(level), message);
    }
}

const char* fviz_log_level_string(FVizLogLevel level)
{
    switch (level)
    {
        case FVIZ_LOG_TRACE:
            return "TRACE";
        case FVIZ_LOG_DEBUG:
            return "DEBUG";
        case FVIZ_LOG_INFO:
            return "INFO";
        case FVIZ_LOG_WARNING:
            return "WARNING";
        case FVIZ_LOG_ERROR:
            return "ERROR";
        case FVIZ_LOG_FATAL:
            return "FATAL";
        case FVIZ_LOG_OFF:
            return "OFF";
        default:
            return "UNKNOWN";
    }
}

FVizResult fviz_log_set_level(FVizLogLevel level)
{
    uint32_t expected;

    if (level < FVIZ_LOG_TRACE || level > FVIZ_LOG_OFF)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid log level");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }

    expected = fviz_atomic_u32_load(&g_fviz_log_state.minimum_level);
    while (!fviz_atomic_u32_compare_exchange(&g_fviz_log_state.minimum_level, &expected, (uint32_t)level))
    {
    }

    return FVIZ_OK;
}

FVizLogLevel fviz_log_get_level(void)
{
    return (FVizLogLevel)fviz_atomic_u32_load(&g_fviz_log_state.minimum_level);
}

void fviz_log_set_callback(FVizLogCallback callback, void* user_data)
{
    fviz_spin_lock(&g_fviz_log_state.callback_lock);
    g_fviz_log_state.callback = callback;
    g_fviz_log_state.user_data = user_data;
    fviz_spin_unlock(&g_fviz_log_state.callback_lock);
}

void fviz_log_reset_callback(void)
{
    fviz_log_set_callback(NULL, NULL);
}

void fviz_log_message(FVizLogLevel level, const char* category, const char* message)
{
    FVizLogCallback callback;
    void* user_data;
    FVizLogLevel minimum_level;

    if (level < FVIZ_LOG_TRACE || level >= FVIZ_LOG_OFF)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid log message level");
        return;
    }

    if (message == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "log message must not be NULL");
        return;
    }

    minimum_level = fviz_log_get_level();
    if (minimum_level == FVIZ_LOG_OFF || level < minimum_level)
    {
        return;
    }

    fviz_spin_lock(&g_fviz_log_state.callback_lock);
    callback = g_fviz_log_state.callback;
    user_data = g_fviz_log_state.user_data;
    fviz_spin_unlock(&g_fviz_log_state.callback_lock);

    if (callback == NULL)
    {
        callback = fviz_default_log_callback;
    }

    callback(level, category, message, user_data);
}
