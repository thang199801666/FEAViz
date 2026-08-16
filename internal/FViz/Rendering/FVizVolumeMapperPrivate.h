#ifndef FVIZ_INTERNAL_RENDERING_VOLUME_MAPPER_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_VOLUME_MAPPER_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizVolumeMapper.h>

struct FVizVolumeMapper
{
    FVizObject base;
    FVizImageData* image;
    FVizVolumeColorPoint* color_points;
    FVizSize color_count;
    FVizSize color_capacity;
    FVizVolumeOpacityPoint* opacity_points;
    FVizSize opacity_count;
    FVizSize opacity_capacity;
    float sampling_step;
    FVizBool shading;
    FVizBool automatic_scalar_range;
    float scalar_range_minimum;
    float scalar_range_maximum;
    FVizBool scalar_range_valid;
    float contrast;
    FVizBool shade_ambient;
    FVizBool shade_diffuse;
    FVizBool shade_specular;
    float specular_power;
};

#endif /* FVIZ_INTERNAL_RENDERING_VOLUME_MAPPER_PRIVATE_H */
