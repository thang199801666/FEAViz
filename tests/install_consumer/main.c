#include <math.h>

#include <FViz/FViz.h>

int main(void)
{
    FVizTransform* transform = NULL;
    FVizVec3 point;
    const FVizVersionInfo version = fviz_version();
    if (version.major != 0u || version.minor != 13u) return 1;
    if (fviz_transform_create(&transform) != FVIZ_OK) return 2;
    fviz_transform_translate(transform, fviz_vec3(1.0f, 2.0f, 3.0f));
    point = fviz_transform_point(transform, fviz_vec3(0.0f, 0.0f, 0.0f));
    fviz_release(transform);
    return fabsf(point.x - 1.0f) < 1.0e-6f &&
        fabsf(point.y - 2.0f) < 1.0e-6f &&
        fabsf(point.z - 3.0f) < 1.0e-6f
        ? 0
        : 3;
}
