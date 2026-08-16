#include <float.h>
#include <math.h>

#include <FViz/Algorithms/FVizCubeSource.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizCubeSource
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizVec3 center;
    double x_length;
    double y_length;
    double z_length;
};

static void fviz_cube_source_destroy(FVizObject* object)
{
    FVizCubeSource* source = (FVizCubeSource*)object;
    fviz_release(source->algorithm);
    source->algorithm = NULL;
}

static const FVizObjectClass g_fviz_cube_source_class = {FVIZ_TYPE_CUBE_SOURCE, "FVizCubeSource", &g_fviz_object_class,
                                                         fviz_cube_source_destroy, NULL};

static FVizMTime fviz_cube_source_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static FVizResult fviz_cube_source_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                   void* state)
{
    FVizCubeSource* source = (FVizCubeSource*)state;
    FVizPolyData* output = NULL;
    const float hx = (float)(source->x_length * 0.5);
    const float hy = (float)(source->y_length * 0.5);
    const float hz = (float)(source->z_length * 0.5);
    const FVizVec3 c = source->center;
    const FVizVec3 corners[8] = {{c.x - hx, c.y - hy, c.z - hz}, {c.x + hx, c.y - hy, c.z - hz},
                                 {c.x + hx, c.y + hy, c.z - hz}, {c.x - hx, c.y + hy, c.z - hz},
                                 {c.x - hx, c.y - hy, c.z + hz}, {c.x + hx, c.y - hy, c.z + hz},
                                 {c.x + hx, c.y + hy, c.z + hz}, {c.x - hx, c.y + hy, c.z + hz}};
    static const uint32_t faces[6][4] = {{0u, 3u, 2u, 1u}, {4u, 5u, 6u, 7u}, {0u, 1u, 5u, 4u},
                                         {3u, 7u, 6u, 2u}, {0u, 4u, 7u, 3u}, {1u, 2u, 6u, 5u}};
    uint32_t face;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (fviz_poly_data_create(&output) != FVIZ_OK || fviz_poly_data_reserve(output, 24u, 12u) != FVIZ_OK) goto fail;
    for (face = 0u; face < 6u; ++face)
    {
        uint32_t ids[4];
        uint32_t i;
        for (i = 0u; i < 4u; ++i)
            if (fviz_poly_data_add_point(output, corners[faces[face][i]], &ids[i]) != FVIZ_OK) goto fail;
        if (fviz_poly_data_add_triangle(output, ids[0], ids[1], ids[2]) != FVIZ_OK ||
            fviz_poly_data_add_triangle(output, ids[0], ids[2], ids[3]) != FVIZ_OK)
            goto fail;
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

FVizResult fviz_cube_source_create(FVizCubeSource** out_source)
{
    FVizCubeSource* source;
    FVizAlgorithmCallbacks callbacks;
    if (out_source == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_source must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_source = NULL;
    source = (FVizCubeSource*)fviz_internal_object_allocate(sizeof(*source), &g_fviz_cube_source_class, NULL);
    if (source == NULL) return fviz_last_error_code();
    source->center = fviz_vec3(0.0f, 0.0f, 0.0f);
    source->x_length = source->y_length = source->z_length = 1.0;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_cube_source_process_request;
    callbacks.get_state_mtime = fviz_cube_source_state_mtime;
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

void fviz_cube_source_set_center(FVizCubeSource* source, FVizVec3 center)
{
    if (source == NULL) return;
    source->center = center;
    fviz_object_modified((FVizObject*)source);
}

FVizResult fviz_cube_source_set_lengths(FVizCubeSource* source, double x_length, double y_length, double z_length)
{
    if (source == NULL || !isfinite(x_length) || !isfinite(y_length) || !isfinite(z_length) || x_length <= 0.0 ||
        y_length <= 0.0 || z_length <= 0.0 || x_length > FLT_MAX || y_length > FLT_MAX || z_length > FLT_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cube lengths must be positive");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    source->x_length = x_length;
    source->y_length = y_length;
    source->z_length = z_length;
    fviz_object_modified((FVizObject*)source);
    return FVIZ_OK;
}

FVizResult fviz_cube_source_set_bounds(FVizCubeSource* source, FVizBounds bounds)
{
    const double x_length = (double)(bounds.max.x - bounds.min.x);
    const double y_length = (double)(bounds.max.y - bounds.min.y);
    const double z_length = (double)(bounds.max.z - bounds.min.z);
    if (source == NULL || bounds.valid == FVIZ_FALSE || x_length <= 0.0 || y_length <= 0.0 || z_length <= 0.0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cube bounds must have positive extents");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    source->center = fviz_vec3(0.5f * (bounds.min.x + bounds.max.x), 0.5f * (bounds.min.y + bounds.max.y),
                               0.5f * (bounds.min.z + bounds.max.z));
    source->x_length = x_length;
    source->y_length = y_length;
    source->z_length = z_length;
    fviz_object_modified((FVizObject*)source);
    return FVIZ_OK;
}

FVizVec3 fviz_cube_source_center(const FVizCubeSource* source)
{
    return source != NULL ? source->center : fviz_vec3(0, 0, 0);
}

double fviz_cube_source_x_length(const FVizCubeSource* source)
{
    return source != NULL ? source->x_length : 0.0;
}

double fviz_cube_source_y_length(const FVizCubeSource* source)
{
    return source != NULL ? source->y_length : 0.0;
}

double fviz_cube_source_z_length(const FVizCubeSource* source)
{
    return source != NULL ? source->z_length : 0.0;
}

FVizAlgorithm* fviz_cube_source_algorithm(FVizCubeSource* source)
{
    return source != NULL ? source->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_cube_source_output_port(FVizCubeSource* source)
{
    return source != NULL ? fviz_algorithm_output_port(source->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_cube_source_output(FVizCubeSource* source)
{
    return source != NULL ? (FVizPolyData*)fviz_algorithm_output_data(source->algorithm, 0u) : NULL;
}

FVizResult fviz_cube_source_update(FVizCubeSource* source)
{
    return source != NULL ? fviz_algorithm_update(source->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
