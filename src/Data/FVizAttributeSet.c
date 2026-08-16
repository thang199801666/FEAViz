#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizAttributeSet.h>

#include <FViz/Core/FVizArrayPrivate.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Data/FVizAttributeSetPrivate.h>

#define FVIZ_ATTRIBUTE_HASH_INDEX_THRESHOLD 12u

static void fviz_attribute_set_destroy(FVizObject* object);
static FVizMTime fviz_attribute_set_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_attribute_set_class = {FVIZ_TYPE_ATTRIBUTE_SET, "FVizAttributeSet",
                                                           &g_fviz_object_class, fviz_attribute_set_destroy,
                                                           fviz_attribute_set_mtime};

static FVizBool fviz_attribute_set_array_modified(FVizObject* caller, FVizEventId event_id, void* call_data,
                                                  void* client_data)
{
    FVizAttributeSet* set = (FVizAttributeSet*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (set != NULL) fviz_object_modified((FVizObject*)set);
    return FVIZ_FALSE;
}

static FVizResult fviz_attribute_set_observe_array(FVizAttributeSet* set, FVizDataArray* array,
                                                   FVizObserverTag* out_tag)
{
    if (out_tag == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (array == NULL) return FVIZ_OK;
    return fviz_object_add_observer((FVizObject*)array, FVIZ_EVENT_MODIFIED, 0.0f, fviz_attribute_set_array_modified,
                                    set, out_tag);
}

static FVizMTime fviz_attribute_set_mtime(const FVizObject* object)
{
    const FVizAttributeSet* set = (const FVizAttributeSet*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(set); ++i)
    {
        const FVizMTime child_mtime = fviz_object_mtime((const FVizObject*)fviz_attribute_set_const_array_at(set, i));
        if (child_mtime > mtime) mtime = child_mtime;
    }
    return mtime;
}

static void fviz_attribute_set_destroy(FVizObject* object)
{
    FVizAttributeSet* set = (FVizAttributeSet*)object;
    fviz_attribute_set_clear(set);
    fviz_release(set->name_index);
    set->name_index = NULL;
    fviz_release(set->entries);
    set->entries = NULL;
}

void fviz_attribute_set_clear(FVizAttributeSet* set)
{
    FVizSize i;
    FVizBool changed;
    if (set == NULL) return;
    changed = fviz_array_count(set->entries) != 0u ? FVIZ_TRUE : FVIZ_FALSE;
    for (i = 0u; i < fviz_array_count(set->entries); ++i)
    {
        FVizAttributeEntry* entry = (FVizAttributeEntry*)fviz_array_at(set->entries, i);
        if (entry->array != NULL && entry->array_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)entry->array, entry->array_modified_tag);
        fviz_release(entry->name);
        fviz_release(entry->array);
    }
    fviz_internal_array_clear(set->entries);
    if (set->name_index != NULL) fviz_hash_map_clear(set->name_index);
    for (i = 0u; i < FVIZ_ATTRIBUTE_ROLE_COUNT; ++i)
    {
        fviz_release(set->active[i]);
        set->active[i] = NULL;
    }
    if (changed == FVIZ_TRUE) fviz_object_modified((FVizObject*)set);
}

FVizResult fviz_attribute_set_create(FVizAttributeSet** out_set)
{
    FVizAttributeSet* set;
    if (out_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_set must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_set = NULL;
    set = (FVizAttributeSet*)fviz_internal_object_allocate(sizeof(FVizAttributeSet), &g_fviz_attribute_set_class, NULL);
    if (set == NULL) return fviz_last_error_code();
    set->name_index = NULL;
    if (fviz_array_create(sizeof(FVizAttributeEntry), &set->entries) != FVIZ_OK)
    {
        fviz_release(set);
        return fviz_last_error_code();
    }
    *out_set = set;
    return FVIZ_OK;
}

static uint64_t fviz_attribute_name_hash(const char* name)
{
    const unsigned char* cursor = (const unsigned char*)name;
    uint64_t hash = UINT64_C(1469598103934665603);
    while (*cursor != 0u)
    {
        hash ^= (uint64_t)*cursor++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static FVizResult fviz_attribute_set_ensure_name_index(FVizAttributeSet* set, FVizSize required_count)
{
    FVizHashMap* index = NULL;
    FVizSize i;
    if (set->name_index != NULL || required_count < FVIZ_ATTRIBUTE_HASH_INDEX_THRESHOLD) return FVIZ_OK;
    if (fviz_hash_map_create_reserve(required_count * 2u, &index) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < fviz_array_count(set->entries); ++i)
    {
        const FVizAttributeEntry* entry = (const FVizAttributeEntry*)fviz_array_const_at(set->entries, i);
        if (fviz_hash_map_set(index, (FVizId)entry->name_hash, (void*)(uintptr_t)(i + 1u)) != FVIZ_OK)
        {
            fviz_release(index);
            return fviz_last_error_code();
        }
    }
    set->name_index = index;
    return FVIZ_OK;
}

static void fviz_attribute_set_refresh_name_index(FVizAttributeSet* set)
{
    FVizSize i;
    if (set->name_index == NULL) return;
    fviz_hash_map_clear(set->name_index);
    for (i = 0u; i < fviz_array_count(set->entries); ++i)
    {
        const FVizAttributeEntry* entry = (const FVizAttributeEntry*)fviz_array_const_at(set->entries, i);
        if (fviz_hash_map_set(set->name_index, (FVizId)entry->name_hash, (void*)(uintptr_t)(i + 1u)) != FVIZ_OK)
        {
            /* The index is an accelerator only; preserve correctness by
             * dropping it and falling back to linear lookup on failure. */
            fviz_release(set->name_index);
            set->name_index = NULL;
            return;
        }
    }
}

static FVizSize fviz_attribute_set_find(const FVizAttributeSet* set, const char* name)
{
    const uint64_t hash = fviz_attribute_name_hash(name);
    FVizSize i;
    if (set->name_index != NULL)
    {
        void* encoded = NULL;
        if (fviz_hash_map_get(set->name_index, (FVizId)hash, &encoded) != FVIZ_FALSE)
        {
            const uintptr_t raw = (uintptr_t)encoded;
            if (raw != 0u)
            {
                const FVizSize index = (FVizSize)(raw - 1u);
                if (index < fviz_array_count(set->entries))
                {
                    const FVizAttributeEntry* entry =
                        (const FVizAttributeEntry*)fviz_array_const_at(set->entries, index);
                    if (entry->name_hash == hash && strcmp(fviz_string_c_str(entry->name), name) == 0) return index;
                }
            }
        }
        /* Hash collisions are legal: fall through to the exact-name scan. */
    }
    for (i = 0u; i < fviz_array_count(set->entries); ++i)
    {
        const FVizAttributeEntry* entry = (const FVizAttributeEntry*)fviz_array_const_at(set->entries, i);
        if (entry->name_hash == hash && strcmp(fviz_string_c_str(entry->name), name) == 0) return i;
    }
    return (FVizSize)-1;
}

FVizSize fviz_attribute_set_count(const FVizAttributeSet* set)
{
    return set != NULL ? fviz_array_count(set->entries) : 0u;
}

const char* fviz_attribute_set_name_at(const FVizAttributeSet* set, FVizSize index)
{
    const FVizAttributeEntry* entry =
        set != NULL ? (const FVizAttributeEntry*)fviz_array_const_at(set->entries, index) : NULL;
    return entry != NULL ? fviz_string_c_str(entry->name) : NULL;
}

FVizDataArray* fviz_attribute_set_array_at(FVizAttributeSet* set, FVizSize index)
{
    FVizAttributeEntry* entry = set != NULL ? (FVizAttributeEntry*)fviz_array_at(set->entries, index) : NULL;
    return entry != NULL ? entry->array : NULL;
}

const FVizDataArray* fviz_attribute_set_const_array_at(const FVizAttributeSet* set, FVizSize index)
{
    const FVizAttributeEntry* entry =
        set != NULL ? (const FVizAttributeEntry*)fviz_array_const_at(set->entries, index) : NULL;
    return entry != NULL ? entry->array : NULL;
}

FVizDataArray* fviz_attribute_set_get(FVizAttributeSet* set, const char* name)
{
    FVizSize index = set != NULL && name != NULL ? fviz_attribute_set_find(set, name) : (FVizSize)-1;
    return index == (FVizSize)-1 ? NULL : fviz_attribute_set_array_at(set, index);
}

const FVizDataArray* fviz_attribute_set_const_get(const FVizAttributeSet* set, const char* name)
{
    FVizSize index = set != NULL && name != NULL ? fviz_attribute_set_find(set, name) : (FVizSize)-1;
    return index == (FVizSize)-1 ? NULL : fviz_attribute_set_const_array_at(set, index);
}

FVizResult fviz_attribute_set_add(FVizAttributeSet* set, const char* name, FVizDataArray* array)
{
    FVizSize index;
    FVizAttributeEntry entry;
    if (set == NULL || name == NULL || name[0] == '\0' || array == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "attribute set requires a name and array");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    index = fviz_attribute_set_find(set, name);
    if (index != (FVizSize)-1)
    {
        FVizAttributeEntry* old = (FVizAttributeEntry*)fviz_array_at(set->entries, index);
        FVizObserverTag new_tag = FVIZ_OBSERVER_TAG_INVALID;
        if (old->array == array) return FVIZ_OK;
        if (fviz_retain(array) == NULL) return fviz_last_error_code();
        if (fviz_attribute_set_observe_array(set, array, &new_tag) != FVIZ_OK)
        {
            fviz_release(array);
            return fviz_last_error_code();
        }
        if (old->array != NULL && old->array_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)old->array, old->array_modified_tag);
        fviz_release(old->array);
        old->array = array;
        old->array_modified_tag = new_tag;
        fviz_object_modified((FVizObject*)set);
        return FVIZ_OK;
    }
    if (fviz_attribute_set_ensure_name_index(set, fviz_array_count(set->entries) + 1u) != FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_string_create_from(name, &entry.name) != FVIZ_OK) return fviz_last_error_code();
    entry.name_hash = fviz_attribute_name_hash(name);
    entry.array = (FVizDataArray*)fviz_retain(array);
    entry.array_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (entry.array == NULL ||
        fviz_attribute_set_observe_array(set, entry.array, &entry.array_modified_tag) != FVIZ_OK ||
        fviz_internal_array_append(set->entries, &entry, 1u) != FVIZ_OK)
    {
        if (entry.array != NULL && entry.array_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)entry.array, entry.array_modified_tag);
        fviz_release(entry.name);
        fviz_release(entry.array);
        return fviz_last_error_code();
    }
    if (set->name_index != NULL && fviz_hash_map_set(set->name_index, (FVizId)entry.name_hash,
                                                     (void*)(uintptr_t)fviz_array_count(set->entries)) != FVIZ_OK)
    {
        FVizAttributeEntry* appended =
            (FVizAttributeEntry*)fviz_array_at(set->entries, fviz_array_count(set->entries) - 1u);
        if (appended->array != NULL && appended->array_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
            (void)fviz_object_remove_observer((FVizObject*)appended->array, appended->array_modified_tag);
        fviz_release(appended->name);
        fviz_release(appended->array);
        (void)fviz_internal_array_resize_untracked(set->entries, fviz_array_count(set->entries) - 1u);
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)set);
    return FVIZ_OK;
}

FVizResult fviz_attribute_set_remove(FVizAttributeSet* set, const char* name)
{
    FVizSize index;
    FVizSize count;
    FVizAttributeEntry* entry;
    if (set == NULL || name == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "attribute set and name must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    index = fviz_attribute_set_find(set, name);
    if (index == (FVizSize)-1) return FVIZ_ERROR_NOT_FOUND;
    entry = (FVizAttributeEntry*)fviz_array_at(set->entries, index);
    {
        FVizSize role;
        for (role = 0u; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            if (set->active[role] != NULL && strcmp(fviz_string_c_str(set->active[role]), name) == 0)
            {
                fviz_release(set->active[role]);
                set->active[role] = NULL;
            }
        }
    }
    if (entry->array != NULL && entry->array_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)entry->array, entry->array_modified_tag);
    fviz_release(entry->name);
    fviz_release(entry->array);
    count = fviz_array_count(set->entries);
    if (index + 1u < count) memmove(entry, entry + 1, (count - index - 1u) * sizeof(*entry));
    if (fviz_internal_array_resize_untracked(set->entries, count - 1u) != FVIZ_OK) return fviz_last_error_code();
    fviz_attribute_set_refresh_name_index(set);
    fviz_object_modified((FVizObject*)set);
    return FVIZ_OK;
}

FVizResult fviz_attribute_set_set_active(FVizAttributeSet* set, FVizAttributeRole role, const char* name)
{
    FVizString* active = NULL;
    if (set == NULL || role < 0 || role >= FVIZ_ATTRIBUTE_ROLE_COUNT)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "attribute role is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    {
        const char* current = fviz_attribute_set_active_name(set, role);
        if ((name == NULL && current == NULL) || (name != NULL && current != NULL && strcmp(name, current) == 0))
            return FVIZ_OK;
    }
    if (name != NULL)
    {
        if (fviz_attribute_set_get(set, name) == NULL)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "active attribute name was not found");
            return FVIZ_ERROR_NOT_FOUND;
        }
        if (fviz_string_create_from(name, &active) != FVIZ_OK) return fviz_last_error_code();
    }
    fviz_release(set->active[role]);
    set->active[role] = active;
    fviz_object_modified((FVizObject*)set);
    return FVIZ_OK;
}

