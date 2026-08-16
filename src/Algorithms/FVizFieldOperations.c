#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Algorithms/FVizFieldOperations.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizObject.h>

#include <FViz/Core/FVizErrorInternal.h>

typedef struct FVizFieldIdEntry
{
    FVizId id;
    FVizSize tuple;
} FVizFieldIdEntry;

static FVizBool fviz_field_integer_type(FVizDataType type)
{
    return type >= FVIZ_DATA_INT8 && type <= FVIZ_DATA_UINT64 && type != FVIZ_DATA_FLOAT32 && type != FVIZ_DATA_FLOAT64;
}

static FVizResult fviz_field_read_id(const FVizDataArray* array, FVizSize tuple, FVizId* out_id)
{
    const void* value = fviz_data_array_const_tuple(array, tuple);
    if (value == NULL || out_id == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
#define FVIZ_READ_SIGNED_ID(type_value, c_type)                                                                        \
    case type_value:                                                                                                   \
        {                                                                                                              \
            const c_type item = *(const c_type*)value;                                                                 \
            if (item < 0) return FVIZ_ERROR_INVALID_ARGUMENT;                                                          \
            *out_id = (FVizId)item;                                                                                    \
            return FVIZ_OK;                                                                                            \
        }
#define FVIZ_READ_UNSIGNED_ID(type_value, c_type)                                                                      \
    case type_value:                                                                                                   \
        *out_id = (FVizId) * (const c_type*)value;                                                                     \
        return FVIZ_OK
    switch (fviz_data_array_type(array))
    {
        FVIZ_READ_SIGNED_ID(FVIZ_DATA_INT8, int8_t);
        FVIZ_READ_UNSIGNED_ID(FVIZ_DATA_UINT8, uint8_t);
        FVIZ_READ_SIGNED_ID(FVIZ_DATA_INT16, int16_t);
        FVIZ_READ_UNSIGNED_ID(FVIZ_DATA_UINT16, uint16_t);
        FVIZ_READ_SIGNED_ID(FVIZ_DATA_INT32, int32_t);
        FVIZ_READ_UNSIGNED_ID(FVIZ_DATA_UINT32, uint32_t);
        FVIZ_READ_SIGNED_ID(FVIZ_DATA_INT64, int64_t);
        FVIZ_READ_UNSIGNED_ID(FVIZ_DATA_UINT64, uint64_t);
        default:
            return FVIZ_ERROR_INVALID_ARGUMENT;
    }
#undef FVIZ_READ_UNSIGNED_ID
#undef FVIZ_READ_SIGNED_ID
}

static int fviz_field_id_entry_compare(const void* left, const void* right)
{
    const FVizFieldIdEntry* a = (const FVizFieldIdEntry*)left;
    const FVizFieldIdEntry* b = (const FVizFieldIdEntry*)right;
    return a->id < b->id ? -1 : (a->id > b->id ? 1 : 0);
}

static const FVizFieldIdEntry* fviz_field_find_id(const FVizFieldIdEntry* entries, FVizSize count, FVizId id)
{
    FVizSize first = 0u;
    FVizSize last = count;
    while (first < last)
    {
        const FVizSize middle = first + (last - first) / 2u;
        if (entries[middle].id < id) first = middle + 1u;
        else
            last = middle;
    }
    return first < count && entries[first].id == id ? &entries[first] : NULL;
}

void fviz_field_gather_options_initialize(FVizFieldGatherOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->missing_id_policy = FVIZ_MISSING_ID_ERROR;
}

void fviz_indexed_average_options_initialize(FVizIndexedAverageOptions* options)
{
    if (options == NULL) return;
    (void)memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->ignore_non_finite = FVIZ_TRUE;
}

static FVizResult fviz_field_derived_scalar(const FVizDataArray* source, uint32_t component, FVizBool magnitude,
                                            FVizDataArray** out_values)
{
    FVizDataArray* output = NULL;
    double* destination;
    FVizSize tuple;
    const uint32_t components = source != NULL ? fviz_data_array_components(source) : 0u;
    if (out_values == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_values = NULL;
    if (source == NULL || components == 0u || (magnitude == FVIZ_FALSE && component >= components))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "field scalar operation arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, 1u, &output) != FVIZ_OK ||
        fviz_data_array_resize(output, fviz_data_array_tuple_count(source)) != FVIZ_OK)
        goto fail;
    destination = (double*)fviz_data_array_data(output);
    for (tuple = 0u; tuple < fviz_data_array_tuple_count(source); ++tuple)
    {
        if (magnitude != FVIZ_FALSE)
        {
            double sum = 0.0;
            uint32_t item;
            for (item = 0u; item < components; ++item)
            {
                double value;
                if (fviz_data_array_get_component(source, tuple, item, &value) != FVIZ_OK) goto fail;
                sum += value * value;
            }
            destination[tuple] = sqrt(sum);
        }
        else if (fviz_data_array_get_component(source, tuple, component, &destination[tuple]) != FVIZ_OK)
            goto fail;
    }
    fviz_object_modified((FVizObject*)output);
    *out_values = output;
    return FVIZ_OK;
fail:
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_field_extract_component(const FVizDataArray* source, uint32_t component, FVizDataArray** out_values)
{
    return fviz_field_derived_scalar(source, component, FVIZ_FALSE, out_values);
}

FVizResult fviz_field_compute_magnitude(const FVizDataArray* source, FVizDataArray** out_values)
{
    return fviz_field_derived_scalar(source, 0u, FVIZ_TRUE, out_values);
}

FVizResult fviz_field_compute_finite_mask(const FVizDataArray* source, FVizDataArray** out_mask)
{
    FVizDataArray* mask = NULL;
    uint8_t* values;
    FVizSize tuple;
    if (out_mask == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_mask = NULL;
    if (source == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "finite mask source must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &mask) != FVIZ_OK ||
        fviz_data_array_resize(mask, fviz_data_array_tuple_count(source)) != FVIZ_OK)
        goto fail;
    values = (uint8_t*)fviz_data_array_data(mask);
    for (tuple = 0u; tuple < fviz_data_array_tuple_count(source); ++tuple)
    {
        uint32_t component;
        values[tuple] = 1u;
        for (component = 0u; component < fviz_data_array_components(source); ++component)
        {
            double value;
            if (fviz_data_array_get_component(source, tuple, component, &value) != FVIZ_OK) goto fail;
            if (!isfinite(value))
            {
                values[tuple] = 0u;
                break;
            }
        }
    }
    fviz_object_modified((FVizObject*)mask);
    *out_mask = mask;
    return FVIZ_OK;
fail:
    fviz_release(mask);
    return fviz_last_error_code();
}

FVizResult fviz_field_gather_by_ids(const FVizDataArray* source_values, const FVizDataArray* source_ids,
                                    const FVizDataArray* target_ids, const FVizFieldGatherOptions* options,
                                    FVizDataArray** out_values, FVizDataArray** out_valid_mask)
{
    FVizFieldGatherOptions defaults;
    FVizFieldIdEntry* entries = NULL;
    FVizDataArray* output = NULL;
    FVizDataArray* mask = NULL;
    unsigned char* output_data;
    uint8_t* mask_data;
    FVizSize bytes;
    FVizSize tuple;
    FVizSize stride;
    if (out_values == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_values = NULL;
    if (out_valid_mask != NULL) *out_valid_mask = NULL;
    fviz_field_gather_options_initialize(&defaults);
    if (options == NULL) options = &defaults;
    if (source_values == NULL || source_ids == NULL || target_ids == NULL ||
        fviz_data_array_components(source_ids) != 1u || fviz_data_array_components(target_ids) != 1u ||
        fviz_field_integer_type(fviz_data_array_type(source_ids)) == FVIZ_FALSE ||
        fviz_field_integer_type(fviz_data_array_type(target_ids)) == FVIZ_FALSE ||
        fviz_data_array_tuple_count(source_values) != fviz_data_array_tuple_count(source_ids) ||
        (options->missing_id_policy != FVIZ_MISSING_ID_ERROR && options->missing_id_policy != FVIZ_MISSING_ID_FILL))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "field gather arguments are incompatible");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_size_multiply(fviz_data_array_tuple_count(source_ids), sizeof(*entries), &bytes) != FVIZ_OK)
        return fviz_last_error_code();
    entries = (FVizFieldIdEntry*)fviz_alloc(bytes);
    if (entries == NULL && bytes != 0u) return fviz_last_error_code();
    for (tuple = 0u; tuple < fviz_data_array_tuple_count(source_ids); ++tuple)
    {
        if (fviz_field_read_id(source_ids, tuple, &entries[tuple].id) != FVIZ_OK) goto invalid;
        entries[tuple].tuple = tuple;
    }
    qsort(entries, (size_t)fviz_data_array_tuple_count(source_ids), sizeof(*entries), fviz_field_id_entry_compare);
    for (tuple = 1u; tuple < fviz_data_array_tuple_count(source_ids); ++tuple)
        if (entries[tuple - 1u].id == entries[tuple].id)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "field gather source IDs must be unique");
            goto fail;
        }
    if (fviz_data_array_create(fviz_data_array_type(source_values), fviz_data_array_components(source_values),
                               &output) != FVIZ_OK ||
        fviz_data_array_resize(output, fviz_data_array_tuple_count(target_ids)) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &mask) != FVIZ_OK ||
        fviz_data_array_resize(mask, fviz_data_array_tuple_count(target_ids)) != FVIZ_OK)
        goto fail;
    output_data = (unsigned char*)fviz_data_array_data(output);
    mask_data = (uint8_t*)fviz_data_array_data(mask);
    stride = fviz_data_array_tuple_stride(source_values);
    for (tuple = 0u; tuple < fviz_data_array_tuple_count(target_ids); ++tuple)
    {
        FVizId id;
        const FVizFieldIdEntry* entry;
        if (fviz_field_read_id(target_ids, tuple, &id) != FVIZ_OK) goto invalid;
        entry = fviz_field_find_id(entries, fviz_data_array_tuple_count(source_ids), id);
        if (entry != NULL)
        {
            (void)memcpy(output_data + tuple * stride, fviz_data_array_const_tuple(source_values, entry->tuple),
                         (size_t)stride);
            mask_data[tuple] = 1u;
        }
        else
        {
            if (options->missing_id_policy == FVIZ_MISSING_ID_ERROR)
            {
                fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "field gather target ID was not found");
                goto fail;
            }
            if (options->fill_tuple != NULL)
                (void)memcpy(output_data + tuple * stride, options->fill_tuple, (size_t)stride);
            else
                (void)memset(output_data + tuple * stride, 0, (size_t)stride);
            mask_data[tuple] = 0u;
        }
    }
    fviz_object_modified((FVizObject*)output);
    fviz_object_modified((FVizObject*)mask);
    fviz_free(entries);
    *out_values = output;
    if (out_valid_mask != NULL) *out_valid_mask = mask;
    else
        fviz_release(mask);
    return FVIZ_OK;
