#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizAttributeSet.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Data/FVizAttributeSetPrivate.h>

static void fviz_attribute_set_destroy(FVizObject* object);
static FVizMTime fviz_attribute_set_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_attribute_set_class = {
    FVIZ_TYPE_ATTRIBUTE_SET, "FVizAttributeSet", &g_fviz_object_class,
    fviz_attribute_set_destroy, fviz_attribute_set_mtime
};

static FVizMTime fviz_attribute_set_mtime(const FVizObject* object)
{
    const FVizAttributeSet* set = (const FVizAttributeSet*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(set); ++i)
    {
        const FVizMTime child_mtime = fviz_object_mtime(
            (const FVizObject*)fviz_attribute_set_const_array_at(set, i));
        if (child_mtime > mtime) mtime = child_mtime;
    }
    return mtime;
}

static void fviz_attribute_set_destroy(FVizObject* object)
{
    FVizAttributeSet* set = (FVizAttributeSet*)object;
    fviz_attribute_set_clear(set);
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
        fviz_release(entry->name);
        fviz_release(entry->array);
    }
    fviz_array_clear(set->entries);
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
    if (fviz_array_create(sizeof(FVizAttributeEntry), &set->entries) != FVIZ_OK)
    {
        fviz_release(set);
        return fviz_last_error_code();
    }
    *out_set = set;
    return FVIZ_OK;
}

static FVizSize fviz_attribute_set_find(const FVizAttributeSet* set, const char* name)
{
    FVizSize i;
    for (i = 0u; i < fviz_array_count(set->entries); ++i)
    {
        const FVizAttributeEntry* entry = (const FVizAttributeEntry*)fviz_array_const_at(set->entries, i);
        if (strcmp(fviz_string_c_str(entry->name), name) == 0) return i;
    }
    return (FVizSize)-1;
}

FVizSize fviz_attribute_set_count(const FVizAttributeSet* set) { return set != NULL ? fviz_array_count(set->entries) : 0u; }

const char* fviz_attribute_set_name_at(const FVizAttributeSet* set, FVizSize index)
{
    const FVizAttributeEntry* entry = set != NULL ? (const FVizAttributeEntry*)fviz_array_const_at(set->entries, index) : NULL;
    return entry != NULL ? fviz_string_c_str(entry->name) : NULL;
}

FVizDataArray* fviz_attribute_set_array_at(FVizAttributeSet* set, FVizSize index)
{
    FVizAttributeEntry* entry = set != NULL ? (FVizAttributeEntry*)fviz_array_at(set->entries, index) : NULL;
    return entry != NULL ? entry->array : NULL;
}

const FVizDataArray* fviz_attribute_set_const_array_at(const FVizAttributeSet* set, FVizSize index)
{
    const FVizAttributeEntry* entry = set != NULL ? (const FVizAttributeEntry*)fviz_array_const_at(set->entries, index) : NULL;
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
        fviz_retain(array);
        fviz_release(old->array);
        old->array = array;
        fviz_object_modified((FVizObject*)set);
        return FVIZ_OK;
    }
    if (fviz_string_create_from(name, &entry.name) != FVIZ_OK) return fviz_last_error_code();
    entry.array = (FVizDataArray*)fviz_retain(array);
    if (fviz_array_push(set->entries, &entry) != FVIZ_OK)
    {
        fviz_release(entry.name);
        fviz_release(entry.array);
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
            if (set->active[role] != NULL &&
                strcmp(fviz_string_c_str(set->active[role]), name) == 0)
            {
                fviz_release(set->active[role]);
                set->active[role] = NULL;
            }
        }
    }
    fviz_release(entry->name);
    fviz_release(entry->array);
    count = fviz_array_count(set->entries);
    if (index + 1u < count) memmove(entry, entry + 1, (count - index - 1u) * sizeof(*entry));
    if (fviz_array_resize(set->entries, count - 1u) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)set);
    return FVIZ_OK;
}

FVizResult fviz_attribute_set_set_active(
    FVizAttributeSet* set,
    FVizAttributeRole role,
    const char* name)
{
    FVizString* active = NULL;
    if (set == NULL || role < 0 || role >= FVIZ_ATTRIBUTE_ROLE_COUNT)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "attribute role is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
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

const char* fviz_attribute_set_active_name(
    const FVizAttributeSet* set,
    FVizAttributeRole role)
{
    return set != NULL && role >= 0 && role < FVIZ_ATTRIBUTE_ROLE_COUNT && set->active[role] != NULL
        ? fviz_string_c_str(set->active[role]) : NULL;
}

FVizDataArray* fviz_attribute_set_active(FVizAttributeSet* set, FVizAttributeRole role)
{
    const char* name = fviz_attribute_set_active_name(set, role);
    return name != NULL ? fviz_attribute_set_get(set, name) : NULL;
}

const FVizDataArray* fviz_attribute_set_const_active(
    const FVizAttributeSet* set,
    FVizAttributeRole role)
{
    const char* name = fviz_attribute_set_active_name(set, role);
    return name != NULL ? fviz_attribute_set_const_get(set, name) : NULL;
}
