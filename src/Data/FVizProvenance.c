#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Data/FVizProvenance.h>

#include <FViz/Core/FVizErrorInternal.h>

static FVizBool fviz_provenance_integer_type(FVizDataType type)
{
    return type >= FVIZ_DATA_INT8 && type <= FVIZ_DATA_UINT64
        ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_provenance_read(
    const FVizDataArray* array, FVizSize tuple, FVizId* out_id)
{
    const void* value;
    if (array == NULL || out_id == NULL ||
        fviz_data_array_components(array) != 1u ||
        fviz_provenance_integer_type(fviz_data_array_type(array)) == FVIZ_FALSE ||
        tuple >= fviz_data_array_tuple_count(array))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    value = fviz_data_array_const_tuple(array, tuple);
    if (value == NULL) return FVIZ_ERROR_INVALID_STATE;
#define FVIZ_PROVENANCE_SIGNED(type_value, c_type) \
    case type_value: { const c_type item = *(const c_type*)value; \
        if (item < 0) return FVIZ_ERROR_INVALID_ARGUMENT; \
        *out_id = (FVizId)item; return FVIZ_OK; }
#define FVIZ_PROVENANCE_UNSIGNED(type_value, c_type) \
    case type_value: *out_id = (FVizId)*(const c_type*)value; return FVIZ_OK
    switch (fviz_data_array_type(array))
    {
        FVIZ_PROVENANCE_SIGNED(FVIZ_DATA_INT8, int8_t);
        FVIZ_PROVENANCE_UNSIGNED(FVIZ_DATA_UINT8, uint8_t);
        FVIZ_PROVENANCE_SIGNED(FVIZ_DATA_INT16, int16_t);
        FVIZ_PROVENANCE_UNSIGNED(FVIZ_DATA_UINT16, uint16_t);
        FVIZ_PROVENANCE_SIGNED(FVIZ_DATA_INT32, int32_t);
        FVIZ_PROVENANCE_UNSIGNED(FVIZ_DATA_UINT32, uint32_t);
        FVIZ_PROVENANCE_SIGNED(FVIZ_DATA_INT64, int64_t);
        FVIZ_PROVENANCE_UNSIGNED(FVIZ_DATA_UINT64, uint64_t);
        default: return FVIZ_ERROR_INVALID_ARGUMENT;
    }
#undef FVIZ_PROVENANCE_UNSIGNED
#undef FVIZ_PROVENANCE_SIGNED
}

const char* fviz_provenance_array_name(FVizProvenanceEntity entity)
{
    if (entity == FVIZ_PROVENANCE_POINT) return FVIZ_ORIGINAL_POINT_IDS_ARRAY_NAME;
    if (entity == FVIZ_PROVENANCE_CELL) return FVIZ_ORIGINAL_CELL_IDS_ARRAY_NAME;
    if (entity == FVIZ_PROVENANCE_FACE) return FVIZ_ORIGINAL_FACE_IDS_ARRAY_NAME;
    return NULL;
}

FVizResult fviz_provenance_create_identity(
    FVizSize tuple_count, FVizDataArray** out_ids)
{
    FVizDataArray* ids = NULL;
    FVizSize tuple;
    if (out_ids == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_ids = NULL;
    if (fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &ids) != FVIZ_OK ||
        fviz_data_array_resize(ids, tuple_count) != FVIZ_OK)
    {
        fviz_release(ids);
        return fviz_last_error_code();
    }
    for (tuple = 0u; tuple < tuple_count; ++tuple)
    {
        const uint64_t id = (uint64_t)tuple;
        if (fviz_data_array_set_tuple(ids, tuple, &id) != FVIZ_OK)
        {
            fviz_release(ids);
            return fviz_last_error_code();
        }
    }
    *out_ids = ids;
    return FVIZ_OK;
}

FVizResult fviz_provenance_validate(
    const FVizDataArray* ids, FVizSize expected_tuple_count)
{
    FVizSize tuple;
    FVizId id;
    if (ids == NULL || fviz_data_array_components(ids) != 1u ||
        fviz_provenance_integer_type(fviz_data_array_type(ids)) == FVIZ_FALSE ||
        fviz_data_array_tuple_count(ids) != expected_tuple_count)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    for (tuple = 0u; tuple < expected_tuple_count; ++tuple)
        if (fviz_provenance_read(ids, tuple, &id) != FVIZ_OK)
            return FVIZ_ERROR_INVALID_ARGUMENT;
    return FVIZ_OK;
}

FVizResult fviz_provenance_resolve(
    const FVizAttributeSet* attributes,
    FVizProvenanceEntity entity,
    FVizSize local_id,
    FVizId fallback,
    FVizId* out_source_id,
    FVizBool* out_persistent)
{
    const char* name = fviz_provenance_array_name(entity);
    const FVizDataArray* ids;
    if (attributes == NULL || name == NULL || out_source_id == NULL)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (out_persistent != NULL) *out_persistent = FVIZ_FALSE;
    ids = fviz_attribute_set_const_get(attributes, name);
    if (ids == NULL)
    {
        *out_source_id = fallback;
        return FVIZ_OK;
    }
    if (fviz_provenance_read(ids, local_id, out_source_id) != FVIZ_OK)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "provenance array is malformed");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (out_persistent != NULL) *out_persistent = FVIZ_TRUE;
    return FVIZ_OK;
}

