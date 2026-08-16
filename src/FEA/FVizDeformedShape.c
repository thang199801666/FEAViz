#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Algorithms/FVizDeformation.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/FEA/FVizDeformedShape.h>
#include <FViz/FEA/FVizResultField.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/FEA/FVizDeformedShapePrivate.h>

static FVizMTime fviz_fea_deformed_local_mtime(const FVizObject* object)
{
    return fviz_internal_object_local_mtime(object);
}

static void fviz_fea_deformed_result_destroy(FVizObject* object)
{
    FVizFEADeformedShapeResult* result = (FVizFEADeformedShapeResult*)object;
    fviz_release(result->displacements);
    fviz_release(result->coverage_mask);
    fviz_release(result->base_grid);
    fviz_release(result->grid);
}

static void fviz_fea_deformed_controller_clear_internal(FVizFEADeformedShapeController* controller,
                                                        FVizBool count_clear)
{
    if (controller == NULL) return;
    fviz_release((FVizObject*)controller->cached_frame);
    fviz_release((FVizObject*)controller->cached_grid);
    fviz_release(controller->cached_field_name);
    fviz_release(controller->cached_instance_name);
    fviz_release(controller->cached_result);
    controller->cached_frame = NULL;
    controller->cached_grid = NULL;
    controller->cached_field_name = NULL;
    controller->cached_instance_name = NULL;
    controller->cached_result = NULL;
    controller->cached_frame_mtime = 0u;
    controller->cached_grid_mtime = 0u;
    if (count_clear != FVIZ_FALSE) ++controller->clears;
}

static void fviz_fea_deformed_controller_destroy(FVizObject* object)
{
    fviz_fea_deformed_controller_clear_internal((FVizFEADeformedShapeController*)object, FVIZ_FALSE);
}

static const FVizObjectClass g_fviz_fea_deformed_result_class = {
    FVIZ_TYPE_FEA_DEFORMED_SHAPE_RESULT, "FVizFEADeformedShapeResult", NULL, fviz_fea_deformed_result_destroy,
    fviz_fea_deformed_local_mtime};

static const FVizObjectClass g_fviz_fea_deformed_controller_class = {
    FVIZ_TYPE_FEA_DEFORMED_SHAPE_CONTROLLER, "FVizFEADeformedShapeController", NULL,
    fviz_fea_deformed_controller_destroy, fviz_fea_deformed_local_mtime};

static const char* fviz_fea_nonnull_string(const char* text)
{
    return text != NULL ? text : "";
}

