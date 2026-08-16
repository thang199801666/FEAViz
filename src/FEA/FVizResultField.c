#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizDataType.h>
#include <FViz/FEA/FVizResultField.h>
#include <FViz/Math/FVizTensor.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/FEA/FVizResultFieldPrivate.h>

static void fviz_fea_field_destroy(FVizObject* object);
static FVizMTime fviz_fea_field_mtime(const FVizObject* object);

static const FVizObjectClass g_fviz_fea_field_class = {
    FVIZ_TYPE_FEA_FIELD,
    "FVizFEAField",
    NULL,
    fviz_fea_field_destroy,
    fviz_fea_field_mtime
};

static FVizMTime fviz_fea_field_mtime(const FVizObject* object)
{
    return fviz_internal_object_local_mtime(object);
}

static FVizBool fviz_fea_field_child_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    FVizFEAField* field = (FVizFEAField*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (field != NULL) fviz_object_modified((FVizObject*)field);
    return FVIZ_FALSE;
}

static FVizBool fviz_fea_data_array_is_integer_ids(const FVizDataArray* array)
{
    FVizDataType type;
    if (array == NULL || fviz_data_array_components(array) != 1u) return FVIZ_FALSE;
    type = fviz_data_array_type(array);
    return (type >= FVIZ_DATA_INT8 && type <= FVIZ_DATA_UINT64) ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_fea_field_component_layout_valid(
    FVizFEAFieldType type, uint32_t components)
{
    switch (type)
    {
        case FVIZ_FEA_FIELD_SCALAR: return components == 1u ? FVIZ_TRUE : FVIZ_FALSE;
        case FVIZ_FEA_FIELD_VECTOR: return components >= 1u ? FVIZ_TRUE : FVIZ_FALSE;
        case FVIZ_FEA_FIELD_TENSOR_3D_SYMMETRIC: return components == 6u ? FVIZ_TRUE : FVIZ_FALSE;
        case FVIZ_FEA_FIELD_TENSOR_2D_SYMMETRIC: return components == 4u ? FVIZ_TRUE : FVIZ_FALSE;
        case FVIZ_FEA_FIELD_TENSOR_3D_FULL: return components == 9u ? FVIZ_TRUE : FVIZ_FALSE;
        default: return FVIZ_FALSE;
    }
}

static FVizFEAInvariantMask fviz_fea_field_default_invariants(FVizFEAFieldType type)
{
    const FVizFEAInvariantMask tensor =
        FVIZ_FEA_INVARIANT_BIT(FVIZ_FEA_INVARIANT_MISES) |
        FVIZ_FEA_INVARIANT_BIT(FVIZ_FEA_INVARIANT_TRESCA) |
        FVIZ_FEA_INVARIANT_BIT(FVIZ_FEA_INVARIANT_PRESSURE) |
        FVIZ_FEA_INVARIANT_BIT(FVIZ_FEA_INVARIANT_MAX_PRINCIPAL) |
        FVIZ_FEA_INVARIANT_BIT(FVIZ_FEA_INVARIANT_MID_PRINCIPAL) |
        FVIZ_FEA_INVARIANT_BIT(FVIZ_FEA_INVARIANT_MIN_PRINCIPAL);
    if (type == FVIZ_FEA_FIELD_VECTOR)
        return FVIZ_FEA_INVARIANT_BIT(FVIZ_FEA_INVARIANT_MAGNITUDE);
    if (type == FVIZ_FEA_FIELD_TENSOR_3D_SYMMETRIC ||
        type == FVIZ_FEA_FIELD_TENSOR_2D_SYMMETRIC ||
        type == FVIZ_FEA_FIELD_TENSOR_3D_FULL)
        return tensor;
    return 0u;
}

static void fviz_fea_field_block_release(FVizFEAFieldBlock* block)
{
    if (block == NULL) return;
    if (block->entity_ids != NULL && block->entity_ids_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)block->entity_ids, block->entity_ids_tag);
    if (block->local_ids != NULL && block->local_ids_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)block->local_ids, block->local_ids_tag);
    if (block->values != NULL && block->values_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)block->values, block->values_tag);
    fviz_release(block->instance_name);
    fviz_release(block->section_point_label);
    fviz_release(block->entity_ids);
    fviz_release(block->local_ids);
    fviz_release(block->values);
    (void)memset(block, 0, sizeof(*block));
}

