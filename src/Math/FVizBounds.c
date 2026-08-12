#include <math.h>

#include <FViz/Math/FVizBounds.h>

FVizBounds fviz_bounds_empty(void)
{
    FVizBounds bounds;
    fviz_bounds_reset(&bounds);
    return bounds;
}

void fviz_bounds_reset(FVizBounds* bounds)
{
    if (bounds == NULL)
    {
        return;
    }
    bounds->min = fviz_vec3(0.0f, 0.0f, 0.0f);
    bounds->max = fviz_vec3(0.0f, 0.0f, 0.0f);
    bounds->valid = FVIZ_FALSE;
}

void fviz_bounds_include_point(FVizBounds* bounds, FVizVec3 point)
{
    if (bounds == NULL)
    {
        return;
    }
    if (bounds->valid == FVIZ_FALSE)
    {
        bounds->min = point;
        bounds->max = point;
        bounds->valid = FVIZ_TRUE;
        return;
    }
    if (point.x < bounds->min.x) bounds->min.x = point.x;
    if (point.y < bounds->min.y) bounds->min.y = point.y;
    if (point.z < bounds->min.z) bounds->min.z = point.z;
    if (point.x > bounds->max.x) bounds->max.x = point.x;
    if (point.y > bounds->max.y) bounds->max.y = point.y;
    if (point.z > bounds->max.z) bounds->max.z = point.z;
}

void fviz_bounds_include_bounds(FVizBounds* bounds, const FVizBounds* other)
{
    if (bounds == NULL || other == NULL || other->valid == FVIZ_FALSE)
    {
        return;
    }
    fviz_bounds_include_point(bounds, other->min);
    fviz_bounds_include_point(bounds, other->max);
}

FVizVec3 fviz_bounds_center(const FVizBounds* bounds)
{
    if (bounds == NULL || bounds->valid == FVIZ_FALSE)
    {
        return fviz_vec3(0.0f, 0.0f, 0.0f);
    }
    return fviz_vec3_scale(fviz_vec3_add(bounds->min, bounds->max), 0.5f);
}

FVizVec3 fviz_bounds_size(const FVizBounds* bounds)
{
    if (bounds == NULL || bounds->valid == FVIZ_FALSE)
    {
        return fviz_vec3(0.0f, 0.0f, 0.0f);
    }
    return fviz_vec3_sub(bounds->max, bounds->min);
}

float fviz_bounds_radius(const FVizBounds* bounds)
{
    return 0.5f * fviz_vec3_length(fviz_bounds_size(bounds));
}