static FVizBool fviz_fea_deformed_strings_equal(const char* a, const char* b)
{
    return strcmp(fviz_fea_nonnull_string(a), fviz_fea_nonnull_string(b)) == 0 ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_fea_deformed_shape_options_initialize(FVizFEADeformedShapeOptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->state = FVIZ_FEA_DEFORMATION_DEFORMED;
    options->displacement_field_name = "U";
    options->scale_mode = FVIZ_FEA_DEFORMATION_SCALE_AUTO;
    options->uniform_scale = 1.0;
    options->auto_target_fraction = 0.10;
    options->auto_minimum_scale = 0.0;
    options->auto_maximum_scale = 1.0e12;
    options->require_complete_nodal_coverage = FVIZ_TRUE;
}

FVizResult fviz_fea_deformed_shape_controller_create(FVizFEADeformedShapeController** out_controller)
{
    FVizFEADeformedShapeController* controller;
    if (out_controller == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "deformed-shape controller output is NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_controller = NULL;
    controller = (FVizFEADeformedShapeController*)fviz_internal_object_allocate(
        sizeof(*controller), &g_fviz_fea_deformed_controller_class, NULL);
    if (controller == NULL) return fviz_last_error_code();
    *out_controller = controller;
    return FVIZ_OK;
}

void fviz_fea_deformed_shape_controller_clear_cache(FVizFEADeformedShapeController* controller)
{
    if (controller == NULL) return;
    fviz_fea_deformed_controller_clear_internal(controller, FVIZ_TRUE);
    fviz_object_modified((FVizObject*)controller);
}

FVizFEADeformedShapeCacheStatistics
fviz_fea_deformed_shape_controller_cache_statistics(const FVizFEADeformedShapeController* controller)
{
    FVizFEADeformedShapeCacheStatistics stats;
    memset(&stats, 0, sizeof(stats));
    if (controller == NULL) return stats;
    stats.hits = controller->hits;
    stats.misses = controller->misses;
    stats.clears = controller->clears;
    stats.populated = controller->cached_result != NULL ? FVIZ_TRUE : FVIZ_FALSE;
    return stats;
}

static FVizBool fviz_fea_is_integer_array(const FVizDataArray* array)
{
    const FVizDataType type = array != NULL ? fviz_data_array_type(array) : FVIZ_DATA_FLOAT32;
    return array != NULL && fviz_data_array_components(array) == 1u && type >= FVIZ_DATA_INT8 &&
           type <= FVIZ_DATA_UINT64;
}

static FVizResult fviz_fea_read_label(const FVizDataArray* array, FVizSize tuple, FVizId* out_id)
{
    const void* data;
    if (out_id == NULL || !fviz_fea_is_integer_array(array) || tuple >= fviz_data_array_tuple_count(array))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "result labels must be a one-component integer array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    data = fviz_data_array_const_data(array);
    switch (fviz_data_array_type(array))
    {
        case FVIZ_DATA_INT8:
            {
                const int8_t v = ((const int8_t*)data)[tuple];
                if (v < 0) goto negative;
                *out_id = (FVizId)v;
                break;
            }
        case FVIZ_DATA_UINT8:
            *out_id = (FVizId)((const uint8_t*)data)[tuple];
            break;
        case FVIZ_DATA_INT16:
            {
                const int16_t v = ((const int16_t*)data)[tuple];
                if (v < 0) goto negative;
                *out_id = (FVizId)v;
                break;
            }
        case FVIZ_DATA_UINT16:
            *out_id = (FVizId)((const uint16_t*)data)[tuple];
            break;
        case FVIZ_DATA_INT32:
            {
                const int32_t v = ((const int32_t*)data)[tuple];
                if (v < 0) goto negative;
                *out_id = (FVizId)v;
                break;
            }
        case FVIZ_DATA_UINT32:
            *out_id = (FVizId)((const uint32_t*)data)[tuple];
            break;
        case FVIZ_DATA_INT64:
            {
                const int64_t v = ((const int64_t*)data)[tuple];
                if (v < 0) goto negative;
                *out_id = (FVizId)v;
                break;
            }
        case FVIZ_DATA_UINT64:
            *out_id = (FVizId)((const uint64_t*)data)[tuple];
            break;
        default:
            goto invalid;
    }
    return FVIZ_OK;
negative:
    fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "negative result labels are not supported");
    return FVIZ_ERROR_INVALID_ARGUMENT;
invalid:
    fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "unsupported result-label type");
    return FVIZ_ERROR_INVALID_ARGUMENT;
}

static FVizResult fviz_fea_deformed_build_point_label_map(const FVizUnstructuredGrid* grid, FVizHashMap** out_map)
{
    const FVizAttributeSet* point_data;
    const FVizDataArray* labels;
    const FVizSize point_count = fviz_unstructured_grid_point_count(grid);
    FVizHashMap* map = NULL;
    FVizSize i;
    if (out_map == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_map = NULL;
    point_data = fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid);
    labels = fviz_attribute_set_const_active(point_data, FVIZ_ATTRIBUTE_GLOBAL_IDS);
    if (labels == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND,
                                "labeled deformation results require active point GlobalIds on the mesh");
        return FVIZ_ERROR_NOT_FOUND;
    }
    if (!fviz_fea_is_integer_array(labels) || fviz_data_array_tuple_count(labels) != point_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "active point GlobalIds must be a one-component integer array matching point count");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_hash_map_create_reserve(point_count * 2u + 1u, &map) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < point_count; ++i)
    {
        FVizId id;
        if (fviz_fea_read_label(labels, i, &id) != FVIZ_OK)
        {
            fviz_release(map);
            return fviz_last_error_code();
        }
        if (fviz_hash_map_contains(map, id) != FVIZ_FALSE)
        {
            fviz_release(map);
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "active point GlobalIds must be unique");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        if (fviz_hash_map_set(map, id, (void*)(uintptr_t)(i + 1u)) != FVIZ_OK)
        {
            fviz_release(map);
            return fviz_last_error_code();
        }
    }
    *out_map = map;
    return FVIZ_OK;
}