static FVizResult fviz_fea_field_block_observe(
    FVizFEAField* field, FVizDataArray* array, FVizObserverTag* out_tag)
{
    if (out_tag != NULL) *out_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (array == NULL) return FVIZ_OK;
    return fviz_object_add_observer(
        (FVizObject*)array, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_fea_field_child_modified, field, out_tag);
}

static void fviz_fea_field_destroy(FVizObject* object)
{
    FVizFEAField* field = (FVizFEAField*)object;
    FVizSize i;
    if (field->blocks != NULL)
    {
        for (i = 0u; i < fviz_array_count(field->blocks); ++i)
            fviz_fea_field_block_release((FVizFEAFieldBlock*)fviz_array_at(field->blocks, i));
    }
    if (field->component_labels != NULL)
    {
        for (i = 0u; i < fviz_array_count(field->component_labels); ++i)
        {
            FVizString** text = (FVizString**)fviz_array_at(field->component_labels, i);
            if (text != NULL) fviz_release(*text);
        }
    }
    fviz_release(field->blocks);
    fviz_release(field->component_labels);
    fviz_release(field->name);
    fviz_release(field->description);
    field->blocks = NULL;
    field->component_labels = NULL;
    field->name = NULL;
    field->description = NULL;
}

void fviz_fea_field_block_descriptor_initialize(FVizFEAFieldBlockDescriptor* descriptor)
{
    if (descriptor == NULL) return;
    (void)memset(descriptor, 0, sizeof(*descriptor));
    descriptor->struct_size = (uint32_t)sizeof(*descriptor);
    descriptor->position = FVIZ_FEA_POSITION_UNKNOWN;
}

FVizResult fviz_fea_field_create(
    const char* name,
    const char* description,
    FVizFEAFieldType field_type,
    FVizFEAField** out_field)
{
    FVizFEAField* field;
    if (out_field == NULL || name == NULL || name[0] == '\0' ||
        field_type < FVIZ_FEA_FIELD_SCALAR || field_type > FVIZ_FEA_FIELD_TENSOR_3D_FULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA field name, type or output is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_field = NULL;
    field = (FVizFEAField*)fviz_internal_object_allocate(sizeof(*field), &g_fviz_fea_field_class, NULL);
    if (field == NULL) return fviz_last_error_code();
    if (fviz_string_create_from(name, &field->name) != FVIZ_OK ||
        fviz_string_create_from(description != NULL ? description : "", &field->description) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizString*), &field->component_labels) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizFEAFieldBlock), &field->blocks) != FVIZ_OK)
    {
        fviz_release(field);
        return fviz_last_error_code();
    }
    field->field_type = field_type;
    field->valid_invariants = fviz_fea_field_default_invariants(field_type);
    *out_field = field;
    return FVIZ_OK;
}

const char* fviz_fea_field_name(const FVizFEAField* field)
{
    return field != NULL && field->name != NULL ? fviz_string_c_str(field->name) : "";
}

const char* fviz_fea_field_description(const FVizFEAField* field)
{
    return field != NULL && field->description != NULL ? fviz_string_c_str(field->description) : "";
}

FVizFEAFieldType fviz_fea_field_type(const FVizFEAField* field)
{
    return field != NULL ? field->field_type : FVIZ_FEA_FIELD_SCALAR;
}