const char* fviz_attribute_set_active_name(const FVizAttributeSet* set, FVizAttributeRole role)
{
    return set != NULL && role >= 0 && role < FVIZ_ATTRIBUTE_ROLE_COUNT && set->active[role] != NULL
               ? fviz_string_c_str(set->active[role])
               : NULL;
}

FVizDataArray* fviz_attribute_set_active(FVizAttributeSet* set, FVizAttributeRole role)
{
    const char* name = fviz_attribute_set_active_name(set, role);
    return name != NULL ? fviz_attribute_set_get(set, name) : NULL;
}

const FVizDataArray* fviz_attribute_set_const_active(const FVizAttributeSet* set, FVizAttributeRole role)
{
    const char* name = fviz_attribute_set_active_name(set, role);
    return name != NULL ? fviz_attribute_set_const_get(set, name) : NULL;
}

static FVizResult fviz_attribute_set_copy_impl(const FVizAttributeSet* source, FVizBool deep,
                                               FVizAttributeSet** out_copy)
{
    FVizAttributeSet* copy = NULL;
    FVizSize i;
    FVizAttributeRole role;
    if (source == NULL || out_copy == NULL)
    {
        if (out_copy != NULL) *out_copy = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "attribute set copy requires source and output");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_copy = NULL;
    if (fviz_attribute_set_create(&copy) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        const FVizDataArray* source_array = fviz_attribute_set_const_array_at(source, i);
        FVizDataArray* array = (FVizDataArray*)source_array;
        if (deep != FVIZ_FALSE && fviz_data_array_deep_copy(source_array, &array) != FVIZ_OK) goto fail;
        if (fviz_attribute_set_add(copy, name, array) != FVIZ_OK)
        {
            if (deep != FVIZ_FALSE) fviz_release(array);
            goto fail;
        }
        if (deep != FVIZ_FALSE) fviz_release(array);
    }
    for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
    {
        const char* name = fviz_attribute_set_active_name(source, role);
        if (name != NULL && fviz_attribute_set_set_active(copy, role, name) != FVIZ_OK) goto fail;
    }
    *out_copy = copy;
    return FVIZ_OK;
fail:
    fviz_release(copy);
    return fviz_last_error_code();
}

FVizResult fviz_attribute_set_shallow_copy(const FVizAttributeSet* source, FVizAttributeSet** out_copy)
{
    return fviz_attribute_set_copy_impl(source, FVIZ_FALSE, out_copy);
}

FVizResult fviz_attribute_set_deep_copy(const FVizAttributeSet* source, FVizAttributeSet** out_copy)
{
    return fviz_attribute_set_copy_impl(source, FVIZ_TRUE, out_copy);
}
