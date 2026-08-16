#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizPartitionedDataSet.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/Data/FVizPartitionedDataSetPrivate.h>

static void fviz_partitioned_data_set_destroy(FVizObject* object);
static FVizMTime fviz_partitioned_data_set_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_partitioned_data_set_class = {
    FVIZ_TYPE_PARTITIONED_DATA_SET,
    "FVizPartitionedDataSet",
    &g_fviz_data_object_class,
    fviz_partitioned_data_set_destroy,
    fviz_partitioned_data_set_mtime
};

static FVizBool fviz_partition_child_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    FVizPartitionedDataSet* data_set = (FVizPartitionedDataSet*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (data_set != NULL) fviz_object_modified((FVizObject*)data_set);
    return FVIZ_FALSE;
}

static FVizResult fviz_partition_entry_observe(
    FVizPartitionedDataSet* data_set, FVizPartitionEntry* entry)
{
    if (entry == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    entry->data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (entry->data == NULL) return FVIZ_OK;
    return fviz_object_add_observer(
        (FVizObject*)entry->data, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_partition_child_modified, data_set, &entry->data_modified_tag);
}

static void fviz_partition_entry_release(FVizPartitionEntry* entry)
{
    if (entry == NULL) return;
    if (entry->data != NULL && entry->data_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)entry->data, entry->data_modified_tag);
    fviz_release(entry->data);
    fviz_release(entry->name);
    entry->data = NULL;
    entry->name = NULL;
    entry->data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
}

static FVizMTime fviz_partitioned_data_set_mtime(const FVizObject* object)
{
    /* Retained partition ModifiedEvents are bridged into this container, avoiding
     * an O(partition-count) MTime scan in render/pipeline cache checks. */
    return fviz_internal_object_local_mtime(object);
}

static void fviz_partitioned_data_set_destroy(FVizObject* object)
{
    FVizPartitionedDataSet* data_set = (FVizPartitionedDataSet*)object;
    fviz_partitioned_data_set_clear(data_set);
    fviz_release(data_set->partitions);
    data_set->partitions = NULL;
}

FVizResult fviz_partitioned_data_set_create(FVizPartitionedDataSet** out_data_set)
{
    FVizPartitionedDataSet* data_set;
    if (out_data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_data_set must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_data_set = NULL;
    data_set = (FVizPartitionedDataSet*)fviz_internal_object_allocate(
        sizeof(*data_set), &g_fviz_partitioned_data_set_class, NULL);
    if (data_set == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizPartitionEntry), &data_set->partitions) != FVIZ_OK)
    {
        fviz_release(data_set);
        return fviz_last_error_code();
    }
    *out_data_set = data_set;
    return FVIZ_OK;
}

FVizSize fviz_partitioned_data_set_count(const FVizPartitionedDataSet* data_set)
{
    return data_set != NULL && data_set->partitions != NULL
        ? fviz_array_count(data_set->partitions) : 0u;
}