FVizResult fviz_fea_field_set_component_labels(
    FVizFEAField* field, const char* const* labels, FVizSize label_count)
{
    FVizArray* replacement = NULL;
    FVizSize i;
    if (field == NULL || (label_count != 0u && labels == NULL))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA component labels are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (label_count != 0u && !fviz_fea_field_component_layout_valid(field->field_type, (uint32_t)label_count))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA component label count is incompatible with field type");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_array_create_reserve(sizeof(FVizString*), label_count, &replacement) != FVIZ_OK)
        return fviz_last_error_code();
    for (i = 0u; i < label_count; ++i)
    {
        FVizString* text = NULL;
        if (labels[i] == NULL || labels[i][0] == '\0' ||
            fviz_string_create_from(labels[i], &text) != FVIZ_OK ||
            fviz_array_push(replacement, &text) != FVIZ_OK)
        {
            FVizSize j;
            fviz_release(text);
            for (j = 0u; j < fviz_array_count(replacement); ++j)
                fviz_release(*(FVizString**)fviz_array_at(replacement, j));
            fviz_release(replacement);
            if (labels[i] == NULL || labels[i][0] == '\0')
                fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA component label must not be empty");
            return labels[i] == NULL || labels[i][0] == '\0'
                ? FVIZ_ERROR_INVALID_ARGUMENT : fviz_last_error_code();
        }
    }
    for (i = 0u; i < fviz_array_count(field->component_labels); ++i)
        fviz_release(*(FVizString**)fviz_array_at(field->component_labels, i));
    fviz_release(field->component_labels);
    field->component_labels = replacement;
    fviz_object_modified((FVizObject*)field);
    return FVIZ_OK;
}

FVizSize fviz_fea_field_component_count(const FVizFEAField* field)
{
    if (field == NULL) return 0u;
    if (field->component_labels != NULL && fviz_array_count(field->component_labels) != 0u)
        return fviz_array_count(field->component_labels);
    if (field->blocks != NULL && fviz_array_count(field->blocks) != 0u)
    {
        const FVizFEAFieldBlock* block = (const FVizFEAFieldBlock*)fviz_array_const_at(field->blocks, 0u);
        return block != NULL && block->values != NULL ? (FVizSize)fviz_data_array_components(block->values) : 0u;
    }
    return 0u;
}

const char* fviz_fea_field_component_label(const FVizFEAField* field, FVizSize component)
{
    const FVizString* const* text;
    if (field == NULL || field->component_labels == NULL || component >= fviz_array_count(field->component_labels))
        return "";
    text = (const FVizString* const*)fviz_array_const_at(field->component_labels, component);
    return text != NULL && *text != NULL ? fviz_string_c_str(*text) : "";
}

FVizResult fviz_fea_field_find_component(
    const FVizFEAField* field, const char* label, FVizSize* out_component)
{
    FVizSize i;
    if (out_component != NULL) *out_component = 0u;
    if (field == NULL || label == NULL || label[0] == '\0' || out_component == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA component lookup arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < fviz_array_count(field->component_labels); ++i)
    {
        const char* current = fviz_fea_field_component_label(field, i);
        if (strcmp(current, label) == 0)
        {
            *out_component = i;
            return FVIZ_OK;
        }
    }
    return FVIZ_ERROR_NOT_FOUND;
}

FVizFEAInvariantMask fviz_fea_field_valid_invariants(const FVizFEAField* field)
{
    return field != NULL ? field->valid_invariants : 0u;
}

