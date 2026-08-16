#ifndef FVIZ_CORE_ERROR_H
#define FVIZ_CORE_ERROR_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>

FVIZ_EXTERN_C_BEGIN

FVIZ_CORE_API const char* fviz_result_string(FVizResult result);
FVIZ_CORE_API FVizResult fviz_last_error_code(void);
FVIZ_CORE_API const char* fviz_last_error_message(void);
FVIZ_CORE_API void fviz_clear_last_error(void);

FVIZ_EXTERN_C_END

#endif /* FVIZ_CORE_ERROR_H */