static FVizBool fviz_fea_deformed_options_equal(const FVizFEADeformedShapeController* controller,
                                                const FVizFEADeformedShapeOptions* options)
{
    const FVizFEADeformedShapeOptions* cached = &controller->cached_options;
    if (cached->state != options->state || cached->scale_mode != options->scale_mode ||
        cached->uniform_scale != options->uniform_scale ||
        cached->auto_target_fraction != options->auto_target_fraction ||
        cached->auto_minimum_scale != options->auto_minimum_scale ||
        cached->auto_maximum_scale != options->auto_maximum_scale ||
        cached->require_complete_nodal_coverage != options->require_complete_nodal_coverage)
        return FVIZ_FALSE;
    if (!fviz_fea_deformed_strings_equal(
            controller->cached_field_name != NULL ? fviz_string_c_str(controller->cached_field_name) : "",
            options->displacement_field_name) ||
        !fviz_fea_deformed_strings_equal(
            controller->cached_instance_name != NULL ? fviz_string_c_str(controller->cached_instance_name) : "",
            options->instance_name))
        return FVIZ_FALSE;
    return FVIZ_TRUE;
}

static FVizResult fviz_fea_deformed_cache_store(FVizFEADeformedShapeController* controller, const FVizFEAFrame* frame,
                                                const FVizUnstructuredGrid* grid,
                                                const FVizFEADeformedShapeOptions* options,
                                                FVizFEADeformedShapeResult* result)
{
    FVizString *field_name = NULL, *instance_name = NULL;
    const char* field = fviz_fea_nonnull_string(options->displacement_field_name);
    const char* instance = fviz_fea_nonnull_string(options->instance_name);
    if (fviz_string_create_from(field, &field_name) != FVIZ_OK ||
        fviz_string_create_from(instance, &instance_name) != FVIZ_OK)
    {
        fviz_release(field_name);
        fviz_release(instance_name);
        return fviz_last_error_code();
    }
    fviz_fea_deformed_controller_clear_internal(controller, FVIZ_FALSE);
    controller->cached_frame = (const FVizFEAFrame*)fviz_retain((FVizObject*)frame);
    controller->cached_grid = (const FVizUnstructuredGrid*)fviz_retain((FVizObject*)grid);
    controller->cached_frame_mtime = fviz_object_mtime((const FVizObject*)frame);
    controller->cached_grid_mtime = fviz_object_mtime((const FVizObject*)grid);
    controller->cached_field_name = field_name;
    controller->cached_instance_name = instance_name;
    controller->cached_options = *options;
    controller->cached_options.displacement_field_name = NULL;
    controller->cached_options.instance_name = NULL;
    controller->cached_result = (FVizFEADeformedShapeResult*)fviz_retain(result);
    return FVIZ_OK;
}

static FVizBool fviz_fea_deformed_cache_matches(const FVizFEADeformedShapeController* controller,
                                                const FVizFEAFrame* frame, const FVizUnstructuredGrid* grid,
                                                const FVizFEADeformedShapeOptions* options)
{
    if (controller->cached_result == NULL || controller->cached_frame != frame || controller->cached_grid != grid)
        return FVIZ_FALSE;
    if (controller->cached_frame_mtime != fviz_object_mtime((const FVizObject*)frame) ||
        controller->cached_grid_mtime != fviz_object_mtime((const FVizObject*)grid))
        return FVIZ_FALSE;
    return fviz_fea_deformed_options_equal(controller, options);
}