FVizResult fviz_fea_field_set_valid_invariants(FVizFEAField* field, FVizFEAInvariantMask invariants)
{
    if (field == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA field must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (field->valid_invariants == invariants) return FVIZ_OK;
    field->valid_invariants = invariants;
    fviz_object_modified((FVizObject*)field);
    return FVIZ_OK;
}

FVizSize fviz_fea_field_block_count(const FVizFEAField* field)
{
    return field != NULL && field->blocks != NULL ? fviz_array_count(field->blocks) : 0u;
}

FVizResult fviz_fea_field_add_block(
    FVizFEAField* field,
    const FVizFEAFieldBlockDescriptor* descriptor,
    FVizSize* out_block_index)
{
    FVizFEAFieldBlock block;
    FVizSize tuple_count;
    if (out_block_index != NULL) *out_block_index = 0u;
    if (field == NULL || descriptor == NULL || descriptor->values == NULL ||
        descriptor->struct_size < sizeof(FVizFEAFieldBlockDescriptor) ||
        descriptor->position <= FVIZ_FEA_POSITION_UNKNOWN || descriptor->position > FVIZ_FEA_POSITION_WHOLE_REGION)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA field block descriptor is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (!fviz_fea_field_component_layout_valid(field->field_type, fviz_data_array_components(descriptor->values)))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA field block value component count is incompatible with field type");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_array_count(field->component_labels) != 0u &&
        fviz_array_count(field->component_labels) != (FVizSize)fviz_data_array_components(descriptor->values))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA field block component count differs from component labels");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    tuple_count = fviz_data_array_tuple_count(descriptor->values);
    if (descriptor->entity_ids != NULL &&
        (!fviz_fea_data_array_is_integer_ids(descriptor->entity_ids) ||
         fviz_data_array_tuple_count(descriptor->entity_ids) != tuple_count))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA entity ids must be one-component integer tuples matching values");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (descriptor->local_ids != NULL &&
        (!fviz_fea_data_array_is_integer_ids(descriptor->local_ids) ||
         fviz_data_array_tuple_count(descriptor->local_ids) != tuple_count))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA local ids must be one-component integer tuples matching values");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(&block, 0, sizeof(block));
    block.position = descriptor->position;
    block.section_point_number = descriptor->section_point_number;
    block.entity_ids_tag = FVIZ_OBSERVER_TAG_INVALID;
    block.local_ids_tag = FVIZ_OBSERVER_TAG_INVALID;
    block.values_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (descriptor->instance_name != NULL && descriptor->instance_name[0] != '\0' &&
        fviz_string_create_from(descriptor->instance_name, &block.instance_name) != FVIZ_OK)
        goto fail;
    if (descriptor->section_point_label != NULL && descriptor->section_point_label[0] != '\0' &&
        fviz_string_create_from(descriptor->section_point_label, &block.section_point_label) != FVIZ_OK)
        goto fail;
    block.entity_ids = descriptor->entity_ids != NULL ? (FVizDataArray*)fviz_retain(descriptor->entity_ids) : NULL;
    block.local_ids = descriptor->local_ids != NULL ? (FVizDataArray*)fviz_retain(descriptor->local_ids) : NULL;
    block.values = (FVizDataArray*)fviz_retain(descriptor->values);
    if ((descriptor->entity_ids != NULL && block.entity_ids == NULL) ||
        (descriptor->local_ids != NULL && block.local_ids == NULL) || block.values == NULL)
        goto fail;
    if (fviz_fea_field_block_observe(field, block.entity_ids, &block.entity_ids_tag) != FVIZ_OK ||
        fviz_fea_field_block_observe(field, block.local_ids, &block.local_ids_tag) != FVIZ_OK ||
        fviz_fea_field_block_observe(field, block.values, &block.values_tag) != FVIZ_OK)
        goto fail;
    if (fviz_array_push(field->blocks, &block) != FVIZ_OK) goto fail;
    if (out_block_index != NULL) *out_block_index = fviz_array_count(field->blocks) - 1u;
    fviz_object_modified((FVizObject*)field);
    return FVIZ_OK;
fail:
    fviz_fea_field_block_release(&block);
    return fviz_last_error_code() != FVIZ_OK ? fviz_last_error_code() : FVIZ_ERROR_OUT_OF_MEMORY;
}

FVizResult fviz_fea_field_remove_block(FVizFEAField* field, FVizSize block_index)
{
    FVizSize count;
    FVizFEAFieldBlock* blocks;
    if (field == NULL || block_index >= fviz_fea_field_block_count(field))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA field block index is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_array_count(field->blocks);
    blocks = (FVizFEAFieldBlock*)fviz_array_data(field->blocks);
    fviz_fea_field_block_release(&blocks[block_index]);
    if (block_index + 1u < count)
        (void)memmove(&blocks[block_index], &blocks[block_index + 1u],
            (size_t)(count - block_index - 1u) * sizeof(*blocks));
    if (fviz_array_resize(field->blocks, count - 1u) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)field);
    return FVIZ_OK;
}

static const FVizFEAFieldBlock* fviz_fea_field_const_block(const FVizFEAField* field, FVizSize block_index)
{
    return field != NULL && field->blocks != NULL && block_index < fviz_array_count(field->blocks)
        ? (const FVizFEAFieldBlock*)fviz_array_const_at(field->blocks, block_index) : NULL;
}

const char* fviz_fea_field_block_instance_name(const FVizFEAField* field, FVizSize block_index)
{
    const FVizFEAFieldBlock* block = fviz_fea_field_const_block(field, block_index);
    return block != NULL && block->instance_name != NULL ? fviz_string_c_str(block->instance_name) : "";
}

