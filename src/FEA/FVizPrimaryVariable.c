#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/FEA/FVizPrimaryVariable.h>
#include <FViz/Mesh/FVizCellArray.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/FEA/FVizPrimaryVariablePrivate.h>

#define FVIZ_FEA_INVALID_LABEL UINT64_MAX
#define FVIZ_FEA_INVALID_LOCAL INT64_MIN

typedef struct FVizFEAContribution
{
    FVizSize point_id;
    FVizSize cell_id;
    FVizSize block_index;
    FVizId entity_label;
    int64_t local_id;
    double value;
} FVizFEAContribution;

typedef struct FVizFEAIPTuple
{
    FVizSize cell_id;
    int64_t order;
    FVizSize original_order;
    FVizId entity_label;
    double value;
} FVizFEAIPTuple;

static void fviz_fea_primary_result_destroy(FVizObject* object);
static void fviz_fea_primary_evaluator_destroy(FVizObject* object);

static FVizMTime fviz_fea_primary_local_mtime(const FVizObject* object)
{
    return fviz_internal_object_local_mtime(object);
}

static const FVizObjectClass g_fviz_fea_primary_result_class = {
    FVIZ_TYPE_FEA_PRIMARY_VARIABLE_RESULT, "FVizFEAPrimaryVariableResult", NULL, fviz_fea_primary_result_destroy,
    fviz_fea_primary_local_mtime};

static const FVizObjectClass g_fviz_fea_primary_evaluator_class = {
    FVIZ_TYPE_FEA_PRIMARY_VARIABLE_EVALUATOR, "FVizFEAPrimaryVariableEvaluator", NULL,
    fviz_fea_primary_evaluator_destroy, fviz_fea_primary_local_mtime};

static void fviz_fea_primary_result_destroy(FVizObject* object)
{
    FVizFEAPrimaryVariableResult* result = (FVizFEAPrimaryVariableResult*)object;
    fviz_release(result->raw_values);
    fviz_release(result->raw_entity_ids);
    fviz_release(result->raw_local_ids);
    fviz_release(result->display_values);
    fviz_release(result->display_entity_ids);
    fviz_release(result->display_local_ids);
    fviz_release(result->discontinuity_mask);
}

static void fviz_fea_primary_evaluator_clear_internal(FVizFEAPrimaryVariableEvaluator* evaluator, FVizBool count_clear)
{
    if (evaluator == NULL) return;
    fviz_release((void*)evaluator->cached_field);
    fviz_release((void*)evaluator->cached_grid);
    fviz_release((void*)evaluator->cached_variable.entity_filter_ids);
    fviz_release(evaluator->cached_instance_name);
    fviz_release(evaluator->cached_component_label);
    fviz_release(evaluator->cached_result);
    evaluator->cached_field = NULL;
    evaluator->cached_grid = NULL;
    evaluator->cached_variable.entity_filter_ids = NULL;
    evaluator->cached_instance_name = NULL;
    evaluator->cached_component_label = NULL;
    evaluator->cached_result = NULL;
    evaluator->cached_field_mtime = 0u;
    evaluator->cached_grid_mtime = 0u;
    evaluator->cached_filter_mtime = 0u;
    (void)memset(&evaluator->cached_variable, 0, sizeof(evaluator->cached_variable));
    if (count_clear) ++evaluator->clears;
}

static void fviz_fea_primary_evaluator_destroy(FVizObject* object)
{
    fviz_fea_primary_evaluator_clear_internal((FVizFEAPrimaryVariableEvaluator*)object, FVIZ_FALSE);
}

void fviz_fea_primary_variable_initialize(FVizFEAPrimaryVariable* variable)
{
    if (variable == NULL) return;
    (void)memset(variable, 0, sizeof(*variable));
    variable->struct_size = (uint32_t)sizeof(*variable);
    variable->source_position = FVIZ_FEA_POSITION_UNKNOWN;
    variable->target_position = FVIZ_FEA_POSITION_UNKNOWN;
    variable->section_point_number = FVIZ_FEA_SECTION_POINT_ANY;
    variable->operation = FVIZ_FEA_PRIMARY_COMPONENT;
    variable->component = 0u;
    variable->invariant = FVIZ_FEA_INVARIANT_NONE;
    variable->averaging_enabled = FVIZ_TRUE;
    variable->average_across_blocks = FVIZ_FALSE;
    variable->averaging_threshold_percent = 100.0;
    variable->local_id_base = FVIZ_FEA_LOCAL_ID_AUTO;
    variable->integration_point_fallback = FVIZ_INTEGRATION_POINT_CELL_MEAN;
    variable->entity_filter_ids = NULL;
}

FVizResult fviz_fea_primary_variable_evaluator_create(FVizFEAPrimaryVariableEvaluator** out_evaluator)
{
    FVizFEAPrimaryVariableEvaluator* evaluator;
    if (out_evaluator == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "primary-variable evaluator output is NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_evaluator = NULL;
    evaluator = (FVizFEAPrimaryVariableEvaluator*)fviz_internal_object_allocate(
        sizeof(*evaluator), &g_fviz_fea_primary_evaluator_class, NULL);
    if (evaluator == NULL) return fviz_last_error_code();
    *out_evaluator = evaluator;
    return FVIZ_OK;
}

void fviz_fea_primary_variable_evaluator_clear_cache(FVizFEAPrimaryVariableEvaluator* evaluator)
{
    if (evaluator == NULL) return;
    fviz_fea_primary_evaluator_clear_internal(evaluator, FVIZ_TRUE);
    fviz_object_modified((FVizObject*)evaluator);
}

FVizFEAPrimaryVariableCacheStatistics
fviz_fea_primary_variable_evaluator_cache_statistics(const FVizFEAPrimaryVariableEvaluator* evaluator)
{
    FVizFEAPrimaryVariableCacheStatistics stats;
    (void)memset(&stats, 0, sizeof(stats));
    if (evaluator == NULL) return stats;
    stats.hits = evaluator->hits;
    stats.misses = evaluator->misses;
    stats.clears = evaluator->clears;
    stats.entries = evaluator->cached_result != NULL ? 1u : 0u;
    return stats;
}

static FVizBool fviz_fea_is_integer_array(const FVizDataArray* array)
{
    const FVizDataType type = array != NULL ? fviz_data_array_type(array) : FVIZ_DATA_FLOAT32;
    return array != NULL && fviz_data_array_components(array) == 1u && type >= FVIZ_DATA_INT8 &&
           type <= FVIZ_DATA_UINT64;
}

static FVizResult fviz_fea_read_id(const FVizDataArray* array, FVizSize tuple, FVizId* out_id)
{
    const void* data;
    if (out_id == NULL || !fviz_fea_is_integer_array(array) || tuple >= fviz_data_array_tuple_count(array))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "result label array must be a one-component integer array");
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
    fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "result entity labels cannot be negative");
    return FVIZ_ERROR_INVALID_ARGUMENT;
invalid:
    fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "result label array has an unsupported type");
    return FVIZ_ERROR_INVALID_ARGUMENT;
}

