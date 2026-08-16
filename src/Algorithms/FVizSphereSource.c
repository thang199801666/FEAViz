#include <float.h>
#include <math.h>

#include <FViz/Algorithms/FVizSphereSource.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizSphereSource
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizVec3 center;
    double radius;
    uint32_t theta_resolution;
    uint32_t phi_resolution;
};

static void fviz_sphere_source_destroy(FVizObject* object)
{
    FVizSphereSource* source = (FVizSphereSource*)object;
    fviz_release(source->algorithm);
    source->algorithm = NULL;
}

static const FVizObjectClass g_fviz_sphere_source_class = {FVIZ_TYPE_SPHERE_SOURCE, "FVizSphereSource",
                                                           &g_fviz_object_class, fviz_sphere_source_destroy, NULL};

static FVizMTime fviz_sphere_source_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static FVizResult fviz_sphere_source_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                     void* state)
{
    FVizSphereSource* source = (FVizSphereSource*)state;
    FVizPolyData* output = NULL;
    FVizVec3* points = NULL;
    uint32_t* triangles = NULL;
    const double pi = 3.1415926535897932384626433832795;
    const uint32_t theta = source->theta_resolution;
    const uint32_t phi = source->phi_resolution;
    const uint32_t ring_count = phi - 1u;
    FVizSize point_count;
    FVizSize triangle_count;
    FVizSize bytes;
    FVizSize point_index = 0u;
    FVizSize triangle_index = 0u;
    uint32_t ring;
    uint32_t south = 0u;
    uint32_t north;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (fviz_size_multiply((FVizSize)ring_count, theta, &point_count) != FVIZ_OK ||
        point_count > (FVizSize)UINT32_MAX - 1u)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "sphere point count exceeds topology capacity");
        return FVIZ_ERROR_OVERFLOW;
    }
    point_count += 2u;
    if (fviz_size_multiply((FVizSize)2u * theta, ring_count, &triangle_count) != FVIZ_OK)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "sphere triangle count overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    if (fviz_size_multiply(point_count, sizeof(*points), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    points = (FVizVec3*)fviz_alloc(bytes);
    if (triangle_count > (FVizSize)-1 / 3u ||
        fviz_size_multiply(triangle_count * 3u, sizeof(*triangles), &bytes) != FVIZ_OK)
    {
        fviz_free(points);
        return FVIZ_ERROR_OVERFLOW;
    }
    triangles = (uint32_t*)fviz_alloc(bytes);
    if (points == NULL || triangles == NULL) goto fail;
    points[point_index++] = fviz_vec3(source->center.x, source->center.y, source->center.z - (float)source->radius);
    for (ring = 1u; ring < phi; ++ring)
    {
        const double polar = pi * (double)ring / (double)phi;
        const double z = cos(polar);
        const double radial = sin(polar);
        uint32_t t;
        for (t = 0u; t < theta; ++t)
        {
            const double azimuth = 2.0 * pi * (double)t / (double)theta;
            FVizVec3 point;
            point.x = source->center.x + (float)(source->radius * radial * cos(azimuth));
            point.y = source->center.y + (float)(source->radius * radial * sin(azimuth));
            point.z = source->center.z + (float)(source->radius * z);
            points[point_index++] = point;
        }
    }
    north = (uint32_t)point_index;
    points[point_index++] = fviz_vec3(source->center.x, source->center.y, source->center.z + (float)source->radius);
    for (ring = 0u; ring < theta; ++ring)
    {
        const uint32_t next = (ring + 1u) % theta;
        triangles[triangle_index++] = south;
        triangles[triangle_index++] = 1u + next;
        triangles[triangle_index++] = 1u + ring;
    }
    for (ring = 0u; ring + 1u < ring_count; ++ring)
    {
        const uint32_t base0 = 1u + ring * theta;
        const uint32_t base1 = base0 + theta;
        uint32_t t;
        for (t = 0u; t < theta; ++t)
        {
            const uint32_t next = (t + 1u) % theta;
            triangles[triangle_index++] = base0 + t;
            triangles[triangle_index++] = base0 + next;
            triangles[triangle_index++] = base1 + next;
            triangles[triangle_index++] = base0 + t;
            triangles[triangle_index++] = base1 + next;
            triangles[triangle_index++] = base1 + t;
        }
    }
    {
        const uint32_t last = 1u + (ring_count - 1u) * theta;
        uint32_t t;
        for (t = 0u; t < theta; ++t)
        {
            const uint32_t next = (t + 1u) % theta;
            triangles[triangle_index++] = last + t;
            triangles[triangle_index++] = last + next;
            triangles[triangle_index++] = north;
        }
    }
    if (point_index != point_count || triangle_index != triangle_count * 3u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "sphere source generated an inconsistent topology size");
        goto fail;
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