invalid:
    fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "field gather IDs must be non-negative integers");
fail:
    fviz_free(entries);
    fviz_release(mask);
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_field_indexed_weighted_average(const FVizDataArray* source_values,
                                               const FVizDataArray* destination_indices, const FVizDataArray* weights,
                                               const FVizIndexedAverageOptions* options, FVizDataArray** out_values,
                                               FVizDataArray** out_valid_mask)
{
    FVizIndexedAverageOptions defaults;
    FVizDataArray* output = NULL;
    FVizDataArray* mask = NULL;
    double* sums = NULL;
    double* weight_sums = NULL;
    double* destination;
    uint8_t* valid;
    FVizSize count;
    FVizSize scalar_count;
    FVizSize bytes;
    FVizSize tuple;
    const uint32_t components = source_values != NULL ? fviz_data_array_components(source_values) : 0u;
    if (out_values == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_values = NULL;
    if (out_valid_mask != NULL) *out_valid_mask = NULL;
    fviz_indexed_average_options_initialize(&defaults);
    if (options == NULL) options = &defaults;
    count = source_values != NULL ? fviz_data_array_tuple_count(source_values) : 0u;
    if (source_values == NULL || destination_indices == NULL || components == 0u ||
        options->destination_tuple_count == 0u || fviz_data_array_components(destination_indices) != 1u ||
        fviz_field_integer_type(fviz_data_array_type(destination_indices)) == FVIZ_FALSE ||
        fviz_data_array_tuple_count(destination_indices) != count ||
        (weights != NULL &&
         (fviz_data_array_components(weights) != 1u || fviz_data_array_tuple_count(weights) != count)))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "indexed average arguments are incompatible");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_size_multiply(options->destination_tuple_count, components, &scalar_count) != FVIZ_OK ||
        fviz_size_multiply(scalar_count, sizeof(double), &bytes) != FVIZ_OK)
        return fviz_last_error_code();
    sums = (double*)fviz_alloc(bytes);
    if (sums == NULL) goto fail;
    (void)memset(sums, 0, (size_t)bytes);
    if (fviz_size_multiply(options->destination_tuple_count, sizeof(double), &bytes) != FVIZ_OK) goto fail;
    weight_sums = (double*)fviz_alloc(bytes);
    if (weight_sums == NULL) goto fail;
    (void)memset(weight_sums, 0, (size_t)bytes);
    for (tuple = 0u; tuple < count; ++tuple)
    {
        FVizId destination_id;
        double weight = 1.0;
        uint32_t component;
        FVizBool finite = FVIZ_TRUE;
        if (fviz_field_read_id(destination_indices, tuple, &destination_id) != FVIZ_OK ||
            destination_id >= options->destination_tuple_count)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "indexed average destination is out of range");
            goto fail;
        }
        if (weights != NULL && fviz_data_array_get_component(weights, tuple, 0u, &weight) != FVIZ_OK) goto fail;
        if (!isfinite(weight)) finite = FVIZ_FALSE;
        for (component = 0u; component < components; ++component)
        {
            double value;
            if (fviz_data_array_get_component(source_values, tuple, component, &value) != FVIZ_OK) goto fail;
            if (!isfinite(value)) finite = FVIZ_FALSE;
        }
        if (finite == FVIZ_FALSE)
        {
            if (options->ignore_non_finite != FVIZ_FALSE) continue;
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "indexed average contains non-finite data");
            goto fail;
        }
        for (component = 0u; component < components; ++component)
        {
            double value;
            (void)fviz_data_array_get_component(source_values, tuple, component, &value);
            sums[(FVizSize)destination_id * components + component] += value * weight;
        }
        weight_sums[destination_id] += weight;
    }
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, components, &output) != FVIZ_OK ||
        fviz_data_array_resize(output, options->destination_tuple_count) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &mask) != FVIZ_OK ||
        fviz_data_array_resize(mask, options->destination_tuple_count) != FVIZ_OK)
        goto fail;
    destination = (double*)fviz_data_array_data(output);
    valid = (uint8_t*)fviz_data_array_data(mask);
    for (tuple = 0u; tuple < options->destination_tuple_count; ++tuple)
    {
        uint32_t component;
        valid[tuple] = weight_sums[tuple] != 0.0 && isfinite(weight_sums[tuple]) ? 1u : 0u;
        for (component = 0u; component < components; ++component)
            destination[tuple * components + component] =
                valid[tuple] != 0u ? sums[tuple * components + component] / weight_sums[tuple] : NAN;
    }
    fviz_object_modified((FVizObject*)output);
    fviz_object_modified((FVizObject*)mask);
    fviz_free(weight_sums);
    fviz_free(sums);
    *out_values = output;
    if (out_valid_mask != NULL) *out_valid_mask = mask;
    else
        fviz_release(mask);
    return FVIZ_OK;