static FVizResult fviz_fea_read_local_id(const FVizDataArray* array, FVizSize tuple, int64_t* out_id)
{
    const void* data;
    if (out_id == NULL || !fviz_fea_is_integer_array(array) || tuple >= fviz_data_array_tuple_count(array))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "result local-id array must be a one-component integer array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    data = fviz_data_array_const_data(array);
    switch (fviz_data_array_type(array))
    {
        case FVIZ_DATA_INT8:
            *out_id = (int64_t)((const int8_t*)data)[tuple];
            break;
        case FVIZ_DATA_UINT8:
            *out_id = (int64_t)((const uint8_t*)data)[tuple];
            break;
        case FVIZ_DATA_INT16:
            *out_id = (int64_t)((const int16_t*)data)[tuple];
            break;
        case FVIZ_DATA_UINT16:
            *out_id = (int64_t)((const uint16_t*)data)[tuple];
            break;
        case FVIZ_DATA_INT32:
            *out_id = (int64_t)((const int32_t*)data)[tuple];
            break;
        case FVIZ_DATA_UINT32:
            *out_id = (int64_t)((const uint32_t*)data)[tuple];
            break;
        case FVIZ_DATA_INT64:
            *out_id = ((const int64_t*)data)[tuple];
            break;
        case FVIZ_DATA_UINT64:
            {
                const uint64_t v = ((const uint64_t*)data)[tuple];
                if (v > (uint64_t)INT64_MAX)
                {
                    fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "result local id exceeds INT64_MAX");
                    return FVIZ_ERROR_INVALID_ARGUMENT;
                }
                *out_id = (int64_t)v;
                break;
            }
        default:
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "result local-id array has an unsupported type");
            return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return FVIZ_OK;
}

