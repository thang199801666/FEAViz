#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizLight.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizLightPrivate.h>

static const FVizObjectClass g_fviz_light_class = {FVIZ_TYPE_LIGHT, "FVizLight", &g_fviz_object_class, NULL, NULL};

static float fviz_light_clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

FVizResult fviz_light_create(FVizLight** out_light)
{
    FVizLight* light;
    if (out_light == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_light must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_light = NULL;
    light = (FVizLight*)fviz_internal_object_allocate(sizeof(FVizLight), &g_fviz_light_class, NULL);
    if (light == NULL) return fviz_last_error_code();
    light->type = FVIZ_LIGHT_HEADLIGHT;
    light->enabled = FVIZ_TRUE;
    light->position = fviz_vec3(0.0f, 0.0f, 1.0f);
    light->color[0] = 1.0f;
    light->color[1] = 1.0f;
    light->color[2] = 1.0f;
    light->intensity = 1.0f;
    *out_light = light;
    return FVIZ_OK;
}

void fviz_light_set_type(FVizLight* light, FVizLightType type)
{
    if (light == NULL || (type != FVIZ_LIGHT_HEADLIGHT && type != FVIZ_LIGHT_SCENE)) return;
    if (light->type != type)
    {
        light->type = type;
        fviz_object_modified((FVizObject*)light);
    }
}

FVizLightType fviz_light_type(const FVizLight* light)
{
    return light != NULL ? light->type : FVIZ_LIGHT_HEADLIGHT;
}

void fviz_light_set_enabled(FVizLight* light, FVizBool enabled)
{
    const FVizBool normalized = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (light != NULL && light->enabled != normalized)
    {
        light->enabled = normalized;
        fviz_object_modified((FVizObject*)light);
    }
}

FVizBool fviz_light_enabled(const FVizLight* light)
{
    return light != NULL ? light->enabled : FVIZ_FALSE;
}

void fviz_light_set_position(FVizLight* light, FVizVec3 position)
{
    if (light == NULL) return;
    if (light->position.x != position.x || light->position.y != position.y || light->position.z != position.z)
    {
        light->position = position;
        fviz_object_modified((FVizObject*)light);
    }
}

FVizVec3 fviz_light_position(const FVizLight* light)
{
    return light != NULL ? light->position : fviz_vec3(0.0f, 0.0f, 0.0f);
}

void fviz_light_set_color(FVizLight* light, float red, float green, float blue)
{
    if (light == NULL) return;
    red = fviz_light_clamp01(red);
    green = fviz_light_clamp01(green);
    blue = fviz_light_clamp01(blue);
    if (light->color[0] != red || light->color[1] != green || light->color[2] != blue)
    {
        light->color[0] = red;
        light->color[1] = green;
        light->color[2] = blue;
        fviz_object_modified((FVizObject*)light);
    }
}

void fviz_light_get_color(const FVizLight* light, float* red, float* green, float* blue)
{
    if (light == NULL) return;
    if (red != NULL) *red = light->color[0];
    if (green != NULL) *green = light->color[1];
    if (blue != NULL) *blue = light->color[2];
}

void fviz_light_set_intensity(FVizLight* light, float intensity)
{
    if (light == NULL) return;
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 16.0f) intensity = 16.0f;
    if (light->intensity != intensity)
    {
        light->intensity = intensity;
        fviz_object_modified((FVizObject*)light);
    }
}

float fviz_light_intensity(const FVizLight* light)
{
    return light != NULL ? light->intensity : 0.0f;
}
