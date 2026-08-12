#include <FViz/FViz.h>
#include <FViz/Core/FVizAllocator.h>
#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizLog.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/System/FVizPlatform.h>

int main(void)
{
    FVizId id = FVIZ_INVALID_ID;
    FVizResult result = FVIZ_OK;
    FVizBool enabled = FVIZ_TRUE;
    FVizAllocator allocator = fviz_allocator_default();
    FVizObject* object = NULL;

    FVIZ_UNUSED(id);
    FVIZ_UNUSED(result);
    FVIZ_UNUSED(enabled);
    FVIZ_UNUSED(allocator);
    FVIZ_UNUSED(object);

    return 0;
}