static FVizResult fviz_fea_build_label_map(const FVizDataArray* labels, FVizSize count, FVizHashMap** out_map)
{
    FVizHashMap* map = NULL;
    FVizSize i;
    if (out_map == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_map = NULL;
    if (labels == NULL) return FVIZ_OK;
    if (!fviz_fea_is_integer_array(labels) || fviz_data_array_tuple_count(labels) != count)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "active GlobalIds must be a one-component integer array matching mesh size");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_hash_map_create_reserve(count * 2u + 1u, &map) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < count; ++i)
    {
        FVizId id;
        if (fviz_fea_read_id(labels, i, &id) != FVIZ_OK)
        {
            fviz_release(map);
            return fviz_last_error_code();
        }
        if (fviz_hash_map_contains(map, id) != FVIZ_FALSE)
        {
            fviz_release(map);
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "active GlobalIds must be unique for primary-variable label resolution");
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

static FVizBool fviz_fea_resolve_label(FVizId label, const FVizHashMap* map, FVizSize count, FVizSize* out_index)
{
    void* value = NULL;
    if (out_index == NULL) return FVIZ_FALSE;
    if (map != NULL)
    {
        if (!fviz_hash_map_get(map, label, &value) || value == NULL) return FVIZ_FALSE;
        *out_index = (FVizSize)((uintptr_t)value - 1u);
        return *out_index < count ? FVIZ_TRUE : FVIZ_FALSE;
    }
    if (label >= (FVizId)count) return FVIZ_FALSE;
    *out_index = (FVizSize)label;
    return FVIZ_TRUE;
}

static FVizResult fviz_fea_build_filter_set(const FVizDataArray* ids, FVizHashMap** out_set)
{
    FVizHashMap* set = NULL;
    FVizSize i, count;
    if (out_set == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_set = NULL;
    if (ids == NULL) return FVIZ_OK;
    if (!fviz_fea_is_integer_array(ids))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "entity filter must be a one-component integer array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_data_array_tuple_count(ids);
    if (fviz_hash_map_create_reserve(count * 2u + 1u, &set) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < count; ++i)
    {
        FVizId id;
        if (fviz_fea_read_id(ids, i, &id) != FVIZ_OK || fviz_hash_map_set(set, id, (void*)(uintptr_t)1u) != FVIZ_OK)
        {
            fviz_release(set);
            return fviz_last_error_code();
        }
    }
    *out_set = set;
    return FVIZ_OK;
}

static FVizBool fviz_fea_filter_accepts(const FVizHashMap* set, FVizId id)
{
    return set == NULL || fviz_hash_map_contains(set, id);
}

static FVizBool fviz_fea_strings_equal(const char* a, const char* b)
{
    if (a == NULL) a = "";
    if (b == NULL) b = "";
    return strcmp(a, b) == 0 ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_fea_primary_cache_matches(const FVizFEAPrimaryVariableEvaluator* evaluator,
                                               const FVizFEAField* field, const FVizUnstructuredGrid* grid,
                                               const FVizFEAPrimaryVariable* variable)
{
    const FVizMTime filter_mtime =
        variable->entity_filter_ids != NULL ? fviz_object_mtime((const FVizObject*)variable->entity_filter_ids) : 0u;
    const FVizFEAPrimaryVariable* cached = &evaluator->cached_variable;
    if (evaluator->cached_result == NULL || evaluator->cached_field != field || evaluator->cached_grid != grid ||
        evaluator->cached_field_mtime != fviz_object_mtime((const FVizObject*)field) ||
        evaluator->cached_grid_mtime != fviz_object_mtime((const FVizObject*)grid) ||
        evaluator->cached_filter_mtime != filter_mtime || cached->entity_filter_ids != variable->entity_filter_ids)
        return FVIZ_FALSE;
    if (!fviz_fea_strings_equal(
            evaluator->cached_instance_name != NULL ? fviz_string_c_str(evaluator->cached_instance_name) : "",
            variable->instance_name) ||
        !fviz_fea_strings_equal(
            evaluator->cached_component_label != NULL ? fviz_string_c_str(evaluator->cached_component_label) : "",
            variable->component_label))
        return FVIZ_FALSE;
    return cached->source_position == variable->source_position &&
           cached->target_position == variable->target_position &&
           cached->section_point_number == variable->section_point_number && cached->operation == variable->operation &&
           cached->component == variable->component && cached->invariant == variable->invariant &&
           cached->averaging_enabled == variable->averaging_enabled &&
           cached->average_across_blocks == variable->average_across_blocks &&
           cached->averaging_threshold_percent == variable->averaging_threshold_percent &&
           cached->local_id_base == variable->local_id_base &&
           cached->integration_point_fallback == variable->integration_point_fallback;
}

static FVizResult fviz_fea_primary_cache_store(FVizFEAPrimaryVariableEvaluator* evaluator, const FVizFEAField* field,
                                               const FVizUnstructuredGrid* grid, const FVizFEAPrimaryVariable* variable,
                                               FVizFEAPrimaryVariableResult* result)
{
    FVizString* instance = NULL;
    FVizString* component = NULL;
    const FVizDataArray* filter = NULL;
    if (variable->instance_name != NULL && variable->instance_name[0] != '\0' &&
        fviz_string_create_from(variable->instance_name, &instance) != FVIZ_OK)
        return fviz_last_error_code();
    if (variable->component_label != NULL && variable->component_label[0] != '\0' &&
        fviz_string_create_from(variable->component_label, &component) != FVIZ_OK)
    {
        fviz_release(instance);
        return fviz_last_error_code();
    }
    filter = variable->entity_filter_ids != NULL ? (const FVizDataArray*)fviz_retain((void*)variable->entity_filter_ids)
                                                 : NULL;
    if (variable->entity_filter_ids != NULL && filter == NULL)
    {
        fviz_release(instance);
        fviz_release(component);
        return fviz_last_error_code();
    }
    fviz_fea_primary_evaluator_clear_internal(evaluator, FVIZ_FALSE);
    evaluator->cached_field = (const FVizFEAField*)fviz_retain((void*)field);
    evaluator->cached_grid = (const FVizUnstructuredGrid*)fviz_retain((void*)grid);
    evaluator->cached_variable = *variable;
    evaluator->cached_variable.instance_name = NULL;
    evaluator->cached_variable.component_label = NULL;
    evaluator->cached_variable.entity_filter_ids = filter;
    evaluator->cached_instance_name = instance;
    evaluator->cached_component_label = component;
    evaluator->cached_result = (FVizFEAPrimaryVariableResult*)fviz_retain(result);
    evaluator->cached_field_mtime = fviz_object_mtime((const FVizObject*)field);
    evaluator->cached_grid_mtime = fviz_object_mtime((const FVizObject*)grid);
    evaluator->cached_filter_mtime = filter != NULL ? fviz_object_mtime((const FVizObject*)filter) : 0u;
    return evaluator->cached_field != NULL && evaluator->cached_grid != NULL && evaluator->cached_result != NULL
               ? FVIZ_OK
               : fviz_last_error_code();
}

static FVizBool fviz_fea_block_basic_match(const FVizFEAField* field, FVizSize block_index,
                                           const FVizFEAPrimaryVariable* variable)
{
    const char* block_instance = fviz_fea_field_block_instance_name(field, block_index);
    const int32_t section = fviz_fea_field_block_section_point_number(field, block_index);
    if (variable->instance_name != NULL && variable->instance_name[0] != '\0' &&
        strcmp(block_instance, variable->instance_name) != 0)
        return FVIZ_FALSE;
    if (variable->section_point_number != FVIZ_FEA_SECTION_POINT_ANY && section != variable->section_point_number)
        return FVIZ_FALSE;
    return FVIZ_TRUE;
}

static FVizBool fviz_fea_field_has_position(const FVizFEAField* field, const FVizFEAPrimaryVariable* variable,
                                            FVizFEAResultPosition position)
{
    FVizSize i;
    for (i = 0u; i < fviz_fea_field_block_count(field); ++i)
        if (fviz_fea_block_basic_match(field, i, variable) && fviz_fea_field_block_position(field, i) == position)
            return FVIZ_TRUE;
    return FVIZ_FALSE;
}

static FVizFEAResultPosition fviz_fea_choose_source_position(const FVizFEAField* field,
                                                             const FVizFEAPrimaryVariable* variable)
{
    static const FVizFEAResultPosition nodal_priority[] = {FVIZ_FEA_POSITION_NODAL, FVIZ_FEA_POSITION_ELEMENT_NODAL,
                                                           FVIZ_FEA_POSITION_INTEGRATION_POINT,
                                                           FVIZ_FEA_POSITION_CENTROID, FVIZ_FEA_POSITION_WHOLE_ELEMENT};
    static const FVizFEAResultPosition element_nodal_priority[] = {
        FVIZ_FEA_POSITION_ELEMENT_NODAL, FVIZ_FEA_POSITION_INTEGRATION_POINT, FVIZ_FEA_POSITION_CENTROID,
        FVIZ_FEA_POSITION_WHOLE_ELEMENT};
    static const FVizFEAResultPosition cell_priority[] = {FVIZ_FEA_POSITION_CENTROID, FVIZ_FEA_POSITION_WHOLE_ELEMENT,
                                                          FVIZ_FEA_POSITION_INTEGRATION_POINT,
                                                          FVIZ_FEA_POSITION_ELEMENT_NODAL};
    const FVizFEAResultPosition* list = nodal_priority;
    FVizSize count = sizeof(nodal_priority) / sizeof(nodal_priority[0]), i;
    if (variable->source_position != FVIZ_FEA_POSITION_UNKNOWN)
        return fviz_fea_field_has_position(field, variable, variable->source_position) ? variable->source_position
                                                                                       : FVIZ_FEA_POSITION_UNKNOWN;
    if (variable->target_position != FVIZ_FEA_POSITION_UNKNOWN &&
        fviz_fea_field_has_position(field, variable, variable->target_position))
        return variable->target_position;
    if (variable->target_position == FVIZ_FEA_POSITION_ELEMENT_NODAL)
    {
        list = element_nodal_priority;
        count = sizeof(element_nodal_priority) / sizeof(element_nodal_priority[0]);
    }
    else if (variable->target_position == FVIZ_FEA_POSITION_CENTROID ||
             variable->target_position == FVIZ_FEA_POSITION_WHOLE_ELEMENT)
    {
        list = cell_priority;
        count = sizeof(cell_priority) / sizeof(cell_priority[0]);
    }
    for (i = 0u; i < count; ++i)
        if (fviz_fea_field_has_position(field, variable, list[i])) return list[i];
    for (i = 0u; i < fviz_fea_field_block_count(field); ++i)
        if (fviz_fea_block_basic_match(field, i, variable)) return fviz_fea_field_block_position(field, i);
    return FVIZ_FEA_POSITION_UNKNOWN;
}

static FVizFEAResultPosition fviz_fea_choose_target_position(FVizFEAResultPosition source,
                                                             FVizFEAResultPosition requested)
{
    if (requested != FVIZ_FEA_POSITION_UNKNOWN) return requested;
    if (source == FVIZ_FEA_POSITION_NODAL || source == FVIZ_FEA_POSITION_ELEMENT_NODAL ||
        source == FVIZ_FEA_POSITION_INTEGRATION_POINT)
        return FVIZ_FEA_POSITION_NODAL;
    return source;
}

static FVizResult fviz_fea_evaluate_block_scalar(const FVizFEAField* field, FVizSize block_index,
                                                 const FVizFEAPrimaryVariable* variable, FVizDataArray** out_scalar)
{
    FVizSize component = variable->component;
    if (out_scalar == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_scalar = NULL;
    if (variable->operation == FVIZ_FEA_PRIMARY_INVARIANT)
        return fviz_fea_field_evaluate_invariant(field, block_index, variable->invariant, out_scalar);
    if (variable->component_label != NULL && variable->component_label[0] != '\0')
    {
        if (fviz_fea_field_find_component(field, variable->component_label, &component) != FVIZ_OK)
            return fviz_last_error_code();
    }
    else if (component == FVIZ_FEA_COMPONENT_BY_LABEL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "component BY_LABEL requires component_label");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_fea_field_evaluate_component(field, block_index, component, out_scalar);
}

static FVizResult fviz_fea_create_raw_arrays(FVizDataArray** values, FVizDataArray** entity_ids,
                                             FVizDataArray** local_ids)
{
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, values) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64, 1u, entity_ids) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_INT64, 1u, local_ids) != FVIZ_OK)
    {
        fviz_release(*values);
        fviz_release(*entity_ids);
        fviz_release(*local_ids);
        *values = NULL;
        *entity_ids = NULL;
        *local_ids = NULL;
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_fea_append_raw(FVizDataArray* values, FVizDataArray* entity_ids, FVizDataArray* local_ids,
                                      double value, FVizId entity_label, int64_t local_id)
{
    const uint64_t eid = (uint64_t)entity_label;
    if (fviz_data_array_append_tuple(values, &value) != FVIZ_OK ||
        fviz_data_array_append_tuple(entity_ids, &eid) != FVIZ_OK ||
        fviz_data_array_append_tuple(local_ids, &local_id) != FVIZ_OK)
        return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizResult fviz_fea_array_range(const FVizDataArray* values, FVizBool* valid, double* minimum, double* maximum)
{
    FVizSize i, count;
    double lo = 0.0, hi = 0.0;
    FVizBool found = FVIZ_FALSE;
    if (valid != NULL) *valid = FVIZ_FALSE;
    if (values == NULL || fviz_data_array_components(values) != 1u) return FVIZ_OK;
    count = fviz_data_array_tuple_count(values);
    for (i = 0u; i < count; ++i)
    {
        double v;
        if (fviz_data_array_get_component(values, i, 0u, &v) != FVIZ_OK) return fviz_last_error_code();
        if (!isfinite(v)) continue;
        if (!found)
        {
            lo = hi = v;
            found = FVIZ_TRUE;
        }
        else
        {
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    }
    if (valid != NULL) *valid = found;
    if (found)
    {
        if (minimum != NULL) *minimum = lo;
        if (maximum != NULL) *maximum = hi;
    }
    return FVIZ_OK;
}

static int fviz_fea_ip_tuple_compare(const void* lhs, const void* rhs)
{
    const FVizFEAIPTuple* a = (const FVizFEAIPTuple*)lhs;
    const FVizFEAIPTuple* b = (const FVizFEAIPTuple*)rhs;
    if (a->cell_id < b->cell_id) return -1;
    if (a->cell_id > b->cell_id) return 1;
    if (a->order < b->order) return -1;
    if (a->order > b->order) return 1;
    if (a->original_order < b->original_order) return -1;
    if (a->original_order > b->original_order) return 1;
    return 0;
}

static FVizResult fviz_fea_append_contribution(FVizArray* contributions, FVizSize point_id, FVizSize cell_id,
                                               FVizSize block_index, FVizId entity_label, int64_t local_id,
                                               double value)
{
    FVizFEAContribution c;
    c.point_id = point_id;
    c.cell_id = cell_id;
    c.block_index = block_index;
    c.entity_label = entity_label;
    c.local_id = local_id;
    c.value = value;
    return fviz_array_push(contributions, &c);
}

static FVizResult fviz_fea_dense_labels(const FVizDataArray* labels, FVizSize count, FVizDataArray** out_ids)
{
    FVizDataArray* ids = NULL;
    FVizSize i;
    if (out_ids == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_ids = NULL;
    if (fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &ids) != FVIZ_OK || fviz_data_array_resize(ids, count) != FVIZ_OK)
    {
        fviz_release(ids);
        return fviz_last_error_code();
    }
    for (i = 0u; i < count; ++i)
    {
        FVizId id = (FVizId)i;
        if (labels != NULL && fviz_fea_read_id(labels, i, &id) != FVIZ_OK)
        {
            fviz_release(ids);
            return fviz_last_error_code();
        }
        ((uint64_t*)fviz_data_array_data(ids))[i] = (uint64_t)id;
    }
    *out_ids = ids;
    return FVIZ_OK;
}

static FVizResult fviz_fea_build_nodal_display(const FVizArray* contributions, const FVizDataArray* point_labels,
                                               FVizSize point_count, const FVizFEAPrimaryVariable* variable,
                                               FVizDataArray** out_values, FVizDataArray** out_ids,
                                               FVizDataArray** out_mask)
{
    FVizDataArray* values = NULL;
    FVizDataArray* ids = NULL;
    FVizDataArray* mask = NULL;
    double *sums = NULL, *mins = NULL, *maxs = NULL;
    FVizSize *counts = NULL, *first_blocks = NULL;
    uint8_t* multi = NULL;
    FVizSize i, n = fviz_array_count(contributions), bytes;
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &values) != FVIZ_OK ||
        fviz_data_array_resize(values, point_count) != FVIZ_OK ||
        fviz_fea_dense_labels(point_labels, point_count, &ids) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &mask) != FVIZ_OK ||
        fviz_data_array_resize(mask, point_count) != FVIZ_OK)
        goto fail;
    if (point_count != 0u)
    {
        if (fviz_size_multiply(point_count, sizeof(double), &bytes) != FVIZ_OK) goto fail;
        sums = (double*)fviz_alloc(bytes);
        mins = (double*)fviz_alloc(bytes);
        maxs = (double*)fviz_alloc(bytes);
        if (fviz_size_multiply(point_count, sizeof(FVizSize), &bytes) != FVIZ_OK) goto fail;
        counts = (FVizSize*)fviz_alloc(bytes);
        first_blocks = (FVizSize*)fviz_alloc(bytes);
        multi = (uint8_t*)fviz_alloc(point_count);
        if (sums == NULL || mins == NULL || maxs == NULL || counts == NULL || first_blocks == NULL || multi == NULL)
            goto fail;
        (void)memset(sums, 0, point_count * sizeof(double));
        (void)memset(counts, 0, point_count * sizeof(FVizSize));
        (void)memset(first_blocks, 0, point_count * sizeof(FVizSize));
        (void)memset(multi, 0, point_count);
    }
    for (i = 0u; i < n; ++i)
    {
        const FVizFEAContribution* c = (const FVizFEAContribution*)fviz_array_const_at(contributions, i);
        const FVizSize p = c->point_id;
        if (p >= point_count || !isfinite(c->value)) continue;
        if (counts[p] == 0u)
        {
            mins[p] = maxs[p] = c->value;
            first_blocks[p] = c->block_index;
        }
        else
        {
            if (c->value < mins[p]) mins[p] = c->value;
            if (c->value > maxs[p]) maxs[p] = c->value;
            if (c->block_index != first_blocks[p]) multi[p] = 1u;
        }
        sums[p] += c->value;
        ++counts[p];
    }
    {
        double* dst = (double*)fviz_data_array_data(values);
        uint8_t* flags = (uint8_t*)fviz_data_array_data(mask);
        for (i = 0u; i < point_count; ++i)
        {
            FVizBool discontinuous = FVIZ_FALSE;
            dst[i] = NAN;
            flags[i] = 0u;
            if (counts[i] == 0u) continue;
            if (!variable->averaging_enabled && counts[i] > 1u) discontinuous = FVIZ_TRUE;
            if (variable->averaging_enabled && !variable->average_across_blocks && multi[i]) discontinuous = FVIZ_TRUE;
            if (variable->averaging_enabled && variable->averaging_threshold_percent >= 0.0 && counts[i] > 1u)
            {
                const double reference = fmax(fmax(fabs(mins[i]), fabs(maxs[i])), 1.0e-30);
                const double spread = 100.0 * fabs(maxs[i] - mins[i]) / reference;
                if (spread > variable->averaging_threshold_percent) discontinuous = FVIZ_TRUE;
            }
            if (discontinuous)
            {
                flags[i] = 1u;
                continue;
            }
            dst[i] = sums[i] / (double)counts[i];
        }
    }
    fviz_free(sums);
    fviz_free(mins);
    fviz_free(maxs);
    fviz_free(counts);
    fviz_free(first_blocks);
    fviz_free(multi);
    *out_values = values;
    *out_ids = ids;
    *out_mask = mask;
    return FVIZ_OK;
fail:
    fviz_free(sums);
    fviz_free(mins);
    fviz_free(maxs);
    fviz_free(counts);
    fviz_free(first_blocks);
    fviz_free(multi);
    fviz_release(values);
    fviz_release(ids);
    fviz_release(mask);
    return fviz_last_error_code();
}

static FVizResult fviz_fea_build_element_nodal_display(const FVizArray* contributions, FVizDataArray** out_values,
                                                       FVizDataArray** out_entity_ids, FVizDataArray** out_local_ids,
                                                       FVizDataArray** out_mask)
{
    FVizDataArray *values = NULL, *entity = NULL, *local = NULL, *mask = NULL;
    FVizSize i, n = fviz_array_count(contributions);
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &values) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &entity) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_INT64, 1u, &local) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &mask) != FVIZ_OK || fviz_data_array_resize(values, n) != FVIZ_OK ||
        fviz_data_array_resize(entity, n) != FVIZ_OK || fviz_data_array_resize(local, n) != FVIZ_OK ||
        fviz_data_array_resize(mask, n) != FVIZ_OK)
        goto fail;
    for (i = 0u; i < n; ++i)
    {
        const FVizFEAContribution* c = (const FVizFEAContribution*)fviz_array_const_at(contributions, i);
        ((double*)fviz_data_array_data(values))[i] = c->value;
        ((uint64_t*)fviz_data_array_data(entity))[i] = (uint64_t)c->entity_label;
        ((int64_t*)fviz_data_array_data(local))[i] = c->local_id;
        ((uint8_t*)fviz_data_array_data(mask))[i] = 0u;
    }
    *out_values = values;
    *out_entity_ids = entity;
    *out_local_ids = local;
    *out_mask = mask;
    return FVIZ_OK;
