#include <FViz/Algorithms/FVizPlaneSource.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Pipeline/FVizExecutive.h>
#include <FViz/Math/FVizVec3.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizPlaneSource
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizVec3 origin;
    FVizVec3 point1;
    FVizVec3 point2;
    uint32_t x_resolution;
    uint32_t y_resolution;
};

static void fviz_plane_source_destroy(FVizObject* object)
{
    FVizPlaneSource* source = (FVizPlaneSource*)object;
    fviz_release(source->algorithm);
    source->algorithm = NULL;
}

static const FVizObjectClass g_fviz_plane_source_class = {FVIZ_TYPE_PLANE_SOURCE, "FVizPlaneSource",
                                                          &g_fviz_object_class, fviz_plane_source_destroy, NULL};

static FVizMTime fviz_plane_source_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static FVizResult fviz_plane_source_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                    void* state)
{
    FVizPlaneSource* source = (FVizPlaneSource*)state;
    FVizPolyData* output = NULL;
    FVizVec3* points = NULL;
    uint32_t* triangles = NULL;
    FVizVec3 axis1;
    FVizVec3 axis2;
    const FVizSize x_points = (FVizSize)source->x_resolution + 1u;
    const FVizSize y_points = (FVizSize)source->y_resolution + 1u;
    FVizSize point_count;
    FVizSize triangle_count;
    FVizSize bytes;
    FVizSize triangle_index = 0u;
    uint32_t y;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (fviz_size_multiply(x_points, y_points, &point_count) != FVIZ_OK ||
        fviz_size_multiply((FVizSize)source->x_resolution, source->y_resolution, &triangle_count) != FVIZ_OK ||
        triangle_count > (FVizSize)-1 / 2u)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "plane source resolution overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    triangle_count *= 2u;
    if (fviz_size_multiply(point_count, sizeof(*points), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    points = (FVizVec3*)fviz_alloc(bytes);
    if (fviz_size_multiply(triangle_count, 3u * sizeof(*triangles), &bytes) != FVIZ_OK)
    {
        fviz_free(points);
        return FVIZ_ERROR_OVERFLOW;
    }
    triangles = (uint32_t*)fviz_alloc(bytes);
    if ((points == NULL && point_count != 0u) || (triangles == NULL && triangle_count != 0u)) goto fail;
    axis1 = fviz_vec3_sub(source->point1, source->origin);
    axis2 = fviz_vec3_sub(source->point2, source->origin);
    for (y = 0u; y <= source->y_resolution; ++y)
    {
        const float v = (float)y / (float)source->y_resolution;
        uint32_t x;
        for (x = 0u; x <= source->x_resolution; ++x)
        {
            const float u = (float)x / (float)source->x_resolution;
            points[(FVizSize)y * x_points + x] =
                fviz_vec3_add(source->origin, fviz_vec3_add(fviz_vec3_scale(axis1, u), fviz_vec3_scale(axis2, v)));
        }
    }
    for (y = 0u; y < source->y_resolution; ++y)
    {
        uint32_t x;
        for (x = 0u; x < source->x_resolution; ++x)
        {
            const uint32_t row = source->x_resolution + 1u;
            const uint32_t a = y * row + x;
            const uint32_t b = a + 1u;
            const uint32_t c = a + row;
            const uint32_t d = c + 1u;
            triangles[triangle_index++] = a;
            triangles[triangle_index++] = b;
            triangles[triangle_index++] = d;
            triangles[triangle_index++] = a;
            triangles[triangle_index++] = d;
            triangles[triangle_index++] = c;
        }
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_reserve(output, point_count, triangle_count) != FVIZ_OK ||
        fviz_poly_data_add_points(output, points, point_count, NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangles(output, triangles, triangle_count) != FVIZ_OK ||
        fviz_poly_data_compute_normals(output) != FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_free(triangles);
    fviz_free(points);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_free(triangles);
    fviz_free(points);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_plane_source_create(FVizPlaneSource** out_source)
{
    FVizPlaneSource* source;
    FVizAlgorithmCallbacks callbacks;
    if (out_source == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_source must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_source = NULL;
    source = (FVizPlaneSource*)fviz_internal_object_allocate(sizeof(*source), &g_fviz_plane_source_class, NULL);
    if (source == NULL) return fviz_last_error_code();
    source->origin = fviz_vec3(-0.5f, -0.5f, 0.0f);
    source->point1 = fviz_vec3(0.5f, -0.5f, 0.0f);
    source->point2 = fviz_vec3(-0.5f, 0.5f, 0.0f);
    source->x_resolution = 1u;
    source->y_resolution = 1u;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_plane_source_process_request;
    callbacks.get_state_mtime = fviz_plane_source_state_mtime;
    callbacks.state_object = (FVizObject*)source;
    if (fviz_algorithm_create(0u, 1u, &callbacks, source, &source->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(source->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(source);
        return fviz_last_error_code();
    }
    *out_source = source;
    return FVIZ_OK;
}

void fviz_plane_source_set_origin(FVizPlaneSource* source, FVizVec3 origin)
{
    if (source == NULL) return;
    source->origin = origin;
    fviz_object_modified((FVizObject*)source);
}

void fviz_plane_source_set_point1(FVizPlaneSource* source, FVizVec3 point1)
{
    if (source == NULL) return;
    source->point1 = point1;
    fviz_object_modified((FVizObject*)source);
}

void fviz_plane_source_set_point2(FVizPlaneSource* source, FVizVec3 point2)
{
    if (source == NULL) return;
    source->point2 = point2;
    fviz_object_modified((FVizObject*)source);
}

FVizResult fviz_plane_source_set_resolution(FVizPlaneSource* source, uint32_t x_resolution, uint32_t y_resolution)
{
    const uint64_t x_points = (uint64_t)x_resolution + 1u;
    const uint64_t y_points = (uint64_t)y_resolution + 1u;
    if (source == NULL || x_resolution == 0u || y_resolution == 0u || x_points > (uint64_t)UINT32_MAX / y_points)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "plane resolution is invalid or exceeds 32-bit topology capacity");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    source->x_resolution = x_resolution;
    source->y_resolution = y_resolution;
    fviz_object_modified((FVizObject*)source);
    return FVIZ_OK;
}

FVizVec3 fviz_plane_source_origin(const FVizPlaneSource* source)
{
    return source != NULL ? source->origin : fviz_vec3(0, 0, 0);
}

FVizVec3 fviz_plane_source_point1(const FVizPlaneSource* source)
{
    return source != NULL ? source->point1 : fviz_vec3(0, 0, 0);
}

FVizVec3 fviz_plane_source_point2(const FVizPlaneSource* source)
{
    return source != NULL ? source->point2 : fviz_vec3(0, 0, 0);
}

uint32_t fviz_plane_source_x_resolution(const FVizPlaneSource* source)
{
    return source != NULL ? source->x_resolution : 0u;
}

uint32_t fviz_plane_source_y_resolution(const FVizPlaneSource* source)
{
    return source != NULL ? source->y_resolution : 0u;
}

FVizAlgorithm* fviz_plane_source_algorithm(FVizPlaneSource* source)
{
    return source != NULL ? source->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_plane_source_output_port(FVizPlaneSource* source)
{
    return source != NULL ? fviz_algorithm_output_port(source->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_plane_source_output(FVizPlaneSource* source)
{
    return source != NULL ? (FVizPolyData*)fviz_algorithm_output_data(source->algorithm, 0u) : NULL;
}

FVizResult fviz_plane_source_update(FVizPlaneSource* source)
{
    return source != NULL ? fviz_algorithm_update(source->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
