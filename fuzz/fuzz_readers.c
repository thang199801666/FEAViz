#define _XOPEN_SOURCE 700

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <FViz/FViz.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    char path[] = "/tmp/fviz_reader_fuzz_XXXXXX";
    FVizUnstructuredGrid* grid = NULL;
    size_t written = 0u;
    int descriptor;
    if (data == NULL || size == 0u || size > 16u * 1024u * 1024u) return 0;
    descriptor = mkstemp(path);
    if (descriptor < 0) return 0;
    while (written < size)
    {
        const ssize_t count = write(descriptor, data + written, size - written);
        if (count <= 0) break;
        written += (size_t)count;
    }
    (void)close(descriptor);
    if ((data[0] & 1u) == 0u)
        (void)fviz_vtu_read(path, &grid);
    else
        (void)fviz_vtk_legacy_read(path, &grid);
    fviz_release(grid);
    (void)unlink(path);
    return 0;
}