static FVizResult fviz_fea_deformed_map_displacements(const FVizFEAField* field, const FVizUnstructuredGrid* grid,
                                                      const FVizFEADeformedShapeOptions* options,
                                                      FVizDataArray** out_vectors, FVizDataArray** out_coverage,
                                                      FVizSize* out_mapped)
{
    const FVizSize point_count = fviz_unstructured_grid_point_count(grid);
    double* vectors = NULL;
    uint8_t* coverage = NULL;
    FVizDataArray *vector_array = NULL, *coverage_array = NULL;
    FVizHashMap* point_map = NULL;
    FVizBool need_map = FVIZ_FALSE;
    FVizBool found_block = FVIZ_FALSE;
    const char* implicit_instance = NULL;
    FVizSize block_index;
    FVizSize mapped = 0u;

    if (out_vectors == NULL || out_coverage == NULL || out_mapped == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_vectors = NULL;
    *out_coverage = NULL;
    *out_mapped = 0u;
    if (fviz_fea_field_type(field) != FVIZ_FEA_FIELD_VECTOR)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "deformed shape displacement field must be a vector field");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (point_count > 0u)
    {
        vectors = (double*)fviz_alloc(point_count * 3u * sizeof(*vectors));
        coverage = (uint8_t*)fviz_alloc(point_count * sizeof(*coverage));
        if (vectors == NULL || coverage == NULL) goto fail;
        memset(vectors, 0, point_count * 3u * sizeof(*vectors));
        memset(coverage, 0, point_count * sizeof(*coverage));
    }

    for (block_index = 0u; block_index < fviz_fea_field_block_count(field); ++block_index)
    {
        const FVizDataArray* ids;
        const FVizDataArray* values;
        const char* block_instance;
        FVizSize tuple_count;
        FVizSize tuple;
        if (fviz_fea_field_block_position(field, block_index) != FVIZ_FEA_POSITION_NODAL) continue;
        block_instance = fviz_fea_field_block_instance_name(field, block_index);
        if (options->instance_name != NULL && options->instance_name[0] != '\0' &&
            strcmp(block_instance, options->instance_name) != 0)
            continue;
        if ((options->instance_name == NULL || options->instance_name[0] == '\0') && block_instance[0] != '\0')
        {
            if (implicit_instance == NULL) implicit_instance = block_instance;
            else if (strcmp(implicit_instance, block_instance) != 0)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                        "multiple displacement instances match; specify instance_name");
                goto fail;
            }
        }
        values = fviz_fea_field_block_const_values(field, block_index);
        ids = fviz_fea_field_block_entity_ids(field, block_index);
        if (values == NULL || fviz_data_array_components(values) < 3u)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "nodal displacement block must contain at least three components");
            goto fail;
        }
        tuple_count = fviz_data_array_tuple_count(values);
        if (ids != NULL)
        {
            if (fviz_data_array_tuple_count(ids) != tuple_count)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "displacement entity-id count must match values");
                goto fail;
            }
            need_map = FVIZ_TRUE;
            if (point_map == NULL && fviz_fea_deformed_build_point_label_map(grid, &point_map) != FVIZ_OK) goto fail;
        }
        else if (tuple_count != point_count || found_block != FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "unlabeled displacement blocks must be a single tuple-aligned nodal block");
            goto fail;
        }
        found_block = FVIZ_TRUE;
        for (tuple = 0u; tuple < tuple_count; ++tuple)
        {
            FVizSize point_id = tuple;
            double x = 0.0, y = 0.0, z = 0.0;
            if (ids != NULL)
            {
                FVizId label;
                void* encoded = NULL;
                if (fviz_fea_read_label(ids, tuple, &label) != FVIZ_OK) goto fail;
                if (fviz_hash_map_get(point_map, label, &encoded) == FVIZ_FALSE || encoded == NULL)
                    continue; /* result for a node outside this mesh subset */
                point_id = (FVizSize)((uintptr_t)encoded - 1u);
            }
            if (coverage[point_id] != 0u)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                        "duplicate nodal displacement contribution maps to the same mesh point");
                goto fail;
            }
            if (fviz_data_array_get_component(values, tuple, 0u, &x) != FVIZ_OK ||
                fviz_data_array_get_component(values, tuple, 1u, &y) != FVIZ_OK ||
                fviz_data_array_get_component(values, tuple, 2u, &z) != FVIZ_OK)
                goto fail;
            vectors[point_id * 3u + 0u] = x;
            vectors[point_id * 3u + 1u] = y;
            vectors[point_id * 3u + 2u] = z;
            coverage[point_id] = 1u;
            ++mapped;
        }
    }
    (void)need_map;
    if (found_block == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "no matching nodal displacement block was found");
        goto fail;
    }
    if (options->require_complete_nodal_coverage != FVIZ_FALSE && mapped != point_count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "displacement field does not cover every mesh point");
        goto fail;
    }
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 3u, &vector_array) != FVIZ_OK ||
        fviz_data_array_append_tuples(vector_array, vectors, point_count) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &coverage_array) != FVIZ_OK ||
        fviz_data_array_append_tuples(coverage_array, coverage, point_count) != FVIZ_OK)
        goto fail;
    fviz_free(vectors);
    fviz_free(coverage);
    fviz_release(point_map);
    *out_vectors = vector_array;
    *out_coverage = coverage_array;
    *out_mapped = mapped;
    return FVIZ_OK;
