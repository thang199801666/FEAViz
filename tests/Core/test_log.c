#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

typedef struct LogCapture
{
    unsigned int count;
    FVizLogLevel last_level;
    char category[64];
    char message[128];
} LogCapture;

static int require_true(int condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "FAILED: %s\n", message);
        return 0;
    }
    return 1;
}

static void capture_log(
    FVizLogLevel level,
    const char* category,
    const char* message,
    void* user_data)
{
    LogCapture* capture = (LogCapture*)user_data;
    capture->count += 1u;
    capture->last_level = level;

#if defined(_MSC_VER)
    (void)strncpy_s(capture->category, sizeof(capture->category), category != NULL ? category : "", _TRUNCATE);
    (void)strncpy_s(capture->message, sizeof(capture->message), message, _TRUNCATE);
#else
    (void)snprintf(capture->category, sizeof(capture->category), "%s", category != NULL ? category : "");
    (void)snprintf(capture->message, sizeof(capture->message), "%s", message);
#endif
}

int main(void)
{
    LogCapture capture = { 0u, FVIZ_LOG_TRACE, { 0 }, { 0 } };

    fviz_log_set_callback(capture_log, &capture);
    if (!require_true(fviz_log_set_level(FVIZ_LOG_WARNING) == FVIZ_OK, "setting log level failed")) return 1;
    if (!require_true(fviz_log_get_level() == FVIZ_LOG_WARNING, "log level query mismatch")) return 1;

    fviz_log_message(FVIZ_LOG_INFO, "Core", "filtered");
    if (!require_true(capture.count == 0u, "log filter did not suppress INFO")) return 1;

    fviz_log_message(FVIZ_LOG_ERROR, "Core", "captured error");
    if (!require_true(capture.count == 1u, "log callback was not invoked")) return 1;
    if (!require_true(capture.last_level == FVIZ_LOG_ERROR, "captured log level mismatch")) return 1;
    if (!require_true(strcmp(capture.category, "Core") == 0, "captured category mismatch")) return 1;
    if (!require_true(strcmp(capture.message, "captured error") == 0, "captured message mismatch")) return 1;
    if (!require_true(strcmp(fviz_log_level_string(FVIZ_LOG_FATAL), "FATAL") == 0, "log level string mismatch")) return 1;

    fviz_log_reset_callback();
    if (!require_true(fviz_log_set_level(FVIZ_LOG_INFO) == FVIZ_OK, "restoring log level failed")) return 1;

    return 0;
}
