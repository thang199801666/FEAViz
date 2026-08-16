#include <math.h>

#include <FViz/Math/FVizFrustum.h>

static FVizPlane fviz_frustum_plane(float a, float b, float c, float d, FVizBool* valid)
{
    FVizPlane plane;
    const float length = sqrtf(a * a + b * b + c * c);
    if (length <= 1.0e-20f)
    {
        plane.normal = fviz_vec3(0.0f, 0.0f, 0.0f);
        plane.distance = 0.0f;
        *valid = FVIZ_FALSE;
        return plane;
    }
    plane.normal = fviz_vec3(a / length, b / length, c / length);
    plane.distance = d / length;
    return plane;
}

FVizFrustum fviz_frustum_from_view_projection(FVizMat4 m)
{
    FVizFrustum result;
    FVizBool valid = FVIZ_TRUE;
    /* Matrix storage is column-major. Extract clip planes from row4 +/- rowN. */
    result.planes[FVIZ_FRUSTUM_LEFT] =
        fviz_frustum_plane(m.m[3] + m.m[0], m.m[7] + m.m[4], m.m[11] + m.m[8], m.m[15] + m.m[12], &valid);
    result.planes[FVIZ_FRUSTUM_RIGHT] =
        fviz_frustum_plane(m.m[3] - m.m[0], m.m[7] - m.m[4], m.m[11] - m.m[8], m.m[15] - m.m[12], &valid);
    result.planes[FVIZ_FRUSTUM_BOTTOM] =
        fviz_frustum_plane(m.m[3] + m.m[1], m.m[7] + m.m[5], m.m[11] + m.m[9], m.m[15] + m.m[13], &valid);
    result.planes[FVIZ_FRUSTUM_TOP] =
        fviz_frustum_plane(m.m[3] - m.m[1], m.m[7] - m.m[5], m.m[11] - m.m[9], m.m[15] - m.m[13], &valid);
    result.planes[FVIZ_FRUSTUM_NEAR] =
        fviz_frustum_plane(m.m[3] + m.m[2], m.m[7] + m.m[6], m.m[11] + m.m[10], m.m[15] + m.m[14], &valid);
    result.planes[FVIZ_FRUSTUM_FAR] =
        fviz_frustum_plane(m.m[3] - m.m[2], m.m[7] - m.m[6], m.m[11] - m.m[10], m.m[15] - m.m[14], &valid);
    result.valid = valid;
    return result;
}

FVizBool fviz_frustum_contains_point(const FVizFrustum* frustum, FVizVec3 point)
{
    unsigned int i;
    if (frustum == NULL || frustum->valid == FVIZ_FALSE) return FVIZ_FALSE;
    for (i = 0u; i < FVIZ_FRUSTUM_PLANE_COUNT; ++i)
        if (fviz_plane_distance_to_point(frustum->planes[i], point) < 0.0f) return FVIZ_FALSE;
    return FVIZ_TRUE;
}

FVizBool fviz_frustum_intersects_bounds(const FVizFrustum* frustum, FVizBounds bounds)
{
    unsigned int i;
    if (frustum == NULL || frustum->valid == FVIZ_FALSE) return FVIZ_FALSE;
    if (bounds.valid == FVIZ_FALSE) return FVIZ_TRUE;
    for (i = 0u; i < FVIZ_FRUSTUM_PLANE_COUNT; ++i)
    {
        const FVizPlane plane = frustum->planes[i];
        const FVizVec3 positive = fviz_vec3(plane.normal.x >= 0.0f ? bounds.max.x : bounds.min.x,
                                            plane.normal.y >= 0.0f ? bounds.max.y : bounds.min.y,
                                            plane.normal.z >= 0.0f ? bounds.max.z : bounds.min.z);
        if (fviz_plane_distance_to_point(plane, positive) < 0.0f) return FVIZ_FALSE;
    }
    return FVIZ_TRUE;
}
