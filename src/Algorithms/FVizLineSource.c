#include <float.h>
#include <math.h>

#include <FViz/Algorithms/FVizLineSource.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizLineSource
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizVec3 point0;
    FVizVec3 point1;
    uint32_t resolution;
};

static void fviz_line_source_destroy(FVizObject* object)
{
    FVizLineSource* source = (FVizLineSource*)object;
    fviz_release(source->algorithm);
    source->algorithm = NULL;
}

static const FVizObjectClass g_fviz_line_source_class = {FVIZ_TYPE_LINE_SOURCE, "FVizLineSource", &g_fviz_object_class,
                                                         fviz_line_source_destroy, NULL};

static FVizMTime fviz_line_source_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static FVizResult fviz_line_source_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                   void* state)
{
    FVizLineSource* source = (FVizLineSource*)state;
    FVizPolyData* output = NULL;
    uint32_t i;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    if (source->resolution < 1u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "line source requires resolution>=1");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_reserve(output, (FVizSize)source->resolution + 1u, (FVizSize)source->resolution) != FVIZ_OK)
        goto fail;
    for (i = 0u; i <= source->resolution; ++i)
    {
        const float t = (float)i / (float)source->resolution;
        const FVizVec3 point = fviz_vec3(
            source->point0.x + (source->point1.x - source->point0.x) * t,
            source->point0.y + (source->point1.y - source->point0.y) * t,
            source->point0.z + (source->point1.z - source->point0.z) * t);
        uint32_t id = 0u;
        if (fviz_poly_data_add_point(output, point, &id) != FVIZ_OK) goto fail;
        if (i > 0u)
        {
            if (fviz_poly_data_add_line(output, id - 1u, id) != FVIZ_OK) goto fail;
        }
    }
    if (fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_line_source_create(FVizLineSource** out_source)
{
    FVizLineSource* source;
    FVizAlgorithmCallbacks callbacks;
    if (out_source == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_source must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_source = NULL;
    source = (FVizLineSource*)fviz_internal_object_allocate(sizeof(*source), &g_fviz_line_source_class, NULL);
    if (source == NULL) return fviz_last_error_code();
    source->point0 = fviz_vec3(0.0f, 0.0f, 0.0f);
    source->point1 = fviz_vec3(1.0f, 0.0f, 0.0f);
    source->resolution = 1u;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_line_source_process_request;
    callbacks.get_state_mtime = fviz_line_source_state_mtime;
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

FVizResult fviz_line_source_set_points(FVizLineSource* source, FVizVec3 point0, FVizVec3 point1)
{
    if (source == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    source->point0 = point0;
    source->point1 = point1;
    fviz_object_modified((FVizObject*)source);
    return FVIZ_OK;
}

FVizResult fviz_line_source_set_resolution(FVizLineSource* source, uint32_t resolution)
{
    if (source == NULL || resolution == 0u) return FVIZ_ERROR_INVALID_ARGUMENT;
    source->resolution = resolution;
    fviz_object_modified((FVizObject*)source);
    return FVIZ_OK;
}

FVizVec3 fviz_line_source_point0(const FVizLineSource* source)
{
    return source != NULL ? source->point0 : fviz_vec3(0, 0, 0);
}

FVizVec3 fviz_line_source_point1(const FVizLineSource* source)
{
    return source != NULL ? source->point1 : fviz_vec3(0, 0, 0);
}

uint32_t fviz_line_source_resolution(const FVizLineSource* source)
{
    return source != NULL ? source->resolution : 0u;
}

FVizAlgorithm* fviz_line_source_algorithm(FVizLineSource* source)
{
    return source != NULL ? source->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_line_source_output_port(FVizLineSource* source)
{
    return source != NULL ? fviz_algorithm_output_port(source->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_line_source_output(FVizLineSource* source)
{
    return source != NULL ? (FVizPolyData*)fviz_algorithm_output_data(source->algorithm, 0u) : NULL;
}

FVizResult fviz_line_source_update(FVizLineSource* source)
{
    return source != NULL ? fviz_algorithm_update(source->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
