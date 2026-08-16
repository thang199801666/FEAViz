#ifndef FVIZ_RENDERING_VOLUME_MAPPER_H
#define FVIZ_RENDERING_VOLUME_MAPPER_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizImageData.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizVolumeMapper FVizVolumeMapper;
#define FVIZ_TYPE_VOLUME_MAPPER UINT64_C(0x3A1B9E5F7D42C860)

typedef struct FVizVolumeColorPoint
{
    float scalar;
    float red;
    float green;
    float blue;
} FVizVolumeColorPoint;

typedef struct FVizVolumeOpacityPoint
{
    float scalar;
    float opacity;
} FVizVolumeOpacityPoint;

typedef struct FVizVolumeRenderOptions
{
    uint32_t struct_size;
    /* Physical world-space distance between ray samples. */
    float sampling_step;
    /* 0.0 disables gradient lighting, 1.0 enables it. */
    float shading;
    FVizBool use_automatic_scalar_range;
    float scalar_range_minimum;
    float scalar_range_maximum;
    /* Windowed transfer function contrast, applied to the normalized scalar. */
    float contrast;
    FVizBool shade_ambient;
    FVizBool shade_diffuse;
    FVizBool shade_specular;
    float specular_power;
} FVizVolumeRenderOptions;

FVIZ_API void fviz_volume_render_options_initialize(FVizVolumeRenderOptions* options);
FVIZ_API FVizResult fviz_volume_mapper_create(FVizVolumeMapper** out_mapper);
FVIZ_API void fviz_volume_mapper_destroy(FVizVolumeMapper* mapper);
FVIZ_API FVizResult fviz_volume_mapper_set_image_data(FVizVolumeMapper* mapper, FVizImageData* image);
FVIZ_API FVizImageData* fviz_volume_mapper_image_data(FVizVolumeMapper* mapper);
FVIZ_API const FVizImageData* fviz_volume_mapper_const_image_data(const FVizVolumeMapper* mapper);
FVIZ_API FVizBounds fviz_volume_mapper_bounds(const FVizVolumeMapper* mapper);
FVIZ_API FVizBool fviz_volume_mapper_is_empty(const FVizVolumeMapper* mapper);

FVIZ_API FVizResult fviz_volume_mapper_add_color_point(FVizVolumeMapper* mapper, float scalar, float red, float green,
                                                       float blue);
FVIZ_API void fviz_volume_mapper_clear_color_points(FVizVolumeMapper* mapper);
FVIZ_API FVizSize fviz_volume_mapper_color_point_count(const FVizVolumeMapper* mapper);
FVIZ_API FVizResult fviz_volume_mapper_color_point_at(const FVizVolumeMapper* mapper, FVizSize index,
                                                      FVizVolumeColorPoint* out_point);

FVIZ_API FVizResult fviz_volume_mapper_add_opacity_point(FVizVolumeMapper* mapper, float scalar, float opacity);
FVIZ_API void fviz_volume_mapper_clear_opacity_points(FVizVolumeMapper* mapper);
FVIZ_API FVizSize fviz_volume_mapper_opacity_point_count(const FVizVolumeMapper* mapper);
FVIZ_API FVizResult fviz_volume_mapper_opacity_point_at(const FVizVolumeMapper* mapper, FVizSize index,
                                                        FVizVolumeOpacityPoint* out_point);

FVIZ_API void fviz_volume_mapper_set_sampling_step(FVizVolumeMapper* mapper, float step);
FVIZ_API float fviz_volume_mapper_sampling_step(const FVizVolumeMapper* mapper);
FVIZ_API void fviz_volume_mapper_set_shading(FVizVolumeMapper* mapper, FVizBool enabled);
FVIZ_API FVizBool fviz_volume_mapper_shading(const FVizVolumeMapper* mapper);
FVIZ_API void fviz_volume_mapper_set_scalar_range(FVizVolumeMapper* mapper, float minimum, float maximum);
FVIZ_API void fviz_volume_mapper_get_scalar_range(const FVizVolumeMapper* mapper, float* minimum, float* maximum);
FVIZ_API void fviz_volume_mapper_use_automatic_scalar_range(FVizVolumeMapper* mapper);
FVIZ_API FVizBool fviz_volume_mapper_scalar_range_valid(const FVizVolumeMapper* mapper);
FVIZ_API void fviz_volume_mapper_set_options(FVizVolumeMapper* mapper, const FVizVolumeRenderOptions* options);
FVIZ_API void fviz_volume_mapper_options(const FVizVolumeMapper* mapper, FVizVolumeRenderOptions* out_options);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_VOLUME_MAPPER_H */
