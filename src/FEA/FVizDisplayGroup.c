#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizProvenance.h>
#include <FViz/FEA/FVizDisplayGroup.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

typedef struct FVizFEAEntitySet
{
    FVizHashMap* labels; /* FVizId -> (void*)1 */
} FVizFEAEntitySet;

struct FVizFEADisplayGroup
{
    FVizObject base;
    char* name;
    FVizBool visible;
    FVizFEAEntitySet nodes;
    FVizFEAEntitySet elements;
    FVizFEAEntitySet faces;
};

static FVizMTime fviz_fea_display_group_local_mtime(const FVizObject* object)
{
    return fviz_internal_object_local_mtime(object);
}

static void fviz_fea_entity_set_destroy(FVizFEAEntitySet* set)
{
    fviz_release(set->labels);
    set->labels = NULL;
}

static void fviz_fea_display_group_destroy(FVizObject* object)
{
    FVizFEADisplayGroup* group = (FVizFEADisplayGroup*)object;
    fviz_release(group->name);
    fviz_fea_entity_set_destroy(&group->nodes);
    fviz_fea_entity_set_destroy(&group->elements);
    fviz_fea_entity_set_destroy(&group->faces);
}

static const FVizObjectClass g_fviz_fea_display_group_class = {
    FVIZ_TYPE_FEA_DISPLAY_GROUP,
    "FVizFEADisplayGroup",
    NULL,
    fviz_fea_display_group_destroy,
    fviz_fea_display_group_local_mtime
};

static FVizResult fviz_fea_entity_set_init(FVizFEAEntitySet* set)
{
    return fviz_hash_map_create_reserve(32u, &set->labels);
}

static FVizResult fviz_fea_entity_set_set(
    FVizFEAEntitySet* set, const uint64_t* labels, FVizSize count)
{
    FVizSize i;
    fviz_hash_map_clear(set->labels);
    for (i = 0u; i < count; ++i)
        if (fviz_hash_map_set(set->labels, (FVizId)labels[i], (void*)(uintptr_t)1) != FVIZ_OK)
            return fviz_last_error_code();
    return FVIZ_OK;
}

static FVizBool fviz_fea_entity_set_contains(const FVizFEAEntitySet* set, FVizId label)
{
    return fviz_hash_map_contains(set->labels, label) != FVIZ_FALSE;
}

static FVizResult fviz_fea_entity_set_combine(
    FVizFEAEntitySet* set, const FVizFEAEntitySet* source, FVizFEADisplayGroupOperation operation)
{
    FVizSize cursor = 0u;
    FVizId key;
    void* value = NULL;
    if (operation == FVIZ_FEA_DISPLAY_GROUP_INTERSECT)
    {
        /* Keep only labels present in both sets. */
        FVizHashMap* intersection = NULL;
        FVizResult result = fviz_hash_map_create_reserve(32u, &intersection);
        if (result != FVIZ_OK) return result;
        cursor = 0u;
        while (fviz_hash_map_iterate(set->labels, &cursor, &key, &value) != FVIZ_FALSE)
            if (fviz_fea_entity_set_contains(source, key))
            {
                result = fviz_hash_map_set(intersection, key, (void*)(uintptr_t)1);
                if (result != FVIZ_OK) { fviz_release(intersection); return result; }
            }
        fviz_release(set->labels);
        set->labels = intersection;
        return FVIZ_OK;
    }
    cursor = 0u;
    while (fviz_hash_map_iterate(source->labels, &cursor, &key, &value) != FVIZ_FALSE)
    {
        if (operation == FVIZ_FEA_DISPLAY_GROUP_ADD || operation == FVIZ_FEA_DISPLAY_GROUP_REPLACE)
        {
            if (fviz_hash_map_set(set->labels, key, (void*)(uintptr_t)1) != FVIZ_OK)
                return fviz_last_error_code();
        }
        else if (operation == FVIZ_FEA_DISPLAY_GROUP_REMOVE)
        {
            (void)fviz_hash_map_erase(set->labels, key);
        }
    }
    return FVIZ_OK;
}