fail:
    fviz_free(vectors);
    fviz_free(coverage);
    fviz_release(point_map);
    fviz_release(vector_array);
    fviz_release(coverage_array);
    return fviz_last_error_code() != FVIZ_OK ? fviz_last_error_code() : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_fea_deformed_shape_evaluate(FVizFEADeformedShapeController* controller, const FVizFEAFrame* frame,
                                            const FVizUnstructuredGrid* grid,
                                            const FVizFEADeformedShapeOptions* options,
                                            FVizFEADeformedShapeResult** out_result)
{
    FVizFEADeformedShapeOptions defaults;
    const FVizFEADeformedShapeOptions* actual = options;
    FVizFEADeformedShapeResult* result = NULL;
    const FVizFEAField* field = NULL;
    FVizDataArray *vectors = NULL, *coverage = NULL;
    FVizSize mapped = 0u;
    double scale = 0.0;

    if (controller == NULL || frame == NULL || grid == NULL || out_result == NULL)
    {
        if (out_result != NULL) *out_result = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "deformed-shape evaluation requires controller, frame, grid and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_result = NULL;
    if (actual == NULL)
    {
        fviz_fea_deformed_shape_options_initialize(&defaults);
        actual = &defaults;
    }
    if (actual->struct_size < sizeof(FVizFEADeformedShapeOptions) || !isfinite(actual->uniform_scale) ||
        !isfinite(actual->auto_target_fraction) || !isfinite(actual->auto_minimum_scale) ||
        !isfinite(actual->auto_maximum_scale))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid deformed-shape options");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_fea_deformed_cache_matches(controller, frame, grid, actual) != FVIZ_FALSE)
    {
        ++controller->hits;
        *out_result = (FVizFEADeformedShapeResult*)fviz_retain(controller->cached_result);
        return FVIZ_OK;
    }
    ++controller->misses;
    result = (FVizFEADeformedShapeResult*)fviz_internal_object_allocate(sizeof(*result),
                                                                        &g_fviz_fea_deformed_result_class, NULL);
    if (result == NULL) return fviz_last_error_code();
    fviz_deformation_metrics_initialize(&result->metrics);
    if (actual->state != FVIZ_FEA_DEFORMATION_UNDEFORMED && actual->state != FVIZ_FEA_DEFORMATION_DEFORMED &&
        actual->state != FVIZ_FEA_DEFORMATION_SUPERIMPOSED)
    {
        fviz_release(result);
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "unsupported deformed-shape state");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    result->state = actual->state;
    result->base_grid = (FVizUnstructuredGrid*)fviz_retain((FVizObject*)grid);

    if (actual->state == FVIZ_FEA_DEFORMATION_UNDEFORMED)
    {
        result->scale_factor = 0.0;
        result->missing_point_count = fviz_unstructured_grid_point_count(grid);
        result->grid = (FVizUnstructuredGrid*)fviz_retain((FVizObject*)grid);
    }
    else
    {
        const char* field_name = actual->displacement_field_name != NULL && actual->displacement_field_name[0] != '\0'
                                     ? actual->displacement_field_name
                                     : "U";
        field = fviz_fea_frame_const_field(frame, field_name);
        if (field == NULL)
        {
            fviz_release(result);
            fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "displacement field was not found in the selected frame");
            return FVIZ_ERROR_NOT_FOUND;
        }
        if (fviz_fea_deformed_map_displacements(field, grid, actual, &vectors, &coverage, &mapped) != FVIZ_OK)
        {
            fviz_release(result);
            return fviz_last_error_code();
        }
        if (actual->scale_mode == FVIZ_FEA_DEFORMATION_SCALE_TRUE) scale = 1.0;
        else if (actual->scale_mode == FVIZ_FEA_DEFORMATION_SCALE_UNIFORM)
            scale = actual->uniform_scale;
        else if (actual->scale_mode == FVIZ_FEA_DEFORMATION_SCALE_AUTO)
        {
            if (fviz_deformation_compute_auto_scale(fviz_unstructured_grid_bounds(grid), vectors,
                                                    actual->auto_target_fraction, actual->auto_minimum_scale,
                                                    actual->auto_maximum_scale, &scale, &result->metrics) != FVIZ_OK)
            {
                fviz_release(vectors);
                fviz_release(coverage);
                fviz_release(result);
                return fviz_last_error_code();
            }
        }
        else
        {
            fviz_release(vectors);
            fviz_release(coverage);
            fviz_release(result);
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "unsupported deformation scale mode");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        if (actual->scale_mode != FVIZ_FEA_DEFORMATION_SCALE_AUTO &&
            fviz_deformation_measure_vectors(vectors, &result->metrics) != FVIZ_OK)
        {
            fviz_release(vectors);
            fviz_release(coverage);
            fviz_release(result);
            return fviz_last_error_code();
        }
        if (fviz_deformation_apply_to_unstructured_grid(grid, vectors, scale, &result->grid) != FVIZ_OK)
        {
            fviz_release(vectors);
            fviz_release(coverage);
            fviz_release(result);
            return fviz_last_error_code();
        }
        result->scale_factor = scale;
        result->mapped_point_count = mapped;
        result->missing_point_count = fviz_unstructured_grid_point_count(grid) - mapped;
        result->displacements = vectors;
        result->coverage_mask = coverage;
    }

    if (fviz_fea_deformed_cache_store(controller, frame, grid, actual, result) != FVIZ_OK)
    {
        fviz_release(result);
        return fviz_last_error_code();
    }
    *out_result = result;
    return FVIZ_OK;
}