FVizFEAResultPosition fviz_fea_field_block_position(const FVizFEAField* field, FVizSize block_index)
{
    const FVizFEAFieldBlock* block = fviz_fea_field_const_block(field, block_index);
    return block != NULL ? block->position : FVIZ_FEA_POSITION_UNKNOWN;
}

int32_t fviz_fea_field_block_section_point_number(const FVizFEAField* field, FVizSize block_index)
{
    const FVizFEAFieldBlock* block = fviz_fea_field_const_block(field, block_index);
    return block != NULL ? block->section_point_number : 0;
}

const char* fviz_fea_field_block_section_point_label(const FVizFEAField* field, FVizSize block_index)
{
    const FVizFEAFieldBlock* block = fviz_fea_field_const_block(field, block_index);
    return block != NULL && block->section_point_label != NULL ? fviz_string_c_str(block->section_point_label) : "";
}

FVizDataArray* fviz_fea_field_block_values(FVizFEAField* field, FVizSize block_index)
{
    FVizFEAFieldBlock* block = field != NULL && field->blocks != NULL && block_index < fviz_array_count(field->blocks)
        ? (FVizFEAFieldBlock*)fviz_array_at(field->blocks, block_index) : NULL;
    return block != NULL ? block->values : NULL;
}

const FVizDataArray* fviz_fea_field_block_const_values(const FVizFEAField* field, FVizSize block_index)
{
    const FVizFEAFieldBlock* block = fviz_fea_field_const_block(field, block_index);
    return block != NULL ? block->values : NULL;
}

const FVizDataArray* fviz_fea_field_block_entity_ids(const FVizFEAField* field, FVizSize block_index)
{
    const FVizFEAFieldBlock* block = fviz_fea_field_const_block(field, block_index);
    return block != NULL ? block->entity_ids : NULL;
}

const FVizDataArray* fviz_fea_field_block_local_ids(const FVizFEAField* field, FVizSize block_index)
{
    const FVizFEAFieldBlock* block = fviz_fea_field_const_block(field, block_index);
    return block != NULL ? block->local_ids : NULL;
}

FVizSize fviz_fea_field_total_tuple_count(const FVizFEAField* field)
{
    FVizSize total = 0u;
    FVizSize i;
    if (field == NULL) return 0u;
    for (i = 0u; i < fviz_fea_field_block_count(field); ++i)
    {
        const FVizDataArray* values = fviz_fea_field_block_const_values(field, i);
        const FVizSize count = values != NULL ? fviz_data_array_tuple_count(values) : 0u;
        if (count > (FVizSize)-1 - total) return (FVizSize)-1;
        total += count;
    }
    return total;
}

const char* fviz_fea_result_position_name(FVizFEAResultPosition position)
{
    switch (position)
    {
        case FVIZ_FEA_POSITION_NODAL: return "NODAL";
        case FVIZ_FEA_POSITION_ELEMENT_NODAL: return "ELEMENT_NODAL";
        case FVIZ_FEA_POSITION_INTEGRATION_POINT: return "INTEGRATION_POINT";
        case FVIZ_FEA_POSITION_CENTROID: return "CENTROID";
        case FVIZ_FEA_POSITION_ELEMENT_FACE: return "ELEMENT_FACE";
        case FVIZ_FEA_POSITION_WHOLE_ELEMENT: return "WHOLE_ELEMENT";
        case FVIZ_FEA_POSITION_WHOLE_REGION: return "WHOLE_REGION";
        default: return "UNKNOWN";
    }
}

const char* fviz_fea_invariant_name(FVizFEAInvariant invariant)
{
    switch (invariant)
    {
        case FVIZ_FEA_INVARIANT_MAGNITUDE: return "Magnitude";
        case FVIZ_FEA_INVARIANT_MISES: return "Mises";
        case FVIZ_FEA_INVARIANT_TRESCA: return "Tresca";
        case FVIZ_FEA_INVARIANT_PRESSURE: return "Pressure";
        case FVIZ_FEA_INVARIANT_MAX_PRINCIPAL: return "Max. Principal";
        case FVIZ_FEA_INVARIANT_MID_PRINCIPAL: return "Mid. Principal";
        case FVIZ_FEA_INVARIANT_MIN_PRINCIPAL: return "Min. Principal";
        default: return "None";
    }
}

