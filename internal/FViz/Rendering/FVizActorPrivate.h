#ifndef FVIZ_INTERNAL_RENDERING_ACTOR_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_ACTOR_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Math/FVizQuat.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Rendering/FVizActor.h>

struct FVizActor
{
    FVizObject base;
    FVizMapper* mapper;
    FVizGlyphMapper* glyph_mapper;
    FVizVolumeMapper* volume_mapper;
    FVizObserverTag mapper_modified_tag;
    FVizObserverTag glyph_mapper_modified_tag;
    FVizObserverTag volume_mapper_modified_tag;
    FVizObserverTag user_transform_modified_tag;
    float color[3];
    FVizBool visible;
    FVizBool pickable;
    FVizBool wireframe;
    FVizBool edge_visible;
    float opacity;
    float edge_color[3];
    float line_width;
    float line_depth_bias;
    FVizLineCap line_cap;
    FVizLineJoin line_join;
    float line_miter_limit;
    float line_dash_length;
    float line_gap_length;
    float line_dash_phase;
    FVizBool line_scalar_coloring;
    FVizBool point_visible;
    float point_size;
    FVizPointShape point_shape;
    float point_color[3];
    FVizBool point_scalar_coloring;
    float ambient;
    float diffuse;
    float specular;
    float specular_power;
    FVizShadingMode shading_mode;
    FVizCullMode cull_mode;
    FVizVec3 position;
    FVizQuat orientation;
    FVizVec3 scale;
    FVizTransform* user_transform;
    FVizBounds world_bounds_cache;
    FVizMTime world_bounds_geometry_mtime;
    FVizMTime world_bounds_user_transform_mtime;
    uint64_t transform_revision;
    uint64_t world_bounds_transform_revision;
    FVizBool world_bounds_cache_initialized;
};

#endif /* FVIZ_INTERNAL_RENDERING_ACTOR_PRIVATE_H */
