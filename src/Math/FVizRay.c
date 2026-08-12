#include <math.h>

#include <FViz/Math/FVizRay.h>

#include <FViz/Core/FVizTypes.h>

FVizRay fviz_ray(FVizVec3 origin, FVizVec3 direction)
{
    FVizRay result;
    result.origin = origin;
    result.direction = fviz_vec3_normalize(direction);
    return result;
}

FVizVec3 fviz_ray_point_at(FVizRay ray, float t)
{
    return fviz_vec3_add(ray.origin, fviz_vec3_scale(ray.direction, t));
}

float fviz_ray_distance_to_point(FVizRay ray, FVizVec3 point)
{
    const FVizVec3 to_point = fviz_vec3_sub(point, ray.origin);
    const float projection = fviz_vec3_dot(to_point, ray.direction);
    const FVizVec3 closest = fviz_ray_point_at(ray, projection);
    return fviz_vec3_length(fviz_vec3_sub(point, closest));
}

FVizBool fviz_ray_intersects_sphere(FVizRay ray, FVizVec3 center, float radius, float* out_t)
{
    const FVizVec3 to_center = fviz_vec3_sub(center, ray.origin);
    const float projection = fviz_vec3_dot(to_center, ray.direction);
    const float distance_squared = fviz_vec3_dot(to_center, to_center) - projection * projection;
    const float radius_squared = radius * radius;
    float discriminant;
    float t;
    if (distance_squared > radius_squared)
    {
        return FVIZ_FALSE;
    }
    discriminant = sqrtf(radius_squared - distance_squared);
    t = projection - discriminant;
    if (t < 0.0f)
    {
        t = projection + discriminant;
    }
    if (t < 0.0f)
    {
        return FVIZ_FALSE;
    }
    if (out_t != NULL) *out_t = t;
    return FVIZ_TRUE;
}