static double fviz_fea_array_value(const FVizDataArray* array, FVizSize tuple, uint32_t component)
{
    const FVizSize components = (FVizSize)fviz_data_array_components(array);
    const FVizSize index = tuple * components + component;
    const void* data = fviz_data_array_const_data(array);
    switch (fviz_data_array_type(array))
    {
        case FVIZ_DATA_INT8: return (double)((const int8_t*)data)[index];
        case FVIZ_DATA_UINT8: return (double)((const uint8_t*)data)[index];
        case FVIZ_DATA_INT16: return (double)((const int16_t*)data)[index];
        case FVIZ_DATA_UINT16: return (double)((const uint16_t*)data)[index];
        case FVIZ_DATA_INT32: return (double)((const int32_t*)data)[index];
        case FVIZ_DATA_UINT32: return (double)((const uint32_t*)data)[index];
        case FVIZ_DATA_INT64: return (double)((const int64_t*)data)[index];
        case FVIZ_DATA_UINT64: return (double)((const uint64_t*)data)[index];
        case FVIZ_DATA_FLOAT32: return (double)((const float*)data)[index];
        case FVIZ_DATA_FLOAT64: return ((const double*)data)[index];
        default: return NAN;
    }
}

static void fviz_fea_tensor_symmetric_components(
    const FVizFEAField* field,
    const FVizDataArray* values,
    FVizSize tuple,
    double* s11, double* s22, double* s33,
    double* s12, double* s13, double* s23)
{
    if (field->field_type == FVIZ_FEA_FIELD_TENSOR_3D_SYMMETRIC)
    {
        *s11 = fviz_fea_array_value(values, tuple, 0u);
        *s22 = fviz_fea_array_value(values, tuple, 1u);
        *s33 = fviz_fea_array_value(values, tuple, 2u);
        *s12 = fviz_fea_array_value(values, tuple, 3u);
        *s13 = fviz_fea_array_value(values, tuple, 4u);
        *s23 = fviz_fea_array_value(values, tuple, 5u);
    }
    else if (field->field_type == FVIZ_FEA_FIELD_TENSOR_2D_SYMMETRIC)
    {
        *s11 = fviz_fea_array_value(values, tuple, 0u);
        *s22 = fviz_fea_array_value(values, tuple, 1u);
        *s33 = fviz_fea_array_value(values, tuple, 2u);
        *s12 = fviz_fea_array_value(values, tuple, 3u);
        *s13 = 0.0;
        *s23 = 0.0;
    }
    else
    {
        const double a11 = fviz_fea_array_value(values, tuple, 0u);
        const double a12 = fviz_fea_array_value(values, tuple, 1u);
        const double a13 = fviz_fea_array_value(values, tuple, 2u);
        const double a21 = fviz_fea_array_value(values, tuple, 3u);
        const double a22 = fviz_fea_array_value(values, tuple, 4u);
        const double a23 = fviz_fea_array_value(values, tuple, 5u);
        const double a31 = fviz_fea_array_value(values, tuple, 6u);
        const double a32 = fviz_fea_array_value(values, tuple, 7u);
        const double a33 = fviz_fea_array_value(values, tuple, 8u);
        *s11 = a11;
        *s22 = a22;
        *s33 = a33;
        *s12 = 0.5 * (a12 + a21);
        *s13 = 0.5 * (a13 + a31);
        *s23 = 0.5 * (a23 + a32);
    }
}


FVizResult fviz_fea_field_evaluate_component(
    const FVizFEAField* field,
    FVizSize block_index,
    FVizSize component,
    FVizDataArray** out_values)
{
    const FVizDataArray* source;
    FVizDataArray* output = NULL;
    FVizSize tuple_count;
    FVizSize i;
    double* destination;
    if (out_values == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_values = NULL;
    source = fviz_fea_field_block_const_values(field, block_index);
    if (field == NULL || source == NULL || component >= (FVizSize)fviz_data_array_components(source))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA component evaluation arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    tuple_count = fviz_data_array_tuple_count(source);
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &output) != FVIZ_OK ||
        fviz_data_array_resize(output, tuple_count) != FVIZ_OK)
    {
        fviz_release(output);
        return fviz_last_error_code();
    }
    destination = (double*)fviz_data_array_data(output);
    for (i = 0u; i < tuple_count; ++i)
        destination[i] = fviz_fea_array_value(source, i, (uint32_t)component);
    *out_values = output;
    return FVIZ_OK;
}