fail:
    fviz_release(values);
    fviz_release(entity);
    fviz_release(local);
    fviz_release(mask);
    return fviz_last_error_code();
}

static FVizResult fviz_fea_build_cell_display(const FVizArray* cell_samples, const FVizDataArray* cell_labels,
                                              FVizSize cell_count, const FVizFEAPrimaryVariable* variable,
                                              FVizDataArray** out_values, FVizDataArray** out_ids,
                                              FVizDataArray** out_mask)
{
    FVizDataArray *values = NULL, *ids = NULL, *mask = NULL;
    double *sums = NULL, *mins = NULL, *maxs = NULL;
    FVizSize *counts = NULL, *first_blocks = NULL;
    uint8_t* multi = NULL;
    FVizSize i, n = fviz_array_count(cell_samples), bytes;
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &values) != FVIZ_OK ||
        fviz_data_array_resize(values, cell_count) != FVIZ_OK ||
        fviz_fea_dense_labels(cell_labels, cell_count, &ids) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &mask) != FVIZ_OK ||
        fviz_data_array_resize(mask, cell_count) != FVIZ_OK)
        goto fail;
    if (cell_count != 0u)
    {
        if (fviz_size_multiply(cell_count, sizeof(double), &bytes) != FVIZ_OK) goto fail;
        sums = (double*)fviz_alloc(bytes);
        mins = (double*)fviz_alloc(bytes);
        maxs = (double*)fviz_alloc(bytes);
        if (fviz_size_multiply(cell_count, sizeof(FVizSize), &bytes) != FVIZ_OK) goto fail;
        counts = (FVizSize*)fviz_alloc(bytes);
        first_blocks = (FVizSize*)fviz_alloc(bytes);
        multi = (uint8_t*)fviz_alloc(cell_count);
        if (sums == NULL || mins == NULL || maxs == NULL || counts == NULL || first_blocks == NULL || multi == NULL)
            goto fail;
        (void)memset(sums, 0, cell_count * sizeof(double));
        (void)memset(counts, 0, cell_count * sizeof(FVizSize));
        (void)memset(first_blocks, 0, cell_count * sizeof(FVizSize));
        (void)memset(multi, 0, cell_count);
    }
    for (i = 0u; i < n; ++i)
    {
        const FVizFEAContribution* c = (const FVizFEAContribution*)fviz_array_const_at(cell_samples, i);
        const FVizSize cell = c->cell_id;
        if (cell >= cell_count || !isfinite(c->value)) continue;
        if (counts[cell] == 0u)
        {
            mins[cell] = maxs[cell] = c->value;
            first_blocks[cell] = c->block_index;
        }
        else
        {
            if (c->value < mins[cell]) mins[cell] = c->value;
            if (c->value > maxs[cell]) maxs[cell] = c->value;
            if (c->block_index != first_blocks[cell]) multi[cell] = 1u;
        }
        sums[cell] += c->value;
        ++counts[cell];
    }
    {
        double* dst = (double*)fviz_data_array_data(values);
        uint8_t* flags = (uint8_t*)fviz_data_array_data(mask);
        for (i = 0u; i < cell_count; ++i)
        {
            FVizBool discontinuous = FVIZ_FALSE;
            dst[i] = NAN;
            flags[i] = 0u;
            if (counts[i] == 0u) continue;
            if (!variable->averaging_enabled && counts[i] > 1u) discontinuous = FVIZ_TRUE;
            if (variable->averaging_enabled && !variable->average_across_blocks && multi[i]) discontinuous = FVIZ_TRUE;
            if (variable->averaging_enabled && variable->averaging_threshold_percent >= 0.0 && counts[i] > 1u)
            {
                const double reference = fmax(fmax(fabs(mins[i]), fabs(maxs[i])), 1.0e-30);
                if (100.0 * fabs(maxs[i] - mins[i]) / reference > variable->averaging_threshold_percent)
                    discontinuous = FVIZ_TRUE;
            }
            if (discontinuous)
            {
                flags[i] = 1u;
                continue;
            }
            dst[i] = sums[i] / (double)counts[i];
        }
    }
    fviz_free(sums);
    fviz_free(mins);
    fviz_free(maxs);
    fviz_free(counts);
    fviz_free(first_blocks);
    fviz_free(multi);
    *out_values = values;
    *out_ids = ids;
    *out_mask = mask;
    return FVIZ_OK;
