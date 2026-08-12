#ifndef FVIZ_CORE_LOG_H
#define FVIZ_CORE_LOG_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizLogLevel
{
    FVIZ_LOG_TRACE = 0,
    FVIZ_LOG_DEBUG = 1,
    FVIZ_LOG_INFO = 2,
    FVIZ_LOG_WARNING = 3,
    FVIZ_LOG_ERROR = 4,
    FVIZ_LOG_FATAL = 5,
    FVIZ_LOG_OFF = 6
} FVizLogLevel;

typedef void (*FVizLogCallback)(
    FVizLogLevel level,
    const char* category,
    const char* message,
    void* user_data);

FVIZ_API const char* fviz_log_level_string(FVizLogLevel level);
FVIZ_API FVizResult fviz_log_set_level(FVizLogLevel level);
FVIZ_API FVizLogLevel fviz_log_get_level(void);
FVIZ_API void fviz_log_set_callback(FVizLogCallback callback, void* user_data);
FVIZ_API void fviz_log_reset_callback(void);
FVIZ_API void fviz_log_message(FVizLogLevel level, const char* category, const char* message);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_LOG_H */