FVizResult fviz_provenance_find(
    const FVizAttributeSet* attributes,
    FVizProvenanceEntity entity,
    FVizId source_id,
    FVizSize* out_local_id)
{
    const char* name = fviz_provenance_array_name(entity);
    const FVizDataArray* ids;
    FVizSize tuple;
    if (attributes == NULL || name == NULL || out_local_id == NULL ||
        source_id == FVIZ_INVALID_ID)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    ids = fviz_attribute_set_const_get(attributes, name);
    if (ids == NULL) return FVIZ_ERROR_NOT_FOUND;
    for (tuple = 0u; tuple < fviz_data_array_tuple_count(ids); ++tuple)
    {
        FVizId id;
        if (fviz_provenance_read(ids, tuple, &id) != FVIZ_OK)
            return FVIZ_ERROR_INVALID_STATE;
        if (id == source_id)
        {
            *out_local_id = tuple;
            return FVIZ_OK;
        }
    }
    return FVIZ_ERROR_NOT_FOUND;
}

FVizResult fviz_provenance_compose(
    const FVizDataArray* upstream_ids,
    const FVizDataArray* local_to_upstream,
    FVizDataArray** out_ids)
{
    FVizDataArray* output = NULL;
    FVizSize tuple;
    if (upstream_ids == NULL || local_to_upstream == NULL || out_ids == NULL ||
        fviz_data_array_components(upstream_ids) != 1u ||
        fviz_data_array_components(local_to_upstream) != 1u ||
        fviz_provenance_integer_type(fviz_data_array_type(upstream_ids)) == FVIZ_FALSE ||
        fviz_provenance_integer_type(fviz_data_array_type(local_to_upstream)) == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_ids = NULL;
    if (fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &output) != FVIZ_OK ||
        fviz_data_array_resize(output, fviz_data_array_tuple_count(local_to_upstream)) != FVIZ_OK)
    {
        fviz_release(output);
        return fviz_last_error_code();
    }
    for (tuple = 0u; tuple < fviz_data_array_tuple_count(local_to_upstream); ++tuple)
    {
        FVizId upstream_tuple;
        FVizId source_id;
        uint64_t value;
        if (fviz_provenance_read(local_to_upstream, tuple, &upstream_tuple) != FVIZ_OK ||
            upstream_tuple >= fviz_data_array_tuple_count(upstream_ids) ||
            fviz_provenance_read(upstream_ids, (FVizSize)upstream_tuple, &source_id) != FVIZ_OK)
        {
            fviz_release(output);
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "provenance composition references an invalid tuple");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        value = (uint64_t)source_id;
        if (fviz_data_array_set_tuple(output, tuple, &value) != FVIZ_OK)
        {
            fviz_release(output);
            return fviz_last_error_code();
        }
    }
    *out_ids = output;
    return FVIZ_OK;
}
