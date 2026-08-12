#include <FViz/FViz.h>

FVizVersionInfo fviz_version(void)
{
    FVizVersionInfo version;
    version.major = FVIZ_VERSION_MAJOR;
    version.minor = FVIZ_VERSION_MINOR;
    version.patch = FVIZ_VERSION_PATCH;
    version.abi = FVIZ_ABI_VERSION;
    return version;
}

const char* fviz_version_string(void)
{
    return FVIZ_VERSION_STRING;
}

uint32_t fviz_abi_version(void)
{
    return FVIZ_ABI_VERSION;
}
