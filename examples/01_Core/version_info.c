#include <stdio.h>

#include <FViz/FViz.h>

int main(void)
{
    const FVizVersionInfo version = fviz_version();

    printf("FEAViz version : %s\n", fviz_version_string());
    printf("ABI version    : %u\n", (unsigned)version.abi);
    printf("Platform       : %s\n", FVIZ_CONFIG_PLATFORM);
    printf("Shared build   : %s\n", FVIZ_CONFIG_SHARED ? "yes" : "no");

    return 0;
}
