#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizMultiBlockDataSet.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/Data/FVizMultiBlockDataSetPrivate.h>

static void fviz_multi_block_data_set_destroy(FVizObject* object);
static FVizMTime fviz_multi_block_data_set_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_multi_block_data_set_class = {
    FVIZ_TYPE_MULTI_BLOCK_DATA_SET, "FVizMultiBlockDataSet", &g_fviz_data_object_class,
    fviz_multi_block_data_set_destroy, fviz_multi_block_data_set_mtime};

static FVizBool fviz_multi_block_child_modified(FVizObject* caller, FVizEventId event_id, void* call_data,
                                                void* client_data)
{
    FVizMultiBlockDataSet* data_set = (FVizMultiBlockDataSet*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (data_set != NULL) fviz_object_modified((FVizObject*)data_set);
    return FVIZ_FALSE;
}

static FVizResult fviz_multi_block_entry_observe(FVizMultiBlockDataSet* data_set, FVizMultiBlockEntry* entry)
{
    if (entry == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    entry->data_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (entry->data == NULL) return FVIZ_OK;
    return fviz_object_add_observer((FVizObject*)entry->data, FVIZ_EVENT_MODIFIED, 0.0f,
                                    fviz_multi_block_child_modified, data_set, &entry->data_modified_tag);
}

static void fviz_multi_block_entry_release(FVizMultiBlockEntry* entry)
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

static FVizMTime fviz_multi_block_data_set_mtime(const FVizObject* object)
{
    /* Child ModifiedEvents are bridged, keeping repeated pipeline MTime queries O(1). */
    return fviz_internal_object_local_mtime(object);
}

static FVizBool fviz_multi_block_would_cycle(const FVizMultiBlockDataSet* target, const FVizDataObject* candidate)
{
    FVizArray* stack = NULL;
    FVizHashMap* visited = NULL;
    FVizBool cycle = FVIZ_FALSE;
    if (target == NULL || candidate == NULL) return FVIZ_FALSE;
    if ((const void*)target == (const void*)candidate) return FVIZ_TRUE;
    if (fviz_object_is_type((const FVizObject*)candidate, FVIZ_TYPE_MULTI_BLOCK_DATA_SET) == FVIZ_FALSE)
        return FVIZ_FALSE;
    if (fviz_array_create(sizeof(const FVizMultiBlockDataSet*), &stack) != FVIZ_OK ||
        fviz_hash_map_create(&visited) != FVIZ_OK)
    {
        /* Allocation failure is handled by the caller's subsequent retain/observe path.
         * Conservatively reject the edge so a retain cycle can never be introduced. */
        cycle = FVIZ_TRUE;
        goto done;
    }
    {
        const FVizMultiBlockDataSet* root = (const FVizMultiBlockDataSet*)candidate;
        if (fviz_array_push(stack, &root) != FVIZ_OK)
        {
            cycle = FVIZ_TRUE;
            goto done;
        }
    }
    while (fviz_array_count(stack) != 0u)
    {
        const FVizSize last = fviz_array_count(stack) - 1u;
        const FVizMultiBlockDataSet* current = *(const FVizMultiBlockDataSet* const*)fviz_array_const_at(stack, last);
        FVizSize i;
        (void)fviz_array_resize(stack, last);
        if (current == target)
        {
            cycle = FVIZ_TRUE;
            break;
        }
        if (fviz_hash_map_contains(visited, (FVizId)(uintptr_t)current) != FVIZ_FALSE) continue;
        if (fviz_hash_map_set(visited, (FVizId)(uintptr_t)current, (void*)current) != FVIZ_OK)
        {
            cycle = FVIZ_TRUE;
            break;
        }
        for (i = 0u; i < fviz_multi_block_data_set_count(current); ++i)
        {
            const FVizDataObject* child = fviz_multi_block_data_set_const_block(current, i);
            if ((const void*)child == (const void*)target)
            {
                cycle = FVIZ_TRUE;
                break;
            }
            if (child != NULL &&
                fviz_object_is_type((const FVizObject*)child, FVIZ_TYPE_MULTI_BLOCK_DATA_SET) != FVIZ_FALSE)
            {
                const FVizMultiBlockDataSet* nested = (const FVizMultiBlockDataSet*)child;
                if (fviz_array_push(stack, &nested) != FVIZ_OK)
                {
                    cycle = FVIZ_TRUE;
                    break;
                }
            }
        }
        if (cycle != FVIZ_FALSE) break;
    }
done:
    fviz_release(visited);
    fviz_release(stack);
    return cycle;
}

static void fviz_multi_block_data_set_destroy(FVizObject* object)
{
    FVizMultiBlockDataSet* data_set = (FVizMultiBlockDataSet*)object;
    fviz_multi_block_data_set_clear(data_set);
    fviz_release(data_set->blocks);
    data_set->blocks = NULL;
}

FVizResult fviz_multi_block_data_set_create(FVizMultiBlockDataSet** out_data_set)
{
    FVizMultiBlockDataSet* data_set;
    if (out_data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_data_set must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_data_set = NULL;
    data_set = (FVizMultiBlockDataSet*)fviz_internal_object_allocate(sizeof(*data_set),
                                                                     &g_fviz_multi_block_data_set_class, NULL);
    if (data_set == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizMultiBlockEntry), &data_set->blocks) != FVIZ_OK)
    {
        fviz_release(data_set);
        return fviz_last_error_code();
    }
    *out_data_set = data_set;
    return FVIZ_OK;
}

FVizSize fviz_multi_block_data_set_count(const FVizMultiBlockDataSet* data_set)
{
    return data_set != NULL && data_set->blocks != NULL ? fviz_array_count(data_set->blocks) : 0u;
}

FVizResult fviz_multi_block_data_set_reserve(FVizMultiBlockDataSet* data_set, FVizSize capacity)
{
    if (data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "multi-block dataset must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_array_reserve(data_set->blocks, capacity);
}

FVizResult fviz_multi_block_data_set_resize(FVizMultiBlockDataSet* data_set, FVizSize count)
{
    FVizSize old_count;
    FVizSize i;
    if (data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "multi-block dataset must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    old_count = fviz_array_count(data_set->blocks);
    if (old_count == count) return FVIZ_OK;
    if (count < old_count)
    {
        for (i = count; i < old_count; ++i)
            fviz_multi_block_entry_release((FVizMultiBlockEntry*)fviz_array_at(data_set->blocks, i));
        if (fviz_array_resize(data_set->blocks, count) != FVIZ_OK) return fviz_last_error_code();
    }
    else
    {
        if (fviz_array_resize(data_set->blocks, count) != FVIZ_OK) return fviz_last_error_code();
        for (i = old_count; i < count; ++i)
            (void)memset(fviz_array_at(data_set->blocks, i), 0, sizeof(FVizMultiBlockEntry));
    }
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

FVizResult fviz_multi_block_data_set_add_block(FVizMultiBlockDataSet* data_set, FVizDataObject* block, const char* name,
                                               FVizSize* out_index)
{
    FVizMultiBlockEntry entry;
    FVizSize index;
    if (out_index != NULL) *out_index = 0u;
    if (data_set == NULL || block == NULL || fviz_data_object_is_data_object(block) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "block must be a data object");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_multi_block_would_cycle(data_set, block) != FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "multi-block hierarchy cannot contain a cycle");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(&entry, 0, sizeof(entry));
    entry.data = (FVizDataObject*)fviz_retain(block);
    if (entry.data == NULL) return fviz_last_error_code();
    if (fviz_multi_block_entry_observe(data_set, &entry) != FVIZ_OK)
    {
        fviz_release(entry.data);
        return fviz_last_error_code();
    }
    if (name != NULL && name[0] != '\0' && fviz_string_create_from(name, &entry.name) != FVIZ_OK)
    {
        fviz_multi_block_entry_release(&entry);
        return fviz_last_error_code();
    }
    index = fviz_array_count(data_set->blocks);
    if (fviz_array_push(data_set->blocks, &entry) != FVIZ_OK)
    {
        fviz_multi_block_entry_release(&entry);
        return fviz_last_error_code();
    }
    if (out_index != NULL) *out_index = index;
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

FVizResult fviz_multi_block_data_set_set_block(FVizMultiBlockDataSet* data_set, FVizSize index, FVizDataObject* block)
{
    FVizMultiBlockEntry* entry;
    FVizMultiBlockEntry replacement;
    if (data_set == NULL || index >= fviz_multi_block_data_set_count(data_set) ||
        (block != NULL && fviz_data_object_is_data_object(block) == FVIZ_FALSE))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "block index or data is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    entry = (FVizMultiBlockEntry*)fviz_array_at(data_set->blocks, index);
    if (entry->data == block) return FVIZ_OK;
    if (block != NULL && fviz_multi_block_would_cycle(data_set, block) != FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "multi-block hierarchy cannot contain a cycle");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(&replacement, 0, sizeof(replacement));
    if (block != NULL)
    {
        replacement.data = (FVizDataObject*)fviz_retain(block);
        if (replacement.data == NULL) return fviz_last_error_code();
        if (fviz_multi_block_entry_observe(data_set, &replacement) != FVIZ_OK)
        {
            fviz_release(replacement.data);
            return fviz_last_error_code();
        }
    }
    if (entry->data != NULL && entry->data_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)entry->data, entry->data_modified_tag);
    fviz_release(entry->data);
    entry->data = replacement.data;
    entry->data_modified_tag = replacement.data_modified_tag;
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

FVizDataObject* fviz_multi_block_data_set_block(FVizMultiBlockDataSet* data_set, FVizSize index)
{
    FVizMultiBlockEntry* entry;
    if (data_set == NULL || index >= fviz_multi_block_data_set_count(data_set)) return NULL;
    entry = (FVizMultiBlockEntry*)fviz_array_at(data_set->blocks, index);
    return entry != NULL ? entry->data : NULL;
}

const FVizDataObject* fviz_multi_block_data_set_const_block(const FVizMultiBlockDataSet* data_set, FVizSize index)
{
    const FVizMultiBlockEntry* entry;
    if (data_set == NULL || index >= fviz_multi_block_data_set_count(data_set)) return NULL;
    entry = (const FVizMultiBlockEntry*)fviz_array_const_at(data_set->blocks, index);
    return entry != NULL ? entry->data : NULL;
}

FVizResult fviz_multi_block_data_set_set_block_name(FVizMultiBlockDataSet* data_set, FVizSize index, const char* name)
{
    FVizMultiBlockEntry* entry;
    FVizString* replacement = NULL;
    const char* current;
    const FVizBool empty_new = name == NULL || name[0] == '\0' ? FVIZ_TRUE : FVIZ_FALSE;
    if (data_set == NULL || index >= fviz_multi_block_data_set_count(data_set))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "block index is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    entry = (FVizMultiBlockEntry*)fviz_array_at(data_set->blocks, index);
    current = entry->name != NULL ? fviz_string_c_str(entry->name) : NULL;
    if ((empty_new != FVIZ_FALSE && current == NULL) ||
        (empty_new == FVIZ_FALSE && current != NULL && strcmp(current, name) == 0))
        return FVIZ_OK;
    if (empty_new == FVIZ_FALSE && fviz_string_create_from(name, &replacement) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_release(entry->name);
    entry->name = replacement;
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

const char* fviz_multi_block_data_set_block_name(const FVizMultiBlockDataSet* data_set, FVizSize index)
{
    const FVizMultiBlockEntry* entry;
    if (data_set == NULL || index >= fviz_multi_block_data_set_count(data_set)) return NULL;
    entry = (const FVizMultiBlockEntry*)fviz_array_const_at(data_set->blocks, index);
    return entry != NULL && entry->name != NULL ? fviz_string_c_str(entry->name) : NULL;
}

FVizBool fviz_multi_block_data_set_find_block(const FVizMultiBlockDataSet* data_set, const char* name,
                                              FVizSize* out_index)
{
    FVizSize i;
    if (out_index != NULL) *out_index = 0u;
    if (data_set == NULL || name == NULL || name[0] == '\0') return FVIZ_FALSE;
    for (i = 0u; i < fviz_multi_block_data_set_count(data_set); ++i)
    {
        const char* candidate = fviz_multi_block_data_set_block_name(data_set, i);
        if (candidate != NULL && strcmp(candidate, name) == 0)
        {
            if (out_index != NULL) *out_index = i;
            return FVIZ_TRUE;
        }
    }
    return FVIZ_FALSE;
}

FVizResult fviz_multi_block_data_set_remove_block(FVizMultiBlockDataSet* data_set, FVizSize index)
{
    FVizSize count;
    FVizMultiBlockEntry* entries;
    if (data_set == NULL || index >= fviz_multi_block_data_set_count(data_set))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "block index is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_array_count(data_set->blocks);
    entries = (FVizMultiBlockEntry*)fviz_array_data(data_set->blocks);
    fviz_multi_block_entry_release(&entries[index]);
    if (index + 1u < count)
        (void)memmove(&entries[index], &entries[index + 1u], (size_t)(count - index - 1u) * sizeof(*entries));
    if (fviz_array_resize(data_set->blocks, count - 1u) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)data_set);
    return FVIZ_OK;
}

void fviz_multi_block_data_set_clear(FVizMultiBlockDataSet* data_set)
{
    FVizSize i;
    FVizSize count;
    if (data_set == NULL || data_set->blocks == NULL) return;
    count = fviz_array_count(data_set->blocks);
    for (i = 0u; i < count; ++i)
        fviz_multi_block_entry_release((FVizMultiBlockEntry*)fviz_array_at(data_set->blocks, i));
    if (count != 0u)
    {
        fviz_array_clear(data_set->blocks);
        fviz_object_modified((FVizObject*)data_set);
    }
}

FVizResult fviz_multi_block_data_set_validate(const FVizMultiBlockDataSet* data_set)
{
    FVizSize i;
    if (data_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "multi-block dataset must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < fviz_multi_block_data_set_count(data_set); ++i)
    {
        const FVizDataObject* block = fviz_multi_block_data_set_const_block(data_set, i);
        if (block != NULL && fviz_data_object_is_data_object(block) == FVIZ_FALSE)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "multi-block child is not a data object");
            return FVIZ_ERROR_INVALID_STATE;
        }
    }
    return FVIZ_OK;
}

typedef struct FVizMultiBlockVisitEntry
{
    const FVizMultiBlockDataSet* parent;
    FVizSize index;
    FVizSize depth;
} FVizMultiBlockVisitEntry;

FVizResult fviz_multi_block_data_set_visit(const FVizMultiBlockDataSet* data_set, FVizBool recursive,
                                           FVizBool leaves_only, FVizMultiBlockVisitFn visitor, void* user_data)
{
    FVizArray* stack = NULL;
    FVizSize i;
    FVizResult result = FVIZ_OK;
    if (data_set == NULL || visitor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "multi-block visitor arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_array_create(sizeof(FVizMultiBlockVisitEntry), &stack) != FVIZ_OK) return fviz_last_error_code();
    for (i = fviz_multi_block_data_set_count(data_set); i > 0u; --i)
    {
        FVizMultiBlockVisitEntry entry = {data_set, i - 1u, 0u};
        if (fviz_array_push(stack, &entry) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
    }
    while (fviz_array_count(stack) != 0u)
    {
        FVizMultiBlockVisitEntry entry =
            *(const FVizMultiBlockVisitEntry*)fviz_array_const_at(stack, fviz_array_count(stack) - 1u);
        const FVizDataObject* block;
        const char* name;
        const FVizBool is_composite =
            fviz_multi_block_data_set_const_block(entry.parent, entry.index) != NULL &&
                    fviz_object_is_type(
                        (const FVizObject*)fviz_multi_block_data_set_const_block(entry.parent, entry.index),
                        FVIZ_TYPE_MULTI_BLOCK_DATA_SET) != FVIZ_FALSE
                ? FVIZ_TRUE
                : FVIZ_FALSE;
        if (fviz_array_resize(stack, fviz_array_count(stack) - 1u) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
        block = fviz_multi_block_data_set_const_block(entry.parent, entry.index);
        if (block == NULL) continue;
        name = fviz_multi_block_data_set_block_name(entry.parent, entry.index);
        if (leaves_only == FVIZ_FALSE || is_composite == FVIZ_FALSE)
        {
            result = visitor(entry.parent, entry.index, block, name, entry.depth, user_data);
            if (result != FVIZ_OK) goto done;
        }
        if (recursive != FVIZ_FALSE && is_composite != FVIZ_FALSE)
        {
            const FVizMultiBlockDataSet* nested = (const FVizMultiBlockDataSet*)block;
            FVizSize child;
            for (child = fviz_multi_block_data_set_count(nested); child > 0u; --child)
            {
                FVizMultiBlockVisitEntry next = {nested, child - 1u, entry.depth + 1u};
                if (fviz_array_push(stack, &next) != FVIZ_OK)
                {
                    result = fviz_last_error_code();
                    goto done;
                }
            }
        }
    }
done:
    fviz_release(stack);
    return result;
}

typedef struct FVizMultiBlockLeafCounter
{
    FVizSize count;
} FVizMultiBlockLeafCounter;

static FVizResult fviz_multi_block_count_leaf(const FVizMultiBlockDataSet* parent, FVizSize index,
                                              const FVizDataObject* block, const char* name, FVizSize depth,
                                              void* user_data)
{
    FVizMultiBlockLeafCounter* counter = (FVizMultiBlockLeafCounter*)user_data;
    (void)parent;
    (void)index;
    (void)block;
    (void)name;
    (void)depth;
    ++counter->count;
    return FVIZ_OK;
}

FVizSize fviz_multi_block_data_set_leaf_count(const FVizMultiBlockDataSet* data_set, FVizBool recursive)
{
    FVizMultiBlockLeafCounter counter = {0u};
    if (data_set == NULL) return 0u;
    if (fviz_multi_block_data_set_visit(data_set, recursive, FVIZ_TRUE, fviz_multi_block_count_leaf, &counter) !=
        FVIZ_OK)
        return 0u;
    return counter.count;
}
