#ifndef FVIZ_INTERNAL_CORE_ARRAY_PRIVATE_H
#define FVIZ_INTERNAL_CORE_ARRAY_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizArray
{
    FVizObject base;
    unsigned char* data;
    FVizSize count;
    FVizSize capacity;
    FVizSize stride;
};

FVizResult fviz_internal_array_resize_untracked(FVizArray* array, FVizSize count);
FVizResult fviz_internal_array_append_uninitialized(
    FVizArray* array, FVizSize count, void** out_first_slot);
FVizResult fviz_internal_array_append(
    FVizArray* array, const void* values, FVizSize count);
void fviz_internal_array_clear(FVizArray* array);

#endif /* FVIZ_INTERNAL_CORE_ARRAY_PRIVATE_H */
