#include <stdio.h>
#include <string.h>

#include <FViz/Rendering/FVizExternalOpenGLSurface.h>

static FVizResult make_current_ok(void* user_data)
{
    (void)user_data;
    return FVIZ_OK;
}

static int require_true(int value, const char* message)
{
    if (!value)
    {
        fprintf(stderr, "%s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    FVizExternalOpenGLSurface surface;
    FVizExternalOpenGLSurface copy;

    memset(&surface, 0xCD, sizeof(surface));
    fviz_external_opengl_surface_initialize(&surface);
    if (!require_true(surface.struct_size == sizeof(surface), "surface struct_size mismatch")) return 1;
    if (!require_true(surface.make_current == NULL, "initialize must clear callbacks")) return 1;
    if (!require_true(fviz_external_opengl_surface_validate(&surface) == FVIZ_ERROR_INVALID_ARGUMENT,
            "surface without make_current must be rejected")) return 1;

    surface.make_current = make_current_ok;
    if (!require_true(fviz_external_opengl_surface_validate(&surface) == FVIZ_OK,
            "valid external surface rejected")) return 1;

    copy = surface;
    copy.struct_size = 1u;
    if (!require_true(fviz_external_opengl_surface_validate(&copy) == FVIZ_ERROR_INVALID_ARGUMENT,
            "undersized surface contract accepted")) return 1;

    copy = surface;
    copy.sample_count = 65u;
    if (!require_true(fviz_external_opengl_surface_validate(&copy) == FVIZ_ERROR_INVALID_ARGUMENT,
            "unreasonable sample count accepted")) return 1;

    return 0;
}
