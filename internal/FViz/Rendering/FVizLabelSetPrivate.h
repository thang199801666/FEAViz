#ifndef FVIZ_INTERNAL_RENDERING_LABEL_SET_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_LABEL_SET_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Rendering/FVizLabelSet.h>

typedef struct FVizLabelSet3DEntry
{
    FVizVec3 position;
    FVizString* text;
} FVizLabelSet3DEntry;

struct FVizLabelSet3D
{
    FVizObject base;
    FVizArray* entries;
    FVizTextProperty* property;
    float pixel_offset[2];
    FVizBool depth_test;
    FVizBool visible;
};

const FVizLabelSet3DEntry* fviz_internal_label_set_3d_entries(const FVizLabelSet3D* label_set);

#endif /* FVIZ_INTERNAL_RENDERING_LABEL_SET_PRIVATE_H */
