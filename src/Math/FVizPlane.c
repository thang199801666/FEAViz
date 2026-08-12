#include <FViz/Math/FVizPlane.h>

FVizPlane fviz_plane_from_point_normal(FVizVec3 point, FVizVec3 normal)
{
    const FVizVec3 n = fviz_vec3_normalize(normal);
    FVizPlane result;
    result.normal = n;
    result.distance = -fviz_vec3_dot(n, point);
    return result;
}

float fviz_plane_distance_to_point(FVizPlane plane, FVizVec3 point)
{
    return fviz_vec3_dot(plane.normal, point) + plane.distance;
}

FVizVec3 fviz_plane_project_point(FVizPlane plane, FVizVec3 point)
{
    const float distance = fviz_plane_distance_to_point(plane, point);
    return fviz_vec3_sub(point, fviz_vec3_scale(plane.normal, distance));
}