fail:
    fviz_free(sums);
    fviz_free(mins);
    fviz_free(maxs);
    fviz_free(counts);
    fviz_free(first_blocks);
    fviz_free(multi);
    fviz_release(values);
    fviz_release(ids);
    fviz_release(mask);
    return fviz_last_error_code();
}

static FVizFEALocalIdBase fviz_fea_detect_local_base(const FVizDataArray* local_ids, FVizFEALocalIdBase requested)
{
    FVizSize i;
    if (requested != FVIZ_FEA_LOCAL_ID_AUTO) return requested;
    if (local_ids == NULL) return FVIZ_FEA_LOCAL_ID_ZERO_BASED;
    for (i = 0u; i < fviz_data_array_tuple_count(local_ids); ++i)
    {
        int64_t id = 0;
        if (fviz_fea_read_local_id(local_ids, i, &id) == FVIZ_OK && id == 0) return FVIZ_FEA_LOCAL_ID_ZERO_BASED;
    }
    return FVIZ_FEA_LOCAL_ID_ONE_BASED;
}

static FVizResult fviz_fea_primary_result_create(FVizFEAPrimaryVariableResult** out_result)
{
    FVizFEAPrimaryVariableResult* result;
    if (out_result == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_result = NULL;
    result = (FVizFEAPrimaryVariableResult*)fviz_internal_object_allocate(sizeof(*result),
                                                                          &g_fviz_fea_primary_result_class, NULL);
    if (result == NULL) return fviz_last_error_code();
    *out_result = result;
    return FVIZ_OK;
}

static FVizResult fviz_fea_primary_process_block(const FVizFEAField* field, FVizSize block_index,
                                                 const FVizUnstructuredGrid* grid,
                                                 const FVizFEAPrimaryVariable* variable, FVizFEAResultPosition source,
                                                 FVizFEAResultPosition target, const FVizHashMap* point_map,
                                                 const FVizHashMap* cell_map, const FVizHashMap* filter_set,
                                                 FVizDataArray* raw_values, FVizDataArray* raw_entity,
                                                 FVizDataArray* raw_local, FVizArray* point_contributions,
                                                 FVizArray* cell_samples, FVizArray* element_node_contributions)
{
    const FVizDataArray* entity_ids = fviz_fea_field_block_entity_ids(field, block_index);
    const FVizDataArray* local_ids = fviz_fea_field_block_local_ids(field, block_index);
    FVizDataArray* scalar = NULL;
    FVizArray* ips = NULL;
    FVizDataArray *dense = NULL, *element_nodal = NULL;
    FVizSize *ip_counts = NULL, *ip_offsets = NULL;
    const FVizSize point_count = fviz_unstructured_grid_point_count(grid),
                   cell_count = fviz_unstructured_grid_cell_count(grid);
    FVizSize tuple_count, i;
    if (fviz_fea_evaluate_block_scalar(field, block_index, variable, &scalar) != FVIZ_OK) return fviz_last_error_code();
    tuple_count = fviz_data_array_tuple_count(scalar);
    if ((entity_ids != NULL && fviz_data_array_tuple_count(entity_ids) != tuple_count) ||
        (local_ids != NULL && fviz_data_array_tuple_count(local_ids) != tuple_count))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "field block ids do not match evaluated tuple count");
        goto fail;
    }
    if (source == FVIZ_FEA_POSITION_NODAL)
    {
        for (i = 0u; i < tuple_count; ++i)
        {
            FVizId label = (FVizId)i;
            FVizSize point_id;
            double value;
            if (entity_ids != NULL && fviz_fea_read_id(entity_ids, i, &label) != FVIZ_OK) goto fail;
            if (!fviz_fea_filter_accepts(filter_set, label)) continue;
            if (!fviz_fea_resolve_label(label, point_map, point_count, &point_id))
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                        "nodal result label does not exist in grid GlobalIds/internal ids");
                goto fail;
            }
            if (fviz_data_array_get_component(scalar, i, 0u, &value) != FVIZ_OK ||
                fviz_fea_append_raw(raw_values, raw_entity, raw_local, value, label, FVIZ_FEA_INVALID_LOCAL) !=
                    FVIZ_OK ||
                fviz_fea_append_contribution(point_contributions, point_id, (FVizSize)-1, block_index, label,
                                             FVIZ_FEA_INVALID_LOCAL, value) != FVIZ_OK)
                goto fail;
        }
    }
    else if (source == FVIZ_FEA_POSITION_ELEMENT_NODAL)
    {
        FVizSize* occurrence = NULL;
        FVizSize bytes = 0u;
        const FVizFEALocalIdBase base = fviz_fea_detect_local_base(local_ids, variable->local_id_base);
        if (cell_count != 0u)
        {
            if (fviz_size_multiply(cell_count, sizeof(FVizSize), &bytes) != FVIZ_OK) goto fail;
            occurrence = (FVizSize*)fviz_alloc(bytes);
            if (occurrence == NULL) goto fail;
            (void)memset(occurrence, 0, bytes);
        }
        for (i = 0u; i < tuple_count; ++i)
        {
            FVizId label;
            FVizSize cell_id, local_index;
            int64_t local_raw;
            FVizCellView view;
            double value;
            if (entity_ids == NULL)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                        "element-nodal field blocks require entity_ids for mesh mapping");
                fviz_free(occurrence);
                goto fail;
            }
            if (fviz_fea_read_id(entity_ids, i, &label) != FVIZ_OK)
            {
                fviz_free(occurrence);
                goto fail;
            }
            if (!fviz_fea_filter_accepts(filter_set, label)) continue;
            if (!fviz_fea_resolve_label(label, cell_map, cell_count, &cell_id))
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                        "element-nodal element label does not exist in grid GlobalIds/internal ids");
                fviz_free(occurrence);
                goto fail;
            }
            if (local_ids != NULL)
            {
                if (fviz_fea_read_local_id(local_ids, i, &local_raw) != FVIZ_OK)
                {
                    fviz_free(occurrence);
                    goto fail;
                }
                if (base == FVIZ_FEA_LOCAL_ID_ONE_BASED) --local_raw;
                if (local_raw < 0)
                {
                    fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                            "element-nodal local id is negative after base conversion");
                    fviz_free(occurrence);
                    goto fail;
                }
                local_index = (FVizSize)local_raw;
            }
            else
                local_index = occurrence[cell_id]++;
            if (fviz_cell_array_cell_view(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), cell_id, &view) !=
                    FVIZ_OK ||
                local_index >= view.point_count)
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "element-nodal local id exceeds cell node count");
                fviz_free(occurrence);
                goto fail;
            }
            if (fviz_data_array_get_component(scalar, i, 0u, &value) != FVIZ_OK ||
                fviz_fea_append_raw(raw_values, raw_entity, raw_local, value, label, (int64_t)local_index) != FVIZ_OK ||
                fviz_fea_append_contribution(point_contributions, (FVizSize)fviz_cell_view_point_id(&view, local_index),
                                             cell_id, block_index, label, (int64_t)local_index, value) != FVIZ_OK ||
                fviz_fea_append_contribution(element_node_contributions,
                                             (FVizSize)fviz_cell_view_point_id(&view, local_index), cell_id,
                                             block_index, label, (int64_t)local_index, value) != FVIZ_OK ||
                fviz_fea_append_contribution(cell_samples, (FVizSize)-1, cell_id, block_index, label,
                                             (int64_t)local_index, value) != FVIZ_OK)
            {
                fviz_free(occurrence);
                goto fail;
            }
        }
        fviz_free(occurrence);
    }
    else if (source == FVIZ_FEA_POSITION_CENTROID || source == FVIZ_FEA_POSITION_WHOLE_ELEMENT)
    {
        for (i = 0u; i < tuple_count; ++i)
        {
            FVizId label = (FVizId)i;
            FVizSize cell_id, k;
            FVizCellView view;
            double value;
            if (entity_ids != NULL && fviz_fea_read_id(entity_ids, i, &label) != FVIZ_OK) goto fail;
            if (!fviz_fea_filter_accepts(filter_set, label)) continue;
            if (!fviz_fea_resolve_label(label, cell_map, cell_count, &cell_id))
            {
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                        "element result label does not exist in grid GlobalIds/internal ids");
                goto fail;
            }
            if (fviz_data_array_get_component(scalar, i, 0u, &value) != FVIZ_OK ||
                fviz_fea_append_raw(raw_values, raw_entity, raw_local, value, label, FVIZ_FEA_INVALID_LOCAL) !=
                    FVIZ_OK ||
                fviz_fea_append_contribution(cell_samples, (FVizSize)-1, cell_id, block_index, label,
                                             FVIZ_FEA_INVALID_LOCAL, value) != FVIZ_OK)
                goto fail;
            if (target == FVIZ_FEA_POSITION_NODAL || target == FVIZ_FEA_POSITION_ELEMENT_NODAL)
            {
                if (fviz_cell_array_cell_view(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), cell_id,
                                              &view) != FVIZ_OK)
                    goto fail;
                for (k = 0u; k < view.point_count; ++k)
                {
                    const FVizSize p = (FVizSize)fviz_cell_view_point_id(&view, k);
                    if (fviz_fea_append_contribution(point_contributions, p, cell_id, block_index, label, (int64_t)k,
                                                     value) != FVIZ_OK ||
                        fviz_fea_append_contribution(element_node_contributions, p, cell_id, block_index, label,
                                                     (int64_t)k, value) != FVIZ_OK)
                        goto fail;
                }
            }
        }
    }
    else if (source == FVIZ_FEA_POSITION_INTEGRATION_POINT)
    {
        FVizSize bytes, total_ip = 0u;
        if (entity_ids == NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                    "integration-point field blocks require entity_ids for mesh mapping");
            goto fail;
        }
        if (fviz_array_create(sizeof(FVizFEAIPTuple), &ips) != FVIZ_OK) goto fail;
        if (cell_count != 0u)
        {
            if (fviz_size_multiply(cell_count, sizeof(FVizSize), &bytes) != FVIZ_OK) goto ip_fail;
            ip_counts = (FVizSize*)fviz_alloc(bytes);
            if (ip_counts == NULL) goto ip_fail;
            (void)memset(ip_counts, 0, bytes);
        }
        for (i = 0u; i < tuple_count; ++i)
        {
            FVizId label;
            FVizSize cell_id;
            int64_t local = (int64_t)i;
            double value;
            FVizFEAIPTuple ip;
            if (fviz_fea_read_id(entity_ids, i, &label) != FVIZ_OK) goto ip_fail;
            if (!fviz_fea_filter_accepts(filter_set, label)) continue;
            if (!fviz_fea_resolve_label(label, cell_map, cell_count, &cell_id))
            {
                fviz_internal_set_error(
                    FVIZ_ERROR_INVALID_ARGUMENT,
                    "integration-point element label does not exist in grid GlobalIds/internal ids");
                goto ip_fail;
            }
            if (local_ids != NULL && fviz_fea_read_local_id(local_ids, i, &local) != FVIZ_OK) goto ip_fail;
            if (fviz_data_array_get_component(scalar, i, 0u, &value) != FVIZ_OK ||
                fviz_fea_append_raw(raw_values, raw_entity, raw_local, value, label, local) != FVIZ_OK)
                goto ip_fail;
            ip.cell_id = cell_id;
            ip.order = local;
            ip.original_order = i;
            ip.entity_label = label;
            ip.value = value;
            if (fviz_array_push(ips, &ip) != FVIZ_OK) goto ip_fail;
            ++ip_counts[cell_id];
            ++total_ip;
            if (fviz_fea_append_contribution(cell_samples, (FVizSize)-1, cell_id, block_index, label, local, value) !=
                FVIZ_OK)
                goto ip_fail;
        }
        if (target == FVIZ_FEA_POSITION_CENTROID || target == FVIZ_FEA_POSITION_WHOLE_ELEMENT)
        {
            fviz_release(ips);
            ips = NULL;
            fviz_free(ip_counts);
            ip_counts = NULL;
            fviz_release(scalar);
            return FVIZ_OK;
        }
        if (fviz_size_multiply(cell_count + 1u, sizeof(FVizSize), &bytes) != FVIZ_OK) goto ip_fail;
        ip_offsets = (FVizSize*)fviz_alloc(bytes);
        if (ip_offsets == NULL) goto ip_fail;
        ip_offsets[0] = 0u;
        for (i = 0u; i < cell_count; ++i)
            ip_offsets[i + 1u] = ip_offsets[i] + ip_counts[i];
        if (fviz_array_count(ips) > 1u)
            qsort(fviz_array_data(ips), (size_t)fviz_array_count(ips), sizeof(FVizFEAIPTuple),
                  fviz_fea_ip_tuple_compare);
        if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &dense) != FVIZ_OK ||
            fviz_data_array_resize(dense, total_ip) != FVIZ_OK)
            goto ip_fail;
        for (i = 0u; i < fviz_array_count(ips); ++i)
            ((double*)fviz_data_array_data(dense))[i] = ((const FVizFEAIPTuple*)fviz_array_const_at(ips, i))->value;
        {
            FVizIntegrationPointExtrapolationOptions options;
            fviz_integration_point_extrapolation_options_initialize(&options);
            options.fallback_policy = variable->integration_point_fallback;
            if (fviz_unstructured_grid_extrapolate_integration_point_data_element_nodal(
                    grid, dense, ip_offsets, NULL, &options, &element_nodal) != FVIZ_OK)
                goto ip_fail;
        }
        {
            FVizSize cell, node, tuple = 0u;
            for (cell = 0u; cell < cell_count; ++cell)
            {
                FVizCellView view;
                FVizId label = (FVizId)cell;
                if (fviz_cell_array_cell_view(fviz_unstructured_grid_cells((FVizUnstructuredGrid*)grid), cell, &view) !=
                    FVIZ_OK)
                    goto ip_fail;
                if (ip_counts[cell] != 0u)
                {
                    const FVizFEAIPTuple* first = (const FVizFEAIPTuple*)fviz_array_const_at(ips, ip_offsets[cell]);
                    label = first->entity_label;
                }
                for (node = 0u; node < view.point_count; ++node, ++tuple)
                {
                    double value = ((const double*)fviz_data_array_const_data(element_nodal))[tuple];
                    if (!isfinite(value) || ip_counts[cell] == 0u) continue;
                    if (fviz_fea_append_contribution(point_contributions,
                                                     (FVizSize)fviz_cell_view_point_id(&view, node), cell, block_index,
                                                     label, (int64_t)node, value) != FVIZ_OK ||
                        fviz_fea_append_contribution(element_node_contributions,
                                                     (FVizSize)fviz_cell_view_point_id(&view, node), cell, block_index,
                                                     label, (int64_t)node, value) != FVIZ_OK)
                        goto ip_fail;
                }
            }
        }
        fviz_release(ips);
        ips = NULL;
        fviz_free(ip_counts);
        ip_counts = NULL;
        fviz_free(ip_offsets);
        ip_offsets = NULL;
        fviz_release(dense);
        dense = NULL;
        fviz_release(element_nodal);
        element_nodal = NULL;
    }
    else
    {
        if (target != source)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                    "requested result-position conversion is not implemented for this source position");
            goto fail;
        }
        for (i = 0u; i < tuple_count; ++i)
        {
            FVizId label = (FVizId)i;
            int64_t local = FVIZ_FEA_INVALID_LOCAL;
            double value;
            if (entity_ids != NULL && fviz_fea_read_id(entity_ids, i, &label) != FVIZ_OK) goto fail;
            if (!fviz_fea_filter_accepts(filter_set, label)) continue;
            if (local_ids != NULL && fviz_fea_read_local_id(local_ids, i, &local) != FVIZ_OK) goto fail;
            if (fviz_data_array_get_component(scalar, i, 0u, &value) != FVIZ_OK ||
                fviz_fea_append_raw(raw_values, raw_entity, raw_local, value, label, local) != FVIZ_OK)
                goto fail;
        }
    }
    fviz_release(scalar);
    return FVIZ_OK;