fail:
    fviz_release(mask);
    fviz_release(output);
    fviz_free(weight_sums);
    fviz_free(sums);
    return fviz_last_error_code();
}

FVizResult fviz_field_apply_tuple_matrix(const FVizDataArray* source_values, const double* matrix,
                                         FVizSize output_tuple_count, FVizDataArray** out_values)
{
    FVizDataArray* output = NULL;
    double* destination;
    FVizSize row;
    const FVizSize input_count = source_values != NULL ? fviz_data_array_tuple_count(source_values) : 0u;
    const uint32_t components = source_values != NULL ? fviz_data_array_components(source_values) : 0u;
    if (out_values == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_values = NULL;
    if (source_values == NULL || matrix == NULL || input_count == 0u || components == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_data_array_create(FVIZ_DATA_FLOAT64, components, &output) != FVIZ_OK ||
        fviz_data_array_resize(output, output_tuple_count) != FVIZ_OK)
        goto fail;
    destination = (double*)fviz_data_array_data(output);
    for (row = 0u; row < output_tuple_count; ++row)
    {
        uint32_t component;
        for (component = 0u; component < components; ++component)
        {
            FVizSize column;
            double sum = 0.0;
            for (column = 0u; column < input_count; ++column)
            {
                double value;
                if (fviz_data_array_get_component(source_values, column, component, &value) != FVIZ_OK) goto fail;
                sum += matrix[row * input_count + column] * value;
            }
            destination[row * components + component] = sum;
        }
    }
    fviz_object_modified((FVizObject*)output);
    *out_values = output;
    return FVIZ_OK;
fail:
    fviz_release(output);
    return fviz_last_error_code();
}

FVizResult fviz_field_least_squares_operator(const double* basis_matrix, FVizSize sample_count,
                                             FVizSize coefficient_count, double* out_operator)
{
    double* augmented = NULL;
    FVizSize width;
    FVizSize scalar_count;
    FVizSize bytes;
    FVizSize row;
    if (basis_matrix == NULL || out_operator == NULL || sample_count == 0u || coefficient_count == 0u ||
        sample_count < coefficient_count)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_size_add(coefficient_count, coefficient_count, &width) != FVIZ_OK ||
        fviz_size_multiply(coefficient_count, width, &scalar_count) != FVIZ_OK ||
        fviz_size_multiply(scalar_count, sizeof(double), &bytes) != FVIZ_OK)
        return fviz_last_error_code();
    augmented = (double*)fviz_alloc(bytes);
    if (augmented == NULL) return fviz_last_error_code();
    (void)memset(augmented, 0, (size_t)bytes);
    for (row = 0u; row < coefficient_count; ++row)
    {
        FVizSize column;
        for (column = 0u; column < coefficient_count; ++column)
        {
            FVizSize sample;
            for (sample = 0u; sample < sample_count; ++sample)
                augmented[row * width + column] +=
                    basis_matrix[sample * coefficient_count + row] * basis_matrix[sample * coefficient_count + column];
        }
        augmented[row * width + coefficient_count + row] = 1.0;
    }
    for (row = 0u; row < coefficient_count; ++row)
    {
        FVizSize pivot = row;
        FVizSize candidate;
        double pivot_abs = fabs(augmented[row * width + row]);
        for (candidate = row + 1u; candidate < coefficient_count; ++candidate)
        {
            const double value = fabs(augmented[candidate * width + row]);
            if (value > pivot_abs)
            {
                pivot_abs = value;
                pivot = candidate;
            }
        }
        if (pivot_abs <= 1.0e-14)
        {
            fviz_free(augmented);
            return FVIZ_ERROR_INVALID_STATE;
        }
        if (pivot != row)
        {
            FVizSize column;
            for (column = 0u; column < width; ++column)
            {
                const double temporary = augmented[row * width + column];
                augmented[row * width + column] = augmented[pivot * width + column];
                augmented[pivot * width + column] = temporary;
            }
        }
        {
            const double divisor = augmented[row * width + row];
            FVizSize column;
            for (column = 0u; column < width; ++column)
                augmented[row * width + column] /= divisor;
        }
        for (candidate = 0u; candidate < coefficient_count; ++candidate)
        {
            FVizSize column;
            const double factor = augmented[candidate * width + row];
            if (candidate == row) continue;
            for (column = 0u; column < width; ++column)
                augmented[candidate * width + column] -= factor * augmented[row * width + column];
        }
    }
    for (row = 0u; row < coefficient_count; ++row)
    {
        FVizSize sample;
        for (sample = 0u; sample < sample_count; ++sample)
        {
            FVizSize column;
            double value = 0.0;
            for (column = 0u; column < coefficient_count; ++column)
                value += augmented[row * width + coefficient_count + column] *
                         basis_matrix[sample * coefficient_count + column];
            out_operator[row * sample_count + sample] = value;
        }
    }
    fviz_free(augmented);
    return FVIZ_OK;
}

