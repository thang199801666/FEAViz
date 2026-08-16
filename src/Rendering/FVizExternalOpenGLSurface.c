#include <stddef.h>
#include <string.h>

#include <FViz/Rendering/FVizExternalOpenGLSurface.h>

void fviz_external_opengl_surface_initialize(FVizExternalOpenGLSurface* surface)
{
    if (surface == NULL) return;
    (void)memset(surface, 0, sizeof(*surface));
    surface->struct_size = (uint32_t)sizeof(*surface);
}

FVizResult fviz_external_opengl_surface_validate(const FVizExternalOpenGLSurface* surface)
{
    if (surface == NULL || surface->make_current == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (surface->struct_size != 0u &&
        surface->struct_size < offsetof(FVizExternalOpenGLSurface, srgb_capable) + sizeof(surface->srgb_capable))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (surface->sample_count > 32u) return FVIZ_ERROR_INVALID_ARGUMENT;
    return FVIZ_OK;
}
