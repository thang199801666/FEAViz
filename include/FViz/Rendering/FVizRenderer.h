#ifndef FVIZ_RENDERING_RENDERER_H
#define FVIZ_RENDERING_RENDERER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Rendering/FVizCamera.h>
#include <FViz/Rendering/FVizScalarLegend.h>
#include <FViz/Rendering/FVizScene.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRenderer FVizRenderer;
#define FVIZ_TYPE_RENDERER UINT64_C(0xB5EBD4F126C68F53)

FVIZ_API FVizResult fviz_renderer_create(FVizRenderer** out_renderer);
FVIZ_API FVizResult fviz_renderer_set_scene(FVizRenderer* renderer, FVizScene* scene);
FVIZ_API FVizScene* fviz_renderer_scene(FVizRenderer* renderer);
FVIZ_API FVizCamera* fviz_renderer_camera(FVizRenderer* renderer);
FVIZ_API void fviz_renderer_set_background(FVizRenderer* renderer, float red, float green, float blue);
FVIZ_API void fviz_renderer_get_background(const FVizRenderer* renderer, float* red, float* green, float* blue);
FVIZ_API void fviz_renderer_fit_camera(FVizRenderer* renderer, float padding);
FVIZ_API void fviz_renderer_set_scalar_legend(FVizRenderer* renderer, FVizScalarLegend* legend);
FVIZ_API FVizScalarLegend* fviz_renderer_scalar_legend(FVizRenderer* renderer);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_RENDERER_H */