FVizResult fviz_field_compute_indexed_discontinuity_mask(const FVizDataArray* source_values,
                                                         const FVizDataArray* destination_indices,
                                                         FVizSize destination_tuple_count, double relative_threshold,
                                                         FVizDataArray** out_mask)
{
    FVizDataArray* mask = NULL;
    double* minima = NULL;
    double* maxima = NULL;
    uint8_t* flags;
    FVizSize scalar_count;
    FVizSize bytes;
    FVizSize tuple;
    const uint32_t components = source_values != NULL ? fviz_data_array_components(source_values) : 0u;
    if (out_mask == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_mask = NULL;
    if (source_values == NULL || destination_indices == NULL || components == 0u || relative_threshold < 0.0 ||
        fviz_data_array_tuple_count(source_values) != fviz_data_array_tuple_count(destination_indices) ||
        fviz_data_array_components(destination_indices) != 1u ||
        fviz_field_integer_type(fviz_data_array_type(destination_indices)) == FVIZ_FALSE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_size_multiply(destination_tuple_count, components, &scalar_count) != FVIZ_OK ||
        fviz_size_multiply(scalar_count, sizeof(double), &bytes) != FVIZ_OK)
        return fviz_last_error_code();
    minima = (double*)fviz_alloc(bytes);
    maxima = (double*)fviz_alloc(bytes);
    if ((minima == NULL || maxima == NULL) && bytes != 0u) goto fail;
    for (tuple = 0u; tuple < scalar_count; ++tuple)
    {
        minima[tuple] = INFINITY;
        maxima[tuple] = -INFINITY;
    }
    for (tuple = 0u; tuple < fviz_data_array_tuple_count(source_values); ++tuple)
    {
        FVizId destination;
        uint32_t component;
        if (fviz_field_read_id(destination_indices, tuple, &destination) != FVIZ_OK ||
            destination >= destination_tuple_count)
            goto invalid;
        for (component = 0u; component < components; ++component)
        {
            double value;
            const FVizSize offset = (FVizSize)destination * components + component;
            if (fviz_data_array_get_component(source_values, tuple, component, &value) != FVIZ_OK) goto fail;
            if (!isfinite(value)) continue;
            if (value < minima[offset]) minima[offset] = value;
            if (value > maxima[offset]) maxima[offset] = value;
        }
    }
    if (fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &mask) != FVIZ_OK ||
        fviz_data_array_resize(mask, destination_tuple_count) != FVIZ_OK)
        goto fail;
    flags = (uint8_t*)fviz_data_array_data(mask);
    for (tuple = 0u; tuple < destination_tuple_count; ++tuple)
    {
        uint32_t component;
        flags[tuple] = 0u;
        for (component = 0u; component < components; ++component)
        {
            const FVizSize offset = tuple * components + component;
            const double reference = fmax(fmax(fabs(minima[offset]), fabs(maxima[offset])), 1.0e-30);
            if (isfinite(minima[offset]) && (maxima[offset] - minima[offset]) / reference > relative_threshold)
            {
                flags[tuple] = 1u;
                break;
            }
        }
    }
    fviz_object_modified((FVizObject*)mask);
    fviz_free(maxima);
    fviz_free(minima);
    *out_mask = mask;
    return FVIZ_OK;
invalid:
    fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "discontinuity destination index is out of range");
fail:
    fviz_release(mask);
    fviz_free(maxima);
    fviz_free(minima);
    return fviz_last_error_code();
}
