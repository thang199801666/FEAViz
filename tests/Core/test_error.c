#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

static int require_true(int condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "FAILED: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    FVizSize value = 0u;

    fviz_clear_last_error();
    if (!require_true(fviz_last_error_code() == FVIZ_OK, "clear did not reset error code")) return 1;
    if (!require_true(fviz_last_error_message()[0] == '\0', "clear did not reset error message")) return 1;

    if (!require_true(fviz_size_add(SIZE_MAX, 1u, &value) == FVIZ_ERROR_OVERFLOW, "overflow operation did not fail")) return 1;
    if (!require_true(fviz_last_error_code() == FVIZ_ERROR_OVERFLOW, "last error code mismatch")) return 1;
    if (!require_true(strstr(fviz_last_error_message(), "overflow") != NULL, "last error message lacks context")) return 1;

    if (!require_true(strcmp(fviz_result_string(FVIZ_OK), "success") == 0, "success result string mismatch")) return 1;
    if (!require_true(strcmp(fviz_result_string(FVIZ_ERROR_OUT_OF_MEMORY), "out of memory") == 0, "OOM result string mismatch")) return 1;
    if (!require_true(strcmp(fviz_result_string((FVizResult)999), "unknown result") == 0, "unknown result string mismatch")) return 1;

    fviz_clear_last_error();
    return 0;
}
