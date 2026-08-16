#ifndef FVIZ_INTERNAL_RENDERING_MAPPER_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_MAPPER_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizMapper.h>

struct FVizMapper
{
    FVizObject base;
    FVizAlgorithm* input_algorithm;
    uint32_t input_port;
    FVizPolyData* poly_data;
    FVizLookupTable* lookup_table;
    FVizBool lookup_table_initialized;
    FVizObserverTag input_algorithm_modified_tag;
    FVizObserverTag poly_data_modified_tag;
    FVizObserverTag lookup_table_modified_tag;
    FVizBool scalar_visibility;
    FVizBool scalar_range_valid;
    float scalar_min;
    float scalar_max;
    FVizDataAssociation association;
    FVizComponentMode component_mode;
    FVizScalarInterpolation scalar_interpolation;
    uint32_t component;
    char array_name[128];
    char opacity_array_name[128];
    FVizPlane clipping_planes[6];
    FVizClipPlaneId clipping_plane_ids[6];
    FVizClipPlaneId next_clipping_plane_id;
    FVizSize clipping_plane_count;
    FVizMTime render_data_mtime;
    FVizMTime color_data_mtime;
    FVizBool gpu_residency_pinned;
};

FVizMTime fviz_internal_mapper_render_data_mtime(const FVizMapper* mapper);
FVizMTime fviz_internal_mapper_color_data_mtime(const FVizMapper* mapper);

#endif /* FVIZ_INTERNAL_RENDERING_MAPPER_PRIVATE_H */