FVizResult fviz_fea_display_group_create(
    const char* name, FVizFEADisplayGroup** out_group)
{
    FVizFEADisplayGroup* group = NULL;
    if (out_group == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_group = NULL;
    group = (FVizFEADisplayGroup*)fviz_internal_object_allocate(
        sizeof(*group), &g_fviz_fea_display_group_class, NULL);
    if (group == NULL) return fviz_last_error_code();
    group->visible = FVIZ_TRUE;
    if (fviz_fea_entity_set_init(&group->nodes) != FVIZ_OK ||
        fviz_fea_entity_set_init(&group->elements) != FVIZ_OK ||
        fviz_fea_entity_set_init(&group->faces) != FVIZ_OK)
    {
        fviz_release(group);
        return fviz_last_error_code();
    }
    {
        const char* text = name != NULL ? name : "";
        const FVizSize length = strlen(text);
        group->name = (char*)fviz_alloc(length + 1u);
        if (group->name == NULL) { fviz_release(group); return fviz_last_error_code(); }
        (void)memcpy(group->name, text, length);
        group->name[length] = '\0';
    }
    *out_group = group;
    return FVIZ_OK;
}

const char* fviz_fea_display_group_name(const FVizFEADisplayGroup* group)
{
    return group != NULL ? group->name : "";
}

void fviz_fea_display_group_clear(FVizFEADisplayGroup* group)
{
    if (group == NULL) return;
    fviz_hash_map_clear(group->nodes.labels);
    fviz_hash_map_clear(group->elements.labels);
    fviz_hash_map_clear(group->faces.labels);
    fviz_object_modified((FVizObject*)group);
}

void fviz_fea_display_group_set_visible(FVizFEADisplayGroup* group, FVizBool visible)
{
    if (group == NULL) return;
    if (group->visible != visible)
    {
        group->visible = visible;
        fviz_object_modified((FVizObject*)group);
    }
}

FVizBool fviz_fea_display_group_visible(const FVizFEADisplayGroup* group)
{
    return group != NULL ? group->visible : FVIZ_FALSE;
}

FVizResult fviz_fea_display_group_set_nodes(
    FVizFEADisplayGroup* group, const uint64_t* node_labels, FVizSize count)
{
    FVizResult result;
    if (group == NULL || (count != 0u && node_labels == NULL)) return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_fea_entity_set_set(&group->nodes, node_labels, count);
    if (result == FVIZ_OK) fviz_object_modified((FVizObject*)group);
    return result;
}

FVizResult fviz_fea_display_group_set_elements(
    FVizFEADisplayGroup* group, const uint64_t* element_labels, FVizSize count)
{
    FVizResult result;
    if (group == NULL || (count != 0u && element_labels == NULL)) return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_fea_entity_set_set(&group->elements, element_labels, count);
    if (result == FVIZ_OK) fviz_object_modified((FVizObject*)group);
    return result;
}

FVizResult fviz_fea_display_group_set_faces(
    FVizFEADisplayGroup* group, const uint64_t* face_labels, FVizSize count)
{
    FVizResult result;
    if (group == NULL || (count != 0u && face_labels == NULL)) return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_fea_entity_set_set(&group->faces, face_labels, count);
    if (result == FVIZ_OK) fviz_object_modified((FVizObject*)group);
    return result;
}

FVizResult fviz_fea_display_group_combine(
    FVizFEADisplayGroup* group,
    const FVizFEADisplayGroup* source,
    FVizFEADisplayGroupOperation operation)
{
    FVizResult result;
    if (group == NULL || source == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_fea_entity_set_combine(&group->nodes, &source->nodes, operation);
    if (result == FVIZ_OK) result = fviz_fea_entity_set_combine(&group->elements, &source->elements, operation);
    if (result == FVIZ_OK) result = fviz_fea_entity_set_combine(&group->faces, &source->faces, operation);
    if (result == FVIZ_OK) fviz_object_modified((FVizObject*)group);
    return result;
}

/* Returns the label array for a grid association, or NULL. */
static const FVizDataArray* fviz_fea_grid_label_array(
    const FVizUnstructuredGrid* grid, FVizFEADisplayGroupEntity entity)
{
    FVizAttributeSet* point_data = fviz_unstructured_grid_point_data((FVizUnstructuredGrid*)grid);
    FVizAttributeSet* cell_data = fviz_unstructured_grid_cell_data((FVizUnstructuredGrid*)grid);
    if (entity == FVIZ_FEA_DISPLAY_GROUP_NODES)
        return fviz_attribute_set_const_get(point_data, FVIZ_ORIGINAL_POINT_IDS_ARRAY_NAME);
    if (entity == FVIZ_FEA_DISPLAY_GROUP_ELEMENTS)
        return fviz_attribute_set_const_get(cell_data, FVIZ_ORIGINAL_CELL_IDS_ARRAY_NAME);
    return fviz_attribute_set_const_get(cell_data, FVIZ_ORIGINAL_FACE_IDS_ARRAY_NAME);
}

/* Builds a UInt8 mask: 1 where the entity's label is present in the set. */
static FVizResult fviz_fea_display_group_build_mask(
    const FVizFEAEntitySet* set,
    const FVizDataArray* labels,
    FVizSize tuple_count,
    FVizDataArray** out_mask)
{
    FVizDataArray* mask = NULL;
    FVizSize i;
    if (out_mask == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_mask = NULL;
    if (fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &mask) != FVIZ_OK ||
        fviz_data_array_resize(mask, tuple_count) != FVIZ_OK)
        goto fail;
    if (labels == NULL || fviz_data_array_tuple_count(labels) != tuple_count)
    {
        /* No label provenance: if the set is empty everything is "in"; if the
         * set is non-empty nothing resolves. */
        const FVizBool empty = fviz_hash_map_count(set->labels) == 0u;
        uint8_t value = empty != FVIZ_FALSE ? 1u : 0u;
        for (i = 0u; i < tuple_count; ++i)
            if (fviz_data_array_set_component(mask, i, 0u, (double)value) != FVIZ_OK) goto fail;
        *out_mask = mask;
        return FVIZ_OK;
    }
    for (i = 0u; i < tuple_count; ++i)
    {
        double label = 0.0;
        uint8_t value = 0u;
        if (fviz_data_array_get_component(labels, i, 0u, &label) == FVIZ_OK)
            value = fviz_fea_entity_set_contains(set, (FVizId)(uint64_t)label) != FVIZ_FALSE ? 1u : 0u;
        if (fviz_data_array_set_component(mask, i, 0u, (double)value) != FVIZ_OK) goto fail;
    }
    *out_mask = mask;
    return FVIZ_OK;
fail:
    fviz_release(mask);
    return fviz_last_error_code();
}

FVizResult fviz_fea_display_group_create_masks(
    const FVizFEADisplayGroup* group,
    const FVizUnstructuredGrid* grid,
    FVizDataArray** out_point_mask,
    FVizDataArray** out_cell_mask)
{
    const FVizDataArray* node_labels;
    const FVizDataArray* cell_labels;
    FVizDataArray* point_mask = NULL;
    FVizDataArray* cell_mask = NULL;
    FVizResult result;
    if (group == NULL || grid == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    node_labels = fviz_fea_grid_label_array(grid, FVIZ_FEA_DISPLAY_GROUP_NODES);
    cell_labels = fviz_fea_grid_label_array(grid, FVIZ_FEA_DISPLAY_GROUP_ELEMENTS);
    result = fviz_fea_display_group_build_mask(&group->nodes, node_labels,
        fviz_unstructured_grid_point_count(grid), &point_mask);
    if (result != FVIZ_OK) return result;
    result = fviz_fea_display_group_build_mask(&group->elements, cell_labels,
        fviz_unstructured_grid_cell_count(grid), &cell_mask);
    if (result != FVIZ_OK) { fviz_release(point_mask); return result; }
    if (out_point_mask != NULL) *out_point_mask = point_mask;
    else fviz_release(point_mask);
    if (out_cell_mask != NULL) *out_cell_mask = cell_mask;
    else fviz_release(cell_mask);
    return FVIZ_OK;
}

FVizResult fviz_fea_display_group_apply_to_surface(
    const FVizFEADisplayGroup* group,
    const FVizPolyData* surface,
    FVizPolyData** out_surface)
{
    const FVizDataArray* cell_labels;
    const FVizDataArray* point_labels;
    FVizDataArray* cell_mask = NULL;
    FVizDataArray* point_mask = NULL;
    if (out_surface != NULL) *out_surface = NULL;
    if (group == NULL || surface == NULL || out_surface == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    cell_labels = fviz_attribute_set_const_get(
        fviz_poly_data_const_cell_data(surface), FVIZ_ORIGINAL_CELL_IDS_ARRAY_NAME);
    point_labels = fviz_attribute_set_const_get(
        fviz_poly_data_const_point_data(surface), FVIZ_ORIGINAL_POINT_IDS_ARRAY_NAME);
    if (fviz_fea_display_group_build_mask(&group->elements, cell_labels,
            fviz_poly_data_triangle_count(surface), &cell_mask) != FVIZ_OK)
        return fviz_last_error_code();
    if (fviz_fea_display_group_build_mask(&group->nodes, point_labels,
            fviz_poly_data_point_count(surface), &point_mask) != FVIZ_OK)
    { fviz_release(cell_mask); return fviz_last_error_code(); }
    /* This reference implementation validates the mask path and returns a deep
     * copy of the surface; triangle-level filtering is left to the caller using
     * the cell mask. */
    fviz_release(cell_mask);
    fviz_release(point_mask);
    {
        FVizPolyData* output = NULL;
        FVizResult result = fviz_poly_data_deep_copy(surface, &output);
        if (result != FVIZ_OK) { fviz_release(output); return fviz_last_error_code(); }
        *out_surface = output;
        return FVIZ_OK;
    }
}

void fviz_fea_display_group_get_statistics(
    const FVizFEADisplayGroup* group,
    const FVizUnstructuredGrid* grid,
    FVizFEADisplayGroupStatistics* out_statistics)
{
    FVizDataArray* point_mask = NULL;
    FVizDataArray* cell_mask = NULL;
    FVizSize i;
    if (out_statistics == NULL) return;
    memset(out_statistics, 0, sizeof(*out_statistics));
    if (group == NULL || grid == NULL) return;
    out_statistics->node_count = fviz_hash_map_count(group->nodes.labels);
    out_statistics->element_count = fviz_hash_map_count(group->elements.labels);
    out_statistics->face_count = fviz_hash_map_count(group->faces.labels);
    if (fviz_fea_display_group_create_masks(group, grid, &point_mask, &cell_mask) != FVIZ_OK)
        return;
    for (i = 0u; i < fviz_data_array_tuple_count(point_mask); ++i)
    {
        double v = 0.0;
        if (fviz_data_array_get_component(point_mask, i, 0u, &v) == FVIZ_OK && v > 0.5)
            ++out_statistics->visible_points;
    }
    for (i = 0u; i < fviz_data_array_tuple_count(cell_mask); ++i)
    {
        double v = 0.0;
        if (fviz_data_array_get_component(cell_mask, i, 0u, &v) == FVIZ_OK && v > 0.5)
            ++out_statistics->visible_cells;
    }
    fviz_release(point_mask);
    fviz_release(cell_mask);
}
