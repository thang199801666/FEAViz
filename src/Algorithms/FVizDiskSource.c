#include <float.h>
#include <math.h>

#include <FViz/Algorithms/FVizDiskSource.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizDiskSource
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    double inner_radius;
    double outer_radius;
    uint32_t radial_resolution;
    uint32_t circumferential_resolution;
    FVizVec3 center;
    FVizVec3 normal;
};

static void fviz_disk_source_destroy(FVizObject* object)
{
    FVizDiskSource* source = (FVizDiskSource*)object;
    fviz_release(source->algorithm);
    source->algorithm = NULL;
}

static const FVizObjectClass g_fviz_disk_source_class = {FVIZ_TYPE_DISK_SOURCE, "FVizDiskSource",
                                                         &g_fviz_object_class, fviz_disk_source_destroy, NULL};

static FVizMTime fviz_disk_source_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static void fviz_disk_source_build_basis(FVizVec3 normal, FVizVec3* out_right, FVizVec3* out_up)
{
    const FVizVec3 n = fviz_vec3_normalize(normal);
    FVizVec3 ref = fviz_vec3(0.0f, 0.0f, 1.0f);
    FVizVec3 right;
    FVizVec3 up;
    if (fviz_vec3_dot(n, ref) > 0.99f)
        ref = fviz_vec3(0.0f, 1.0f, 0.0f);
    right = fviz_vec3_normalize(fviz_vec3_cross(n, ref));
    up = fviz_vec3_normalize(fviz_vec3_cross(right, n));
    *out_right = right;
    *out_up = up;
}

