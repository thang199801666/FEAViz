#include <float.h>
#include <math.h>

#include <FViz/Algorithms/FVizArrowSource.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizArrowSource
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    double shaft_radius;
    double tip_radius;
    double tip_length;
    uint32_t radial_resolution;
};

static void fviz_arrow_source_destroy(FVizObject* object)
{
    FVizArrowSource* source = (FVizArrowSource*)object;
    fviz_release(source->algorithm);
    source->algorithm = NULL;
}

static const FVizObjectClass g_fviz_arrow_source_class = {
    FVIZ_TYPE_ARROW_SOURCE, "FVizArrowSource", &g_fviz_object_class,
    fviz_arrow_source_destroy, NULL
};

static FVizMTime fviz_arrow_source_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static FVizResult fviz_arrow_source_process_request(
    FVizAlgorithm* algorithm,
    const FVizPipelineRequestInfo* request,
    void* state)
{
    FVizArrowSource* source = (FVizArrowSource*)state;
    FVizPolyData* output = NULL;
    FVizVec3* points = NULL;
    uint32_t* triangles = NULL;
    const uint32_t n = source->radial_resolution;
    const double pi = 3.1415926535897932384626433832795;
    const double shaft_end = 1.0 - source->tip_length;
    FVizSize point_count;
    FVizSize triangle_count;
    FVizSize bytes;
    FVizSize ti = 0u;
    uint32_t i;
    uint32_t rear_center;
    uint32_t rear_ring;
    uint32_t front_ring;
    uint32_t tip_ring;
    uint32_t apex;

    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    point_count = (FVizSize)3u * n + 2u;
    triangle_count = (FVizSize)6u * n;
    if (point_count > UINT32_MAX) return FVIZ_ERROR_OVERFLOW;
    if (fviz_size_multiply(point_count, sizeof(*points), &bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    points = (FVizVec3*)fviz_alloc(bytes);
    if (triangle_count > (FVizSize)-1 / 3u ||
        fviz_size_multiply(triangle_count * 3u, sizeof(*triangles), &bytes) != FVIZ_OK)
    {
        fviz_free(points);
        return FVIZ_ERROR_OVERFLOW;
    }
    triangles = (uint32_t*)fviz_alloc(bytes);
    if (points == NULL || triangles == NULL) goto fail;

    rear_center = 0u;
    rear_ring = 1u;
    front_ring = rear_ring + n;
    tip_ring = front_ring + n;
    apex = tip_ring + n;
    points[rear_center] = fviz_vec3(0.0f, 0.0f, 0.0f);
    for (i = 0u; i < n; ++i)
    {
        const double angle = 2.0 * pi * (double)i / (double)n;
        const float c = (float)cos(angle);
        const float s = (float)sin(angle);
        points[rear_ring + i] = fviz_vec3(0.0f,
            (float)source->shaft_radius * c,
            (float)source->shaft_radius * s);
        points[front_ring + i] = fviz_vec3((float)shaft_end,
            (float)source->shaft_radius * c,
            (float)source->shaft_radius * s);
        points[tip_ring + i] = fviz_vec3((float)shaft_end,
            (float)source->tip_radius * c,
            (float)source->tip_radius * s);
    }
    points[apex] = fviz_vec3(1.0f, 0.0f, 0.0f);

#define TRI(a,b,c) do { triangles[ti++] = (a); triangles[ti++] = (b); triangles[ti++] = (c); } while (0)
    for (i = 0u; i < n; ++i)
    {
        const uint32_t next = (i + 1u) % n;
        /* Rear cap faces -X. */
        TRI(rear_center, rear_ring + next, rear_ring + i);
        /* Shaft wall. */
        TRI(rear_ring + i, rear_ring + next, front_ring + next);
        TRI(rear_ring + i, front_ring + next, front_ring + i);
        /* Shoulder annulus faces +X. */
        TRI(front_ring + i, front_ring + next, tip_ring + next);
        TRI(front_ring + i, tip_ring + next, tip_ring + i);
        /* Cone. */
        TRI(tip_ring + i, tip_ring + next, apex);
    }
#undef TRI

    if (ti != triangle_count * 3u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "arrow source generated inconsistent topology");
        goto fail;
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_reserve(output, point_count, triangle_count) != FVIZ_OK ||
        fviz_poly_data_add_points(output, points, point_count, NULL) != FVIZ_OK ||
        fviz_poly_data_add_triangles(output, triangles, triangle_count) != FVIZ_OK ||
        fviz_poly_data_compute_normals(output) != FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port,
            (FVizDataObject*)output) != FVIZ_OK)
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

FVizResult fviz_arrow_source_create(FVizArrowSource** out_source)
{
    FVizArrowSource* source;
    FVizAlgorithmCallbacks callbacks;
    if (out_source == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_source must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_source = NULL;
    source = (FVizArrowSource*)fviz_internal_object_allocate(
        sizeof(*source), &g_fviz_arrow_source_class, NULL);
    if (source == NULL) return fviz_last_error_code();
    source->shaft_radius = 0.03;
    source->tip_radius = 0.08;
    source->tip_length = 0.25;
    source->radial_resolution = 12u;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_arrow_source_process_request;
    callbacks.get_state_mtime = fviz_arrow_source_state_mtime;
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

static FVizResult fviz_arrow_source_set_positive(
    FVizArrowSource* source, double value, double* target, const char* name)
{
    if (source == NULL || target == NULL || !isfinite(value) || value <= 0.0 || value > FLT_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, name);
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (*target != value)
    {
        *target = value;
        fviz_object_modified((FVizObject*)source);
    }
    return FVIZ_OK;
}

FVizResult fviz_arrow_source_set_shaft_radius(FVizArrowSource* source, double radius)
{
    return fviz_arrow_source_set_positive(source, radius,
        source != NULL ? &source->shaft_radius : NULL, "arrow shaft radius must be positive");
}

FVizResult fviz_arrow_source_set_tip_radius(FVizArrowSource* source, double radius)
{
    return fviz_arrow_source_set_positive(source, radius,
        source != NULL ? &source->tip_radius : NULL, "arrow tip radius must be positive");
}

FVizResult fviz_arrow_source_set_tip_length(FVizArrowSource* source, double length)
{
    if (source == NULL || !isfinite(length) || length <= 0.0 || length >= 1.0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "arrow tip length must be between zero and one");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (source->tip_length != length)
    {
        source->tip_length = length;
        fviz_object_modified((FVizObject*)source);
    }
    return FVIZ_OK;
}

FVizResult fviz_arrow_source_set_radial_resolution(FVizArrowSource* source, uint32_t resolution)
{
    if (source == NULL || resolution < 3u || resolution > 1000000u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "arrow radial resolution must be at least three");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (source->radial_resolution != resolution)
    {
        source->radial_resolution = resolution;
        fviz_object_modified((FVizObject*)source);
    }
    return FVIZ_OK;
}

double fviz_arrow_source_shaft_radius(const FVizArrowSource* source) { return source != NULL ? source->shaft_radius : 0.0; }
double fviz_arrow_source_tip_radius(const FVizArrowSource* source) { return source != NULL ? source->tip_radius : 0.0; }
double fviz_arrow_source_tip_length(const FVizArrowSource* source) { return source != NULL ? source->tip_length : 0.0; }
uint32_t fviz_arrow_source_radial_resolution(const FVizArrowSource* source) { return source != NULL ? source->radial_resolution : 0u; }
FVizAlgorithm* fviz_arrow_source_algorithm(FVizArrowSource* source) { return source != NULL ? source->algorithm : NULL; }
FVizAlgorithmOutput* fviz_arrow_source_output_port(FVizArrowSource* source) { return source != NULL ? fviz_algorithm_output_port(source->algorithm, 0u) : NULL; }
FVizPolyData* fviz_arrow_source_output(FVizArrowSource* source) { return source != NULL ? (FVizPolyData*)fviz_algorithm_output_data(source->algorithm, 0u) : NULL; }
FVizResult fviz_arrow_source_update(FVizArrowSource* source) { return source != NULL ? fviz_algorithm_update(source->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT; }