FVizResult fviz_fea_field_evaluate_invariant(
    const FVizFEAField* field,
    FVizSize block_index,
    FVizFEAInvariant invariant,
    FVizDataArray** out_values)
{
    const FVizDataArray* source;
    FVizDataArray* output = NULL;
    FVizSize tuple_count;
    FVizSize i;
    uint32_t component_count;
    double* destination;
    if (out_values == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_values = NULL;
    source = fviz_fea_field_block_const_values(field, block_index);
    if (field == NULL || source == NULL || invariant <= FVIZ_FEA_INVARIANT_NONE ||
        invariant > FVIZ_FEA_INVARIANT_MIN_PRINCIPAL ||
        (field->valid_invariants & FVIZ_FEA_INVARIANT_BIT(invariant)) == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA invariant is not valid for this field");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    component_count = fviz_data_array_components(source);
    if (invariant == FVIZ_FEA_INVARIANT_MAGNITUDE && field->field_type != FVIZ_FEA_FIELD_VECTOR)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "Magnitude invariant requires a vector field");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (invariant != FVIZ_FEA_INVARIANT_MAGNITUDE &&
        field->field_type != FVIZ_FEA_FIELD_TENSOR_3D_SYMMETRIC &&
        field->field_type != FVIZ_FEA_FIELD_TENSOR_2D_SYMMETRIC &&
        field->field_type != FVIZ_FEA_FIELD_TENSOR_3D_FULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "Tensor invariant requires a tensor field");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    tuple_count = fviz_data_array_tuple_count(source);
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &output) != FVIZ_OK ||
        fviz_data_array_resize(output, tuple_count) != FVIZ_OK)
    {
        fviz_release(output);
        return fviz_last_error_code();
    }
    destination = (double*)fviz_data_array_data(output);
    for (i = 0u; i < tuple_count; ++i)
    {
        if (invariant == FVIZ_FEA_INVARIANT_MAGNITUDE)
        {
            uint32_t c;
            double sum = 0.0;
            for (c = 0u; c < component_count; ++c)
            {
                const double v = fviz_fea_array_value(source, i, c);
                sum += v * v;
            }
            destination[i] = sqrt(sum);
        }
        else
        {
            double s11, s22, s33, s12, s13, s23;
            double principal[3];
            FVizSymmetricTensor3d tensor;
            fviz_fea_tensor_symmetric_components(field, source, i, &s11, &s22, &s33, &s12, &s13, &s23);
            tensor.xx = s11; tensor.yy = s22; tensor.zz = s33;
            tensor.xy = s12; tensor.yz = s23; tensor.xz = s13;
            if (invariant == FVIZ_FEA_INVARIANT_MISES)
            {
                const double normal =
                    (s11 - s22) * (s11 - s22) +
                    (s22 - s33) * (s22 - s33) +
                    (s33 - s11) * (s33 - s11);
                destination[i] = sqrt(0.5 * normal + 3.0 * (s12 * s12 + s13 * s13 + s23 * s23));
                continue;
            }
            if (invariant == FVIZ_FEA_INVARIANT_PRESSURE)
            {
                destination[i] = -(s11 + s22 + s33) / 3.0;
                continue;
            }
            (void)fviz_symmetric_tensor3d_eigensystem(&tensor, principal, NULL);
            switch (invariant)
            {
                case FVIZ_FEA_INVARIANT_TRESCA: destination[i] = principal[0] - principal[2]; break;
                case FVIZ_FEA_INVARIANT_MAX_PRINCIPAL: destination[i] = principal[0]; break;
                case FVIZ_FEA_INVARIANT_MID_PRINCIPAL: destination[i] = principal[1]; break;
                case FVIZ_FEA_INVARIANT_MIN_PRINCIPAL: destination[i] = principal[2]; break;
                default: destination[i] = NAN; break;
            }
        }
    }
    *out_values = output;
    return FVIZ_OK;
}
