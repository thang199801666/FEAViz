#include <float.h>
#include <math.h>

#include <FViz/Algorithms/FVizCylinderSource.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizCylinderSource
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    double height;
    double radius;
    uint32_t resolution;
    FVizVec3 center;
    FVizVec3 direction;
    FVizBool capping;
};

static void fviz_cylinder_source_destroy(FVizObject* object)
{
    FVizCylinderSource* source = (FVizCylinderSource*)object;
    fviz_release(source->algorithm);
    source->algorithm = NULL;
}

static const FVizObjectClass g_fviz_cylinder_source_class = {
    FVIZ_TYPE_CYLINDER_SOURCE, "FVizCylinderSource", &g_fviz_object_class, fviz_cylinder_source_destroy, NULL};

static FVizMTime fviz_cylinder_source_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static void fviz_cylinder_source_build_basis(FVizVec3 direction, FVizVec3* out_right, FVizVec3* out_up)
{
    const FVizVec3 d = fviz_vec3_normalize(direction);
    FVizVec3 ref = fviz_vec3(0.0f, 0.0f, 1.0f);
    FVizVec3 right;
    FVizVec3 up;
    if (fviz_vec3_dot(d, ref) > 0.99f)
        ref = fviz_vec3(0.0f, 1.0f, 0.0f);
    right = fviz_vec3_normalize(fviz_vec3_cross(d, ref));
    up = fviz_vec3_normalize(fviz_vec3_cross(right, d));
    *out_right = right;
    *out_up = up;
}

