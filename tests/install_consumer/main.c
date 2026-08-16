#include <math.h>

#include <FViz/FViz.h>

int main(void)
{
    FVizTransform* transform = NULL;
    FVizCompositeGeometryFilter* composite = NULL;
    FVizVec3 point;
    const FVizVersionInfo version = fviz_version();
    const FVizCacheKey key = fviz_cache_key_append_string(
        fviz_cache_key_initialize(), "consumer");
    if (version.major != (uint32_t)FVIZ_VERSION_MAJOR ||
        version.minor != (uint32_t)FVIZ_VERSION_MINOR ||
        version.patch != (uint32_t)FVIZ_VERSION_PATCH ||
        version.abi != (uint32_t)FVIZ_ABI_VERSION || key == 0u) return 1;
    if (fviz_transform_create(&transform) != FVIZ_OK) return 2;
    if (fviz_composite_geometry_filter_create(&composite) != FVIZ_OK) return 4;
    if (fviz_composite_geometry_filter_set_parallel_enabled(composite, FVIZ_FALSE) != FVIZ_OK ||
        fviz_composite_geometry_filter_set_parallel_threshold(composite, 2u) != FVIZ_OK ||
        fviz_composite_geometry_filter_set_cache_byte_capacity(composite, 1024u) != FVIZ_OK ||
        fviz_composite_geometry_filter_parallel_threshold(composite) != 2u ||
        fviz_composite_geometry_filter_cache_byte_capacity(composite) != 1024u)
    {
        fviz_release(composite);
        fviz_release(transform);
        return 5;
    }
    fviz_release(composite);
    fviz_transform_translate(transform, fviz_vec3(1.0f, 2.0f, 3.0f));
    point = fviz_transform_point(transform, fviz_vec3(0.0f, 0.0f, 0.0f));
    fviz_release(transform);
    return fabsf(point.x - 1.0f) < 1.0e-6f &&
        fabsf(point.y - 2.0f) < 1.0e-6f &&
        fabsf(point.z - 3.0f) < 1.0e-6f
        ? 0
        : 3;
}