FVizFEADeformationState fviz_fea_deformed_shape_result_state(const FVizFEADeformedShapeResult* result)
{
    return result != NULL ? result->state : FVIZ_FEA_DEFORMATION_UNDEFORMED;
}

double fviz_fea_deformed_shape_result_scale_factor(const FVizFEADeformedShapeResult* result)
{
    return result != NULL ? result->scale_factor : 0.0;
}

const FVizDeformationMetrics* fviz_fea_deformed_shape_result_metrics(const FVizFEADeformedShapeResult* result)
{
    return result != NULL ? &result->metrics : NULL;
}

FVizSize fviz_fea_deformed_shape_result_mapped_point_count(const FVizFEADeformedShapeResult* result)
{
    return result != NULL ? result->mapped_point_count : 0u;
}

FVizSize fviz_fea_deformed_shape_result_missing_point_count(const FVizFEADeformedShapeResult* result)
{
    return result != NULL ? result->missing_point_count : 0u;
}

const FVizDataArray* fviz_fea_deformed_shape_result_displacements(const FVizFEADeformedShapeResult* result)
{
    return result != NULL ? result->displacements : NULL;
}

const FVizDataArray* fviz_fea_deformed_shape_result_coverage_mask(const FVizFEADeformedShapeResult* result)
{
    return result != NULL ? result->coverage_mask : NULL;
}

const FVizUnstructuredGrid* fviz_fea_deformed_shape_result_base_grid(const FVizFEADeformedShapeResult* result)
{
    return result != NULL ? result->base_grid : NULL;
}

const FVizUnstructuredGrid* fviz_fea_deformed_shape_result_grid(const FVizFEADeformedShapeResult* result)
{
    return result != NULL ? result->grid : NULL;
}