ip_fail:
    fviz_release(ips);
    fviz_free(ip_counts);
    fviz_free(ip_offsets);
    fviz_release(dense);
    fviz_release(element_nodal);
fail:
    fviz_release(scalar);
    return fviz_last_error_code();
}

FVizResult fviz_fea_primary_variable_evaluate(FVizFEAPrimaryVariableEvaluator* evaluator, const FVizFEAField* field,
                                              const FVizUnstructuredGrid* grid,
                                              const FVizFEAPrimaryVariable* user_variable,
                                              FVizFEAPrimaryVariableResult** out_result)
{
    FVizFEAPrimaryVariable defaults;
    const FVizFEAPrimaryVariable* variable = user_variable;
    FVizFEAPrimaryVariableResult* result = NULL;
    FVizHashMap *point_map = NULL, *cell_map = NULL, *filter_set = NULL;
    FVizArray *point_contributions = NULL, *cell_samples = NULL, *element_node_contributions = NULL;
    const FVizDataArray *point_labels, *cell_labels;
    FVizFEAResultPosition source, target;
    FVizSize i, matched = 0u;
    if (out_result == NULL || evaluator == NULL || field == NULL || grid == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "primary-variable evaluation requires evaluator, field, grid and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_result = NULL;
    if (variable == NULL)
    {
        fviz_fea_primary_variable_initialize(&defaults);
        variable = &defaults;
    }
    else if (variable->struct_size < sizeof(FVizFEAPrimaryVariable))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "primary-variable descriptor struct is too small");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (variable->operation != FVIZ_FEA_PRIMARY_COMPONENT && variable->operation != FVIZ_FEA_PRIMARY_INVARIANT)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid primary-variable operation");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (variable->local_id_base < FVIZ_FEA_LOCAL_ID_AUTO || variable->local_id_base > FVIZ_FEA_LOCAL_ID_ONE_BASED)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid primary-variable local-id base");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_fea_primary_cache_matches(evaluator, field, grid, variable))
    {
        ++evaluator->hits;
        *out_result = (FVizFEAPrimaryVariableResult*)fviz_retain(evaluator->cached_result);
        return *out_result != NULL ? FVIZ_OK : fviz_last_error_code();
    }
    ++evaluator->misses;
    if (fviz_unstructured_grid_validate(grid) != FVIZ_OK) return fviz_last_error_code();
    source = fviz_fea_choose_source_position(field, variable);
    if (source == FVIZ_FEA_POSITION_UNKNOWN)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND,
                                "no field block matches the requested instance/section/source position");
        return FVIZ_ERROR_NOT_FOUND;
    }
    target = fviz_fea_choose_target_position(source, variable->target_position);
    point_labels = fviz_attribute_set_const_active(fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid),
                                                   FVIZ_ATTRIBUTE_GLOBAL_IDS);
    cell_labels = fviz_attribute_set_const_active(fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid),
                                                  FVIZ_ATTRIBUTE_GLOBAL_IDS);
    if (fviz_fea_build_label_map(point_labels, fviz_unstructured_grid_point_count(grid), &point_map) != FVIZ_OK ||
        fviz_fea_build_label_map(cell_labels, fviz_unstructured_grid_cell_count(grid), &cell_map) != FVIZ_OK ||
        fviz_fea_build_filter_set(variable->entity_filter_ids, &filter_set) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizFEAContribution), &point_contributions) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizFEAContribution), &cell_samples) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizFEAContribution), &element_node_contributions) != FVIZ_OK ||
        fviz_fea_primary_result_create(&result) != FVIZ_OK ||
        fviz_fea_create_raw_arrays(&result->raw_values, &result->raw_entity_ids, &result->raw_local_ids) != FVIZ_OK)
        goto fail;
    result->source_position = source;
    result->target_position = target;
    for (i = 0u; i < fviz_fea_field_block_count(field); ++i)
    {
        if (!fviz_fea_block_basic_match(field, i, variable) || fviz_fea_field_block_position(field, i) != source)
            continue;
        if (fviz_fea_primary_process_block(field, i, grid, variable, source, target, point_map, cell_map, filter_set,
                                           result->raw_values, result->raw_entity_ids, result->raw_local_ids,
                                           point_contributions, cell_samples, element_node_contributions) != FVIZ_OK)
            goto fail;
        ++matched;
    }
    if (matched == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "no primary-variable blocks were selected");
        goto fail;
    }
    if (target == FVIZ_FEA_POSITION_NODAL)
    {
        if (source == FVIZ_FEA_POSITION_NODAL || source == FVIZ_FEA_POSITION_ELEMENT_NODAL ||
            source == FVIZ_FEA_POSITION_INTEGRATION_POINT || source == FVIZ_FEA_POSITION_CENTROID ||
            source == FVIZ_FEA_POSITION_WHOLE_ELEMENT)
        {
            if (fviz_fea_build_nodal_display(
                    point_contributions, point_labels, fviz_unstructured_grid_point_count(grid), variable,
                    &result->display_values, &result->display_entity_ids, &result->discontinuity_mask) != FVIZ_OK)
                goto fail;
            result->association = FVIZ_FEA_DISPLAY_ASSOCIATION_POINT;
        }
        else
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                    "source position cannot currently be converted to nodal display");
            goto fail;
        }
    }
    else if (target == FVIZ_FEA_POSITION_ELEMENT_NODAL)
    {
        if (fviz_fea_build_element_nodal_display(element_node_contributions, &result->display_values,
                                                 &result->display_entity_ids, &result->display_local_ids,
                                                 &result->discontinuity_mask) != FVIZ_OK)
            goto fail;
        result->association = FVIZ_FEA_DISPLAY_ASSOCIATION_ELEMENT_NODE;
    }
    else if (target == FVIZ_FEA_POSITION_CENTROID || target == FVIZ_FEA_POSITION_WHOLE_ELEMENT)
    {
        if (source == FVIZ_FEA_POSITION_NODAL)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                    "nodal-to-centroid primary-variable reduction is not implemented yet");
            goto fail;
        }
        if (fviz_fea_build_cell_display(cell_samples, cell_labels, fviz_unstructured_grid_cell_count(grid), variable,
                                        &result->display_values, &result->display_entity_ids,
                                        &result->discontinuity_mask) != FVIZ_OK)
            goto fail;
        result->association = FVIZ_FEA_DISPLAY_ASSOCIATION_CELL;
    }
    else if (target == source)
    {
        if (fviz_data_array_deep_copy(result->raw_values, &result->display_values) != FVIZ_OK ||
            fviz_data_array_deep_copy(result->raw_entity_ids, &result->display_entity_ids) != FVIZ_OK ||
            fviz_data_array_deep_copy(result->raw_local_ids, &result->display_local_ids) != FVIZ_OK ||
            fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &result->discontinuity_mask) != FVIZ_OK ||
            fviz_data_array_resize(result->discontinuity_mask, fviz_data_array_tuple_count(result->display_values)) !=
                FVIZ_OK)
            goto fail;
        (void)memset(fviz_data_array_data(result->discontinuity_mask), 0,
                     fviz_data_array_tuple_count(result->display_values));
        result->association = FVIZ_FEA_DISPLAY_ASSOCIATION_RAW;
    }
    else
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                "requested target result position is not supported by the primary-variable engine");
        goto fail;
    }
    if (fviz_fea_array_range(result->raw_values, &result->raw_range_valid, &result->raw_minimum,
                             &result->raw_maximum) != FVIZ_OK ||
        fviz_fea_array_range(result->display_values, &result->display_range_valid, &result->display_minimum,
                             &result->display_maximum) != FVIZ_OK)
        goto fail;
    if (fviz_fea_primary_cache_store(evaluator, field, grid, variable, result) != FVIZ_OK) goto fail;
    *out_result = result;
    fviz_release(point_map);
    fviz_release(cell_map);
    fviz_release(filter_set);
    fviz_release(point_contributions);
    fviz_release(cell_samples);
    fviz_release(element_node_contributions);
    return FVIZ_OK;
