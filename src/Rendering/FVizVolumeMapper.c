#include <math.h>
#include <stddef.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Rendering/FVizVolumeMapper.h>
#include <FViz/Rendering/FVizVolumeMapperPrivate.h>

static void fviz_volume_mapper_object_destroy(FVizObject* object);
static FVizMTime fviz_volume_mapper_mtime(const FVizObject* object);

static const FVizObjectClass g_fviz_volume_mapper_class = {FVIZ_TYPE_VOLUME_MAPPER, "FVizVolumeMapper",
                                                           &g_fviz_object_class, fviz_volume_mapper_object_destroy,
                                                           fviz_volume_mapper_mtime};

static FVizMTime fviz_volume_mapper_mtime(const FVizObject* object)
{
    const FVizVolumeMapper* mapper = (const FVizVolumeMapper*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    FVizMTime child = fviz_object_mtime((const FVizObject*)mapper->image);
    if (child > mtime) mtime = child;
    return mtime;
}

static void fviz_volume_mapper_object_destroy(FVizObject* object)
{
    FVizVolumeMapper* mapper = (FVizVolumeMapper*)object;
    fviz_free(mapper->color_points);
    fviz_free(mapper->opacity_points);
    fviz_release(mapper->image);
    mapper->color_points = NULL;
    mapper->opacity_points = NULL;
    mapper->image = NULL;
}

static FVizResult fviz_volume_mapper_reserve_color_points(FVizVolumeMapper* mapper, FVizSize capacity)
{
    FVizVolumeColorPoint* points;
    if (capacity <= mapper->color_capacity) return FVIZ_OK;
    points = (FVizVolumeColorPoint*)fviz_realloc(mapper->color_points, capacity * (FVizSize)sizeof(*points));
    if (points == NULL) return fviz_last_error_code();
    mapper->color_points = points;
    mapper->color_capacity = capacity;
    return FVIZ_OK;
}

static FVizResult fviz_volume_mapper_reserve_opacity_points(FVizVolumeMapper* mapper, FVizSize capacity)
{
    FVizVolumeOpacityPoint* points;
    if (capacity <= mapper->opacity_capacity) return FVIZ_OK;
    points = (FVizVolumeOpacityPoint*)fviz_realloc(mapper->opacity_points, capacity * (FVizSize)sizeof(*points));
    if (points == NULL) return fviz_last_error_code();
    mapper->opacity_points = points;
    mapper->opacity_capacity = capacity;
    return FVIZ_OK;
}

void fviz_volume_render_options_initialize(FVizVolumeRenderOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->sampling_step = 0.0f;
    options->shading = 0.0f;
    options->use_automatic_scalar_range = FVIZ_TRUE;
    options->contrast = 1.0f;
    options->shade_ambient = FVIZ_TRUE;
    options->shade_diffuse = FVIZ_TRUE;
    options->shade_specular = FVIZ_TRUE;
    options->specular_power = 40.0f;
}

FVizResult fviz_volume_mapper_create(FVizVolumeMapper** out_mapper)
{
    FVizVolumeMapper* mapper;
    if (out_mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_mapper = NULL;
    mapper = (FVizVolumeMapper*)fviz_internal_object_allocate(sizeof(*mapper), &g_fviz_volume_mapper_class, NULL);
    if (mapper == NULL) return fviz_last_error_code();
    mapper->automatic_scalar_range = FVIZ_TRUE;
    mapper->contrast = 1.0f;
    mapper->shade_ambient = FVIZ_TRUE;
    mapper->shade_diffuse = FVIZ_TRUE;
    mapper->shade_specular = FVIZ_TRUE;
    mapper->specular_power = 40.0f;
    *out_mapper = mapper;
    return FVIZ_OK;
}

void fviz_volume_mapper_destroy(FVizVolumeMapper* mapper)
{
    if (mapper != NULL) fviz_release(mapper);
}

FVizResult fviz_volume_mapper_set_image_data(FVizVolumeMapper* mapper, FVizImageData* image)
{
    if (mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "volume mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (image != NULL && fviz_retain(image) == NULL) return fviz_last_error_code();
    fviz_release(mapper->image);
    mapper->image = image;
    if (image != NULL)
    {
        const FVizAttributeSet* point_data = fviz_image_data_const_point_data(image);
        if (point_data != NULL)
        {
            const FVizDataArray* active = fviz_attribute_set_const_active(point_data, FVIZ_ATTRIBUTE_SCALARS);
            if (active != NULL)
            {
                double minimum = 0.0;
                double maximum = 0.0;
                (void)fviz_data_array_get_range(active, 0, FVIZ_FALSE, &minimum, &maximum);
                if (minimum <= maximum)
                {
                    mapper->scalar_range_minimum = (float)minimum;
                    mapper->scalar_range_maximum = (float)maximum;
                    mapper->scalar_range_valid = FVIZ_TRUE;
                }
            }
        }
    }
    fviz_object_modified((FVizObject*)mapper);
    return FVIZ_OK;
}

FVizImageData* fviz_volume_mapper_image_data(FVizVolumeMapper* mapper)
{
    return mapper != NULL ? mapper->image : NULL;
}

const FVizImageData* fviz_volume_mapper_const_image_data(const FVizVolumeMapper* mapper)
{
    return mapper != NULL ? mapper->image : NULL;
}

FVizBounds fviz_volume_mapper_bounds(const FVizVolumeMapper* mapper)
{
    if (mapper != NULL && mapper->image != NULL) return fviz_image_data_bounds(mapper->image);
    return fviz_bounds_empty();
}

FVizBool fviz_volume_mapper_is_empty(const FVizVolumeMapper* mapper)
{
    const FVizAttributeSet* point_data;
    if (mapper == NULL || mapper->image == NULL) return FVIZ_TRUE;
    point_data = fviz_image_data_const_point_data(mapper->image);
    return point_data == NULL || fviz_attribute_set_const_active(point_data, FVIZ_ATTRIBUTE_SCALARS) == NULL;
}

FVizResult fviz_volume_mapper_add_color_point(FVizVolumeMapper* mapper, float scalar, float red, float green,
                                              float blue)
{
    FVizSize position;
    FVizSize index;
    FVizSize required;
    if (mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "volume mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (red < 0.0f) red = 0.0f;
    if (green < 0.0f) green = 0.0f;
    if (blue < 0.0f) blue = 0.0f;
    if (red > 1.0f) red = 1.0f;
    if (green > 1.0f) green = 1.0f;
    if (blue > 1.0f) blue = 1.0f;
    position = 0u;
    while (position < mapper->color_count && mapper->color_points[position].scalar < scalar)
        ++position;
    required = mapper->color_count + 1u;
    if (fviz_volume_mapper_reserve_color_points(mapper, required) != FVIZ_OK) return fviz_last_error_code();
    for (index = mapper->color_count; index > position; --index)
        mapper->color_points[index] = mapper->color_points[index - 1u];
    mapper->color_points[position].scalar = scalar;
    mapper->color_points[position].red = red;
    mapper->color_points[position].green = green;
    mapper->color_points[position].blue = blue;
    ++mapper->color_count;
    fviz_object_modified((FVizObject*)mapper);
    return FVIZ_OK;
}

void fviz_volume_mapper_clear_color_points(FVizVolumeMapper* mapper)
{
    if (mapper == NULL) return;
    mapper->color_count = 0u;
    fviz_object_modified((FVizObject*)mapper);
}

FVizSize fviz_volume_mapper_color_point_count(const FVizVolumeMapper* mapper)
{
    return mapper != NULL ? mapper->color_count : 0u;
}

FVizResult fviz_volume_mapper_color_point_at(const FVizVolumeMapper* mapper, FVizSize index,
                                             FVizVolumeColorPoint* out_point)
{
    if (mapper == NULL || out_point == NULL) return fviz_last_error_code();
    if (index >= mapper->color_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "color point index out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_point = mapper->color_points[index];
    return FVIZ_OK;
}

FVizResult fviz_volume_mapper_add_opacity_point(FVizVolumeMapper* mapper, float scalar, float opacity)
{
    FVizSize position;
    FVizSize index;
    FVizSize required;
    if (mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "volume mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    position = 0u;
    while (position < mapper->opacity_count && mapper->opacity_points[position].scalar < scalar)
        ++position;
    required = mapper->opacity_count + 1u;
    if (fviz_volume_mapper_reserve_opacity_points(mapper, required) != FVIZ_OK) return fviz_last_error_code();
    for (index = mapper->opacity_count; index > position; --index)
        mapper->opacity_points[index] = mapper->opacity_points[index - 1u];
    mapper->opacity_points[position].scalar = scalar;
    mapper->opacity_points[position].opacity = opacity;
    ++mapper->opacity_count;
    fviz_object_modified((FVizObject*)mapper);
    return FVIZ_OK;
}

void fviz_volume_mapper_clear_opacity_points(FVizVolumeMapper* mapper)
{
    if (mapper == NULL) return;
    mapper->opacity_count = 0u;
    fviz_object_modified((FVizObject*)mapper);
}

FVizSize fviz_volume_mapper_opacity_point_count(const FVizVolumeMapper* mapper)
{
    return mapper != NULL ? mapper->opacity_count : 0u;
}

FVizResult fviz_volume_mapper_opacity_point_at(const FVizVolumeMapper* mapper, FVizSize index,
                                               FVizVolumeOpacityPoint* out_point)
{
    if (mapper == NULL || out_point == NULL) return fviz_last_error_code();
    if (index >= mapper->opacity_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "opacity point index out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_point = mapper->opacity_points[index];
    return FVIZ_OK;
}

void fviz_volume_mapper_set_sampling_step(FVizVolumeMapper* mapper, float step)
{
    if (mapper == NULL) return;
    mapper->sampling_step = step > 0.0f ? step : 0.0f;
    fviz_object_modified((FVizObject*)mapper);
}

float fviz_volume_mapper_sampling_step(const FVizVolumeMapper* mapper)
{
    return mapper != NULL ? mapper->sampling_step : 0.0f;
}

void fviz_volume_mapper_set_shading(FVizVolumeMapper* mapper, FVizBool enabled)
{
    if (mapper == NULL) return;
    mapper->shading = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_object_modified((FVizObject*)mapper);
}

FVizBool fviz_volume_mapper_shading(const FVizVolumeMapper* mapper)
{
    return mapper != NULL && mapper->shading != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_volume_mapper_set_scalar_range(FVizVolumeMapper* mapper, float minimum, float maximum)
{
    if (mapper == NULL) return;
    mapper->scalar_range_minimum = minimum;
    mapper->scalar_range_maximum = maximum;
    mapper->scalar_range_valid = FVIZ_TRUE;
    mapper->automatic_scalar_range = FVIZ_FALSE;
    fviz_object_modified((FVizObject*)mapper);
}

void fviz_volume_mapper_get_scalar_range(const FVizVolumeMapper* mapper, float* minimum, float* maximum)
{
    if (mapper == NULL) return;
    if (minimum != NULL) *minimum = mapper->scalar_range_minimum;
    if (maximum != NULL) *maximum = mapper->scalar_range_maximum;
}

void fviz_volume_mapper_use_automatic_scalar_range(FVizVolumeMapper* mapper)
{
    if (mapper == NULL) return;
    mapper->automatic_scalar_range = FVIZ_TRUE;
    if (mapper->image != NULL)
    {
        const FVizAttributeSet* point_data = fviz_image_data_const_point_data(mapper->image);
        if (point_data != NULL)
        {
            const FVizDataArray* active = fviz_attribute_set_const_active(point_data, FVIZ_ATTRIBUTE_SCALARS);
            if (active != NULL)
            {
                double minimum = 0.0;
                double maximum = 0.0;
                if (fviz_data_array_get_range(active, 0, FVIZ_FALSE, &minimum, &maximum) == FVIZ_OK &&
                    minimum <= maximum)
                {
                    mapper->scalar_range_minimum = (float)minimum;
                    mapper->scalar_range_maximum = (float)maximum;
                    mapper->scalar_range_valid = FVIZ_TRUE;
                }
            }
        }
    }
    fviz_object_modified((FVizObject*)mapper);
}

FVizBool fviz_volume_mapper_scalar_range_valid(const FVizVolumeMapper* mapper)
{
    return mapper != NULL && mapper->scalar_range_valid != FVIZ_FALSE;
}

void fviz_volume_mapper_set_options(FVizVolumeMapper* mapper, const FVizVolumeRenderOptions* options)
{
    if (mapper == NULL || options == NULL) return;
    if (options->sampling_step > 0.0f) mapper->sampling_step = options->sampling_step;
    mapper->shading = options->shading > 0.0f ? FVIZ_TRUE : FVIZ_FALSE;
    if (options->use_automatic_scalar_range != FVIZ_FALSE)
    {
        fviz_volume_mapper_use_automatic_scalar_range(mapper);
    }
    else
    {
        mapper->scalar_range_minimum = options->scalar_range_minimum;
        mapper->scalar_range_maximum = options->scalar_range_maximum;
        mapper->scalar_range_valid = FVIZ_TRUE;
        mapper->automatic_scalar_range = FVIZ_FALSE;
    }
    mapper->contrast = options->contrast > 0.0f ? options->contrast : 1.0f;
    mapper->shade_ambient = options->shade_ambient != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    mapper->shade_diffuse = options->shade_diffuse != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    mapper->shade_specular = options->shade_specular != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    mapper->specular_power = options->specular_power;
    fviz_object_modified((FVizObject*)mapper);
}

void fviz_volume_mapper_options(const FVizVolumeMapper* mapper, FVizVolumeRenderOptions* out_options)
{
    if (mapper == NULL || out_options == NULL) return;
    fviz_volume_render_options_initialize(out_options);
    out_options->sampling_step = mapper->sampling_step;
    out_options->shading = mapper->shading != FVIZ_FALSE ? 1.0f : 0.0f;
    out_options->use_automatic_scalar_range = mapper->automatic_scalar_range != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_options->scalar_range_minimum = mapper->scalar_range_minimum;
    out_options->scalar_range_maximum = mapper->scalar_range_maximum;
    out_options->contrast = mapper->contrast;
    out_options->shade_ambient = mapper->shade_ambient;
    out_options->shade_diffuse = mapper->shade_diffuse;
    out_options->shade_specular = mapper->shade_specular;
    out_options->specular_power = mapper->specular_power;
}
