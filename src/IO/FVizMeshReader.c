#include <ctype.h>
#include <string.h>

#include <FViz/IO/FVizMeshReader.h>
#include <FViz/IO/FVizOBJReader.h>
#include <FViz/IO/FVizSTLReader.h>

#include <FViz/Core/FVizErrorInternal.h>

static const char* fviz_extension(const char* path)
{
    const char* dot;
    const char* slash;
    if (path == NULL) return NULL;
    dot = strrchr(path, '.');
    slash = strrchr(path, '/');
#if defined(_WIN32)
    {
        const char* backslash = strrchr(path, '\\');
        if (backslash != NULL && (slash == NULL || backslash > slash)) slash = backslash;
    }
#endif
    if (dot == NULL || (slash != NULL && dot < slash)) return NULL;
    return dot + 1;
}

static FVizBool fviz_extension_equals(const char* extension, const char* expected)
{
    while (*extension != '\0' && *expected != '\0')
    {
        if (tolower((unsigned char)*extension) != tolower((unsigned char)*expected)) return FVIZ_FALSE;
        ++extension;
        ++expected;
    }
    return *extension == '\0' && *expected == '\0' ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_mesh_format_supported(const char* path)
{
    const char* extension = fviz_extension(path);
    if (extension == NULL) return FVIZ_FALSE;
    return fviz_extension_equals(extension, "obj") == FVIZ_TRUE ||
        fviz_extension_equals(extension, "stl") == FVIZ_TRUE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizResult fviz_mesh_read(const char* path, FVizPolyData** out_poly_data)
{
    const char* extension;
    if (path == NULL || out_poly_data == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "mesh path and output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    extension = fviz_extension(path);
    if (extension != NULL && fviz_extension_equals(extension, "obj") == FVIZ_TRUE)
    {
        return fviz_obj_read(path, out_poly_data);
    }
    if (extension != NULL && fviz_extension_equals(extension, "stl") == FVIZ_TRUE)
    {
        return fviz_stl_read(path, out_poly_data);
    }
    fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported mesh file extension (Phase 8 supports OBJ and STL)");
    return FVIZ_ERROR_NOT_SUPPORTED;
}