FVizResult fviz_partitioned_data_set_reserve(
    FVizPartitionedDataSet* data_set, FVizSize capacity)
{
    if (data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "partitioned dataset must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_array_reserve(data_set->partitions, capacity);
}

FVizResult fviz_partitioned_data_set_resize(FVizPartitionedDataSet* data_set, FVizSize count)
{
    FVizSize old_count;
    FVizSize i;
    if (data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "partitioned dataset must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    old_count = fviz_array_count(data_set->partitions);
    if (count == old_count) return FVIZ_OK;
    if (count < old_count)
    {
        for (i = count; i < old_count; ++i)
            fviz_partition_entry_release((FVizPartitionEntry*)fviz_array_at(data_set->partitions, i));
        if (fviz_array_resize(data_set->partitions, count) != FVIZ_OK) return fviz_last_error_code();
    }
    else
    {
        if (fviz_array_resize(data_set->partitions, count) != FVIZ_OK) return fviz_last_error_code();
        for (i = old_count; i < count; ++i)
        {
            FVizPartitionEntry* entry = (FVizPartitionEntry*)fviz_array_at(data_set->partitions, i);
            (void)memset(entry, 0, sizeof(*entry));
        }
    }
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

FVizResult fviz_partitioned_data_set_add_partition(
    FVizPartitionedDataSet* data_set,
    FVizDataObject* partition,
    const char* name,
    FVizSize* out_index)
{
    FVizPartitionEntry entry;
    FVizSize index;
    if (out_index != NULL) *out_index = 0u;
    if (data_set == NULL || partition == NULL ||
        fviz_data_object_is_data_object(partition) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "partition must be a data object");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(&entry, 0, sizeof(entry));
    entry.data = (FVizDataObject*)fviz_retain(partition);
    if (entry.data == NULL) return fviz_last_error_code();
    entry.data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (fviz_partition_entry_observe(data_set, &entry) != FVIZ_OK)
    {
        fviz_release(entry.data);
        return fviz_last_error_code();
    }
    if (name != NULL && name[0] != '\0')
    {
        if (fviz_string_create_from(name, &entry.name) != FVIZ_OK)
        {
            fviz_partition_entry_release(&entry);
            return fviz_last_error_code();
        }
    }
    index = fviz_array_count(data_set->partitions);
    if (fviz_array_push(data_set->partitions, &entry) != FVIZ_OK)
    {
        fviz_partition_entry_release(&entry);
        return fviz_last_error_code();
    }
    if (out_index != NULL) *out_index = index;
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

FVizResult fviz_partitioned_data_set_set_partition(
    FVizPartitionedDataSet* data_set,
    FVizSize index,
    FVizDataObject* partition)
{
    FVizPartitionEntry* entry;
    FVizDataObject* replacement = NULL;
    if (data_set == NULL || index >= fviz_partitioned_data_set_count(data_set) ||
        (partition != NULL && fviz_data_object_is_data_object(partition) == FVIZ_FALSE))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "partition index or data is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    entry = (FVizPartitionEntry*)fviz_array_at(data_set->partitions, index);
    if (entry->data == partition) return FVIZ_OK;
    if (partition != NULL)
    {
        FVizPartitionEntry observed;
        (void)memset(&observed, 0, sizeof(observed));
        replacement = (FVizDataObject*)fviz_retain(partition);
        if (replacement == NULL) return fviz_last_error_code();
        observed.data = replacement;
        observed.data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
        if (fviz_partition_entry_observe(data_set, &observed) != FVIZ_OK)
        {
            fviz_release(replacement);
            return fviz_last_error_code();
        }
        if (entry->data != NULL && entry->data_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)entry->data, entry->data_modified_tag);
        fviz_release(entry->data);
        entry->data = replacement;
        entry->data_modified_tag = observed.data_modified_tag;
    }
    else
    {
        if (entry->data != NULL && entry->data_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)entry->data, entry->data_modified_tag);
        fviz_release(entry->data);
        entry->data = NULL;
        entry->data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    }
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

FVizDataObject* fviz_partitioned_data_set_partition(FVizPartitionedDataSet* data_set, FVizSize index)
{
    FVizPartitionEntry* entry;
    if (data_set == NULL || index >= fviz_partitioned_data_set_count(data_set)) return NULL;
    entry = (FVizPartitionEntry*)fviz_array_at(data_set->partitions, index);
    return entry != NULL ? entry->data : NULL;
}

const FVizDataObject* fviz_partitioned_data_set_const_partition(
    const FVizPartitionedDataSet* data_set, FVizSize index)
{
    const FVizPartitionEntry* entry;
    if (data_set == NULL || index >= fviz_partitioned_data_set_count(data_set)) return NULL;
    entry = (const FVizPartitionEntry*)fviz_array_const_at(data_set->partitions, index);
    return entry != NULL ? entry->data : NULL;
}

FVizResult fviz_partitioned_data_set_set_partition_name(
    FVizPartitionedDataSet* data_set,
    FVizSize index,
    const char* name)
{
    FVizPartitionEntry* entry;
    FVizString* replacement = NULL;
    if (data_set == NULL || index >= fviz_partitioned_data_set_count(data_set))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "partition index is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    entry = (FVizPartitionEntry*)fviz_array_at(data_set->partitions, index);
    {
        const char* current = entry->name != NULL ? fviz_string_c_str(entry->name) : NULL;
        const FVizBool empty_new = name == NULL || name[0] == '\0' ? FVIZ_TRUE : FVIZ_FALSE;
        if ((empty_new != FVIZ_FALSE && current == NULL) ||
            (empty_new == FVIZ_FALSE && current != NULL && strcmp(current, name) == 0))
            return FVIZ_OK;
    }
    if (name != NULL && name[0] != '\0')
    {
        if (fviz_string_create_from(name, &replacement) != FVIZ_OK) return fviz_last_error_code();
    }
    fviz_release(entry->name);
    entry->name = replacement;
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

const char* fviz_partitioned_data_set_partition_name(
    const FVizPartitionedDataSet* data_set,
    FVizSize index)
{
    const FVizPartitionEntry* entry;
    if (data_set == NULL || index >= fviz_partitioned_data_set_count(data_set)) return NULL;
    entry = (const FVizPartitionEntry*)fviz_array_const_at(data_set->partitions, index);
    return entry != NULL && entry->name != NULL ? fviz_string_c_str(entry->name) : NULL;
}

FVizResult fviz_partitioned_data_set_remove_partition(FVizPartitionedDataSet* data_set, FVizSize index)
{
    FVizSize count;
    FVizPartitionEntry* entries;
    if (data_set == NULL || index >= fviz_partitioned_data_set_count(data_set))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "partition index is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_array_count(data_set->partitions);
    entries = (FVizPartitionEntry*)fviz_array_data(data_set->partitions);
    fviz_partition_entry_release(&entries[index]);
    if (index + 1u < count)
        (void)memmove(&entries[index], &entries[index + 1u],
            (size_t)(count - index - 1u) * sizeof(*entries));
    if (fviz_array_resize(data_set->partitions, count - 1u) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

void fviz_partitioned_data_set_clear(FVizPartitionedDataSet* data_set)
{
    FVizSize i;
    FVizSize count;
    if (data_set == NULL || data_set->partitions == NULL) return;
    count = fviz_array_count(data_set->partitions);
    for (i = 0u; i < count; ++i)
        fviz_partition_entry_release((FVizPartitionEntry*)fviz_array_at(data_set->partitions, i));
    if (count > 0u)
    {
        fviz_array_clear(data_set->partitions);
        fviz_object_modified((FVizObject*)data_set);
    }
}

FVizResult fviz_partitioned_data_set_validate(const FVizPartitionedDataSet* data_set)
{
    FVizSize i;
    if (data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "partitioned dataset must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < fviz_partitioned_data_set_count(data_set); ++i)
    {
        const FVizDataObject* partition = fviz_partitioned_data_set_const_partition(data_set, i);
        if (partition == NULL || fviz_data_object_is_data_object(partition) == FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "partitioned dataset contains an empty or invalid partition");
            return FVIZ_ERROR_INVALID_STATE;
        }
    }
    return FVIZ_OK;
}