FVizResult fviz_sphere_source_create(FVizSphereSource** out_source)
{
    FVizSphereSource* source;
    FVizAlgorithmCallbacks callbacks;
    if (out_source == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_source must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_source = NULL;
    source = (FVizSphereSource*)fviz_internal_object_allocate(sizeof(*source), &g_fviz_sphere_source_class, NULL);
    if (source == NULL) return fviz_last_error_code();
    source->center = fviz_vec3(0.0f, 0.0f, 0.0f);
    source->radius = 0.5;
    source->theta_resolution = 16u;
    source->phi_resolution = 8u;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_sphere_source_process_request;
    callbacks.get_state_mtime = fviz_sphere_source_state_mtime;
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

void fviz_sphere_source_set_center(FVizSphereSource* source, FVizVec3 center)
{
    if (source == NULL) return;
    source->center = center;
    fviz_object_modified((FVizObject*)source);
}

FVizResult fviz_sphere_source_set_radius(FVizSphereSource* source, double radius)
{
    if (source == NULL || !isfinite(radius) || radius <= 0.0 || radius > FLT_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "sphere radius must be positive");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    source->radius = radius;
    fviz_object_modified((FVizObject*)source);
    return FVIZ_OK;
}

FVizResult fviz_sphere_source_set_resolution(FVizSphereSource* source, uint32_t theta_resolution,
                                             uint32_t phi_resolution)
{
    uint64_t points;
    if (source == NULL || theta_resolution < 3u || phi_resolution < 2u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "sphere resolution is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    points = 2u + (uint64_t)(phi_resolution - 1u) * theta_resolution;
    if (points > UINT32_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "sphere resolution exceeds 32-bit topology capacity");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    source->theta_resolution = theta_resolution;
    source->phi_resolution = phi_resolution;
    fviz_object_modified((FVizObject*)source);
    return FVIZ_OK;
}

FVizVec3 fviz_sphere_source_center(const FVizSphereSource* source)
{
    return source != NULL ? source->center : fviz_vec3(0, 0, 0);
}

double fviz_sphere_source_radius(const FVizSphereSource* source)
{
    return source != NULL ? source->radius : 0.0;
}

uint32_t fviz_sphere_source_theta_resolution(const FVizSphereSource* source)
{
    return source != NULL ? source->theta_resolution : 0u;
}

uint32_t fviz_sphere_source_phi_resolution(const FVizSphereSource* source)
{
    return source != NULL ? source->phi_resolution : 0u;
}

FVizAlgorithm* fviz_sphere_source_algorithm(FVizSphereSource* source)
{
    return source != NULL ? source->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_sphere_source_output_port(FVizSphereSource* source)
{
    return source != NULL ? fviz_algorithm_output_port(source->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_sphere_source_output(FVizSphereSource* source)
{
    return source != NULL ? (FVizPolyData*)fviz_algorithm_output_data(source->algorithm, 0u) : NULL;
}

FVizResult fviz_sphere_source_update(FVizSphereSource* source)
{
    return source != NULL ? fviz_algorithm_update(source->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
