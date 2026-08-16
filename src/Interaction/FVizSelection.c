#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizProvenance.h>
#include <FViz/Interaction/FVizSelection.h>
#include <FViz/Rendering/FVizActor.h>
#include <FViz/Rendering/FVizGlyphMapper.h>
#include <FViz/Rendering/FVizScene.h>
#include <FViz/Rendering/FVizRenderer.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizSelectionPrivate.h>

static void fviz_selection_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_selection_class = {FVIZ_TYPE_SELECTION, "FVizSelection", &g_fviz_object_class,
                                                       fviz_selection_destroy, NULL};

static void fviz_named_selection_collection_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_named_selection_collection_class = {
    FVIZ_TYPE_NAMED_SELECTION_COLLECTION, "FVizNamedSelectionCollection", &g_fviz_object_class,
    fviz_named_selection_collection_destroy, NULL};

void fviz_selection_record_initialize(FVizSelectionRecord* record)
{
    if (record == NULL) return;
    (void)memset(record, 0, sizeof(*record));
    record->struct_size = (uint32_t)sizeof(*record);
    record->rendered_id = SIZE_MAX;
    record->original_point_id = FVIZ_INVALID_ID;
    record->original_cell_id = FVIZ_INVALID_ID;
    record->original_face_id = FVIZ_INVALID_ID;
    record->state = FVIZ_SELECTION_VALID;
}

static void fviz_selection_destroy(FVizObject* object)
{
    FVizSelection* selection = (FVizSelection*)object;
    fviz_selection_clear(selection);
    fviz_release(selection->items);
    selection->items = NULL;
}

FVizResult fviz_selection_create(FVizSelection** out_selection)
{
    FVizSelection* selection;
    if (out_selection == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_selection must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_selection = NULL;
    selection = (FVizSelection*)fviz_internal_object_allocate(sizeof(FVizSelection), &g_fviz_selection_class, NULL);
    if (selection == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizSelectionItem), &selection->items) != FVIZ_OK)
    {
        fviz_release(selection);
        return fviz_last_error_code();
    }
    *out_selection = selection;
    return FVIZ_OK;
}

void fviz_selection_clear(FVizSelection* selection)
{
    FVizSize i;
    if (selection == NULL || selection->items == NULL) return;
    for (i = 0u; i < fviz_array_count(selection->items); ++i)
    {
        FVizSelectionItem* item = (FVizSelectionItem*)fviz_array_at(selection->items, i);
        fviz_release(item->record.actor);
    }
    fviz_array_clear(selection->items);
    fviz_object_modified((FVizObject*)selection);
}

static FVizSize fviz_selection_find_identity(const FVizSelection* selection, const FVizActor* actor,
                                             FVizSelectionAssociation association, FVizSize id)
{
    FVizSize i;
    if (selection == NULL || actor == NULL) return SIZE_MAX;
    for (i = 0u; i < fviz_array_count(selection->items); ++i)
    {
        const FVizSelectionItem* item = (const FVizSelectionItem*)fviz_array_const_at(selection->items, i);
        if (item != NULL && item->record.actor == actor && item->record.association == association &&
            item->record.rendered_id == id)
            return i;
    }
    return SIZE_MAX;
}

FVizResult fviz_selection_add(FVizSelection* selection, FVizActor* actor, FVizSelectionAssociation association,
                              FVizSize id)
{
    FVizSelectionRecord record;
    const FVizPolyData* data;
    if (selection == NULL || actor == NULL || association < FVIZ_SELECTION_ACTOR ||
        association > FVIZ_SELECTION_GLYPH_INSTANCE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "selection item is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_selection_record_initialize(&record);
    record.actor = actor;
    record.association = association;
    record.rendered_id = id;
    data = fviz_actor_const_poly_data(actor);
    if (association == FVIZ_SELECTION_GLYPH_INSTANCE)
        record.output_mtime = fviz_object_mtime((const FVizObject*)fviz_actor_const_glyph_mapper(actor));
    else
        record.output_mtime = fviz_object_mtime((const FVizObject*)data);
    if (association == FVIZ_SELECTION_POINT)
    {
        FVizBool persistent = FVIZ_FALSE;
        if (data != NULL && fviz_provenance_resolve(fviz_poly_data_const_point_data(data), FVIZ_PROVENANCE_POINT, id,
                                                    FVIZ_INVALID_ID, &record.original_point_id, &persistent) == FVIZ_OK)
            record.persistent = persistent;
    }
    else if (association == FVIZ_SELECTION_CELL)
    {
        FVizBool cell_persistent = FVIZ_FALSE;
        FVizBool face_persistent = FVIZ_FALSE;
        if (data != NULL)
        {
            (void)fviz_provenance_resolve(fviz_poly_data_const_cell_data(data), FVIZ_PROVENANCE_CELL, id,
                                          FVIZ_INVALID_ID, &record.original_cell_id, &cell_persistent);
            (void)fviz_provenance_resolve(fviz_poly_data_const_cell_data(data), FVIZ_PROVENANCE_FACE, id,
                                          FVIZ_INVALID_ID, &record.original_face_id, &face_persistent);
        }
        FVIZ_UNUSED(face_persistent);
        /* A face ID enriches a cell selection but cannot by itself relocate
         * that cell after topology changes. */
        record.persistent = cell_persistent;
    }
    return fviz_selection_add_record(selection, &record);
}

FVizResult fviz_selection_add_record(FVizSelection* selection, const FVizSelectionRecord* record)
{
    FVizSelectionItem item;
    if (selection == NULL || record == NULL || record->struct_size < sizeof(FVizSelectionRecord) ||
        record->actor == NULL || record->association < FVIZ_SELECTION_ACTOR ||
        record->association > FVIZ_SELECTION_GLYPH_INSTANCE || record->scalar_component_count > 4u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_selection_find_identity(selection, record->actor, record->association, record->rendered_id) != SIZE_MAX)
        return FVIZ_OK;
    if (fviz_retain(record->actor) == NULL) return fviz_last_error_code();
    item.record = *record;
    item.record.struct_size = (uint32_t)sizeof(item.record);
    if (fviz_array_push(selection->items, &item) != FVIZ_OK)
    {
        fviz_release(record->actor);
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)selection);
    return FVIZ_OK;
}

