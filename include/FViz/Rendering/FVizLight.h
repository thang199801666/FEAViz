#ifndef FVIZ_RENDERING_LIGHT_H
#define FVIZ_RENDERING_LIGHT_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizLight FVizLight;
#define FVIZ_TYPE_LIGHT UINT64_C(0xB7C5918F24A60D3E)

typedef enum FVizLightType
{
    FVIZ_LIGHT_HEADLIGHT = 0,
    FVIZ_LIGHT_SCENE = 1
} FVizLightType;

FVIZ_API FVizResult fviz_light_create(FVizLight** out_light);
FVIZ_API void fviz_light_set_type(FVizLight* light, FVizLightType type);
FVIZ_API FVizLightType fviz_light_type(const FVizLight* light);
FVIZ_API void fviz_light_set_enabled(FVizLight* light, FVizBool enabled);
FVIZ_API FVizBool fviz_light_enabled(const FVizLight* light);
FVIZ_API void fviz_light_set_position(FVizLight* light, FVizVec3 position);
FVIZ_API FVizVec3 fviz_light_position(const FVizLight* light);
FVIZ_API void fviz_light_set_color(FVizLight* light, float red, float green, float blue);
FVIZ_API void fviz_light_get_color(const FVizLight* light, float* red, float* green, float* blue);
FVIZ_API void fviz_light_set_intensity(FVizLight* light, float intensity);
FVIZ_API float fviz_light_intensity(const FVizLight* light);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_LIGHT_H */
