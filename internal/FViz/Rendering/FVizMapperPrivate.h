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
    FVizSize clipping_plane_count;
};

#endif /* FVIZ_INTERNAL_RENDERING_MAPPER_PRIVATE_H */
