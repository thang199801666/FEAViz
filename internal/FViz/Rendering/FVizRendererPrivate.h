#ifndef FVIZ_INTERNAL_RENDERING_RENDERER_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_RENDERER_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizArray.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizScalarLegend.h>

typedef struct FVizRendererChildObserver
{
    FVizObject* object;
    FVizArray* owner_array;
    FVizObserverTag tag;
} FVizRendererChildObserver;

struct FVizRenderer
{
    FVizObject base;
    FVizScene* scene;
    FVizCamera* camera;
    FVizObserverTag scene_modified_tag;
    FVizObserverTag camera_modified_tag;
    FVizScalarLegend* scalar_legend;
    FVizArray* passes;
    FVizRenderGraph* render_graph;
    FVizBool render_graph_dirty;
    FVizArray* lights;
    FVizArray* text_actors_2d;
    FVizArray* billboard_text_actors_3d;
    FVizArray* label_sets_3d;
    FVizArray* child_dependency_observers;
    float background[3];
    float background2[3];
    FVizBool gradient_background;
    FVizTransparencyMode transparency_mode;
    FVizWeightedOITOptions weighted_oit_options;
    float viewport[4];
    int layer;
    FVizBool interactive;
    FVizBool frustum_culling;
    FVizBool small_object_culling;
    float small_object_threshold_pixels;
    FVizFrustum frustum_cache;
    FVizMTime frustum_camera_mtime;
    float frustum_aspect_ratio;
    FVizBool frustum_cache_valid;
};

#endif /* FVIZ_INTERNAL_RENDERING_RENDERER_PRIVATE_H */
