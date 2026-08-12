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
    const FVizVersionInfo version = fviz_version();

    if (!require_true(version.major == FVIZ_VERSION_MAJOR, "major version mismatch")) return 1;
    if (!require_true(version.minor == FVIZ_VERSION_MINOR, "minor version mismatch")) return 1;
    if (!require_true(version.patch == FVIZ_VERSION_PATCH, "patch version mismatch")) return 1;
    if (!require_true(version.abi == FVIZ_ABI_VERSION, "ABI version mismatch")) return 1;
    if (!require_true(fviz_abi_version() == FVIZ_ABI_VERSION, "runtime ABI version mismatch")) return 1;
    if (!require_true(strcmp(fviz_version_string(), FVIZ_VERSION_STRING) == 0, "version string mismatch")) return 1;

    printf("FEAViz %s ABI %u\n", fviz_version_string(), (unsigned)fviz_abi_version());
    return 0;
}
