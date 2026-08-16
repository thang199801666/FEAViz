#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizLabelSet.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizLabelSetPrivate.h>

static void fviz_label_set_3d_destroy(FVizObject* object);
static FVizMTime fviz_label_set_3d_mtime(const FVizObject* object);

static const FVizObjectClass g_fviz_label_set_3d_class = {
    FVIZ_TYPE_LABEL_SET_3D,
    "FVizLabelSet3D",
    &g_fviz_object_class,
    fviz_label_set_3d_destroy,
    fviz_label_set_3d_mtime
};

static FVizMTime fviz_label_set_3d_mtime(const FVizObject* object)
{
    const FVizLabelSet3D* label_set = (const FVizLabelSet3D*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    FVizMTime child = fviz_object_mtime((const FVizObject*)label_set->property);
    if (child > mtime) mtime = child;
    child = fviz_object_mtime((const FVizObject*)label_set->entries);
    if (child > mtime) mtime = child;
    /* Label strings are mutated only through LabelSet APIs, which modify this object.
       Avoid an O(N) child-string scan on every render invalidation query. */
    return mtime;
}

static void fviz_label_set_3d_destroy(FVizObject* object)
{
    FVizLabelSet3D* label_set = (FVizLabelSet3D*)object;
    FVizSize i;
    if (label_set->entries != NULL)
    {
        for (i = 0u; i < fviz_array_count(label_set->entries); ++i)
        {
            FVizLabelSet3DEntry* entry = (FVizLabelSet3DEntry*)fviz_array_at(label_set->entries, i);
            if (entry != NULL) fviz_release(entry->text);
        }
    }
    fviz_release(label_set->entries);
    fviz_release(label_set->property);
    label_set->entries = NULL;
    label_set->property = NULL;
}

FVizResult fviz_label_set_3d_create(FVizLabelSet3D** out_label_set)
{
    FVizLabelSet3D* label_set;
    if (out_label_set == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_label_set must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_label_set = NULL;
    label_set = (FVizLabelSet3D*)fviz_internal_object_allocate(sizeof(*label_set), &g_fviz_label_set_3d_class, NULL);
    if (label_set == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizLabelSet3DEntry), &label_set->entries) != FVIZ_OK ||
        fviz_text_property_create(&label_set->property) != FVIZ_OK)
    {
        fviz_release(label_set);
        return fviz_last_error_code();
    }
    label_set->visible = FVIZ_TRUE;
    label_set->depth_test = FVIZ_TRUE;
    label_set->pixel_offset[0] = 0.0f;
    label_set->pixel_offset[1] = 0.0f;
    *out_label_set = label_set;
    return FVIZ_OK;
}

void fviz_label_set_3d_clear(FVizLabelSet3D* label_set)
{
    FVizSize i;
    if (label_set == NULL || label_set->entries == NULL) return;
    for (i = 0u; i < fviz_array_count(label_set->entries); ++i)
    {
        FVizLabelSet3DEntry* entry = (FVizLabelSet3DEntry*)fviz_array_at(label_set->entries, i);
        if (entry != NULL) { fviz_release(entry->text); entry->text = NULL; }
    }
    fviz_array_clear(label_set->entries);
    fviz_object_modified((FVizObject*)label_set);
}