FVizResult fviz_selection_get_record(const FVizSelection* selection, FVizSize index, FVizSelectionRecord* out_record)
{
    const FVizSelectionItem* item =
        selection != NULL ? (const FVizSelectionItem*)fviz_array_const_at(selection->items, index) : NULL;
    if (item == NULL || out_record == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_record = item->record;
    return FVIZ_OK;
}

FVizResult fviz_selection_refresh(FVizSelection* selection)
{
    FVizSize i;
    if (selection == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < fviz_array_count(selection->items); ++i)
    {
        FVizSelectionRecord* record = &((FVizSelectionItem*)fviz_array_at(selection->items, i))->record;
        const FVizPolyData* data = fviz_actor_const_poly_data(record->actor);
        const FVizGlyphMapper* glyph_mapper = fviz_actor_const_glyph_mapper(record->actor);
        const FVizMTime mtime = record->association == FVIZ_SELECTION_GLYPH_INSTANCE
                                    ? fviz_object_mtime((const FVizObject*)glyph_mapper)
                                    : fviz_object_mtime((const FVizObject*)data);
        if (record->output_mtime == mtime) continue;
        if (record->association == FVIZ_SELECTION_ACTOR)
            record->state = (data != NULL || glyph_mapper != NULL) ? FVIZ_SELECTION_VALID : FVIZ_SELECTION_INVALID;
        else if (record->association == FVIZ_SELECTION_GLYPH_INSTANCE)
            record->state = glyph_mapper != NULL && record->rendered_id < fviz_glyph_mapper_instance_count(glyph_mapper)
                                ? FVIZ_SELECTION_VALID
                                : FVIZ_SELECTION_INVALID;
        else if (record->persistent == FVIZ_FALSE)
            record->state = FVIZ_SELECTION_INVALID;
        else if (record->association == FVIZ_SELECTION_POINT)
            record->state =
                data != NULL && fviz_provenance_find(fviz_poly_data_const_point_data(data), FVIZ_PROVENANCE_POINT,
                                                     record->original_point_id, &record->rendered_id) == FVIZ_OK
                    ? FVIZ_SELECTION_VALID
                    : FVIZ_SELECTION_INVALID;
        else if (record->association == FVIZ_SELECTION_CELL)
            record->state =
                data != NULL && fviz_provenance_find(fviz_poly_data_const_cell_data(data), FVIZ_PROVENANCE_CELL,
                                                     record->original_cell_id, &record->rendered_id) == FVIZ_OK
                    ? FVIZ_SELECTION_VALID
                    : FVIZ_SELECTION_INVALID;
        else
            record->state = FVIZ_SELECTION_INVALID;
        record->output_mtime = mtime;
    }
    fviz_object_modified((FVizObject*)selection);
    return FVIZ_OK;
}

FVizBool fviz_selection_contains(const FVizSelection* selection, const FVizActor* actor,
                                 FVizSelectionAssociation association, FVizSize id)
{
    if (association < FVIZ_SELECTION_ACTOR || association > FVIZ_SELECTION_GLYPH_INSTANCE) return FVIZ_FALSE;
    return fviz_selection_find_identity(selection, actor, association, id) != SIZE_MAX ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizResult fviz_selection_remove(FVizSelection* selection, const FVizActor* actor, FVizSelectionAssociation association,
                                 FVizSize id)
{
    const FVizSize index = fviz_selection_find_identity(selection, actor, association, id);
    FVizSelectionItem* items;
    FVizSize count;
    if (selection == NULL || actor == NULL || association < FVIZ_SELECTION_ACTOR ||
        association > FVIZ_SELECTION_GLYPH_INSTANCE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (index == SIZE_MAX) return FVIZ_ERROR_NOT_FOUND;
    count = fviz_array_count(selection->items);
    items = (FVizSelectionItem*)fviz_array_data(selection->items);
    fviz_release(items[index].record.actor);
    if (index + 1u < count)
        (void)memmove(&items[index], &items[index + 1u], (size_t)(count - index - 1u) * sizeof(*items));
    if (fviz_array_resize(selection->items, count - 1u) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)selection);
    return FVIZ_OK;
}

FVizResult fviz_selection_copy(const FVizSelection* source, FVizSelection** out_selection)
{
    FVizSelection* copy = NULL;
    FVizSize i;
    if (source == NULL || out_selection == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_selection = NULL;
    if (fviz_selection_create(&copy) != FVIZ_OK) return fviz_last_error_code();
    for (i = 0u; i < fviz_selection_count(source); ++i)
    {
        FVizSelectionRecord record;
        if (fviz_selection_get_record(source, i, &record) != FVIZ_OK ||
            fviz_selection_add_record(copy, &record) != FVIZ_OK)
        {
            fviz_release(copy);
            return fviz_last_error_code();
        }
    }
    *out_selection = copy;
    return FVIZ_OK;
}

FVizResult fviz_selection_apply(FVizSelection* selection, const FVizSelection* incoming, FVizSelectionModifier modifier)
{
    FVizSelection* snapshot = NULL;
    const FVizSelection* source = incoming;
    FVizSize i;
    if (selection == NULL || incoming == NULL || modifier < FVIZ_SELECTION_REPLACE || modifier > FVIZ_SELECTION_TOGGLE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (selection == incoming)
    {
        if (fviz_selection_copy(incoming, &snapshot) != FVIZ_OK) return fviz_last_error_code();
        source = snapshot;
    }
    if (modifier == FVIZ_SELECTION_REPLACE) fviz_selection_clear(selection);
    for (i = 0u; i < fviz_selection_count(source); ++i)
    {
        FVizSelectionRecord record;
        FVizBool contains;
        if (fviz_selection_get_record(source, i, &record) != FVIZ_OK)
        {
            fviz_release(snapshot);
            return fviz_last_error_code();
        }
        contains = fviz_selection_contains(selection, record.actor, record.association, record.rendered_id);
        if (modifier == FVIZ_SELECTION_SUBTRACT || (modifier == FVIZ_SELECTION_TOGGLE && contains != FVIZ_FALSE))
        {
            if (contains != FVIZ_FALSE)
                (void)fviz_selection_remove(selection, record.actor, record.association, record.rendered_id);
        }
        else if (contains == FVIZ_FALSE)
        {
            if (fviz_selection_add_record(selection, &record) != FVIZ_OK)
            {
                fviz_release(snapshot);
                return fviz_last_error_code();
            }
        }
    }
    fviz_release(snapshot);
    return FVIZ_OK;
}

static double fviz_selection_component_as_double(const void* tuple, FVizDataType type, uint32_t component)
{
    switch (type)
    {
        case FVIZ_DATA_INT8:
            return (double)((const int8_t*)tuple)[component];
        case FVIZ_DATA_UINT8:
            return (double)((const uint8_t*)tuple)[component];
        case FVIZ_DATA_INT16:
            return (double)((const int16_t*)tuple)[component];
        case FVIZ_DATA_UINT16:
            return (double)((const uint16_t*)tuple)[component];
        case FVIZ_DATA_INT32:
            return (double)((const int32_t*)tuple)[component];
        case FVIZ_DATA_UINT32:
            return (double)((const uint32_t*)tuple)[component];
        case FVIZ_DATA_INT64:
            return (double)((const int64_t*)tuple)[component];
        case FVIZ_DATA_UINT64:
            return (double)((const uint64_t*)tuple)[component];
        case FVIZ_DATA_FLOAT32:
            return (double)((const float*)tuple)[component];
        case FVIZ_DATA_FLOAT64:
            return ((const double*)tuple)[component];
        default:
            return 0.0;
    }
}

FVizResult fviz_selection_probe(FVizSelection* selection, FVizSize index, const char* array_name)
{
    FVizSelectionItem* item;
    FVizSelectionRecord* record;
    const FVizPolyData* data;
    const FVizVec3* points;
    FVizVec3 world;
    if (selection == NULL || array_name == NULL || array_name[0] == '\0') return FVIZ_ERROR_INVALID_ARGUMENT;
    item = (FVizSelectionItem*)fviz_array_at(selection->items, index);
    if (item == NULL) return FVIZ_ERROR_NOT_FOUND;
    record = &item->record;
    if (record->state != FVIZ_SELECTION_VALID || record->association == FVIZ_SELECTION_ACTOR ||
        record->association == FVIZ_SELECTION_EDGE)
        return FVIZ_ERROR_INVALID_STATE;
    if (record->association == FVIZ_SELECTION_GLYPH_INSTANCE)
    {
        const FVizGlyphMapper* glyph_mapper = fviz_actor_const_glyph_mapper(record->actor);
        FVizGlyphInstance instance;
        FVizMat4 model;
        if (glyph_mapper == NULL ||
            fviz_glyph_mapper_get_instance(glyph_mapper, record->rendered_id, &instance) != FVIZ_OK)
            return FVIZ_ERROR_NOT_FOUND;
        model = fviz_actor_transform_matrix(record->actor);
        record->world_position = fviz_vec3(model.m[0] * instance.position.x + model.m[4] * instance.position.y +
                                               model.m[8] * instance.position.z + model.m[12],
                                           model.m[1] * instance.position.x + model.m[5] * instance.position.y +
                                               model.m[9] * instance.position.z + model.m[13],
                                           model.m[2] * instance.position.x + model.m[6] * instance.position.y +
                                               model.m[10] * instance.position.z + model.m[14]);
        record->has_world_position = FVIZ_TRUE;
        record->scalar_component_count = 0u;
        fviz_object_modified((FVizObject*)selection);
        return FVIZ_OK;
    }
    data = fviz_actor_const_poly_data(record->actor);
    if (data == NULL) return FVIZ_ERROR_NOT_FOUND;
    points = fviz_poly_data_points(data);
    if (record->association == FVIZ_SELECTION_POINT)
    {
        if (record->rendered_id >= fviz_poly_data_point_count(data)) return FVIZ_ERROR_NOT_FOUND;
        world = points[record->rendered_id];
    }
    else
    {
        const uint32_t* triangle;
        if (record->rendered_id >= fviz_poly_data_triangle_count(data)) return FVIZ_ERROR_NOT_FOUND;
        triangle = fviz_poly_data_triangle_indices(data) + record->rendered_id * 3u;
        world = fviz_vec3_scale(
            fviz_vec3_add(fviz_vec3_add(points[triangle[0]], points[triangle[1]]), points[triangle[2]]), 1.0f / 3.0f);
    }
    {
        const FVizMat4 model = fviz_actor_transform_matrix(record->actor);
        world = fviz_vec3(model.m[0] * world.x + model.m[4] * world.y + model.m[8] * world.z + model.m[12],
                          model.m[1] * world.x + model.m[5] * world.y + model.m[9] * world.z + model.m[13],
                          model.m[2] * world.x + model.m[6] * world.y + model.m[10] * world.z + model.m[14]);
    }
    record->world_position = world;
    record->has_world_position = FVIZ_TRUE;
    {
        const FVizAttributeSet* attributes = record->association == FVIZ_SELECTION_POINT
                                                 ? fviz_poly_data_const_point_data(data)
                                                 : fviz_poly_data_const_cell_data(data);
        const FVizDataArray* array = fviz_attribute_set_const_get(attributes, array_name);
        const void* tuple;
        uint32_t component;
        if (array == NULL || record->rendered_id >= fviz_data_array_tuple_count(array)) return FVIZ_ERROR_NOT_FOUND;
        tuple = fviz_data_array_const_tuple(array, record->rendered_id);
        record->scalar_component_count = fviz_data_array_components(array);
        if (record->scalar_component_count > 4u) record->scalar_component_count = 4u;
        for (component = 0u; component < record->scalar_component_count; ++component)
            record->scalar_tuple[component] =
                fviz_selection_component_as_double(tuple, fviz_data_array_type(array), component);
    }
    fviz_object_modified((FVizObject*)selection);
    return FVIZ_OK;
}

FVizSelectionState fviz_selection_state(const FVizSelection* selection, FVizSize index)
{
    const FVizSelectionItem* item =
        selection != NULL ? (const FVizSelectionItem*)fviz_array_const_at(selection->items, index) : NULL;
    return item != NULL ? item->record.state : FVIZ_SELECTION_INVALID;
}

FVizSize fviz_selection_count(const FVizSelection* selection)
{
    return selection != NULL ? fviz_array_count(selection->items) : 0u;
}

FVizActor* fviz_selection_actor(FVizSelection* selection, FVizSize index)
{
    FVizSelectionItem* item = selection != NULL ? (FVizSelectionItem*)fviz_array_at(selection->items, index) : NULL;
    return item != NULL ? item->record.actor : NULL;
}

FVizSelectionAssociation fviz_selection_association(const FVizSelection* selection, FVizSize index)
{
    const FVizSelectionItem* item =
        selection != NULL ? (const FVizSelectionItem*)fviz_array_const_at(selection->items, index) : NULL;
    return item != NULL ? item->record.association : FVIZ_SELECTION_ACTOR;
}

FVizSize fviz_selection_id(const FVizSelection* selection, FVizSize index)
{
    const FVizSelectionItem* item =
        selection != NULL ? (const FVizSelectionItem*)fviz_array_const_at(selection->items, index) : NULL;
    return item != NULL ? item->record.rendered_id : SIZE_MAX;
}

FVizResult fviz_selection_create_mask(const FVizSelection* selection, const FVizActor* actor,
                                      FVizSelectionAssociation association, FVizSize value_count, FVizBool inverse,
                                      FVizDataArray** out_mask)
{
    FVizDataArray* mask = NULL;
    uint8_t* values;
    FVizSize index;
    if (selection == NULL || actor == NULL || out_mask == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_mask = NULL;
    if (fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &mask) != FVIZ_OK ||
        fviz_data_array_resize(mask, value_count) != FVIZ_OK)
    {
        fviz_release(mask);
        return fviz_last_error_code();
    }
    values = (uint8_t*)fviz_data_array_data(mask);
    (void)memset(values, inverse != FVIZ_FALSE ? 1 : 0, (size_t)value_count);
    for (index = 0u; index < fviz_array_count(selection->items); ++index)
    {
        const FVizSelectionRecord* record =
            &((const FVizSelectionItem*)fviz_array_const_at(selection->items, index))->record;
        if (record->actor == actor && record->association == association && record->state == FVIZ_SELECTION_VALID &&
            record->rendered_id < value_count)
            values[record->rendered_id] = inverse != FVIZ_FALSE ? 0u : 1u;
    }
    *out_mask = mask;
    return FVIZ_OK;
}

FVizResult fviz_selection_convert_association(const FVizSelection* selection, FVizActor* actor,
                                              FVizSelectionAssociation target_association,
                                              FVizSelection** out_selection)
{
    const FVizPolyData* data;
    const uint32_t* triangles;
    FVizSelection* converted = NULL;
    FVizSize triangle_count;
    FVizSize index;
    if (selection == NULL || actor == NULL || out_selection == NULL ||
        (target_association != FVIZ_SELECTION_POINT && target_association != FVIZ_SELECTION_CELL))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_selection = NULL;
    data = fviz_actor_const_poly_data(actor);
    if (data == NULL) return FVIZ_ERROR_INVALID_STATE;
    triangles = fviz_poly_data_triangle_indices(data);
    triangle_count = fviz_poly_data_triangle_count(data);
    if (fviz_selection_create(&converted) != FVIZ_OK) return fviz_last_error_code();
    for (index = 0u; index < fviz_array_count(selection->items); ++index)
    {
        const FVizSelectionRecord* record =
            &((const FVizSelectionItem*)fviz_array_const_at(selection->items, index))->record;
        FVizSize triangle;
        if (record->actor != actor || record->state != FVIZ_SELECTION_VALID) continue;
        if (record->association == target_association)
        {
            if (fviz_selection_add(converted, actor, target_association, record->rendered_id) != FVIZ_OK) goto fail;
        }
        else if (record->association == FVIZ_SELECTION_POINT && target_association == FVIZ_SELECTION_CELL)
        {
            for (triangle = 0u; triangle < triangle_count; ++triangle)
            {
                const uint32_t* ids = &triangles[triangle * 3u];
                if (ids[0] == record->rendered_id || ids[1] == record->rendered_id || ids[2] == record->rendered_id)
                    if (fviz_selection_add(converted, actor, FVIZ_SELECTION_CELL, triangle) != FVIZ_OK) goto fail;
            }
        }
        else if (record->association == FVIZ_SELECTION_CELL && target_association == FVIZ_SELECTION_POINT)
        {
            const uint32_t* ids;
            uint32_t corner;
            if (record->rendered_id >= triangle_count) continue;
            ids = &triangles[record->rendered_id * 3u];
            for (corner = 0u; corner < 3u; ++corner)
                if (fviz_selection_add(converted, actor, FVIZ_SELECTION_POINT, ids[corner]) != FVIZ_OK) goto fail;
        }
        else
        {
            fviz_release(converted);
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
    }
    *out_selection = converted;
    return FVIZ_OK;
fail:
    fviz_release(converted);
    return fviz_last_error_code();
}

static FVizResult fviz_selection_gather_attributes(const FVizAttributeSet* source, FVizAttributeSet* destination,
                                                   const FVizSize* tuple_ids, FVizSize tuple_count)
{
    FVizSize array_index;
    FVizAttributeRole role;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source); ++array_index)
    {
        const FVizDataArray* source_array = fviz_attribute_set_const_array_at(source, array_index);
        const char* name = fviz_attribute_set_name_at(source, array_index);
        FVizDataArray* output = NULL;
        FVizSize tuple;
        if (source_array == NULL || name == NULL) continue;
        if (fviz_data_array_create(fviz_data_array_type(source_array), fviz_data_array_components(source_array),
                                   &output) != FVIZ_OK)
            return fviz_last_error_code();
        for (tuple = 0u; tuple < tuple_count; ++tuple)
        {
            const void* value = fviz_data_array_const_tuple(source_array, tuple_ids[tuple]);
            if (value == NULL || fviz_data_array_append_tuple(output, value) != FVIZ_OK)
            {
                fviz_release(output);
                return FVIZ_ERROR_INVALID_STATE;
            }
        }
        if (fviz_attribute_set_add(destination, name, output) != FVIZ_OK)
        {
            fviz_release(output);
            return fviz_last_error_code();
        }
        fviz_release(output);
    }
    for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
    {
        const char* active = fviz_attribute_set_active_name(source, role);
        if (active != NULL && fviz_attribute_set_const_get(destination, active) != NULL)
            (void)fviz_attribute_set_set_active(destination, role, active);
    }
    return FVIZ_OK;
}

FVizResult fviz_selection_extract_poly_data(const FVizSelection* selection, const FVizActor* actor,
                                            FVizSelectionAssociation association, FVizPolyData** out_poly_data)
{
    const FVizPolyData* source;
    const uint32_t* triangles;
    FVizPolyData* output = NULL;
    FVizSize* point_ids = NULL;
    FVizSize* cell_ids = NULL;
    uint32_t* point_map = NULL;
    FVizSize point_count = 0u;
    FVizSize cell_count = 0u;
    FVizSize index;
    FVizSize bytes;
    if (selection == NULL || actor == NULL || out_poly_data == NULL ||
        (association != FVIZ_SELECTION_POINT && association != FVIZ_SELECTION_CELL))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_poly_data = NULL;
    source = fviz_actor_const_poly_data(actor);
    if (source == NULL) return FVIZ_ERROR_INVALID_STATE;
    triangles = fviz_poly_data_triangle_indices(source);
    if (fviz_size_multiply(fviz_poly_data_point_count(source), sizeof(*point_map), &bytes) != FVIZ_OK) goto fail;
    point_map = (uint32_t*)fviz_alloc(bytes);
    if (fviz_size_multiply(fviz_poly_data_point_count(source), sizeof(*point_ids), &bytes) != FVIZ_OK) goto fail;
    point_ids = (FVizSize*)fviz_alloc(bytes);
    if (fviz_size_multiply(fviz_poly_data_triangle_count(source), sizeof(*cell_ids), &bytes) != FVIZ_OK) goto fail;
    cell_ids = (FVizSize*)fviz_alloc(bytes);
    if ((point_map == NULL && fviz_poly_data_point_count(source) != 0u) ||
        (point_ids == NULL && fviz_poly_data_point_count(source) != 0u) ||
        (cell_ids == NULL && fviz_poly_data_triangle_count(source) != 0u) || fviz_poly_data_create(&output) != FVIZ_OK)
        goto fail;
    for (index = 0u; index < fviz_poly_data_point_count(source); ++index)
        point_map[index] = UINT32_MAX;
    for (index = 0u; index < fviz_array_count(selection->items); ++index)
    {
        const FVizSelectionRecord* record =
            &((const FVizSelectionItem*)fviz_array_const_at(selection->items, index))->record;
        if (record->actor != actor || record->association != association || record->state != FVIZ_SELECTION_VALID)
            continue;
        if (association == FVIZ_SELECTION_POINT)
        {
            FVizVec3 point;
            uint32_t new_id;
            if (record->rendered_id >= fviz_poly_data_point_count(source)) continue;
            if (fviz_poly_data_get_point(source, record->rendered_id, &point) != FVIZ_OK ||
                fviz_poly_data_add_point(output, point, &new_id) != FVIZ_OK ||
                fviz_poly_data_add_vertex(output, new_id) != FVIZ_OK)
                goto fail;
            point_ids[point_count++] = record->rendered_id;
        }
        else
        {
            uint32_t new_ids[3];
            uint32_t corner;
            if (record->rendered_id >= fviz_poly_data_triangle_count(source)) continue;
            for (corner = 0u; corner < 3u; ++corner)
            {
                const uint32_t old_id = triangles[record->rendered_id * 3u + corner];
                if (point_map[old_id] == UINT32_MAX)
                {
                    FVizVec3 point;
                    if (fviz_poly_data_get_point(source, old_id, &point) != FVIZ_OK ||
                        fviz_poly_data_add_point(output, point, &point_map[old_id]) != FVIZ_OK)
                        goto fail;
                    point_ids[point_count++] = old_id;
                }
                new_ids[corner] = point_map[old_id];
            }
            if (fviz_poly_data_add_triangle(output, new_ids[0], new_ids[1], new_ids[2]) != FVIZ_OK) goto fail;
            cell_ids[cell_count++] = record->rendered_id;
        }
    }
    if (fviz_selection_gather_attributes(fviz_poly_data_const_point_data(source), fviz_poly_data_point_data(output),
                                         point_ids, point_count) != FVIZ_OK ||
        (association == FVIZ_SELECTION_CELL &&
         fviz_selection_gather_attributes(fviz_poly_data_const_cell_data(source), fviz_poly_data_cell_data(output),
                                          cell_ids, cell_count) != FVIZ_OK))
        goto fail;
    for (index = 0u; index < fviz_attribute_set_count(fviz_poly_data_const_field_data(source)); ++index)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(fviz_poly_data_const_field_data(source), index);
        FVizDataArray* copy = NULL;
        if (array != NULL && fviz_data_array_deep_copy(array, &copy) == FVIZ_OK)
        {
            if (fviz_attribute_set_add(fviz_poly_data_field_data(output),
                                       fviz_attribute_set_name_at(fviz_poly_data_const_field_data(source), index),
                                       copy) != FVIZ_OK)
            {
                fviz_release(copy);
                goto fail;
            }
            fviz_release(copy);
        }
    }
    fviz_free(cell_ids);
    fviz_free(point_ids);
    fviz_free(point_map);
    *out_poly_data = output;
    return FVIZ_OK;
fail:
    fviz_free(cell_ids);
    fviz_free(point_ids);
    fviz_free(point_map);
    fviz_release(output);
    return fviz_last_error_code();
}

static FVizSize fviz_named_selection_collection_find(const FVizNamedSelectionCollection* collection, const char* name)
{
    FVizSize index;
    if (collection == NULL || name == NULL) return SIZE_MAX;
    for (index = 0u; index < fviz_array_count(collection->entries); ++index)
    {
        const FVizNamedSelectionEntry* entry =
            (const FVizNamedSelectionEntry*)fviz_array_const_at(collection->entries, index);
        if (strcmp(fviz_string_c_str(entry->name), name) == 0) return index;
    }
    return SIZE_MAX;
}

void fviz_named_selection_collection_clear(FVizNamedSelectionCollection* collection)
{
    FVizSize index;
    if (collection == NULL || collection->entries == NULL) return;
    for (index = 0u; index < fviz_array_count(collection->entries); ++index)
    {
        FVizNamedSelectionEntry* entry = (FVizNamedSelectionEntry*)fviz_array_at(collection->entries, index);
        fviz_release(entry->name);
        fviz_release(entry->selection);
    }
    fviz_array_clear(collection->entries);
    fviz_object_modified((FVizObject*)collection);
}

static void fviz_named_selection_collection_destroy(FVizObject* object)
{
    FVizNamedSelectionCollection* collection = (FVizNamedSelectionCollection*)object;
    fviz_named_selection_collection_clear(collection);
    fviz_release(collection->entries);
}

FVizResult fviz_named_selection_collection_create(FVizNamedSelectionCollection** out_collection)
{
    FVizNamedSelectionCollection* collection;
    if (out_collection == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_collection = NULL;
    collection = (FVizNamedSelectionCollection*)fviz_internal_object_allocate(
        sizeof(*collection), &g_fviz_named_selection_collection_class, NULL);
    if (collection == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizNamedSelectionEntry), &collection->entries) != FVIZ_OK)
    {
        fviz_release(collection);
        return fviz_last_error_code();
    }
    *out_collection = collection;
    return FVIZ_OK;
}

FVizResult fviz_named_selection_collection_set(FVizNamedSelectionCollection* collection, const char* name,
                                               FVizSelection* selection)
{
    FVizSize index;
    FVizNamedSelectionEntry entry;
    if (collection == NULL || name == NULL || name[0] == '\0' || selection == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    index = fviz_named_selection_collection_find(collection, name);
    if (index != SIZE_MAX)
    {
        FVizNamedSelectionEntry* current = (FVizNamedSelectionEntry*)fviz_array_at(collection->entries, index);
        FVizSelection* replacement = (FVizSelection*)fviz_retain(selection);
        if (replacement == NULL) return fviz_last_error_code();
        fviz_release(current->selection);
        current->selection = replacement;
        fviz_object_modified((FVizObject*)collection);
        return FVIZ_OK;
    }
    entry.name = NULL;
    entry.selection = (FVizSelection*)fviz_retain(selection);
    if (entry.selection == NULL || fviz_string_create_from(name, &entry.name) != FVIZ_OK ||
        fviz_array_push(collection->entries, &entry) != FVIZ_OK)
    {
        fviz_release(entry.name);
        fviz_release(entry.selection);
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)collection);
    return FVIZ_OK;
}

FVizResult fviz_named_selection_collection_remove(FVizNamedSelectionCollection* collection, const char* name)
{
    FVizSize index;
    FVizNamedSelectionEntry* entries;
    FVizSize count;
    if (collection == NULL || name == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    index = fviz_named_selection_collection_find(collection, name);
    if (index == SIZE_MAX) return FVIZ_ERROR_NOT_FOUND;
    entries = (FVizNamedSelectionEntry*)fviz_array_data(collection->entries);
    count = fviz_array_count(collection->entries);
    fviz_release(entries[index].name);
    fviz_release(entries[index].selection);
    if (index + 1u < count)
        (void)memmove(&entries[index], &entries[index + 1u], (size_t)(count - index - 1u) * sizeof(*entries));
    (void)fviz_array_resize(collection->entries, count - 1u);
    fviz_object_modified((FVizObject*)collection);
    return FVIZ_OK;
}

FVizSize fviz_named_selection_collection_count(const FVizNamedSelectionCollection* collection)
{
    return collection != NULL ? fviz_array_count(collection->entries) : 0u;
}

const char* fviz_named_selection_collection_name(const FVizNamedSelectionCollection* collection, FVizSize index)
{
    const FVizNamedSelectionEntry* entry =
        collection != NULL ? (const FVizNamedSelectionEntry*)fviz_array_const_at(collection->entries, index) : NULL;
    return entry != NULL ? fviz_string_c_str(entry->name) : NULL;
}

FVizSelection* fviz_named_selection_collection_get(FVizNamedSelectionCollection* collection, const char* name)
{
    const FVizSize index = fviz_named_selection_collection_find(collection, name);
    FVizNamedSelectionEntry* entry =
        index != SIZE_MAX ? (FVizNamedSelectionEntry*)fviz_array_at(collection->entries, index) : NULL;
    return entry != NULL ? entry->selection : NULL;
}

static FVizVec3 fviz_selection_transform_point(FVizMat4 matrix, FVizVec3 point)
{
    return fviz_vec3(matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
                     matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
                     matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]);
}

static FVizBool fviz_selection_point_on_segment(double px, double py, double ax, double ay, double bx, double by)
{
    const double cross = (px - ax) * (by - ay) - (py - ay) * (bx - ax);
    const double scale = 1.0 + ((bx - ax) < 0.0 ? -(bx - ax) : (bx - ax)) + ((by - ay) < 0.0 ? -(by - ay) : (by - ay));
    const double dot = (px - ax) * (bx - ax) + (py - ay) * (by - ay);
    const double length2 = (bx - ax) * (bx - ax) + (by - ay) * (by - ay);
    if ((cross < 0.0 ? -cross : cross) > 1.0e-9 * scale) return FVIZ_FALSE;
    return dot >= -1.0e-9 && dot <= length2 + 1.0e-9 ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_selection_point_in_polygon(const int* xy_points, FVizSize point_count, double x, double y)
{
    FVizBool inside = FVIZ_FALSE;
    FVizSize i;
    FVizSize j;
    if (xy_points == NULL || point_count < 3u) return FVIZ_FALSE;
    j = point_count - 1u;
    for (i = 0u; i < point_count; ++i)
    {
        const double xi = (double)xy_points[i * 2u + 0u];
        const double yi = (double)xy_points[i * 2u + 1u];
        const double xj = (double)xy_points[j * 2u + 0u];
        const double yj = (double)xy_points[j * 2u + 1u];
        if (fviz_selection_point_on_segment(x, y, xj, yj, xi, yi) != FVIZ_FALSE) return FVIZ_TRUE;
        if (((yi > y) != (yj > y)) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
            inside = inside == FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        j = i;
    }
    return inside;
}

static double fviz_selection_orientation(double ax, double ay, double bx, double by, double cx, double cy)
{
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static FVizBool fviz_selection_segments_intersect(double ax, double ay, double bx, double by, double cx, double cy,
                                                  double dx, double dy)
{
    const double o1 = fviz_selection_orientation(ax, ay, bx, by, cx, cy);
    const double o2 = fviz_selection_orientation(ax, ay, bx, by, dx, dy);
    const double o3 = fviz_selection_orientation(cx, cy, dx, dy, ax, ay);
    const double o4 = fviz_selection_orientation(cx, cy, dx, dy, bx, by);
    if (((o1 > 0.0 && o2 < 0.0) || (o1 < 0.0 && o2 > 0.0)) && ((o3 > 0.0 && o4 < 0.0) || (o3 < 0.0 && o4 > 0.0)))
        return FVIZ_TRUE;
    if (fviz_selection_point_on_segment(cx, cy, ax, ay, bx, by) != FVIZ_FALSE ||
        fviz_selection_point_on_segment(dx, dy, ax, ay, bx, by) != FVIZ_FALSE ||
        fviz_selection_point_on_segment(ax, ay, cx, cy, dx, dy) != FVIZ_FALSE ||
        fviz_selection_point_on_segment(bx, by, cx, cy, dx, dy) != FVIZ_FALSE)
        return FVIZ_TRUE;
    return FVIZ_FALSE;
}

static FVizBool fviz_selection_segment_intersects_polygon(const int* xy_points, FVizSize point_count, double ax,
                                                          double ay, double bx, double by)
{
    FVizSize i;
    FVizSize j;
    if (fviz_selection_point_in_polygon(xy_points, point_count, ax, ay) != FVIZ_FALSE ||
        fviz_selection_point_in_polygon(xy_points, point_count, bx, by) != FVIZ_FALSE)
        return FVIZ_TRUE;
    j = point_count - 1u;
    for (i = 0u; i < point_count; ++i)
    {
        if (fviz_selection_segments_intersect(ax, ay, bx, by, (double)xy_points[j * 2u], (double)xy_points[j * 2u + 1u],
                                              (double)xy_points[i * 2u], (double)xy_points[i * 2u + 1u]) != FVIZ_FALSE)
            return FVIZ_TRUE;
        j = i;
    }
    return FVIZ_FALSE;
}

static FVizBool fviz_selection_point_in_triangle(double px, double py, const double sx[3], const double sy[3])
{
    const double o0 = fviz_selection_orientation(sx[0], sy[0], sx[1], sy[1], px, py);
    const double o1 = fviz_selection_orientation(sx[1], sy[1], sx[2], sy[2], px, py);
    const double o2 = fviz_selection_orientation(sx[2], sy[2], sx[0], sy[0], px, py);
    const FVizBool has_negative = o0 < 0.0 || o1 < 0.0 || o2 < 0.0 ? FVIZ_TRUE : FVIZ_FALSE;
    const FVizBool has_positive = o0 > 0.0 || o1 > 0.0 || o2 > 0.0 ? FVIZ_TRUE : FVIZ_FALSE;
    return has_negative != FVIZ_FALSE && has_positive != FVIZ_FALSE ? FVIZ_FALSE : FVIZ_TRUE;
}

static FVizBool fviz_selection_triangle_intersects_polygon(const int* xy_points, FVizSize point_count,
                                                           const double sx[3], const double sy[3])
{
    FVizSize i;
    FVizSize edge;
    for (i = 0u; i < 3u; ++i)
        if (fviz_selection_point_in_polygon(xy_points, point_count, sx[i], sy[i]) != FVIZ_FALSE) return FVIZ_TRUE;
    for (i = 0u; i < point_count; ++i)
        if (fviz_selection_point_in_triangle((double)xy_points[i * 2u], (double)xy_points[i * 2u + 1u], sx, sy) !=
            FVIZ_FALSE)
            return FVIZ_TRUE;
    for (edge = 0u; edge < 3u; ++edge)
        if (fviz_selection_segment_intersects_polygon(xy_points, point_count, sx[edge], sy[edge], sx[(edge + 1u) % 3u],
                                                      sy[(edge + 1u) % 3u]) != FVIZ_FALSE)
            return FVIZ_TRUE;
    return FVIZ_FALSE;
}

static FVizBool fviz_selection_project_world(FVizRenderer* renderer, FVizVec3 world, int width, int height,
                                             double* out_x, double* out_y)
{
    FVizVec3 display;
    if (fviz_renderer_world_to_display(renderer, world, width, height, &display) != FVIZ_OK) return FVIZ_FALSE;
    *out_x = (double)display.x;
    *out_y = (double)display.y;
    return FVIZ_TRUE;
}

static FVizBool fviz_selection_actor_region_intersects(FVizRenderer* renderer, const FVizActor* actor, int width,
                                                       int height, const int* xy_points, FVizSize point_count)
{
    const FVizBounds bounds = fviz_actor_bounds(actor);
    FVizVec3 corners[8];
    double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;
    FVizBool have_point = FVIZ_FALSE;
    FVizSize i;
    if (bounds.valid == FVIZ_FALSE) return FVIZ_FALSE;
    corners[0] = fviz_vec3(bounds.min.x, bounds.min.y, bounds.min.z);
    corners[1] = fviz_vec3(bounds.max.x, bounds.min.y, bounds.min.z);
    corners[2] = fviz_vec3(bounds.max.x, bounds.max.y, bounds.min.z);
    corners[3] = fviz_vec3(bounds.min.x, bounds.max.y, bounds.min.z);
    corners[4] = fviz_vec3(bounds.min.x, bounds.min.y, bounds.max.z);
    corners[5] = fviz_vec3(bounds.max.x, bounds.min.y, bounds.max.z);
    corners[6] = fviz_vec3(bounds.max.x, bounds.max.y, bounds.max.z);
    corners[7] = fviz_vec3(bounds.min.x, bounds.max.y, bounds.max.z);
    for (i = 0u; i < 8u; ++i)
    {
        double x;
        double y;
        if (fviz_selection_project_world(renderer, corners[i], width, height, &x, &y) == FVIZ_FALSE) continue;
        if (fviz_selection_point_in_polygon(xy_points, point_count, x, y) != FVIZ_FALSE) return FVIZ_TRUE;
        if (have_point == FVIZ_FALSE)
        {
            min_x = max_x = x;
            min_y = max_y = y;
            have_point = FVIZ_TRUE;
        }
        else
        {
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
    if (have_point == FVIZ_FALSE) return FVIZ_FALSE;
    for (i = 0u; i < point_count; ++i)
    {
        const double x = (double)xy_points[i * 2u];
        const double y = (double)xy_points[i * 2u + 1u];
        if (x >= min_x && x <= max_x && y >= min_y && y <= max_y) return FVIZ_TRUE;
    }
    return FVIZ_FALSE;
}

static FVizBounds fviz_selection_bounds_from_points(const FVizVec3* points, FVizSize count)
{
    FVizBounds bounds = fviz_bounds_empty();
    FVizSize i;
    for (i = 0u; i < count; ++i)
        fviz_bounds_include_point(&bounds, points[i]);
    return bounds;
}

FVizResult fviz_selection_select_frustum(FVizRenderer* renderer, const FVizFrustum* frustum,
                                         FVizSelectionAssociation association, FVizSelection** out_selection)
{
    FVizSelection* selection = NULL;
    FVizScene* scene;
    FVizSize actor_index;
    if (renderer == NULL || frustum == NULL || frustum->valid == FVIZ_FALSE || out_selection == NULL ||
        association < FVIZ_SELECTION_ACTOR || association > FVIZ_SELECTION_GLYPH_INSTANCE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_selection = NULL;
    if (fviz_renderer_update(renderer) != FVIZ_OK || fviz_selection_create(&selection) != FVIZ_OK)
        return fviz_last_error_code();
    scene = fviz_renderer_scene(renderer);
    for (actor_index = 0u; scene != NULL && actor_index < fviz_scene_actor_count(scene); ++actor_index)
    {
        FVizActor* actor = fviz_scene_actor(scene, actor_index);
        FVizMat4 model;
        if (actor == NULL || fviz_actor_is_visible(actor) == FVIZ_FALSE || fviz_actor_pickable(actor) == FVIZ_FALSE ||
            fviz_frustum_intersects_bounds(frustum, fviz_actor_bounds(actor)) == FVIZ_FALSE)
            continue;
        model = fviz_actor_transform_matrix(actor);
        if (association == FVIZ_SELECTION_ACTOR)
        {
            if (fviz_selection_add(selection, actor, association, 0u) != FVIZ_OK) goto fail;
            continue;
        }
        if (association == FVIZ_SELECTION_GLYPH_INSTANCE)
        {
            const FVizGlyphMapper* glyphs = fviz_actor_const_glyph_mapper(actor);
            FVizSize instance_index;
            if (glyphs == NULL) continue;
            for (instance_index = 0u; instance_index < fviz_glyph_mapper_instance_count(glyphs); ++instance_index)
            {
                FVizGlyphInstance instance;
                if (fviz_glyph_mapper_get_instance(glyphs, instance_index, &instance) == FVIZ_OK &&
                    fviz_frustum_contains_point(frustum, fviz_selection_transform_point(model, instance.position)) !=
                        FVIZ_FALSE &&
                    fviz_selection_add(selection, actor, association, instance_index) != FVIZ_OK)
                    goto fail;
            }
            continue;
        }
        {
            const FVizPolyData* data = fviz_actor_const_poly_data(actor);
            const FVizVec3* points;
            if (data == NULL) continue;
            points = fviz_poly_data_points(data);
            if (association == FVIZ_SELECTION_POINT)
            {
                FVizSize point_index;
                for (point_index = 0u; point_index < fviz_poly_data_point_count(data); ++point_index)
                    if (fviz_frustum_contains_point(
                            frustum, fviz_selection_transform_point(model, points[point_index])) != FVIZ_FALSE &&
                        fviz_selection_add(selection, actor, association, point_index) != FVIZ_OK)
                        goto fail;
            }
            else
            {
                const uint32_t* indices = fviz_poly_data_triangle_indices(data);
                FVizSize triangle_index;
                for (triangle_index = 0u; triangle_index < fviz_poly_data_triangle_count(data); ++triangle_index)
                {
                    const uint32_t* tri = indices + triangle_index * 3u;
                    FVizVec3 world[3] = {fviz_selection_transform_point(model, points[tri[0]]),
                                         fviz_selection_transform_point(model, points[tri[1]]),
                                         fviz_selection_transform_point(model, points[tri[2]])};
                    if (association == FVIZ_SELECTION_CELL)
                    {
                        if (fviz_frustum_intersects_bounds(frustum, fviz_selection_bounds_from_points(world, 3u)) !=
                                FVIZ_FALSE &&
                            fviz_selection_add(selection, actor, association, triangle_index) != FVIZ_OK)
                            goto fail;
                    }
                    else if (association == FVIZ_SELECTION_EDGE)
                    {
                        FVizSize edge;
                        for (edge = 0u; edge < 3u; ++edge)
                        {
                            const FVizVec3 edge_points[2] = {world[edge], world[(edge + 1u) % 3u]};
                            if (fviz_frustum_intersects_bounds(
                                    frustum, fviz_selection_bounds_from_points(edge_points, 2u)) != FVIZ_FALSE &&
                                fviz_selection_add(selection, actor, association, triangle_index * 3u + edge) !=
                                    FVIZ_OK)
                                goto fail;
                        }
                    }
                }
            }
        }
    }
    *out_selection = selection;
    return FVIZ_OK;
fail:
    fviz_release(selection);
    return fviz_last_error_code();
}

void fviz_selection_region_options_initialize(FVizSelectionRegionOptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->visibility_policy = FVIZ_SELECTION_THROUGH;
    options->cancellation_check_interval = 1024u;
}

void fviz_selection_region_statistics_initialize(FVizSelectionRegionStatistics* statistics)
{
    if (statistics == NULL) return;
    memset(statistics, 0, sizeof(*statistics));
    statistics->struct_size = (uint32_t)sizeof(*statistics);
}

static FVizResult fviz_selection_region_visit_candidate(const FVizSelectionRegionOptions* options,
                                                        FVizSelectionRegionStatistics* statistics)
{
    uint32_t interval;
    if (statistics->candidates_tested != UINT64_MAX) ++statistics->candidates_tested;
    if (options->cancel == NULL) return FVIZ_OK;
    interval = options->cancellation_check_interval != 0u ? options->cancellation_check_interval : 1024u;
    if ((statistics->candidates_tested % interval) == 0u && options->cancel(options->cancel_user_data) != FVIZ_FALSE)
    {
        statistics->cancelled = FVIZ_TRUE;
        fviz_internal_set_error(FVIZ_ERROR_CANCELLED, "region selection was cancelled");
        return FVIZ_ERROR_CANCELLED;
    }
    return FVIZ_OK;
}

static FVizResult fviz_selection_region_add(FVizSelection* selection, FVizActor* actor,
                                            FVizSelectionAssociation association, FVizSize id,
                                            const FVizSelectionRegionOptions* options,
                                            FVizSelectionRegionStatistics* statistics)
{
    if (options->maximum_results != 0u && fviz_selection_count(selection) >= options->maximum_results)
    {
        statistics->overflow = FVIZ_TRUE;
        return FVIZ_ERROR_OVERFLOW;
    }
    return fviz_selection_add(selection, actor, association, id);
}

static FVizResult fviz_selection_select_polygon_internal(FVizRenderer* renderer, int display_width, int display_height,
                                                         const int* xy_points, FVizSize point_count,
                                                         FVizSelectionAssociation association,
                                                         const FVizSelectionRegionOptions* options,
                                                         FVizSelectionRegionStatistics* statistics,
                                                         FVizSelection** out_selection)
{
    FVizSelection* selection = NULL;
    FVizScene* scene;
    FVizSize actor_index;
    float viewport[4];
    float aspect_ratio;
    FVizResult candidate_result;
    if (renderer == NULL || display_width <= 0 || display_height <= 0 || xy_points == NULL || point_count < 3u ||
        out_selection == NULL || association < FVIZ_SELECTION_ACTOR || association > FVIZ_SELECTION_GLYPH_INSTANCE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_selection = NULL;
    if (fviz_renderer_update(renderer) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_selection_create(&selection) != FVIZ_OK) return fviz_last_error_code();
    fviz_renderer_get_viewport(renderer, &viewport[0], &viewport[1], &viewport[2], &viewport[3]);
    {
        const float viewport_width = (viewport[2] - viewport[0]) * (float)display_width;
        const float viewport_height = (viewport[3] - viewport[1]) * (float)display_height;
        aspect_ratio = viewport_height > 0.0f ? viewport_width / viewport_height : 1.0f;
    }
    scene = fviz_renderer_scene(renderer);
    for (actor_index = 0u; scene != NULL && actor_index < fviz_scene_actor_count(scene); ++actor_index)
    {
        FVizActor* actor = fviz_scene_actor(scene, actor_index);
        const FVizMat4 model = actor != NULL ? fviz_actor_transform_matrix(actor) : fviz_mat4_identity();
        if (actor == NULL || fviz_actor_is_visible(actor) == FVIZ_FALSE || fviz_actor_pickable(actor) == FVIZ_FALSE ||
            fviz_renderer_actor_is_renderable(renderer, actor, aspect_ratio,
                                              (int)(((viewport[3] - viewport[1]) * (float)display_height) > 1.0f
                                                        ? ((viewport[3] - viewport[1]) * (float)display_height)
                                                        : 1.0f)) == FVIZ_FALSE)
            continue;
        if (association == FVIZ_SELECTION_GLYPH_INSTANCE)
        {
            const FVizGlyphMapper* glyphs = fviz_actor_const_glyph_mapper(actor);
            FVizSize instance_index;
            if (glyphs == NULL) continue;
            for (instance_index = 0u; instance_index < fviz_glyph_mapper_instance_count(glyphs); ++instance_index)
            {
                FVizGlyphInstance instance;
                double sx;
                double sy;
                candidate_result = fviz_selection_region_visit_candidate(options, statistics);
                if (candidate_result != FVIZ_OK) goto fail;
                if (fviz_glyph_mapper_get_instance(glyphs, instance_index, &instance) == FVIZ_OK &&
                    fviz_selection_project_world(renderer, fviz_selection_transform_point(model, instance.position),
                                                 display_width, display_height, &sx, &sy) != FVIZ_FALSE &&
                    fviz_selection_point_in_polygon(xy_points, point_count, sx, sy) != FVIZ_FALSE &&
                    (candidate_result = fviz_selection_region_add(selection, actor, association, instance_index,
                                                                  options, statistics)) != FVIZ_OK)
                {
                    if (candidate_result == FVIZ_ERROR_OVERFLOW) goto complete;
                    goto fail;
                }
            }
            continue;
        }
        if (association == FVIZ_SELECTION_ACTOR)
        {
            candidate_result = fviz_selection_region_visit_candidate(options, statistics);
            if (candidate_result != FVIZ_OK) goto fail;
            if (fviz_selection_actor_region_intersects(renderer, actor, display_width, display_height, xy_points,
                                                       point_count) != FVIZ_FALSE &&
                (candidate_result =
                     fviz_selection_region_add(selection, actor, association, 0u, options, statistics)) != FVIZ_OK)
            {
                if (candidate_result == FVIZ_ERROR_OVERFLOW) goto complete;
                goto fail;
            }
            continue;
        }
        {
            const FVizPolyData* poly_data = fviz_actor_const_poly_data(actor);
            const FVizVec3* points;
            if (poly_data == NULL) continue;
            points = fviz_poly_data_points(poly_data);
            if (association == FVIZ_SELECTION_POINT)
            {
                FVizSize point_index;
                for (point_index = 0u; point_index < fviz_poly_data_point_count(poly_data); ++point_index)
                {
                    double sx;
                    double sy;
                    candidate_result = fviz_selection_region_visit_candidate(options, statistics);
                    if (candidate_result != FVIZ_OK) goto fail;
                    if (fviz_selection_project_world(renderer,
                                                     fviz_selection_transform_point(model, points[point_index]),
                                                     display_width, display_height, &sx, &sy) != FVIZ_FALSE &&
                        fviz_selection_point_in_polygon(xy_points, point_count, sx, sy) != FVIZ_FALSE &&
                        (candidate_result = fviz_selection_region_add(selection, actor, association, point_index,
                                                                      options, statistics)) != FVIZ_OK)
                    {
                        if (candidate_result == FVIZ_ERROR_OVERFLOW) goto complete;
                        goto fail;
                    }
                }
            }
            else
            {
                const uint32_t* indices = fviz_poly_data_triangle_indices(poly_data);
                FVizSize triangle_index;
                for (triangle_index = 0u; triangle_index < fviz_poly_data_triangle_count(poly_data); ++triangle_index)
                {
                    const uint32_t ids[3] = {indices[triangle_index * 3u], indices[triangle_index * 3u + 1u],
                                             indices[triangle_index * 3u + 2u]};
                    double sx[3];
                    double sy[3];
                    FVizSize corner;
                    FVizBool projected = FVIZ_TRUE;
                    if (association == FVIZ_SELECTION_CELL)
                    {
                        candidate_result = fviz_selection_region_visit_candidate(options, statistics);
                        if (candidate_result != FVIZ_OK) goto fail;
                    }
                    for (corner = 0u; corner < 3u; ++corner)
                    {
                        if (fviz_selection_project_world(
                                renderer, fviz_selection_transform_point(model, points[ids[corner]]), display_width,
                                display_height, &sx[corner], &sy[corner]) == FVIZ_FALSE)
                        {
                            projected = FVIZ_FALSE;
                            break;
                        }
                    }
                    if (projected == FVIZ_FALSE) continue;
                    if (association == FVIZ_SELECTION_CELL)
                    {
                        if (fviz_selection_triangle_intersects_polygon(xy_points, point_count, sx, sy) != FVIZ_FALSE &&
                            (candidate_result = fviz_selection_region_add(selection, actor, association, triangle_index,
                                                                          options, statistics)) != FVIZ_OK)
                        {
                            if (candidate_result == FVIZ_ERROR_OVERFLOW) goto complete;
                            goto fail;
                        }
                    }
                    else if (association == FVIZ_SELECTION_EDGE)
                    {
                        FVizSize edge;
                        for (edge = 0u; edge < 3u; ++edge)
                        {
                            candidate_result = fviz_selection_region_visit_candidate(options, statistics);
                            if (candidate_result != FVIZ_OK) goto fail;
                            if (fviz_selection_segment_intersects_polygon(xy_points, point_count, sx[edge], sy[edge],
                                                                          sx[(edge + 1u) % 3u],
                                                                          sy[(edge + 1u) % 3u]) != FVIZ_FALSE &&
                                (candidate_result = fviz_selection_region_add(selection, actor, association,
                                                                              triangle_index * 3u + edge, options,
                                                                              statistics)) != FVIZ_OK)
                            {
                                if (candidate_result == FVIZ_ERROR_OVERFLOW) goto complete;
                                goto fail;
                            }
                        }
                    }
                }
            }
        }
    }
complete:
    statistics->results_returned = fviz_selection_count(selection);
    *out_selection = selection;
    return FVIZ_OK;
fail:
    fviz_release(selection);
    return fviz_last_error_code();
}

FVizResult fviz_selection_select_polygon_with_options(FVizRenderer* renderer, int display_width, int display_height,
                                                      const int* xy_points, FVizSize point_count,
                                                      FVizSelectionAssociation association,
                                                      const FVizSelectionRegionOptions* options,
                                                      FVizSelectionRegionStatistics* statistics,
                                                      FVizSelection** out_selection)
{
    FVizSelectionRegionOptions defaults;
    FVizSelectionRegionStatistics local_statistics;
    const FVizSelectionRegionOptions* applied = options;
    FVizSelectionRegionStatistics* reported = statistics;
    if (applied == NULL)
    {
        fviz_selection_region_options_initialize(&defaults);
        applied = &defaults;
    }
    if (reported == NULL) reported = &local_statistics;
    fviz_selection_region_statistics_initialize(reported);
    if (applied->struct_size < sizeof(*applied) ||
        (statistics != NULL && statistics->struct_size < sizeof(*statistics)) ||
        applied->visibility_policy != FVIZ_SELECTION_THROUGH)
    {
        if (applied->visibility_policy == FVIZ_SELECTION_VISIBLE_ONLY)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED,
                                    "visible-only region selection requires the asynchronous integer-ID backend");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_selection_select_polygon_internal(renderer, display_width, display_height, xy_points, point_count,
                                                  association, applied, reported, out_selection);
}

FVizResult fviz_selection_select_polygon(FVizRenderer* renderer, int display_width, int display_height,
                                         const int* xy_points, FVizSize point_count,
                                         FVizSelectionAssociation association, FVizSelection** out_selection)
{
    return fviz_selection_select_polygon_with_options(renderer, display_width, display_height, xy_points, point_count,
                                                      association, NULL, NULL, out_selection);
}

FVizResult fviz_selection_select_rectangle(FVizRenderer* renderer, int display_width, int display_height, int start_x,
                                           int start_y, int end_x, int end_y, FVizSelectionAssociation association,
                                           FVizSelection** out_selection)
{
    const int minimum_x = start_x < end_x ? start_x : end_x;
    const int minimum_y = start_y < end_y ? start_y : end_y;
    const int maximum_x = start_x > end_x ? start_x : end_x;
    const int maximum_y = start_y > end_y ? start_y : end_y;
    const int polygon[8] = {minimum_x, minimum_y, maximum_x, minimum_y, maximum_x, maximum_y, minimum_x, maximum_y};
    if (minimum_x < 0 || minimum_y < 0 || maximum_x >= display_width || maximum_y >= display_height)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    return fviz_selection_select_polygon(renderer, display_width, display_height, polygon, 4u, association,
                                         out_selection);
}