static FVizResult fviz_cylinder_source_process_request(FVizAlgorithm* algorithm,
                                                       const FVizPipelineRequestInfo* request, void* state)
{
    FVizCylinderSource* source = (FVizCylinderSource*)state;
    FVizPolyData* output = NULL;
    FVizVec3 right;
    FVizVec3 up;
    FVizVec3 top_center;
    FVizVec3 base_center;
    uint32_t i;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (source->resolution < 3u || source->height <= 0.0 || source->radius <= 0.0)
    {
        fviz_internal_set_error(
            FVIZ_ERROR_INVALID_ARGUMENT, "cylinder source requires resolution>=3, positive height and radius");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_reserve(output, (FVizSize)source->resolution * 2u, (FVizSize)source->resolution * 2u) != FVIZ_OK)
        goto fail;
    fviz_cylinder_source_build_basis(source->direction, &right, &up);
    top_center = fviz_vec3_add(source->center, fviz_vec3_scale(source->direction, (float)source->height));
    base_center = source->center;
    {
        uint32_t* top_ids = NULL;
        uint32_t* base_ids = NULL;
        top_ids = (uint32_t*)fviz_alloc((FVizSize)source->resolution * (FVizSize)sizeof(uint32_t));
        base_ids = (uint32_t*)fviz_alloc((FVizSize)source->resolution * (FVizSize)sizeof(uint32_t));
        if (top_ids == NULL || base_ids == NULL)
        {
            fviz_free(top_ids);
            fviz_free(base_ids);
            goto fail;
        }
        for (i = 0u; i < source->resolution; ++i)
        {
            const double angle = (double)i * 2.0 * 3.14159265358979323846 / (double)source->resolution;
            const float r = (float)source->radius;
            const FVizVec3 offset = fviz_vec3_add(
                fviz_vec3_scale(right, r * (float)cos(angle)), fviz_vec3_scale(up, r * (float)sin(angle)));
            if (fviz_poly_data_add_point(output, fviz_vec3_add(base_center, offset), &base_ids[i]) != FVIZ_OK ||
                fviz_poly_data_add_point(output, fviz_vec3_add(top_center, offset), &top_ids[i]) != FVIZ_OK)
            {
                fviz_free(top_ids);
                fviz_free(base_ids);
                goto fail;
            }
        }
        for (i = 0u; i < source->resolution; ++i)
        {
            const uint32_t next = (i + 1u) % source->resolution;
            if (fviz_poly_data_add_triangle(output, base_ids[i], top_ids[i], top_ids[next]) != FVIZ_OK ||
                fviz_poly_data_add_triangle(output, base_ids[i], top_ids[next], base_ids[next]) != FVIZ_OK)
            {
                fviz_free(top_ids);
                fviz_free(base_ids);
                goto fail;
            }
        }
        if (source->capping != FVIZ_FALSE)
        {
            for (i = 1u; i + 1u < source->resolution; ++i)
            {
                if (fviz_poly_data_add_triangle(output, base_ids[0], base_ids[i + 1u], base_ids[i]) != FVIZ_OK ||
                    fviz_poly_data_add_triangle(output, top_ids[0], top_ids[i], top_ids[i + 1u]) != FVIZ_OK)
                {
                    fviz_free(top_ids);
                    fviz_free(base_ids);
                    goto fail;
                }
            }
        }
        fviz_free(top_ids);
        fviz_free(base_ids);
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

FVizResult fviz_cylinder_source_create(FVizCylinderSource** out_source)
{
    FVizCylinderSource* source;
    FVizAlgorithmCallbacks callbacks;
    if (out_source == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_source must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_source = NULL;
    source = (FVizCylinderSource*)fviz_internal_object_allocate(sizeof(*source), &g_fviz_cylinder_source_class, NULL);
    if (source == NULL) return fviz_last_error_code();
    source->height = 1.0;
    source->radius = 0.5;
    source->resolution = 12u;
    source->center = fviz_vec3(0.0f, 0.0f, 0.0f);
    source->direction = fviz_vec3(0.0f, 0.0f, 1.0f);
    source->capping = FVIZ_TRUE;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_cylinder_source_process_request;
    callbacks.get_state_mtime = fviz_cylinder_source_state_mtime;
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

void fviz_cylinder_source_set_height(FVizCylinderSource* source, double height)
{
    if (source == NULL) return;
    source->height = height;
    fviz_object_modified((FVizObject*)source);
}

void fviz_cylinder_source_set_radius(FVizCylinderSource* source, double radius)
{
    if (source == NULL) return;
    source->radius = radius;
    fviz_object_modified((FVizObject*)source);
}

void fviz_cylinder_source_set_resolution(FVizCylinderSource* source, uint32_t resolution)
{
    if (source == NULL) return;
    source->resolution = resolution > 3u ? resolution : 3u;
    fviz_object_modified((FVizObject*)source);
}

void fviz_cylinder_source_set_center(FVizCylinderSource* source, FVizVec3 center)
{
    if (source == NULL) return;
    source->center = center;
    fviz_object_modified((FVizObject*)source);
}

void fviz_cylinder_source_set_direction(FVizCylinderSource* source, FVizVec3 direction)
{
    if (source == NULL) return;
    source->direction = fviz_vec3_normalize(direction);
    fviz_object_modified((FVizObject*)source);
}

FVizResult fviz_cylinder_source_set_capping(FVizCylinderSource* source, FVizBool capping)
{
    if (source == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    source->capping = capping != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_object_modified((FVizObject*)source);
    return FVIZ_OK;
}

double fviz_cylinder_source_height(const FVizCylinderSource* source)
{
    return source != NULL ? source->height : 0.0;
}

double fviz_cylinder_source_radius(const FVizCylinderSource* source)
{
    return source != NULL ? source->radius : 0.0;
}

uint32_t fviz_cylinder_source_resolution(const FVizCylinderSource* source)
{
    return source != NULL ? source->resolution : 0u;
}

FVizVec3 fviz_cylinder_source_center(const FVizCylinderSource* source)
{
    return source != NULL ? source->center : fviz_vec3(0, 0, 0);
}

FVizVec3 fviz_cylinder_source_direction(const FVizCylinderSource* source)
{
    return source != NULL ? source->direction : fviz_vec3(0, 0, 1);
}

FVizBool fviz_cylinder_source_capping(const FVizCylinderSource* source)
{
    return source != NULL && source->capping != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizAlgorithm* fviz_cylinder_source_algorithm(FVizCylinderSource* source)
{
    return source != NULL ? source->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_cylinder_source_output_port(FVizCylinderSource* source)
{
    return source != NULL ? fviz_algorithm_output_port(source->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_cylinder_source_output(FVizCylinderSource* source)
{
    return source != NULL ? (FVizPolyData*)fviz_algorithm_output_data(source->algorithm, 0u) : NULL;
}

FVizResult fviz_cylinder_source_update(FVizCylinderSource* source)
{
    return source != NULL ? fviz_algorithm_update(source->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