fail:
    fviz_release(point_map);
    fviz_release(cell_map);
    fviz_release(filter_set);
    fviz_release(point_contributions);
    fviz_release(cell_samples);
    fviz_release(element_node_contributions);
    fviz_release(result);
    return fviz_last_error_code();
}

FVizFEAResultPosition fviz_fea_primary_variable_result_source_position(const FVizFEAPrimaryVariableResult* result)
{
    return result != NULL ? result->source_position : FVIZ_FEA_POSITION_UNKNOWN;
}

FVizFEAResultPosition fviz_fea_primary_variable_result_target_position(const FVizFEAPrimaryVariableResult* result)
{
    return result != NULL ? result->target_position : FVIZ_FEA_POSITION_UNKNOWN;
}

FVizFEADisplayAssociation fviz_fea_primary_variable_result_association(const FVizFEAPrimaryVariableResult* result)
{
    return result != NULL ? result->association : FVIZ_FEA_DISPLAY_ASSOCIATION_NONE;
}

const FVizDataArray* fviz_fea_primary_variable_result_raw_values(const FVizFEAPrimaryVariableResult* result)
{
    return result != NULL ? result->raw_values : NULL;
}

const FVizDataArray* fviz_fea_primary_variable_result_raw_entity_ids(const FVizFEAPrimaryVariableResult* result)
{
    return result != NULL ? result->raw_entity_ids : NULL;
}

