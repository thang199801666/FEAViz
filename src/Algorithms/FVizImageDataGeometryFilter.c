#include <limits.h>
#include <string.h>

#include <FViz/Algorithms/FVizImageDataGeometryFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizImageDataGeometryFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
};

static void fviz_image_geometry_destroy(FVizObject* object)
{
    FVizImageDataGeometryFilter* filter = (FVizImageDataGeometryFilter*)object;
    fviz_release(filter->algorithm);
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_image_geometry_class = {FVIZ_TYPE_IMAGE_DATA_GEOMETRY_FILTER,
                                                            "FVizImageDataGeometryFilter", &g_fviz_object_class,
                                                            fviz_image_geometry_destroy, NULL};

static FVizMTime fviz_image_geometry_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static FVizResult fviz_image_geometry_copy_attribute_set(const FVizAttributeSet* source, FVizAttributeSet* destination)
{
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        const FVizDataArray* source_array = fviz_attribute_set_const_array_at(source, i);
        FVizDataArray* copy = NULL;
        FVizAttributeRole role;
        if (fviz_data_array_deep_copy(source_array, &copy) != FVIZ_OK ||
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

static FVizId fviz_image_geometry_source_cell_id(const FVizImageData* input, FVizId cell_id)
{
    const FVizDataArray* upstream =
        fviz_attribute_set_const_get(fviz_image_data_const_cell_data(input), "FVizOriginalCellIds");
    if (upstream != NULL && fviz_data_array_type(upstream) == FVIZ_DATA_UINT64 &&
        fviz_data_array_components(upstream) == 1u &&
        fviz_data_array_tuple_count(upstream) == fviz_image_data_cell_count(input))
    {
        const uint64_t* values = (const uint64_t*)fviz_data_array_const_data(upstream);
        if (values != NULL && cell_id < fviz_data_array_tuple_count(upstream)) return (FVizId)values[cell_id];
    }
    return cell_id;
}

static FVizResult fviz_image_geometry_copy_cell_data(const FVizImageData* input, FVizPolyData* output,
                                                     const FVizDataArray* source_cell_indices)
{
    const FVizAttributeSet* source = fviz_image_data_const_cell_data(input);
    FVizAttributeSet* destination = fviz_poly_data_cell_data(output);
    FVizSize array_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source, array_index);
        const FVizDataArray* source_array = fviz_attribute_set_const_array_at(source, array_index);
        FVizDataArray* out_array = NULL;
        const uint64_t* source_ids;
        FVizSize i;
        FVizAttributeRole role;
        if (name != NULL && strcmp(name, "FVizOriginalCellIds") == 0) continue;
        if (fviz_data_array_tuple_count(source_array) != fviz_image_data_cell_count(input)) continue;
        source_ids = (const uint64_t*)fviz_data_array_const_data(source_cell_indices);
        if (source_ids == NULL && fviz_data_array_tuple_count(source_cell_indices) != 0u)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "ImageData geometry provenance storage is unavailable");
            return FVIZ_ERROR_INVALID_STATE;
        }
        if (fviz_data_array_create(fviz_data_array_type(source_array), fviz_data_array_components(source_array),
                                   &out_array) != FVIZ_OK ||
            fviz_data_array_resize(out_array, fviz_data_array_tuple_count(source_cell_indices)) != FVIZ_OK)
            goto fail;
        {
            const FVizSize stride = fviz_data_array_tuple_stride(source_array);
            unsigned char* destination_data = (unsigned char*)fviz_data_array_data(out_array);
            for (i = 0u; i < fviz_data_array_tuple_count(source_cell_indices); ++i)
            {
                const FVizSize source_id = (FVizSize)source_ids[i];
                const void* tuple;
                if (source_id >= fviz_data_array_tuple_count(source_array)) goto fail;
                tuple = fviz_data_array_const_tuple(source_array, source_id);
                if (tuple == NULL || destination_data == NULL) goto fail;
                (void)memcpy(destination_data + i * stride, tuple, stride);
            }
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

static FVizResult fviz_image_geometry_append_cell(const FVizImageData* input, FVizPolyData* output,
                                                  FVizDataArray* provenance, FVizDataArray* source_cell_indices,
                                                  FVizId cell_id, const FVizId* ids, uint32_t count)
{
    uint32_t compatible[8];
    uint32_t i;
    const uint64_t source_cell = fviz_image_geometry_source_cell_id(input, cell_id);
    const uint64_t local_cell = cell_id;
    for (i = 0u; i < count; ++i)
    {
        if (ids[i] > UINT32_MAX)
        {
            fviz_internal_set_error(
                FVIZ_ERROR_NOT_SUPPORTED,
                "ImageData geometry output currently requires renderable UINT32 PolyData point IDs");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        compatible[i] = (uint32_t)ids[i];
    }
    switch (count)
    {
        case 1u:
            if (fviz_poly_data_add_vertex(output, compatible[0]) != FVIZ_OK) return fviz_last_error_code();
            break;
        case 2u:
            if (fviz_poly_data_add_line(output, compatible[0], compatible[1]) != FVIZ_OK) return fviz_last_error_code();
            break;
        case 4u:
            if (fviz_poly_data_add_triangle(output, compatible[0], compatible[1], compatible[2]) != FVIZ_OK ||
                fviz_data_array_append_tuple(provenance, &source_cell) != FVIZ_OK ||
                fviz_data_array_append_tuple(source_cell_indices, &local_cell) != FVIZ_OK ||
                fviz_poly_data_add_triangle(output, compatible[0], compatible[2], compatible[3]) != FVIZ_OK ||
                fviz_data_array_append_tuple(provenance, &source_cell) != FVIZ_OK ||
                fviz_data_array_append_tuple(source_cell_indices, &local_cell) != FVIZ_OK)
                return fviz_last_error_code();
            return FVIZ_OK;
        default:
            (void)input;
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "unsupported structured geometry cell arity");
            return FVIZ_ERROR_NOT_SUPPORTED;
    }
    if (fviz_data_array_append_tuple(provenance, &source_cell) != FVIZ_OK ||
        fviz_data_array_append_tuple(source_cell_indices, &local_cell) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizResult fviz_image_geometry_estimated_triangles(const FVizImageData* input, FVizSize* out_triangles)
{
    FVizSize dims[3] = {0u, 0u, 0u};
    const uint32_t dimension = fviz_image_data_dimension(input);
    FVizSize triangles = 0u;
    if (out_triangles == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "triangle estimate output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_image_data_dimensions(input, dims);
    if (dimension == 2u)
    {
        FVizSize cells = fviz_image_data_cell_count(input);
        if (fviz_size_multiply(cells, 2u, &triangles) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    }
    else if (dimension == 3u)
    {
        const FVizSize cx = dims[0] - 1u;
        const FVizSize cy = dims[1] - 1u;
        const FVizSize cz = dims[2] - 1u;
        FVizSize xy, yz, zx, sum;
        if (fviz_size_multiply(cx, cy, &xy) != FVIZ_OK || fviz_size_multiply(cy, cz, &yz) != FVIZ_OK ||
            fviz_size_multiply(cz, cx, &zx) != FVIZ_OK || xy > (FVizSize)-1 - yz || xy + yz > (FVizSize)-1 - zx)
            return FVIZ_ERROR_OVERFLOW;
        sum = xy + yz + zx;
        if (fviz_size_multiply(sum, 4u, &triangles) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    }
    *out_triangles = triangles;
    return FVIZ_OK;
}

static FVizResult fviz_image_geometry_materialize_points(const FVizImageData* input, FVizVec3* points,
                                                         FVizSize point_count)
{
    int64_t extent[6];
    double origin[3];
    double spacing[3];
    double direction[9];
    FVizSize cursor = 0u;
    int64_t k;
    if (point_count == 0u) return FVIZ_OK;
    if (points == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "point output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_image_data_extent(input, extent);
    fviz_image_data_origin(input, origin);
    fviz_image_data_spacing(input, spacing);
    fviz_image_data_direction(input, direction);
    for (k = extent[4]; k <= extent[5]; ++k)
    {
        int64_t j;
        for (j = extent[2]; j <= extent[3]; ++j)
        {
            int64_t i;
            for (i = extent[0]; i <= extent[1]; ++i)
            {
                const double sx = (double)i * spacing[0];
                const double sy = (double)j * spacing[1];
                const double sz = (double)k * spacing[2];
                const double x = origin[0] + direction[0] * sx + direction[1] * sy + direction[2] * sz;
                const double y = origin[1] + direction[3] * sx + direction[4] * sy + direction[5] * sz;
                const double z = origin[2] + direction[6] * sx + direction[7] * sy + direction[8] * sz;
                if (cursor >= point_count)
                {
                    fviz_internal_set_error(FVIZ_ERROR_INTERNAL,
                                            "structured point materialization exceeded expected count");
                    return FVIZ_ERROR_INTERNAL;
                }
                points[cursor++] = fviz_vec3((float)x, (float)y, (float)z);
                if (i == INT64_MAX) break;
            }
            if (j == INT64_MAX) break;
        }
        if (k == INT64_MAX) break;
    }
    if (cursor != point_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "structured point materialization count mismatch");
        return FVIZ_ERROR_INTERNAL;
    }
    return FVIZ_OK;
}

static FVizResult fviz_image_geometry_process_request(FVizAlgorithm* algorithm, const FVizPipelineRequestInfo* request,
                                                      void* state)
{
    FVizImageDataGeometryFilter* filter = (FVizImageDataGeometryFilter*)state;
    FVizImageData* input;
    FVizPolyData* output = NULL;
    FVizDataArray* provenance = NULL;
    FVizDataArray* source_cell_indices = NULL;
    FVizVec3* points = NULL;
    FVizSize point_count;
    FVizSize point_bytes;
    FVizSize i;
    uint32_t dimension;
    FVizSize estimated_triangles = 0u;
    (void)filter;
    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizImageData*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "ImageData geometry filter has no input");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_image_data_validate(input) != FVIZ_OK) return fviz_last_error_code();
    point_count = fviz_image_data_point_count(input);
    if (point_count > (FVizSize)UINT32_MAX + 1u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                "ImageData geometry rendering currently requires at most UINT32_MAX+1 points");
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    if (fviz_size_multiply(point_count, sizeof(FVizVec3), &point_bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    points = point_count != 0u ? (FVizVec3*)fviz_alloc(point_bytes) : NULL;
    if (point_count != 0u && points == NULL) return fviz_last_error_code();
    if (fviz_image_geometry_materialize_points(input, points, point_count) != FVIZ_OK ||
        fviz_image_geometry_estimated_triangles(input, &estimated_triangles) != FVIZ_OK)
        goto fail;
    if (fviz_poly_data_create(&output) != FVIZ_OK ||
        fviz_poly_data_reserve(output, point_count, estimated_triangles) != FVIZ_OK ||
        fviz_poly_data_add_points(output, points, point_count, NULL) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &provenance) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &source_cell_indices) != FVIZ_OK ||
        fviz_data_array_reserve(provenance, estimated_triangles != 0u ? estimated_triangles
                                                                      : fviz_image_data_cell_count(input)) != FVIZ_OK ||
        fviz_data_array_reserve(source_cell_indices,
                                estimated_triangles != 0u ? estimated_triangles : fviz_image_data_cell_count(input)) !=
            FVIZ_OK)
        goto fail;
    if (fviz_image_geometry_copy_attribute_set(fviz_image_data_const_point_data(input),
                                               fviz_poly_data_point_data(output)) != FVIZ_OK ||
        fviz_image_geometry_copy_attribute_set(fviz_image_data_const_field_data(input),
                                               fviz_poly_data_field_data(output)) != FVIZ_OK)
        goto fail;

    dimension = fviz_image_data_dimension(input);
    if (point_count != 0u)
    {
        if (dimension < 3u)
        {
            for (i = 0u; i < fviz_image_data_cell_count(input); ++i)
            {
                FVizId ids[8];
                uint32_t count;
                if (fviz_image_data_cell_point_ids(input, (FVizId)i, ids, &count) != FVIZ_OK ||
                    fviz_image_geometry_append_cell(input, output, provenance, source_cell_indices, (FVizId)i, ids,
                                                    count) != FVIZ_OK)
                    goto fail;
            }
        }
        else
        {
            int64_t extent[6];
            int64_t ijk[3];
            fviz_image_data_extent(input, extent);
            for (i = 0u; i < fviz_image_data_cell_count(input); ++i)
            {
                FVizId ids[8];
                uint32_t count;
                const uint32_t faces[6][4] = {{0u, 4u, 7u, 3u}, {1u, 2u, 6u, 5u}, {0u, 1u, 5u, 4u},
                                              {3u, 7u, 6u, 2u}, {0u, 3u, 2u, 1u}, {4u, 5u, 6u, 7u}};
                uint32_t face;
                if (fviz_image_data_cell_ijk(input, (FVizId)i, ijk) != FVIZ_OK ||
                    fviz_image_data_cell_point_ids(input, (FVizId)i, ids, &count) != FVIZ_OK || count != 8u)
                    goto fail;
                for (face = 0u; face < 6u; ++face)
                {
                    FVizBool boundary = FVIZ_FALSE;
                    FVizId face_ids[4];
                    switch (face)
                    {
                        case 0u:
                            boundary = ijk[0] == extent[0] ? FVIZ_TRUE : FVIZ_FALSE;
                            break;
                        case 1u:
                            boundary = ijk[0] == extent[1] - 1 ? FVIZ_TRUE : FVIZ_FALSE;
                            break;
                        case 2u:
                            boundary = ijk[1] == extent[2] ? FVIZ_TRUE : FVIZ_FALSE;
                            break;
                        case 3u:
                            boundary = ijk[1] == extent[3] - 1 ? FVIZ_TRUE : FVIZ_FALSE;
                            break;
                        case 4u:
                            boundary = ijk[2] == extent[4] ? FVIZ_TRUE : FVIZ_FALSE;
                            break;
                        case 5u:
                            boundary = ijk[2] == extent[5] - 1 ? FVIZ_TRUE : FVIZ_FALSE;
                            break;
                        default:
                            break;
                    }
                    if (boundary == FVIZ_FALSE) continue;
                    face_ids[0] = ids[faces[face][0]];
                    face_ids[1] = ids[faces[face][1]];
                    face_ids[2] = ids[faces[face][2]];
                    face_ids[3] = ids[faces[face][3]];
                    if (fviz_image_geometry_append_cell(input, output, provenance, source_cell_indices, (FVizId)i,
                                                        face_ids, 4u) != FVIZ_OK)
                        goto fail;
                }
            }
        }
    }
    if (fviz_attribute_set_add(fviz_poly_data_cell_data(output), "FVizOriginalCellIds", provenance) != FVIZ_OK ||
        fviz_image_geometry_copy_cell_data(input, output, source_cell_indices) != FVIZ_OK ||
        (fviz_poly_data_poly_cell_count(output) != 0u && fviz_poly_data_compute_normals(output) != FVIZ_OK) ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
        goto fail;
    fviz_free(points);
    fviz_release(provenance);
    fviz_release(source_cell_indices);
    fviz_release(output);
    return FVIZ_OK;
fail:
    fviz_free(points);
    fviz_release(provenance);
    fviz_release(source_cell_indices);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_image_data_geometry_filter_create(FVizImageDataGeometryFilter** out_filter)
{
    FVizImageDataGeometryFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizImageDataGeometryFilter*)fviz_internal_object_allocate(sizeof(*filter), &g_fviz_image_geometry_class,
                                                                         NULL);
    if (filter == NULL) return fviz_last_error_code();
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_image_geometry_process_request;
    callbacks.get_state_mtime = fviz_image_geometry_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_IMAGE_DATA, FVIZ_FALSE, FVIZ_FALSE) !=
            FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_POLY_DATA) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_image_data_geometry_filter_set_input_data(FVizImageDataGeometryFilter* filter, FVizImageData* input)
{
    return filter != NULL ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_image_data_geometry_filter_set_input_connection(FVizImageDataGeometryFilter* filter,
                                                                FVizAlgorithmOutput* input)
{
    return filter != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_image_data_geometry_filter_algorithm(FVizImageDataGeometryFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_image_data_geometry_filter_output_port(FVizImageDataGeometryFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizPolyData* fviz_image_data_geometry_filter_output(FVizImageDataGeometryFilter* filter)
{
    return filter != NULL ? (FVizPolyData*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_image_data_geometry_filter_update(FVizImageDataGeometryFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}