FVizResult fviz_label_set_3d_reserve(FVizLabelSet3D* label_set, FVizSize capacity)
{
    if (label_set == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_array_reserve(label_set->entries, capacity);
}

FVizResult fviz_label_set_3d_add(
    FVizLabelSet3D* label_set, FVizVec3 position, const char* utf8, FVizSize* out_index)
{
    FVizLabelSet3DEntry entry;
    FVizSize index;
    if (label_set == NULL || utf8 == NULL ||
        !isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    (void)memset(&entry, 0, sizeof(entry));
    entry.position = position;
    if (fviz_string_create_from(utf8, &entry.text) != FVIZ_OK) return fviz_last_error_code();
    index = fviz_array_count(label_set->entries);
    if (fviz_array_push(label_set->entries, &entry) != FVIZ_OK)
    {
        fviz_release(entry.text);
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)label_set);
    if (out_index != NULL) *out_index = index;
    return FVIZ_OK;
}

FVizSize fviz_label_set_3d_count(const FVizLabelSet3D* label_set)
{
    return label_set != NULL ? fviz_array_count(label_set->entries) : 0u;
}

FVizVec3 fviz_label_set_3d_position_at(const FVizLabelSet3D* label_set, FVizSize index)
{
    const FVizLabelSet3DEntry* entry = label_set != NULL ?
        (const FVizLabelSet3DEntry*)fviz_array_const_at(label_set->entries, index) : NULL;
    return entry != NULL ? entry->position : fviz_vec3(0.0f, 0.0f, 0.0f);
}

const char* fviz_label_set_3d_text_at(const FVizLabelSet3D* label_set, FVizSize index)
{
    const FVizLabelSet3DEntry* entry = label_set != NULL ?
        (const FVizLabelSet3DEntry*)fviz_array_const_at(label_set->entries, index) : NULL;
    return entry != NULL ? fviz_string_c_str(entry->text) : "";
}

FVizResult fviz_label_set_3d_set_position(FVizLabelSet3D* label_set, FVizSize index, FVizVec3 position)
{
    FVizLabelSet3DEntry* entry;
    if (label_set == NULL || !isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    entry = (FVizLabelSet3DEntry*)fviz_array_at(label_set->entries, index);
    if (entry == NULL) return FVIZ_ERROR_NOT_FOUND;
    entry->position = position;
    fviz_object_modified((FVizObject*)label_set);
    return FVIZ_OK;
}

FVizResult fviz_label_set_3d_set_text(FVizLabelSet3D* label_set, FVizSize index, const char* utf8)
{
    FVizLabelSet3DEntry* entry;
    if (label_set == NULL || utf8 == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    entry = (FVizLabelSet3DEntry*)fviz_array_at(label_set->entries, index);
    if (entry == NULL) return FVIZ_ERROR_NOT_FOUND;
    if (fviz_string_set(entry->text, utf8) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)label_set);
    return FVIZ_OK;
}

FVizTextProperty* fviz_label_set_3d_text_property(FVizLabelSet3D* label_set)
{
    return label_set != NULL ? label_set->property : NULL;
}

const FVizTextProperty* fviz_label_set_3d_const_text_property(const FVizLabelSet3D* label_set)
{
    return label_set != NULL ? label_set->property : NULL;
}

void fviz_label_set_3d_set_visible(FVizLabelSet3D* label_set, FVizBool visible)
{
    if (label_set == NULL) return;
    visible = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (label_set->visible != visible) { label_set->visible = visible; fviz_object_modified((FVizObject*)label_set); }
}

FVizBool fviz_label_set_3d_visible(const FVizLabelSet3D* label_set)
{
    return label_set != NULL ? label_set->visible : FVIZ_FALSE;
}

void fviz_label_set_3d_set_depth_test(FVizLabelSet3D* label_set, FVizBool enabled)
{
    if (label_set == NULL) return;
    enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (label_set->depth_test != enabled) { label_set->depth_test = enabled; fviz_object_modified((FVizObject*)label_set); }
}

FVizBool fviz_label_set_3d_depth_test(const FVizLabelSet3D* label_set)
{
    return label_set != NULL ? label_set->depth_test : FVIZ_FALSE;
}

void fviz_label_set_3d_set_pixel_offset(FVizLabelSet3D* label_set, float x, float y)
{
    if (label_set == NULL || !isfinite(x) || !isfinite(y)) return;
    if (label_set->pixel_offset[0] != x || label_set->pixel_offset[1] != y)
    {
        label_set->pixel_offset[0] = x;
        label_set->pixel_offset[1] = y;
        fviz_object_modified((FVizObject*)label_set);
    }
}

void fviz_label_set_3d_get_pixel_offset(const FVizLabelSet3D* label_set, float* x, float* y)
{
    if (x != NULL) *x = label_set != NULL ? label_set->pixel_offset[0] : 0.0f;
    if (y != NULL) *y = label_set != NULL ? label_set->pixel_offset[1] : 0.0f;
}

const FVizLabelSet3DEntry* fviz_internal_label_set_3d_entries(const FVizLabelSet3D* label_set)
{
    return label_set != NULL ? (const FVizLabelSet3DEntry*)fviz_array_const_data(label_set->entries) : NULL;
}