const FVizDataArray* fviz_fea_primary_variable_result_raw_local_ids(const FVizFEAPrimaryVariableResult* result)
{
    return result != NULL ? result->raw_local_ids : NULL;
}

const FVizDataArray* fviz_fea_primary_variable_result_display_values(const FVizFEAPrimaryVariableResult* result)
{
    return result != NULL ? result->display_values : NULL;
}

const FVizDataArray* fviz_fea_primary_variable_result_display_entity_ids(const FVizFEAPrimaryVariableResult* result)
{
    return result != NULL ? result->display_entity_ids : NULL;
}

const FVizDataArray* fviz_fea_primary_variable_result_display_local_ids(const FVizFEAPrimaryVariableResult* result)
{
    return result != NULL ? result->display_local_ids : NULL;
}

const FVizDataArray* fviz_fea_primary_variable_result_discontinuity_mask(const FVizFEAPrimaryVariableResult* result)
{
    return result != NULL ? result->discontinuity_mask : NULL;
}

FVizBool fviz_fea_primary_variable_result_raw_range(const FVizFEAPrimaryVariableResult* result, double* out_minimum,
                                                    double* out_maximum)
{
    if (result == NULL || !result->raw_range_valid) return FVIZ_FALSE;
    if (out_minimum != NULL) *out_minimum = result->raw_minimum;
    if (out_maximum != NULL) *out_maximum = result->raw_maximum;
    return FVIZ_TRUE;
}

FVizBool fviz_fea_primary_variable_result_display_range(const FVizFEAPrimaryVariableResult* result, double* out_minimum,
                                                        double* out_maximum)
{
    if (result == NULL || !result->display_range_valid) return FVIZ_FALSE;
    if (out_minimum != NULL) *out_minimum = result->display_minimum;
    if (out_maximum != NULL) *out_maximum = result->display_maximum;
    return FVIZ_TRUE;
}
