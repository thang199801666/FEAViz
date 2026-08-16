#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Data/FVizTemporalDataSet.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/Data/FVizTemporalDataSetPrivate.h>

static void fviz_temporal_data_set_destroy(FVizObject* object);
static FVizMTime fviz_temporal_data_set_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_temporal_data_set_class = {
    FVIZ_TYPE_TEMPORAL_DATA_SET,
    "FVizTemporalDataSet",
    &g_fviz_data_object_class,
    fviz_temporal_data_set_destroy,
    fviz_temporal_data_set_mtime
};

static FVizMTime fviz_temporal_data_set_mtime(const FVizObject* object)
{
    /* Child ModifiedEvents are bridged into the container, so aggregate MTime is
     * O(1) even for thousands of retained FEA frames. */
    return fviz_internal_object_local_mtime(object);
}

static FVizBool fviz_temporal_child_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    FVizTemporalDataSet* data_set = (FVizTemporalDataSet*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (data_set != NULL) fviz_object_modified((FVizObject*)data_set);
    return FVIZ_FALSE;
}

static FVizResult fviz_temporal_entry_observe(
    FVizTemporalDataSet* data_set, FVizTemporalEntry* entry)
{
    if (data_set == NULL || entry == NULL || entry->data == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    entry->data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    return fviz_object_add_observer(
        (FVizObject*)entry->data, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_temporal_child_modified, data_set, &entry->data_modified_tag);
}

static void fviz_temporal_entry_release(FVizTemporalEntry* entry)
{
    if (entry == NULL) return;
    if (entry->data != NULL && entry->data_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)entry->data, entry->data_modified_tag);
    fviz_release(entry->data);
    entry->data = NULL;
    entry->data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
}

static void fviz_temporal_data_set_destroy(FVizObject* object)
{
    FVizTemporalDataSet* data_set = (FVizTemporalDataSet*)object;
    fviz_temporal_data_set_clear(data_set);
    fviz_release(data_set->steps);
    data_set->steps = NULL;
}

FVizResult fviz_temporal_data_set_create(FVizTemporalDataSet** out_data_set)
{
    FVizTemporalDataSet* data_set;
    if (out_data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_data_set must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_data_set = NULL;
    data_set = (FVizTemporalDataSet*)fviz_internal_object_allocate(
        sizeof(*data_set), &g_fviz_temporal_data_set_class, NULL);
    if (data_set == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizTemporalEntry), &data_set->steps) != FVIZ_OK)
    {
        fviz_release(data_set);
        return fviz_last_error_code();
    }
    *out_data_set = data_set;
    return FVIZ_OK;
}

FVizSize fviz_temporal_data_set_step_count(const FVizTemporalDataSet* data_set)
{
    return data_set != NULL && data_set->steps != NULL ? fviz_array_count(data_set->steps) : 0u;
}