static FVizResult fviz_disk_source_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                   void* state)
{
    FVizDiskSource* source = (FVizDiskSource*)state;
    FVizPolyData* output = NULL;
    FVizVec3 right;
    FVizVec3 up;
    uint32_t ring;
    uint32_t i;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (source->circumferential_resolution < 3u || source->radial_resolution == 0u ||
        source->outer_radius < source->inner_radius || source->outer_radius <= 0.0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "disk source requires circumference>=3, radial_resolution>=1, radii valid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_disk_source_build_basis(source->normal, &right, &up);
    {
        const FVizSize points_per_ring = (FVizSize)source->circumferential_resolution;
        const FVizSize ring_count = (FVizSize)source->radial_resolution + 1u;
        const FVizSize point_count = points_per_ring * ring_count;
        uint32_t* ids = NULL;
        if (fviz_poly_data_create(&output) != FVIZ_OK ||
            fviz_poly_data_reserve(output, point_count, (FVizSize)source->circumferential_resolution *
                                   (FVizSize)source->radial_resolution * 2u) != FVIZ_OK)
            goto fail;
        ids = (uint32_t*)fviz_alloc(point_count * (FVizSize)sizeof(uint32_t));
        if (ids == NULL) goto fail;
        for (ring = 0u; ring < ring_count; ++ring)
        {
            const double t = (double)ring / (double)(source->radial_resolution);
            const double radius = source->inner_radius + t * (source->outer_radius - source->inner_radius);
            for (i = 0u; i < source->circumferential_resolution; ++i)
            {
                const double angle = (double)i * 2.0 * 3.14159265358979323846 / (double)source->circumferential_resolution;
                const float r = (float)radius;
                const FVizVec3 offset = fviz_vec3_add(
                    fviz_vec3_scale(right, r * (float)cos(angle)), fviz_vec3_scale(up, r * (float)sin(angle)));
                const FVizSize id = (FVizSize)ring * points_per_ring + (FVizSize)i;
                if (fviz_poly_data_add_point(output, fviz_vec3_add(source->center, offset), &ids[id]) != FVIZ_OK)
                {
                    fviz_free(ids);
                    goto fail;
                }
            }
        }
        for (ring = 0u; ring < (uint32_t)source->radial_resolution; ++ring)
        {
            for (i = 0u; i < source->circumferential_resolution; ++i)
            {
                const uint32_t next = (i + 1u) % source->circumferential_resolution;
                const uint32_t a = ids[(FVizSize)ring * points_per_ring + (FVizSize)i];
                const uint32_t b = ids[(FVizSize)ring * points_per_ring + (FVizSize)next];
                const uint32_t c = ids[((FVizSize)ring + 1u) * points_per_ring + (FVizSize)next];
                const uint32_t d = ids[((FVizSize)ring + 1u) * points_per_ring + (FVizSize)i];
                if (fviz_poly_data_add_triangle(output, a, b, c) != FVIZ_OK ||
                    fviz_poly_data_add_triangle(output, a, c, d) != FVIZ_OK)
                {
                    fviz_free(ids);
                    goto fail;
                }
            }
        }
        fviz_free(ids);
    }
    if (fviz_poly_data_compute_normals(output) != FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_disk_source_create(FVizDiskSource** out_source)
{
    FVizDiskSource* source;
    FVizAlgorithmCallbacks callbacks;
    if (out_source == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_source must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_source = NULL;
    source = (FVizDiskSource*)fviz_internal_object_allocate(sizeof(*source), &g_fviz_disk_source_class, NULL);
    if (source == NULL) return fviz_last_error_code();
    source->inner_radius = 0.0;
    source->outer_radius = 0.5;
    source->radial_resolution = 1u;
    source->circumferential_resolution = 16u;
    source->center = fviz_vec3(0.0f, 0.0f, 0.0f);
    source->normal = fviz_vec3(0.0f, 0.0f, 1.0f);
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_disk_source_process_request;
    callbacks.get_state_mtime = fviz_disk_source_state_mtime;
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

void fviz_disk_source_set_inner_radius(FVizDiskSource* source, double inner_radius)
{
    if (source == NULL) return;
    source->inner_radius = inner_radius;
    fviz_object_modified((FVizObject*)source);
}

void fviz_disk_source_set_outer_radius(FVizDiskSource* source, double outer_radius)
{
    if (source == NULL) return;
    source->outer_radius = outer_radius;
    fviz_object_modified((FVizObject*)source);
}

void fviz_disk_source_set_radial_resolution(FVizDiskSource* source, uint32_t radial_resolution)
{
    if (source == NULL) return;
    source->radial_resolution = radial_resolution;
    fviz_object_modified((FVizObject*)source);
}

void fviz_disk_source_set_circumferential_resolution(FVizDiskSource* source, uint32_t circumferential_resolution)
{
    if (source == NULL) return;
    source->circumferential_resolution = circumferential_resolution > 3u ? circumferential_resolution : 3u;
    fviz_object_modified((FVizObject*)source);
}

void fviz_disk_source_set_center(FVizDiskSource* source, FVizVec3 center)
{
    if (source == NULL) return;
    source->center = center;
    fviz_object_modified((FVizObject*)source);
}

void fviz_disk_source_set_normal(FVizDiskSource* source, FVizVec3 normal)
{
    if (source == NULL) return;
    source->normal = fviz_vec3_normalize(normal);
    fviz_object_modified((FVizObject*)source);
}

double fviz_disk_source_inner_radius(const FVizDiskSource* source)
{
    return source != NULL ? source->inner_radius : 0.0;
}

double fviz_disk_source_outer_radius(const FVizDiskSource* source)
{
    return source != NULL ? source->outer_radius : 0.0;
}

uint32_t fviz_disk_source_radial_resolution(const FVizDiskSource* source)
{
    return source != NULL ? source->radial_resolution : 0u;
}

uint32_t fviz_disk_source_circumferential_resolution(const FVizDiskSource* source)
{
    return source != NULL ? source->circumferential_resolution : 0u;
}

FVizVec3 fviz_disk_source_center(const FVizDiskSource* source)
{
    return source != NULL ? source->center : fviz_vec3(0, 0, 0);
}

FVizVec3 fviz_disk_source_normal(const FVizDiskSource* source)
{
    return source != NULL ? source->normal : fviz_vec3(0, 0, 1);
}

FVizAlgorithm* fviz_disk_source_algorithm(FVizDiskSource* source)
{
    return source != NULL ? source->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_disk_source_output_port(FVizDiskSource* source)
{
    return source != NULL ? fviz_algorithm_output_port(source->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_disk_source_output(FVizDiskSource* source)
{
    return source != NULL ? (FVizPolyData*)fviz_algorithm_output_data(source->algorithm, 0u) : NULL;
}

FVizResult fviz_disk_source_update(FVizDiskSource* source)
{
    return source != NULL ? fviz_algorithm_update(source->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
