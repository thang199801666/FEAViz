#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Algorithms/FVizGeometryFilters.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

#ifndef FVIZ_PI
#define FVIZ_PI 3.14159265358979323846
#endif

struct FVizTriangleFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizBool pass_verts;
    FVizBool pass_lines;
};

struct FVizPolyDataNormalsFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    char array_name[64];
};

struct FVizFeatureEdgesFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    double feature_angle;
    FVizBool boundary_edges;
    FVizBool feature_edges;
    FVizBool non_manifold_edges;
    FVizBool manifold_edges;
};

struct FVizPolyDataConnectivityFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizConnectivityExtractionMode extraction_mode;
    char array_name[64];
    uint32_t region_count;
};

typedef struct FVizEdgeRecord
{
    uint32_t key0;
    uint32_t key1;
    FVizVec3 normal;
} FVizEdgeRecord;

static FVizMTime fviz_geometry_filter_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static FVizResult fviz_geometry_copy_attributes(const FVizAttributeSet* source, FVizAttributeSet* destination,
                                                FVizSize required_tuple_count, FVizBool require_tuple_count)
{
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        const FVizDataArray* array = fviz_attribute_set_const_array_at(source, i);
        FVizDataArray* copy = NULL;
        FVizAttributeRole role;
        if (require_tuple_count != FVIZ_FALSE && fviz_data_array_tuple_count(array) != required_tuple_count) continue;
        if (fviz_data_array_deep_copy(array, &copy) != FVIZ_OK ||
            fviz_attribute_set_add(destination, name, copy) != FVIZ_OK)
        {
            fviz_release(copy);
            return fviz_last_error_code();
        }
        fviz_release(copy);
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(destination, role, name);
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_geometry_copy_points_and_data(const FVizPolyData* input, FVizPolyData* output)
{
    const FVizVec3* points = fviz_poly_data_points(input);
    const FVizDataArray* scalars = fviz_poly_data_const_scalars(input);
    if (fviz_poly_data_reserve(output, fviz_poly_data_point_count(input), 0u) != FVIZ_OK ||
        fviz_poly_data_add_points_ids(output, points, fviz_poly_data_point_count(input), NULL) != FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_geometry_copy_attributes(fviz_poly_data_const_point_data(input), fviz_poly_data_point_data(output),
                                      fviz_poly_data_point_count(input), FVIZ_TRUE) != FVIZ_OK ||
        fviz_geometry_copy_attributes(fviz_poly_data_const_field_data(input), fviz_poly_data_field_data(output), 0u,
                                      FVIZ_FALSE) != FVIZ_OK)
        return fviz_last_error_code();
    if (scalars != NULL)
    {
        FVizDataArray* copy = NULL;
        if (fviz_data_array_deep_copy(scalars, &copy) != FVIZ_OK || fviz_poly_data_set_scalars(output, copy) != FVIZ_OK)
        {
            fviz_release(copy);
            return fviz_last_error_code();
        }
        fviz_release(copy);
    }
    return FVIZ_OK;
}

static FVizResult fviz_append_source_id(FVizDataArray* provenance, FVizSize source_id)
{
    const uint64_t value = (uint64_t)source_id;
    return fviz_data_array_append_tuple(provenance, &value);
}

static FVizResult fviz_triangle_copy_cell_data(const FVizPolyData* input, FVizPolyData* output,
                                               const FVizDataArray* provenance)
{
    const FVizAttributeSet* source_set = fviz_poly_data_const_cell_data(input);
    const FVizSize source_cell_count = fviz_poly_data_cell_count(input);
    FVizSize array_index;
    FVizSize tuple_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source_set); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source_set, array_index);
        const FVizDataArray* source = fviz_attribute_set_const_array_at(source_set, array_index);
        FVizDataArray* destination = NULL;
        FVizAttributeRole role;
        if (strcmp(name, "FVizOriginalCellIds") == 0 || fviz_data_array_tuple_count(source) != source_cell_count)
            continue;
        if (fviz_data_array_create(fviz_data_array_type(source), fviz_data_array_components(source), &destination) !=
                FVIZ_OK ||
            fviz_data_array_reserve(destination, fviz_data_array_tuple_count(provenance)) != FVIZ_OK)
            goto fail;
        for (tuple_index = 0u; tuple_index < fviz_data_array_tuple_count(provenance); ++tuple_index)
        {
            double source_id_value = 0.0;
            FVizSize source_id;
            if (fviz_data_array_get_component(provenance, tuple_index, 0u, &source_id_value) != FVIZ_OK) goto fail;
            source_id = (FVizSize)source_id_value;
            if (source_id >= source_cell_count ||
                fviz_data_array_append_tuple(destination, fviz_data_array_const_tuple(source, source_id)) != FVIZ_OK)
                goto fail;
        }
        if (fviz_attribute_set_add(fviz_poly_data_cell_data(output), name, destination) != FVIZ_OK) goto fail;
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source_set, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(fviz_poly_data_cell_data(output), role, name);
        }
        fviz_release(destination);
        continue;
    fail:
        fviz_release(destination);
        return fviz_last_error_code();
    }
    {
        const FVizDataArray* upstream_ids = fviz_attribute_set_const_get(source_set, "FVizOriginalCellIds");
        FVizDataArray* ids = NULL;
        FVizBool failed = FVIZ_FALSE;
        if (upstream_ids != NULL && fviz_data_array_tuple_count(upstream_ids) == source_cell_count)
        {
            FVizSize provenance_tuple;
            if (fviz_data_array_create(fviz_data_array_type(upstream_ids), fviz_data_array_components(upstream_ids),
                                       &ids) != FVIZ_OK ||
                fviz_data_array_reserve(ids, fviz_data_array_tuple_count(provenance)) != FVIZ_OK)
            {
                failed = FVIZ_TRUE;
            }
            for (provenance_tuple = 0u;
                 failed == FVIZ_FALSE && provenance_tuple < fviz_data_array_tuple_count(provenance); ++provenance_tuple)
            {
                double source_id_value = 0.0;
                FVizSize source_id;
                if (fviz_data_array_get_component(provenance, provenance_tuple, 0u, &source_id_value) != FVIZ_OK)
                {
                    failed = FVIZ_TRUE;
                    break;
                }
                source_id = (FVizSize)source_id_value;
                if (source_id >= source_cell_count ||
                    fviz_data_array_append_tuple(ids, fviz_data_array_const_tuple(upstream_ids, source_id)) != FVIZ_OK)
                {
                    if (source_id >= source_cell_count)
                        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                                "cell provenance references an invalid source cell");
                    failed = FVIZ_TRUE;
                }
            }
        }
        else if (fviz_data_array_deep_copy(provenance, &ids) != FVIZ_OK)
        {
            failed = FVIZ_TRUE;
        }
        if (failed == FVIZ_FALSE &&
            fviz_attribute_set_add(fviz_poly_data_cell_data(output), "FVizOriginalCellIds", ids) != FVIZ_OK)
            failed = FVIZ_TRUE;
        fviz_release(ids);
        if (failed != FVIZ_FALSE) return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static void fviz_triangle_filter_destroy(FVizObject* object)
{
    FVizTriangleFilter* filter = (FVizTriangleFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_triangle_filter_class = {FVIZ_TYPE_TRIANGLE_FILTER, "FVizTriangleFilter",
                                                             &g_fviz_object_class, fviz_triangle_filter_destroy, NULL};

static FVizResult fviz_triangle_filter_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                       void* state)
{
    FVizTriangleFilter* filter = (FVizTriangleFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    FVizDataArray* provenance = NULL;
    FVizSize source_base;
    FVizSize i;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL || fviz_poly_data_validate(input) != FVIZ_OK)
    {
        if (input == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "triangle filter has no input");
        return input == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK || fviz_geometry_copy_points_and_data(input, output) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &provenance) != FVIZ_OK)
        goto fail;

    source_base = 0u;
    if (filter->pass_verts != FVIZ_FALSE)
    {
        const FVizCellArray* cells = fviz_poly_data_verts(input);
        for (i = 0u; i < fviz_cell_array_count(cells); ++i)
        {
            FVizCellView view;
            FVizSize j;
            if (fviz_cell_array_cell_view(cells, i, &view) != FVIZ_OK) goto fail;
            for (j = 0u; j < view.point_count; ++j)
            {
                const FVizId id = fviz_cell_view_point_id(&view, j);
                if (fviz_poly_data_add_cell_ids(output, FVIZ_CELL_VERTEX, 1u, &id) != FVIZ_OK ||
                    fviz_append_source_id(provenance, source_base + i) != FVIZ_OK)
                    goto fail;
            }
        }
    }
    source_base += fviz_poly_data_vert_cell_count(input);

    if (filter->pass_lines != FVIZ_FALSE)
    {
        const FVizCellArray* cells = fviz_poly_data_lines(input);
        for (i = 0u; i < fviz_cell_array_count(cells); ++i)
        {
            FVizCellView view;
            FVizSize j;
            if (fviz_cell_array_cell_view(cells, i, &view) != FVIZ_OK) goto fail;
            for (j = 0u; j + 1u < view.point_count; ++j)
            {
                const FVizId segment[2] = {fviz_cell_view_point_id(&view, j), fviz_cell_view_point_id(&view, j + 1u)};
                if (fviz_poly_data_add_cell_ids(output, FVIZ_CELL_LINE, 2u, segment) != FVIZ_OK ||
                    fviz_append_source_id(provenance, source_base + i) != FVIZ_OK)
                    goto fail;
            }
        }
    }
    source_base += fviz_poly_data_line_cell_count(input);

    {
        const FVizCellArray* cells = fviz_poly_data_polys(input);
        for (i = 0u; i < fviz_cell_array_count(cells); ++i)
        {
            FVizCellView view;
            FVizSize j;
            if (fviz_cell_array_cell_view(cells, i, &view) != FVIZ_OK) goto fail;
            /* Fan triangulation matches vtkTriangleFilter for ordinary convex polygons. */
            for (j = 1u; j + 1u < view.point_count; ++j)
            {
                const FVizId tri[3] = {fviz_cell_view_point_id(&view, 0u), fviz_cell_view_point_id(&view, j),
                                       fviz_cell_view_point_id(&view, j + 1u)};
                if (fviz_poly_data_add_cell_ids(output, FVIZ_CELL_TRIANGLE, 3u, tri) != FVIZ_OK ||
                    fviz_append_source_id(provenance, source_base + i) != FVIZ_OK)
                    goto fail;
            }
        }
    }
    source_base += fviz_poly_data_poly_cell_count(input);

    {
        const FVizCellArray* cells = fviz_poly_data_strips(input);
        for (i = 0u; i < fviz_cell_array_count(cells); ++i)
        {
            FVizCellView view;
            FVizSize j;
            if (fviz_cell_array_cell_view(cells, i, &view) != FVIZ_OK) goto fail;
            for (j = 0u; j + 2u < view.point_count; ++j)
            {
                const FVizId a =
                    (j & 1u) == 0u ? fviz_cell_view_point_id(&view, j) : fviz_cell_view_point_id(&view, j + 1u);
                const FVizId b =
                    (j & 1u) == 0u ? fviz_cell_view_point_id(&view, j + 1u) : fviz_cell_view_point_id(&view, j);
                const FVizId tri[3] = {a, b, fviz_cell_view_point_id(&view, j + 2u)};
                if (fviz_poly_data_add_cell_ids(output, FVIZ_CELL_TRIANGLE, 3u, tri) != FVIZ_OK ||
                    fviz_append_source_id(provenance, source_base + i) != FVIZ_OK)
                    goto fail;
            }
        }
    }
    if (fviz_triangle_copy_cell_data(input, output, provenance) != FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_release(provenance);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_release(provenance);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_triangle_filter_create(FVizTriangleFilter** out_filter)
{
    FVizTriangleFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizTriangleFilter*)fviz_internal_object_allocate(sizeof(*filter), &g_fviz_triangle_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->pass_verts = FVIZ_TRUE;
    filter->pass_lines = FVIZ_TRUE;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_triangle_filter_process_request;
    callbacks.get_state_mtime = fviz_geometry_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) !=
            FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

void fviz_triangle_filter_set_pass_verts(FVizTriangleFilter* filter, FVizBool enabled)
{
    if (filter != NULL && filter->pass_verts != enabled)
    {
        filter->pass_verts = enabled;
        fviz_object_modified((FVizObject*)filter);
    }
}

void fviz_triangle_filter_set_pass_lines(FVizTriangleFilter* filter, FVizBool enabled)
{
    if (filter != NULL && filter->pass_lines != enabled)
    {
        filter->pass_lines = enabled;
        fviz_object_modified((FVizObject*)filter);
    }
}

FVizBool fviz_triangle_filter_pass_verts(const FVizTriangleFilter* filter)
{
    return filter != NULL ? filter->pass_verts : FVIZ_FALSE;
}

FVizBool fviz_triangle_filter_pass_lines(const FVizTriangleFilter* filter)
{
    return filter != NULL ? filter->pass_lines : FVIZ_FALSE;
}

FVizResult fviz_triangle_filter_set_input_data(FVizTriangleFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_triangle_filter_set_input_connection(FVizTriangleFilter* filter, FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_triangle_filter_algorithm(FVizTriangleFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_triangle_filter_output_port(FVizTriangleFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_triangle_filter_output(FVizTriangleFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_triangle_filter_update(FVizTriangleFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}

static void fviz_poly_data_normals_filter_destroy(FVizObject* object)
{
    FVizPolyDataNormalsFilter* filter = (FVizPolyDataNormalsFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_poly_data_normals_filter_class = {FVIZ_TYPE_POLY_DATA_NORMALS_FILTER,
                                                                      "FVizPolyDataNormalsFilter", &g_fviz_object_class,
                                                                      fviz_poly_data_normals_filter_destroy, NULL};

static FVizResult fviz_poly_data_normals_filter_process_request(FVizAlgorithm* algorithm,
                                                                const FVizPipelineRequestInfo* request, void* state)
{
    FVizPolyDataNormalsFilter* filter = (FVizPolyDataNormalsFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    FVizDataArray* normals_array = NULL;
    const FVizVec3* normals;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "poly data normals filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_poly_data_deep_copy(input, &output) != FVIZ_OK || fviz_poly_data_compute_normals(output) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_FLOAT32, 3u, &normals_array) != FVIZ_OK ||
        fviz_data_array_reserve(normals_array, fviz_poly_data_point_count(output)) != FVIZ_OK)
        goto fail;
    normals = fviz_poly_data_normals(output);
    if (fviz_data_array_append_tuples(normals_array, normals, fviz_poly_data_point_count(output)) != FVIZ_OK) goto fail;
    (void)fviz_attribute_set_remove(fviz_poly_data_point_data(output), filter->array_name);
    if (fviz_attribute_set_add(fviz_poly_data_point_data(output), filter->array_name, normals_array) != FVIZ_OK ||
        fviz_attribute_set_set_active(fviz_poly_data_point_data(output), FVIZ_ATTRIBUTE_NORMALS, filter->array_name) !=
            FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_release(normals_array);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_release(normals_array);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_poly_data_normals_filter_create(FVizPolyDataNormalsFilter** out_filter)
{
    FVizPolyDataNormalsFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizPolyDataNormalsFilter*)fviz_internal_object_allocate(sizeof(*filter),
                                                                       &g_fviz_poly_data_normals_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    (void)memcpy(filter->array_name, "Normals", sizeof("Normals"));
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_poly_data_normals_filter_process_request;
    callbacks.get_state_mtime = fviz_geometry_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) !=
            FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_poly_data_normals_filter_set_array_name(FVizPolyDataNormalsFilter* filter, const char* name)
{
    const FVizSize length = name != NULL ? strlen(name) : 0u;
    if (filter == NULL || name == NULL || length == 0u || length >= sizeof(filter->array_name))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "normal array name must contain 1..63 characters");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (strcmp(filter->array_name, name) != 0)
    {
        (void)memcpy(filter->array_name, name, length + 1u);
        fviz_object_modified((FVizObject*)filter);
    }
    return FVIZ_OK;
}

const char* fviz_poly_data_normals_filter_array_name(const FVizPolyDataNormalsFilter* filter)
{
    return filter != NULL ? filter->array_name : NULL;
}

FVizResult fviz_poly_data_normals_filter_set_input_data(FVizPolyDataNormalsFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_poly_data_normals_filter_set_input_connection(FVizPolyDataNormalsFilter* filter,
                                                              FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_poly_data_normals_filter_algorithm(FVizPolyDataNormalsFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_poly_data_normals_filter_output_port(FVizPolyDataNormalsFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_poly_data_normals_filter_output(FVizPolyDataNormalsFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_poly_data_normals_filter_update(FVizPolyDataNormalsFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}

static FVizSize fviz_connectivity_find(FVizSize* parent, FVizSize value)
{
    FVizSize root = value;
    while (parent[root] != root)
        root = parent[root];
    while (parent[value] != value)
    {
        const FVizSize next = parent[value];
        parent[value] = root;
        value = next;
    }
    return root;
}

static void fviz_connectivity_union(FVizSize* parent, uint8_t* rank, FVizSize a, FVizSize b)
{
    FVizSize root_a = fviz_connectivity_find(parent, a);
    FVizSize root_b = fviz_connectivity_find(parent, b);
    if (root_a == root_b) return;
    if (rank[root_a] < rank[root_b]) parent[root_a] = root_b;
    else if (rank[root_a] > rank[root_b])
        parent[root_b] = root_a;
    else
    {
        parent[root_b] = root_a;
        ++rank[root_a];
    }
}

static FVizResult fviz_connectivity_copy_cell(FVizPolyData* output, const FVizCellArray* cells, FVizSize cell_id)
{
    const FVizCellType type = fviz_cell_array_type(cells, cell_id);
    const FVizSize point_count = fviz_cell_array_point_count(cells, cell_id);
    const uint32_t* ids = fviz_cell_array_point_ids(cells, cell_id);
    switch (type)
    {
        case FVIZ_CELL_VERTEX:
        case FVIZ_CELL_POLY_VERTEX:
            return fviz_poly_data_add_poly_vertex(output, point_count, ids);
        case FVIZ_CELL_LINE:
            return fviz_poly_data_add_line(output, ids[0], ids[1]);
        case FVIZ_CELL_POLY_LINE:
            return fviz_poly_data_add_poly_line(output, point_count, ids);
        case FVIZ_CELL_TRIANGLE:
            return fviz_poly_data_add_triangle(output, ids[0], ids[1], ids[2]);
        case FVIZ_CELL_QUAD:
        case FVIZ_CELL_POLYGON:
            return fviz_poly_data_add_polygon(output, point_count, ids);
        case FVIZ_CELL_TRIANGLE_STRIP:
            return fviz_poly_data_add_triangle_strip(output, point_count, ids);
        default:
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                    "connectivity filter encountered unsupported PolyData cell type");
            return FVIZ_ERROR_INVALID_STATE;
    }
}

static void fviz_poly_data_connectivity_filter_destroy(FVizObject* object)
{
    FVizPolyDataConnectivityFilter* filter = (FVizPolyDataConnectivityFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_poly_data_connectivity_filter_class = {
    FVIZ_TYPE_POLY_DATA_CONNECTIVITY_FILTER, "FVizPolyDataConnectivityFilter", &g_fviz_object_class,
    fviz_poly_data_connectivity_filter_destroy, NULL};

static FVizResult fviz_poly_data_connectivity_filter_process_request(FVizAlgorithm* algorithm,
                                                                     const FVizPipelineRequestInfo* request,
                                                                     void* state)
{
    FVizPolyDataConnectivityFilter* filter = (FVizPolyDataConnectivityFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    FVizDataArray* region_array = NULL;
    FVizDataArray* selected_ids = NULL;
    FVizSize* parent = NULL;
    FVizSize* first_cell_for_point = NULL;
    FVizSize* root_region = NULL;
    FVizSize* region_sizes = NULL;
    uint8_t* rank = NULL;
    uint32_t* cell_regions = NULL;
    const FVizCellArray* categories[4];
    FVizSize actual_cell_count;
    FVizSize point_count;
    FVizSize bytes;
    FVizSize global_cell;
    FVizSize category;
    FVizSize i;
    uint32_t region_count = 0u;
    uint32_t largest_region = 0u;
    FVizSize largest_region_size = 0u;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL || fviz_poly_data_validate(input) != FVIZ_OK)
    {
        if (input == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "connectivity filter has no input");
        return input == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
    }
    actual_cell_count = fviz_poly_data_cell_count(input);
    point_count = fviz_poly_data_point_count(input);
    categories[0] = fviz_poly_data_verts(input);
    categories[1] = fviz_poly_data_lines(input);
    categories[2] = fviz_poly_data_polys(input);
    categories[3] = fviz_poly_data_strips(input);
    if (actual_cell_count > UINT32_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "connectivity region labeling exceeds uint32 cell capacity");
        return FVIZ_ERROR_OVERFLOW;
    }
    if (actual_cell_count == 0u)
    {
        if (fviz_poly_data_deep_copy(input, &output) != FVIZ_OK ||
            fviz_data_array_create(FVIZ_DATA_UINT32, 1u, &region_array) != FVIZ_OK ||
            fviz_attribute_set_add(fviz_poly_data_cell_data(output), filter->array_name, region_array) != FVIZ_OK ||
            fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) !=
                FVIZ_OK)
            goto fail;
        filter->region_count = 0u;
        fviz_release(region_array);
        fviz_release(output);
        return FVIZ_OK;
    }

    if (fviz_size_multiply(actual_cell_count, sizeof(*parent), &bytes) != FVIZ_OK) goto fail;
    parent = (FVizSize*)fviz_alloc(bytes);
    root_region = (FVizSize*)fviz_alloc(bytes);
    region_sizes = (FVizSize*)fviz_alloc(bytes);
    if (parent == NULL || root_region == NULL || region_sizes == NULL) goto fail;
    if (fviz_size_multiply(actual_cell_count, sizeof(*rank), &bytes) != FVIZ_OK) goto fail;
    rank = (uint8_t*)fviz_alloc(bytes);
    if (rank == NULL) goto fail;
    if (fviz_size_multiply(actual_cell_count, sizeof(*cell_regions), &bytes) != FVIZ_OK) goto fail;
    cell_regions = (uint32_t*)fviz_alloc(bytes);
    if (cell_regions == NULL) goto fail;
    if (point_count != 0u)
    {
        if (fviz_size_multiply(point_count, sizeof(*first_cell_for_point), &bytes) != FVIZ_OK) goto fail;
        first_cell_for_point = (FVizSize*)fviz_alloc(bytes);
        if (first_cell_for_point == NULL) goto fail;
    }
    for (i = 0u; i < actual_cell_count; ++i)
    {
        parent[i] = i;
        root_region[i] = (FVizSize)-1;
        region_sizes[i] = 0u;
        rank[i] = 0u;
    }
    for (i = 0u; i < point_count; ++i)
        first_cell_for_point[i] = (FVizSize)-1;

    global_cell = 0u;
    for (category = 0u; category < 4u; ++category)
    {
        for (i = 0u; i < fviz_cell_array_count(categories[category]); ++i, ++global_cell)
        {
            const uint32_t* ids = fviz_cell_array_point_ids(categories[category], i);
            const FVizSize n = fviz_cell_array_point_count(categories[category], i);
            FVizSize j;
            for (j = 0u; j < n; ++j)
            {
                const FVizSize point_id = (FVizSize)ids[j];
                if (first_cell_for_point[point_id] == (FVizSize)-1) first_cell_for_point[point_id] = global_cell;
                else
                    fviz_connectivity_union(parent, rank, global_cell, first_cell_for_point[point_id]);
            }
        }
    }
    for (i = 0u; i < actual_cell_count; ++i)
    {
        const FVizSize root = fviz_connectivity_find(parent, i);
        if (root_region[root] == (FVizSize)-1)
        {
            if (region_count == UINT32_MAX)
            {
                fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "connectivity region count overflow");
                goto fail;
            }
            root_region[root] = region_count++;
        }
        cell_regions[i] = (uint32_t)root_region[root];
        ++region_sizes[root_region[root]];
    }
    for (i = 0u; i < region_count; ++i)
    {
        if (region_sizes[i] > largest_region_size)
        {
            largest_region_size = region_sizes[i];
            largest_region = (uint32_t)i;
        }
    }
    filter->region_count = region_count;

    if (filter->extraction_mode == FVIZ_CONNECTIVITY_ALL_REGIONS)
    {
        if (fviz_poly_data_deep_copy(input, &output) != FVIZ_OK ||
            fviz_data_array_create(FVIZ_DATA_UINT32, 1u, &region_array) != FVIZ_OK ||
            fviz_data_array_reserve(region_array, actual_cell_count) != FVIZ_OK)
            goto fail;
        for (i = 0u; i < actual_cell_count; ++i)
            if (fviz_data_array_append_tuple(region_array, &cell_regions[i]) != FVIZ_OK) goto fail;
        if (fviz_attribute_set_add(fviz_poly_data_cell_data(output), filter->array_name, region_array) != FVIZ_OK)
            goto fail;
    }
    else
    {
        if (fviz_poly_data_create(&output) != FVIZ_OK || fviz_geometry_copy_points_and_data(input, output) != FVIZ_OK ||
            fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &selected_ids) != FVIZ_OK ||
            fviz_data_array_create(FVIZ_DATA_UINT32, 1u, &region_array) != FVIZ_OK)
            goto fail;
        global_cell = 0u;
        for (category = 0u; category < 4u; ++category)
        {
            for (i = 0u; i < fviz_cell_array_count(categories[category]); ++i, ++global_cell)
            {
                if (cell_regions[global_cell] == largest_region)
                {
                    const uint64_t source_id = (uint64_t)global_cell;
                    const uint32_t region_id = largest_region;
                    if (fviz_connectivity_copy_cell(output, categories[category], i) != FVIZ_OK ||
                        fviz_data_array_append_tuple(selected_ids, &source_id) != FVIZ_OK ||
                        fviz_data_array_append_tuple(region_array, &region_id) != FVIZ_OK)
                        goto fail;
                }
            }
        }
        if (fviz_triangle_copy_cell_data(input, output, selected_ids) != FVIZ_OK ||
            fviz_attribute_set_add(fviz_poly_data_cell_data(output), filter->array_name, region_array) != FVIZ_OK)
            goto fail;
    }
    if (fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_free(parent);
    fviz_free(first_cell_for_point);
    fviz_free(root_region);
    fviz_free(region_sizes);
    fviz_free(rank);
    fviz_free(cell_regions);
    fviz_release(region_array);
    fviz_release(selected_ids);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_free(parent);
    fviz_free(first_cell_for_point);
    fviz_free(root_region);
    fviz_free(region_sizes);
    fviz_free(rank);
    fviz_free(cell_regions);
    fviz_release(region_array);
    fviz_release(selected_ids);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_poly_data_connectivity_filter_create(FVizPolyDataConnectivityFilter** out_filter)
{
    FVizPolyDataConnectivityFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizPolyDataConnectivityFilter*)fviz_internal_object_allocate(
        sizeof(*filter), &g_fviz_poly_data_connectivity_filter_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->extraction_mode = FVIZ_CONNECTIVITY_ALL_REGIONS;
    filter->region_count = 0u;
    (void)memcpy(filter->array_name, "RegionId", sizeof("RegionId"));
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_poly_data_connectivity_filter_process_request;
    callbacks.get_state_mtime = fviz_geometry_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) !=
            FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

void fviz_poly_data_connectivity_filter_set_extraction_mode(FVizPolyDataConnectivityFilter* filter,
                                                            FVizConnectivityExtractionMode mode)
{
    if (filter == NULL || (mode != FVIZ_CONNECTIVITY_ALL_REGIONS && mode != FVIZ_CONNECTIVITY_LARGEST_REGION)) return;
    if (filter->extraction_mode != mode)
    {
        filter->extraction_mode = mode;
        fviz_object_modified((FVizObject*)filter);
    }
}

FVizConnectivityExtractionMode
fviz_poly_data_connectivity_filter_extraction_mode(const FVizPolyDataConnectivityFilter* filter)
{
    return filter != NULL ? filter->extraction_mode : FVIZ_CONNECTIVITY_ALL_REGIONS;
}

FVizResult fviz_poly_data_connectivity_filter_set_array_name(FVizPolyDataConnectivityFilter* filter, const char* name)
{
    const FVizSize length = name != NULL ? strlen(name) : 0u;
    if (filter == NULL || name == NULL || length == 0u || length >= sizeof(filter->array_name))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "connectivity array name must contain 1..63 characters");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (strcmp(filter->array_name, name) != 0)
    {
        (void)memcpy(filter->array_name, name, length + 1u);
        fviz_object_modified((FVizObject*)filter);
    }
    return FVIZ_OK;
}

const char* fviz_poly_data_connectivity_filter_array_name(const FVizPolyDataConnectivityFilter* filter)
{
    return filter != NULL ? filter->array_name : NULL;
}

uint32_t fviz_poly_data_connectivity_filter_region_count(const FVizPolyDataConnectivityFilter* filter)
{
    return filter != NULL ? filter->region_count : 0u;
}

FVizResult fviz_poly_data_connectivity_filter_set_input_data(FVizPolyDataConnectivityFilter* filter,
                                                             FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_poly_data_connectivity_filter_set_input_connection(FVizPolyDataConnectivityFilter* filter,
                                                                   FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_poly_data_connectivity_filter_algorithm(FVizPolyDataConnectivityFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_poly_data_connectivity_filter_output_port(FVizPolyDataConnectivityFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_poly_data_connectivity_filter_output(FVizPolyDataConnectivityFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_poly_data_connectivity_filter_update(FVizPolyDataConnectivityFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}

static void fviz_feature_edges_filter_destroy(FVizObject* object)
{
    FVizFeatureEdgesFilter* filter = (FVizFeatureEdgesFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_feature_edges_filter_class = {FVIZ_TYPE_FEATURE_EDGES_FILTER,
                                                                  "FVizFeatureEdgesFilter", &g_fviz_object_class,
                                                                  fviz_feature_edges_filter_destroy, NULL};

static FVizVec3 fviz_polygon_normal(const FVizVec3* points, const uint32_t* ids, FVizSize n)
{
    FVizVec3 normal = fviz_vec3(0.0f, 0.0f, 0.0f);
    FVizSize i;
    for (i = 0u; i < n; ++i)
    {
        const FVizVec3 a = points[ids[i]];
        const FVizVec3 b = points[ids[(i + 1u) % n]];
        normal.x += (a.y - b.y) * (a.z + b.z);
        normal.y += (a.z - b.z) * (a.x + b.x);
        normal.z += (a.x - b.x) * (a.y + b.y);
    }
    return fviz_vec3_normalize(normal);
}

static void fviz_edge_record_set(FVizEdgeRecord* edge, uint32_t a, uint32_t b, FVizVec3 normal)
{
    edge->key0 = a < b ? a : b;
    edge->key1 = a < b ? b : a;
    edge->normal = normal;
}

static int fviz_edge_record_compare(const void* left, const void* right)
{
    const FVizEdgeRecord* a = (const FVizEdgeRecord*)left;
    const FVizEdgeRecord* b = (const FVizEdgeRecord*)right;
    if (a->key0 < b->key0) return -1;
    if (a->key0 > b->key0) return 1;
    if (a->key1 < b->key1) return -1;
    if (a->key1 > b->key1) return 1;
    return 0;
}

static FVizResult fviz_feature_edges_filter_process_request(FVizAlgorithm* algorithm,
                                                            const FVizPipelineRequestInfo* request, void* state)
{
    FVizFeatureEdgesFilter* filter = (FVizFeatureEdgesFilter*)state;
    FVizPolyData* input;
    FVizPolyData* output = NULL;
    FVizDataArray* edge_types = NULL;
    FVizEdgeRecord* edges = NULL;
    FVizSize edge_capacity = 0u;
    FVizSize edge_count = 0u;
    const FVizVec3* points;
    FVizSize i;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizPolyData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL || fviz_poly_data_validate(input) != FVIZ_OK)
    {
        if (input == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "feature edges filter has no input");
        return input == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
    }
    for (i = 0u; i < fviz_poly_data_poly_cell_count(input); ++i)
    {
        const FVizSize n = fviz_cell_array_point_count(fviz_poly_data_polys(input), i);
        if (fviz_size_add(edge_capacity, n, &edge_capacity) != FVIZ_OK) return fviz_last_error_code();
    }
    for (i = 0u; i < fviz_poly_data_strip_cell_count(input); ++i)
    {
        const FVizSize n = fviz_cell_array_point_count(fviz_poly_data_strips(input), i);
        FVizSize triangle_edges = 0u;
        if (fviz_size_multiply(n - 2u, 3u, &triangle_edges) != FVIZ_OK ||
            fviz_size_add(edge_capacity, triangle_edges, &edge_capacity) != FVIZ_OK)
            return fviz_last_error_code();
    }
    if (edge_capacity != 0u)
    {
        FVizSize bytes = 0u;
        if (fviz_size_multiply(edge_capacity, sizeof(*edges), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
        edges = (FVizEdgeRecord*)fviz_alloc(bytes);
        if (edges == NULL) return fviz_last_error_code();
    }
    points = fviz_poly_data_points(input);
    for (i = 0u; i < fviz_poly_data_poly_cell_count(input); ++i)
    {
        const FVizCellArray* cells = fviz_poly_data_polys(input);
        const uint32_t* ids = fviz_cell_array_point_ids(cells, i);
        const FVizSize n = fviz_cell_array_point_count(cells, i);
        const FVizVec3 normal = fviz_polygon_normal(points, ids, n);
        FVizSize j;
        for (j = 0u; j < n; ++j)
        {
            const uint32_t a = ids[j];
            const uint32_t b = ids[(j + 1u) % n];
            if (a == b) continue;
            fviz_edge_record_set(&edges[edge_count++], a, b, normal);
        }
    }
    for (i = 0u; i < fviz_poly_data_strip_cell_count(input); ++i)
    {
        const FVizCellArray* cells = fviz_poly_data_strips(input);
        const uint32_t* ids = fviz_cell_array_point_ids(cells, i);
        const FVizSize n = fviz_cell_array_point_count(cells, i);
        FVizSize j;
        for (j = 0u; j + 2u < n; ++j)
        {
            const uint32_t a = (j & 1u) == 0u ? ids[j] : ids[j + 1u];
            const uint32_t b = (j & 1u) == 0u ? ids[j + 1u] : ids[j];
            const uint32_t c = ids[j + 2u];
            const FVizVec3 normal = fviz_vec3_normalize(
                fviz_vec3_cross(fviz_vec3_sub(points[b], points[a]), fviz_vec3_sub(points[c], points[a])));
            fviz_edge_record_set(&edges[edge_count++], a, b, normal);
            fviz_edge_record_set(&edges[edge_count++], b, c, normal);
            fviz_edge_record_set(&edges[edge_count++], c, a, normal);
        }
    }
    if (edge_count > 1u) qsort(edges, edge_count, sizeof(*edges), fviz_edge_record_compare);
    if (fviz_poly_data_create(&output) != FVIZ_OK || fviz_geometry_copy_points_and_data(input, output) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &edge_types) != FVIZ_OK)
        goto fail;
    i = 0u;
    while (i < edge_count)
    {
        FVizSize end = i + 1u;
        FVizFeatureEdgeType type = (FVizFeatureEdgeType)0;
        FVizBool emit = FVIZ_FALSE;
        while (end < edge_count && edges[end].key0 == edges[i].key0 && edges[end].key1 == edges[i].key1)
            ++end;
        if (end - i == 1u)
        {
            type = FVIZ_FEATURE_EDGE_BOUNDARY;
            emit = filter->boundary_edges;
        }
        else if (end - i > 2u)
        {
            type = FVIZ_FEATURE_EDGE_NON_MANIFOLD;
            emit = filter->non_manifold_edges;
        }
        else
        {
            double dot = (double)fviz_vec3_dot(edges[i].normal, edges[i + 1u].normal);
            double angle;
            if (dot < -1.0) dot = -1.0;
            if (dot > 1.0) dot = 1.0;
            angle = acos(dot) * (180.0 / FVIZ_PI);
            if (filter->feature_edges != FVIZ_FALSE && angle > filter->feature_angle)
            {
                type = FVIZ_FEATURE_EDGE_FEATURE;
                emit = FVIZ_TRUE;
            }
            else if (filter->manifold_edges != FVIZ_FALSE)
            {
                type = FVIZ_FEATURE_EDGE_MANIFOLD;
                emit = FVIZ_TRUE;
            }
        }
        if (emit != FVIZ_FALSE)
        {
            const uint8_t value = (uint8_t)type;
            if (fviz_poly_data_add_line(output, edges[i].key0, edges[i].key1) != FVIZ_OK ||
                fviz_data_array_append_tuple(edge_types, &value) != FVIZ_OK)
                goto fail;
        }
        i = end;
    }
    if (fviz_attribute_set_add(fviz_poly_data_cell_data(output), "EdgeType", edge_types) != FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_free(edges);
    fviz_release(edge_types);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_free(edges);
    fviz_release(edge_types);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_feature_edges_filter_create(FVizFeatureEdgesFilter** out_filter)
{
    FVizFeatureEdgesFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizFeatureEdgesFilter*)fviz_internal_object_allocate(sizeof(*filter), &g_fviz_feature_edges_filter_class,
                                                                    NULL);
    if (filter == NULL) return fviz_last_error_code();
    filter->feature_angle = 30.0;
    filter->boundary_edges = FVIZ_TRUE;
    filter->feature_edges = FVIZ_TRUE;
    filter->non_manifold_edges = FVIZ_TRUE;
    filter->manifold_edges = FVIZ_FALSE;
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_feature_edges_filter_process_request;
    callbacks.get_state_mtime = fviz_geometry_filter_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA, FVIZ_FALSE, FVIZ_FALSE) !=
            FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_feature_edges_filter_set_feature_angle(FVizFeatureEdgesFilter* filter, double degrees)
{
    if (filter == NULL || !isfinite(degrees) || degrees < 0.0 || degrees > 180.0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "feature angle must be finite and within [0, 180]");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (filter->feature_angle != degrees)
    {
        filter->feature_angle = degrees;
        fviz_object_modified((FVizObject*)filter);
    }
    return FVIZ_OK;
}

double fviz_feature_edges_filter_feature_angle(const FVizFeatureEdgesFilter* filter)
{
    return filter != NULL ? filter->feature_angle : 0.0;
}

void fviz_feature_edges_filter_set_boundary_edges(FVizFeatureEdgesFilter* filter, FVizBool enabled)
{
    if (filter != NULL && filter->boundary_edges != enabled)
    {
        filter->boundary_edges = enabled;
        fviz_object_modified((FVizObject*)filter);
    }
}

void fviz_feature_edges_filter_set_feature_edges(FVizFeatureEdgesFilter* filter, FVizBool enabled)
{
    if (filter != NULL && filter->feature_edges != enabled)
    {
        filter->feature_edges = enabled;
        fviz_object_modified((FVizObject*)filter);
    }
}

void fviz_feature_edges_filter_set_non_manifold_edges(FVizFeatureEdgesFilter* filter, FVizBool enabled)
{
    if (filter != NULL && filter->non_manifold_edges != enabled)
    {
        filter->non_manifold_edges = enabled;
        fviz_object_modified((FVizObject*)filter);
    }
}

void fviz_feature_edges_filter_set_manifold_edges(FVizFeatureEdgesFilter* filter, FVizBool enabled)
{
    if (filter != NULL && filter->manifold_edges != enabled)
    {
        filter->manifold_edges = enabled;
        fviz_object_modified((FVizObject*)filter);
    }
}

FVizResult fviz_feature_edges_filter_set_input_data(FVizFeatureEdgesFilter* filter, FVizPolyData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_feature_edges_filter_set_input_connection(FVizFeatureEdgesFilter* filter, FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_feature_edges_filter_algorithm(FVizFeatureEdgesFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_feature_edges_filter_output_port(FVizFeatureEdgesFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_feature_edges_filter_output(FVizFeatureEdgesFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_feature_edges_filter_update(FVizFeatureEdgesFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