FVizResult fviz_temporal_data_set_reserve(FVizTemporalDataSet* data_set, FVizSize capacity)
{
    if (data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "temporal dataset must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_array_reserve(data_set->steps, capacity);
}

static FVizSize fviz_temporal_lower_bound(const FVizTemporalDataSet* data_set, double time)
{
    FVizSize first = 0u;
    FVizSize count = fviz_temporal_data_set_step_count(data_set);
    while (count > 0u)
    {
        const FVizSize step = count / 2u;
        const FVizSize index = first + step;
        const FVizTemporalEntry* entry =
            (const FVizTemporalEntry*)fviz_array_const_at(data_set->steps, index);
        if (entry->time < time)
        {
            first = index + 1u;
            count -= step + 1u;
        }
        else
            count = step;
    }
    return first;
}

FVizResult fviz_temporal_data_set_add_step(
    FVizTemporalDataSet* data_set,
    double time,
    FVizDataObject* data,
    FVizSize* out_index)
{
    FVizSize index;
    FVizSize count;
    FVizTemporalEntry* entries;
    FVizDataObject* retained;
    if (out_index != NULL) *out_index = 0u;
    if (data_set == NULL || data == NULL || !isfinite(time) ||
        fviz_data_object_is_data_object(data) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "temporal step requires finite time and data object");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    index = fviz_temporal_lower_bound(data_set, time);
    count = fviz_temporal_data_set_step_count(data_set);
    if (index < count)
    {
        const FVizTemporalEntry* existing =
            (const FVizTemporalEntry*)fviz_array_const_at(data_set->steps, index);
        if (existing->time == time)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "temporal dataset already contains this time step");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    retained = (FVizDataObject*)fviz_retain(data);
    if (retained == NULL) return fviz_last_error_code();
    if (fviz_array_resize(data_set->steps, count + 1u) != FVIZ_OK)
    {
        fviz_release(retained);
        return fviz_last_error_code();
    }
    entries = (FVizTemporalEntry*)fviz_array_data(data_set->steps);
    if (index < count)
        (void)memmove(&entries[index + 1u], &entries[index],
            (size_t)(count - index) * sizeof(*entries));
    (void)memset(&entries[index], 0, sizeof(entries[index]));
    entries[index].time = time;
    entries[index].data = retained;
    entries[index].data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (fviz_temporal_entry_observe(data_set, &entries[index]) != FVIZ_OK)
    {
        fviz_release(retained);
        if (index < count)
            (void)memmove(&entries[index], &entries[index + 1u],
                (size_t)(count - index) * sizeof(*entries));
        (void)fviz_array_resize(data_set->steps, count);
        return fviz_last_error_code();
    }
    if (out_index != NULL) *out_index = index;
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

FVizResult fviz_temporal_data_set_set_step(
    FVizTemporalDataSet* data_set,
    FVizSize index,
    double time,
    FVizDataObject* data)
{
    FVizTemporalEntry* entry;
    FVizDataObject* retained;
    FVizSize count;
    if (data_set == NULL || data == NULL || !isfinite(time) ||
        index >= fviz_temporal_data_set_step_count(data_set) ||
        fviz_data_object_is_data_object(data) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "temporal step index, time or data is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_temporal_data_set_step_count(data_set);
    if ((index > 0u && time <= fviz_temporal_data_set_time(data_set, index - 1u)) ||
        (index + 1u < count && time >= fviz_temporal_data_set_time(data_set, index + 1u)))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "temporal step times must remain strictly increasing");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    entry = (FVizTemporalEntry*)fviz_array_at(data_set->steps, index);
    if (entry->data == data && entry->time == time) return FVIZ_OK;
    retained = (FVizDataObject*)fviz_retain(data);
    if (retained == NULL) return fviz_last_error_code();
    {
        FVizTemporalEntry replacement;
        (void)memset(&replacement, 0, sizeof(replacement));
        replacement.time = time;
        replacement.data = retained;
        replacement.data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
        if (fviz_temporal_entry_observe(data_set, &replacement) != FVIZ_OK)
        {
            fviz_release(retained);
            return fviz_last_error_code();
        }
        fviz_temporal_entry_release(entry);
        *entry = replacement;
    }
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

FVizResult fviz_temporal_data_set_append_steps(
    FVizTemporalDataSet* data_set,
    const double* times,
    FVizDataObject* const* data,
    FVizSize count,
    FVizSize* out_first_index)
{
    FVizSize old_count;
    FVizSize i;
    FVizTemporalEntry* entries;
    if (out_first_index != NULL) *out_first_index = 0u;
    if (data_set == NULL || (count != 0u && (times == NULL || data == NULL)))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "temporal batch append arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    old_count = fviz_temporal_data_set_step_count(data_set);
    if (out_first_index != NULL) *out_first_index = old_count;
    if (count == 0u) return FVIZ_OK;
    for (i = 0u; i < count; ++i)
    {
        if (!isfinite(times[i]) || data[i] == NULL ||
            fviz_data_object_is_data_object(data[i]) == FVIZ_FALSE ||
            (i > 0u && times[i] <= times[i - 1u]) ||
            (i == 0u && old_count > 0u &&
             times[i] <= fviz_temporal_data_set_time(data_set, old_count - 1u)))
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                "temporal batch times must be finite, strictly increasing, and newer than existing steps");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    if (count > (FVizSize)-1 - old_count) return FVIZ_ERROR_OVERFLOW;
    if (fviz_array_reserve(data_set->steps, old_count + count) != FVIZ_OK ||
        fviz_array_resize(data_set->steps, old_count + count) != FVIZ_OK)
        return fviz_last_error_code();
    entries = (FVizTemporalEntry*)fviz_array_data(data_set->steps);
    for (i = 0u; i < count; ++i)
    {
        FVizTemporalEntry* entry = &entries[old_count + i];
        (void)memset(entry, 0, sizeof(*entry));
        entry->time = times[i];
        entry->data = (FVizDataObject*)fviz_retain(data[i]);
        entry->data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
        if (entry->data == NULL || fviz_temporal_entry_observe(data_set, entry) != FVIZ_OK)
        {
            FVizSize rollback;
            if (entry->data != NULL) fviz_temporal_entry_release(entry);
            for (rollback = 0u; rollback < i; ++rollback)
                fviz_temporal_entry_release(&entries[old_count + rollback]);
            (void)fviz_array_resize(data_set->steps, old_count);
            return fviz_last_error_code();
        }
    }
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

double fviz_temporal_data_set_time(const FVizTemporalDataSet* data_set, FVizSize index)
{
    const FVizTemporalEntry* entry;
    if (data_set == NULL || index >= fviz_temporal_data_set_step_count(data_set)) return 0.0;
    entry = (const FVizTemporalEntry*)fviz_array_const_at(data_set->steps, index);
    return entry != NULL ? entry->time : 0.0;
}

FVizDataObject* fviz_temporal_data_set_data(FVizTemporalDataSet* data_set, FVizSize index)
{
    FVizTemporalEntry* entry;
    if (data_set == NULL || index >= fviz_temporal_data_set_step_count(data_set)) return NULL;
    entry = (FVizTemporalEntry*)fviz_array_at(data_set->steps, index);
    return entry != NULL ? entry->data : NULL;
}

const FVizDataObject* fviz_temporal_data_set_const_data(const FVizTemporalDataSet* data_set, FVizSize index)
{
    const FVizTemporalEntry* entry;
    if (data_set == NULL || index >= fviz_temporal_data_set_step_count(data_set)) return NULL;
    entry = (const FVizTemporalEntry*)fviz_array_const_at(data_set->steps, index);
    return entry != NULL ? entry->data : NULL;
}

FVizResult fviz_temporal_data_set_time_range(
    const FVizTemporalDataSet* data_set,
    double* out_minimum,
    double* out_maximum)
{
    const FVizSize count = fviz_temporal_data_set_step_count(data_set);
    if (out_minimum != NULL) *out_minimum = 0.0;
    if (out_maximum != NULL) *out_maximum = 0.0;
    if (data_set == NULL || out_minimum == NULL || out_maximum == NULL || count == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "temporal time range requires a non-empty dataset");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_minimum = fviz_temporal_data_set_time(data_set, 0u);
    *out_maximum = fviz_temporal_data_set_time(data_set, count - 1u);
    return FVIZ_OK;
}

FVizResult fviz_temporal_data_set_find_nearest(
    const FVizTemporalDataSet* data_set,
    double time,
    FVizSize* out_index)
{
    FVizSize index;
    FVizSize count;
    if (out_index != NULL) *out_index = 0u;
    if (data_set == NULL || out_index == NULL || !isfinite(time) ||
        (count = fviz_temporal_data_set_step_count(data_set)) == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "nearest-time query requires finite time and non-empty dataset");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    index = fviz_temporal_lower_bound(data_set, time);
    if (index == 0u) *out_index = 0u;
    else if (index >= count) *out_index = count - 1u;
    else
    {
        const double before = fviz_temporal_data_set_time(data_set, index - 1u);
        const double after = fviz_temporal_data_set_time(data_set, index);
        *out_index = fabs(time - before) <= fabs(after - time) ? index - 1u : index;
    }
    return FVIZ_OK;
}

FVizResult fviz_temporal_data_set_find_bracket(
    const FVizTemporalDataSet* data_set,
    double time,
    FVizSize* out_lower_index,
    FVizSize* out_upper_index,
    double* out_alpha)
{
    FVizSize count;
    FVizSize upper;
    if (out_lower_index != NULL) *out_lower_index = 0u;
    if (out_upper_index != NULL) *out_upper_index = 0u;
    if (out_alpha != NULL) *out_alpha = 0.0;
    if (data_set == NULL || out_lower_index == NULL || out_upper_index == NULL ||
        out_alpha == NULL || !isfinite(time) ||
        (count = fviz_temporal_data_set_step_count(data_set)) == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "temporal bracket query requires finite time and a non-empty dataset");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    upper = fviz_temporal_lower_bound(data_set, time);
    if (upper == 0u)
    {
        *out_lower_index = *out_upper_index = 0u;
        return FVIZ_OK;
    }
    if (upper >= count)
    {
        *out_lower_index = *out_upper_index = count - 1u;
        return FVIZ_OK;
    }
    if (fviz_temporal_data_set_time(data_set, upper) == time)
    {
        *out_lower_index = *out_upper_index = upper;
        return FVIZ_OK;
    }
    *out_lower_index = upper - 1u;
    *out_upper_index = upper;
    {
        const double lower_time = fviz_temporal_data_set_time(data_set, upper - 1u);
        const double upper_time = fviz_temporal_data_set_time(data_set, upper);
        *out_alpha = (time - lower_time) / (upper_time - lower_time);
        if (*out_alpha < 0.0) *out_alpha = 0.0;
        else if (*out_alpha > 1.0) *out_alpha = 1.0;
    }
    return FVIZ_OK;
}

FVizResult fviz_temporal_data_set_remove_step(FVizTemporalDataSet* data_set, FVizSize index)
{
    FVizSize count;
    FVizTemporalEntry* entries;
    if (data_set == NULL || index >= fviz_temporal_data_set_step_count(data_set))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "temporal step index is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_temporal_data_set_step_count(data_set);
    entries = (FVizTemporalEntry*)fviz_array_data(data_set->steps);
    fviz_temporal_entry_release(&entries[index]);
    if (index + 1u < count)
        (void)memmove(&entries[index], &entries[index + 1u],
            (size_t)(count - index - 1u) * sizeof(*entries));
    if (fviz_array_resize(data_set->steps, count - 1u) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

void fviz_temporal_data_set_clear(FVizTemporalDataSet* data_set)
{
    FVizSize i;
    FVizSize count;
    if (data_set == NULL || data_set->steps == NULL) return;
    count = fviz_array_count(data_set->steps);
    for (i = 0u; i < count; ++i)
    {
        FVizTemporalEntry* entry = (FVizTemporalEntry*)fviz_array_at(data_set->steps, i);
        fviz_temporal_entry_release(entry);
    }
    if (count > 0u)
    {
        fviz_array_clear(data_set->steps);
        fviz_object_modified((FVizObject*)data_set);
    }
}

FVizResult fviz_temporal_data_set_validate(const FVizTemporalDataSet* data_set)
{
    FVizSize i;
    if (data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "temporal dataset must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < fviz_temporal_data_set_step_count(data_set); ++i)
    {
        const FVizTemporalEntry* entry =
            (const FVizTemporalEntry*)fviz_array_const_at(data_set->steps, i);
        if (entry == NULL || !isfinite(entry->time) || entry->data == NULL ||
            fviz_data_object_is_data_object(entry->data) == FVIZ_FALSE ||
            (i > 0u && entry->time <= fviz_temporal_data_set_time(data_set, i - 1u)))
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "temporal dataset is not strictly ordered or contains invalid data");
            return FVIZ_ERROR_INVALID_STATE;
        }
    }
    return FVIZ_OK;
}
