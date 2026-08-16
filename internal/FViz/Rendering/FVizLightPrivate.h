#ifndef FVIZ_INTERNAL_RENDERING_LIGHT_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_LIGHT_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizLight.h>

struct FVizLight
{
    FVizObject base;
    FVizLightType type;
    FVizBool enabled;
    FVizVec3 position;
    float color[3];
    float intensity;
};

#endif /* FVIZ_INTERNAL_RENDERING_LIGHT_PRIVATE_H */
