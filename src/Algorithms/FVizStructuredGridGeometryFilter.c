#include <limits.h>
#include <string.h>

#include <FViz/Algorithms/FVizStructuredGridGeometryFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizStructuredGridGeometryFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
};

static void fviz_structured_geometry_destroy(FVizObject* object)
{
    FVizStructuredGridGeometryFilter* filter = (FVizStructuredGridGeometryFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_structured_geometry_class = {
    FVIZ_TYPE_STRUCTURED_GRID_GEOMETRY_FILTER,
    "FVizStructuredGridGeometryFilter",
    &g_fviz_object_class,
    fviz_structured_geometry_destroy,
    NULL
};

static FVizMTime fviz_structured_geometry_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static FVizResult fviz_structured_geometry_copy_attributes(
    const FVizAttributeSet* source, FVizAttributeSet* destination)
{
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        const FVizDataArray* array = fviz_attribute_set_const_array_at(source, i);
        FVizDataArray* copy = NULL;
        FVizAttributeRole role;
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

static FVizId fviz_structured_geometry_original_cell_id(
    const FVizStructuredGrid* input, FVizId local_cell)
{
    const FVizDataArray* original = fviz_attribute_set_const_get(
        fviz_structured_grid_const_cell_data(input), "FVizOriginalCellIds");
    if (original != NULL && fviz_data_array_type(original) == FVIZ_DATA_UINT64 &&
        fviz_data_array_components(original) == 1u &&
        fviz_data_array_tuple_count(original) == fviz_structured_grid_cell_count(input))
    {
        const uint64_t* ids = (const uint64_t*)fviz_data_array_const_data(original);
        if (ids != NULL && local_cell < (FVizId)fviz_data_array_tuple_count(original))
            return (FVizId)ids[(FVizSize)local_cell];
    }
    return local_cell;
}

static FVizResult fviz_structured_geometry_append_primitive(
    const FVizStructuredGrid* input,
    FVizPolyData* output,
    FVizDataArray* provenance,
    FVizDataArray* source_indices,
    FVizId cell_id,
    const FVizId* ids,
    uint32_t count)
{
    uint32_t compatible[8];
    uint32_t i;
    const uint64_t original = (uint64_t)fviz_structured_geometry_original_cell_id(input, cell_id);
    const uint64_t local = (uint64_t)cell_id;
    for (i = 0u; i < count; ++i)
    {
        if (ids[i] > UINT32_MAX)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                "StructuredGrid geometry currently requires UINT32 render point IDs");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        compatible[i] = (uint32_t)ids[i];
    }
    if (count == 1u)
    {
        if (fviz_poly_data_add_vertex(output, compatible[0]) != FVIZ_OK) return fviz_last_error_code();
        if (fviz_data_array_append_tuple(provenance, &original) != FVIZ_OK ||
            fviz_data_array_append_tuple(source_indices, &local) != FVIZ_OK) return fviz_last_error_code();
    }
    else if (count == 2u)
    {
        if (fviz_poly_data_add_line(output, compatible[0], compatible[1]) != FVIZ_OK) return fviz_last_error_code();
        if (fviz_data_array_append_tuple(provenance, &original) != FVIZ_OK ||
            fviz_data_array_append_tuple(source_indices, &local) != FVIZ_OK) return fviz_last_error_code();
    }
    else if (count == 4u)
    {
        if (fviz_poly_data_add_triangle(output, compatible[0], compatible[1], compatible[2]) != FVIZ_OK ||
            fviz_data_array_append_tuple(provenance, &original) != FVIZ_OK ||
            fviz_data_array_append_tuple(source_indices, &local) != FVIZ_OK ||
            fviz_poly_data_add_triangle(output, compatible[0], compatible[2], compatible[3]) != FVIZ_OK ||
            fviz_data_array_append_tuple(provenance, &original) != FVIZ_OK ||
            fviz_data_array_append_tuple(source_indices, &local) != FVIZ_OK)
            return fviz_last_error_code();
    }
    else
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported StructuredGrid primitive arity");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    return FVIZ_OK;
}

static FVizResult fviz_structured_geometry_copy_cell_data(
    const FVizStructuredGrid* input, FVizPolyData* output, const FVizDataArray* source_indices)
{
    const FVizAttributeSet* source = fviz_structured_grid_const_cell_data(input);
    FVizAttributeSet* destination = fviz_poly_data_cell_data(output);
    const uint64_t* source_ids = (const uint64_t*)fviz_data_array_const_data(source_indices);
    FVizSize array_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source, array_index);
        const FVizDataArray* source_array = fviz_attribute_set_const_array_at(source, array_index);
        FVizDataArray* out_array = NULL;
        FVizSize i;
        FVizAttributeRole role;
        if (name != NULL && strcmp(name, "FVizOriginalCellIds") == 0) continue;
        if (fviz_data_array_tuple_count(source_array) != fviz_structured_grid_cell_count(input)) continue;
        if (fviz_data_array_create(
                fviz_data_array_type(source_array), fviz_data_array_components(source_array), &out_array) != FVIZ_OK ||
            fviz_data_array_resize(out_array, fviz_data_array_tuple_count(source_indices)) != FVIZ_OK)
            goto fail;
        for (i = 0u; i < fviz_data_array_tuple_count(source_indices); ++i)
        {
            const FVizSize source_id = source_ids != NULL ? (FVizSize)source_ids[i] : (FVizSize)-1;
            const void* tuple = source_id < fviz_data_array_tuple_count(source_array)
                ? fviz_data_array_const_tuple(source_array, source_id) : NULL;
            void* out_tuple = fviz_data_array_tuple(out_array, i);
            if (tuple == NULL || out_tuple == NULL) goto fail;
            (void)memcpy(out_tuple, tuple, fviz_data_array_tuple_stride(source_array));
        }
        if (fviz_attribute_set_add(destination, name, out_array) != FVIZ_OK) goto fail;
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(destination, role, name);
        }
        fviz_release(out_array);
        continue;
fail:
        fviz_release(out_array);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_structured_geometry_process_request(
    FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request, void* state)
{
    FVizStructuredGrid* input;
    FVizPolyData* output = NULL;
    FVizDataArray* provenance = NULL;
    FVizDataArray* source_indices = NULL;
    FVizSize cell_count;
    FVizSize i;
    uint32_t dimension;
    (void)state;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizStructuredGrid*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL || fviz_structured_grid_validate(input) != FVIZ_OK)
    {
        if (input == NULL) fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "StructuredGrid geometry filter has no input");
        return input == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
    }
    if (fviz_structured_grid_point_count(input) > (FVizSize)UINT32_MAX + 1u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
            "StructuredGrid geometry rendering currently requires at most UINT32_MAX+1 points");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_reserve(
            output, fviz_structured_grid_point_count(input), fviz_structured_grid_cell_count(input) * 2u) != FVIZ_OK ||
        fviz_poly_data_add_points(
            output, fviz_structured_grid_points(input), fviz_structured_grid_point_count(input), NULL) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &provenance) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &source_indices) != FVIZ_OK ||
        fviz_structured_geometry_copy_attributes(
            fviz_structured_grid_const_point_data(input), fviz_poly_data_point_data(output)) != FVIZ_OK ||
        fviz_structured_geometry_copy_attributes(
            fviz_structured_grid_const_field_data(input), fviz_poly_data_field_data(output)) != FVIZ_OK)
        goto fail;

    cell_count = fviz_structured_grid_cell_count(input);
    dimension = fviz_structured_grid_dimension(input);
    if (dimension < 3u)
    {
        for (i = 0u; i < cell_count; ++i)
        {
            FVizId ids[8];
            uint32_t count = 0u;
            if (fviz_structured_grid_cell_point_ids(input, (FVizId)i, ids, &count) != FVIZ_OK ||
                fviz_structured_geometry_append_primitive(
                    input, output, provenance, source_indices, (FVizId)i, ids, count) != FVIZ_OK)
                goto fail;
        }
    }
    else
    {
        int64_t extent[6];
        static const uint32_t faces[6][4] = {
            {0u,4u,7u,3u}, {1u,2u,6u,5u},
            {0u,1u,5u,4u}, {3u,7u,6u,2u},
            {0u,3u,2u,1u}, {4u,5u,6u,7u}
        };
        fviz_structured_grid_extent(input, extent);
        for (i = 0u; i < cell_count; ++i)
        {
            FVizId ids[8];
            uint32_t count = 0u;
            int64_t ijk[3];
            uint32_t face;
            if (fviz_structured_grid_cell_ijk(input, (FVizId)i, ijk) != FVIZ_OK ||
                fviz_structured_grid_cell_point_ids(input, (FVizId)i, ids, &count) != FVIZ_OK || count != 8u)
                goto fail;
            for (face = 0u; face < 6u; ++face)
            {
                FVizBool boundary = FVIZ_FALSE;
                FVizId face_ids[4];
                switch (face)
                {
                    case 0u: boundary = ijk[0] == extent[0] ? FVIZ_TRUE : FVIZ_FALSE; break;
                    case 1u: boundary = ijk[0] == extent[1] - 1 ? FVIZ_TRUE : FVIZ_FALSE; break;
                    case 2u: boundary = ijk[1] == extent[2] ? FVIZ_TRUE : FVIZ_FALSE; break;
                    case 3u: boundary = ijk[1] == extent[3] - 1 ? FVIZ_TRUE : FVIZ_FALSE; break;
                    case 4u: boundary = ijk[2] == extent[4] ? FVIZ_TRUE : FVIZ_FALSE; break;
                    case 5u: boundary = ijk[2] == extent[5] - 1 ? FVIZ_TRUE : FVIZ_FALSE; break;
                    default: break;
                }
                if (boundary == FVIZ_FALSE) continue;
                face_ids[0] = ids[faces[face][0]]; face_ids[1] = ids[faces[face][1]];
                face_ids[2] = ids[faces[face][2]]; face_ids[3] = ids[faces[face][3]];
                if (fviz_structured_geometry_append_primitive(
                        input, output, provenance, source_indices, (FVizId)i, face_ids, 4u) != FVIZ_OK)
                    goto fail;
            }
        }
    }
    if (fviz_attribute_set_add(fviz_poly_data_cell_data(output), "FVizOriginalCellIds", provenance) != FVIZ_OK ||
        fviz_structured_geometry_copy_cell_data(input, output, source_indices) != FVIZ_OK ||
        (fviz_poly_data_poly_cell_count(output) != 0u && fviz_poly_data_compute_normals(output) != FVIZ_OK) ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_release(source_indices);
    fviz_release(provenance);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_release(source_indices);
    fviz_release(provenance);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_structured_grid_geometry_filter_create(
    FVizStructuredGridGeometryFilter** out_filter)
{
    FVizStructuredGridGeometryFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizStructuredGridGeometryFilter*)fviz_internal_object_allocate(
        sizeof(*filter), &g_fviz_structured_geometry_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_structured_geometry_process_request;
    callbacks.get_state_mtime = fviz_structured_geometry_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(
            filter->algorithm, 0u, FVIZ_TYPE_STRUCTURED_GRID, FVIZ_FALSE, FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_structured_grid_geometry_filter_set_input_data(
    FVizStructuredGridGeometryFilter* filter, FVizStructuredGrid* input)
{
    if (filter == NULL || input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "StructuredGrid geometry filter input is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input);
}

FVizResult fviz_structured_grid_geometry_filter_set_input_connection(
    FVizStructuredGridGeometryFilter* filter, FVizAlgorithmOutput* input)
{
    if (filter == NULL || input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "StructuredGrid geometry filter connection is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_algorithm_set_input_connection(filter->algorithm, 0u, input);
}

FVizAlgorithm* fviz_structured_grid_geometry_filter_algorithm(FVizStructuredGridGeometryFilter* filter)
{ return filter != NULL ? filter->algorithm : NULL; }
FVizAlgorithmOutput* fviz_structured_grid_geometry_filter_output_port(FVizStructuredGridGeometryFilter* filter)
{ return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL; }
FVizPolyData* fviz_structured_grid_geometry_filter_output(FVizStructuredGridGeometryFilter* filter)
{ return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL; }
FVizResult fviz_structured_grid_geometry_filter_update(FVizStructuredGridGeometryFilter* filter)
{
    if (filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_algorithm_update(filter->algorithm);
}
