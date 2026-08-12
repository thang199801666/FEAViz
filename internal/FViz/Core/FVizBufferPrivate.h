#ifndef FVIZ_INTERNAL_CORE_BUFFER_PRIVATE_H
#define FVIZ_INTERNAL_CORE_BUFFER_PRIVATE_H

#include <FViz/Core/FVizBuffer.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizBuffer
{
    FVizObject base;
    unsigned char* data;
    FVizSize size;
    FVizBufferReleaseFn release_fn;
    void* release_user_data;
    FVizBool external;
};

#endif /* FVIZ_INTERNAL_CORE_BUFFER_PRIVATE_H */
