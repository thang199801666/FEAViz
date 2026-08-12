#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Interaction/FVizSelection.h>
#include <FViz/Rendering/FVizActor.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizSelectionPrivate.h>

static void fviz_selection_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_selection_class = {
    FVIZ_TYPE_SELECTION,
    "FVizSelection",
    &g_fviz_object_class,
    fviz_selection_destroy,
    NULL
};

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

static FVizId fviz_selection_original_id(
    const FVizPolyData* data,
    FVizSelectionAssociation association,
    const char* name,
    FVizSize rendered_id)
{
    const FVizAttributeSet* attributes;
    const FVizDataArray* array;
    const uint64_t* value;
    if (data == NULL) return FVIZ_INVALID_ID;
    attributes = association == FVIZ_SELECTION_POINT
        ? fviz_poly_data_const_point_data(data)
        : fviz_poly_data_const_cell_data(data);
    array = fviz_attribute_set_const_get(attributes, name);
    if (array == NULL || fviz_data_array_type(array) != FVIZ_DATA_UINT64 ||
        fviz_data_array_components(array) != 1u ||
        rendered_id >= fviz_data_array_tuple_count(array))
        return FVIZ_INVALID_ID;
    value = (const uint64_t*)fviz_data_array_const_tuple(array, rendered_id);
    return value != NULL ? (FVizId)*value : FVIZ_INVALID_ID;
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
    selection = (FVizSelection*)fviz_internal_object_allocate(
        sizeof(FVizSelection), &g_fviz_selection_class, NULL);
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

FVizResult fviz_selection_add(
    FVizSelection* selection,
    FVizActor* actor,
    FVizSelectionAssociation association,
    FVizSize id)
{
    FVizSelectionRecord record;
    const FVizPolyData* data;
    if (selection == NULL || actor == NULL || association < FVIZ_SELECTION_ACTOR ||
        association > FVIZ_SELECTION_CELL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "selection item is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_selection_record_initialize(&record);
    record.actor = actor;
    record.association = association;
    record.rendered_id = id;
    data = fviz_actor_const_poly_data(actor);
    record.output_mtime = fviz_object_mtime((const FVizObject*)data);
    if (association == FVIZ_SELECTION_POINT)
        record.original_point_id = fviz_selection_original_id(
            data, association, "FVizOriginalPointIds", id);
    else if (association == FVIZ_SELECTION_CELL)
    {
        record.original_cell_id = fviz_selection_original_id(
            data, association, "FVizOriginalCellIds", id);
        record.original_face_id = fviz_selection_original_id(
            data, association, "FVizOriginalFaceIds", id);
    }
    record.persistent = record.original_point_id != FVIZ_INVALID_ID ||
        record.original_cell_id != FVIZ_INVALID_ID ? FVIZ_TRUE : FVIZ_FALSE;
    return fviz_selection_add_record(selection, &record);
}

FVizResult fviz_selection_add_record(
    FVizSelection* selection,
    const FVizSelectionRecord* record)
{
    FVizSelectionItem item;
    if (selection == NULL || record == NULL ||
        record->struct_size < sizeof(FVizSelectionRecord) || record->actor == NULL ||
        record->association < FVIZ_SELECTION_ACTOR || record->association > FVIZ_SELECTION_CELL ||
        record->scalar_component_count > 4u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
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

FVizResult fviz_selection_get_record(
    const FVizSelection* selection,
    FVizSize index,
    FVizSelectionRecord* out_record)
{
    const FVizSelectionItem* item = selection != NULL
        ? (const FVizSelectionItem*)fviz_array_const_at(selection->items, index) : NULL;
    if (item == NULL || out_record == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_record = item->record;
    return FVIZ_OK;
}

static FVizBool fviz_selection_find_original(
    const FVizPolyData* data,
    FVizSelectionAssociation association,
    const char* name,
    FVizId original_id,
    FVizSize* out_rendered_id)
{
    const FVizAttributeSet* attributes = association == FVIZ_SELECTION_POINT
        ? fviz_poly_data_const_point_data(data) : fviz_poly_data_const_cell_data(data);
    const FVizDataArray* array = fviz_attribute_set_const_get(attributes, name);
    FVizSize i;
    if (array == NULL || original_id == FVIZ_INVALID_ID ||
        fviz_data_array_type(array) != FVIZ_DATA_UINT64 ||
        fviz_data_array_components(array) != 1u)
        return FVIZ_FALSE;
    for (i = 0u; i < fviz_data_array_tuple_count(array); ++i)
    {
        const uint64_t* value = (const uint64_t*)fviz_data_array_const_tuple(array, i);
        if (value != NULL && (FVizId)*value == original_id)
        {
            *out_rendered_id = i;
            return FVIZ_TRUE;
        }
    }
    return FVIZ_FALSE;
}

FVizResult fviz_selection_refresh(FVizSelection* selection)
{
    FVizSize i;
    if (selection == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < fviz_array_count(selection->items); ++i)
    {
        FVizSelectionRecord* record = &((FVizSelectionItem*)
            fviz_array_at(selection->items, i))->record;
        const FVizPolyData* data = fviz_actor_const_poly_data(record->actor);
        const FVizMTime mtime = fviz_object_mtime((const FVizObject*)data);
        if (record->output_mtime == mtime) continue;
        if (record->association == FVIZ_SELECTION_ACTOR)
            record->state = data != NULL ? FVIZ_SELECTION_VALID : FVIZ_SELECTION_INVALID;
        else if (record->persistent == FVIZ_FALSE)
            record->state = FVIZ_SELECTION_INVALID;
        else if (record->association == FVIZ_SELECTION_POINT)
            record->state = fviz_selection_find_original(data, record->association,
                "FVizOriginalPointIds", record->original_point_id, &record->rendered_id)
                ? FVIZ_SELECTION_VALID : FVIZ_SELECTION_INVALID;
        else
            record->state = fviz_selection_find_original(data, record->association,
                "FVizOriginalCellIds", record->original_cell_id, &record->rendered_id)
                ? FVIZ_SELECTION_VALID : FVIZ_SELECTION_INVALID;
        record->output_mtime = mtime;
    }
    fviz_object_modified((FVizObject*)selection);
    return FVIZ_OK;
}

static double fviz_selection_component_as_double(
    const void* tuple,
    FVizDataType type,
    uint32_t component)
{
    switch (type)
    {
        case FVIZ_DATA_INT8: return (double)((const int8_t*)tuple)[component];
        case FVIZ_DATA_UINT8: return (double)((const uint8_t*)tuple)[component];
        case FVIZ_DATA_INT16: return (double)((const int16_t*)tuple)[component];
        case FVIZ_DATA_UINT16: return (double)((const uint16_t*)tuple)[component];
        case FVIZ_DATA_INT32: return (double)((const int32_t*)tuple)[component];
        case FVIZ_DATA_UINT32: return (double)((const uint32_t*)tuple)[component];
        case FVIZ_DATA_INT64: return (double)((const int64_t*)tuple)[component];
        case FVIZ_DATA_UINT64: return (double)((const uint64_t*)tuple)[component];
        case FVIZ_DATA_FLOAT32: return (double)((const float*)tuple)[component];
        case FVIZ_DATA_FLOAT64: return ((const double*)tuple)[component];
        default: return 0.0;
    }
}

FVizResult fviz_selection_probe(
    FVizSelection* selection,
    FVizSize index,
    const char* array_name)
{
    FVizSelectionItem* item;
    FVizSelectionRecord* record;
    const FVizPolyData* data;
    const FVizVec3* points;
    FVizVec3 world;
    if (selection == NULL || array_name == NULL || array_name[0] == '\0')
        return FVIZ_ERROR_INVALID_ARGUMENT;
    item = (FVizSelectionItem*)fviz_array_at(selection->items, index);
    if (item == NULL) return FVIZ_ERROR_NOT_FOUND;
    record = &item->record;
    if (record->state != FVIZ_SELECTION_VALID ||
        record->association == FVIZ_SELECTION_ACTOR)
        return FVIZ_ERROR_INVALID_STATE;
    data = fviz_actor_const_poly_data(record->actor);
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
        world = fviz_vec3_scale(fviz_vec3_add(fviz_vec3_add(
            points[triangle[0]], points[triangle[1]]), points[triangle[2]]), 1.0f / 3.0f);
    }
    {
        const FVizMat4 model = fviz_actor_transform_matrix(record->actor);
        world = fviz_vec3(
            model.m[0] * world.x + model.m[4] * world.y + model.m[8] * world.z + model.m[12],
            model.m[1] * world.x + model.m[5] * world.y + model.m[9] * world.z + model.m[13],
            model.m[2] * world.x + model.m[6] * world.y + model.m[10] * world.z + model.m[14]);
    }
    record->world_position = world;
    record->has_world_position = FVIZ_TRUE;
    {
        const FVizAttributeSet* attributes = record->association == FVIZ_SELECTION_POINT
            ? fviz_poly_data_const_point_data(data) : fviz_poly_data_const_cell_data(data);
        const FVizDataArray* array = fviz_attribute_set_const_get(attributes, array_name);
        const void* tuple;
        uint32_t component;
        if (array == NULL || record->rendered_id >= fviz_data_array_tuple_count(array))
            return FVIZ_ERROR_NOT_FOUND;
        tuple = fviz_data_array_const_tuple(array, record->rendered_id);
        record->scalar_component_count = fviz_data_array_components(array);
        if (record->scalar_component_count > 4u) record->scalar_component_count = 4u;
        for (component = 0u; component < record->scalar_component_count; ++component)
            record->scalar_tuple[component] = fviz_selection_component_as_double(
                tuple, fviz_data_array_type(array), component);
    }
    fviz_object_modified((FVizObject*)selection);
    return FVIZ_OK;
}

FVizSelectionState fviz_selection_state(
    const FVizSelection* selection,
    FVizSize index)
{
    const FVizSelectionItem* item = selection != NULL
        ? (const FVizSelectionItem*)fviz_array_const_at(selection->items, index) : NULL;
    return item != NULL ? item->record.state : FVIZ_SELECTION_INVALID;
}

FVizSize fviz_selection_count(const FVizSelection* selection)
{
    return selection != NULL ? fviz_array_count(selection->items) : 0u;
}

FVizActor* fviz_selection_actor(FVizSelection* selection, FVizSize index)
{
    FVizSelectionItem* item = selection != NULL
        ? (FVizSelectionItem*)fviz_array_at(selection->items, index)
        : NULL;
    return item != NULL ? item->record.actor : NULL;
}

FVizSelectionAssociation fviz_selection_association(
    const FVizSelection* selection,
    FVizSize index)
{
    const FVizSelectionItem* item = selection != NULL
        ? (const FVizSelectionItem*)fviz_array_const_at(selection->items, index)
        : NULL;
    return item != NULL ? item->record.association : FVIZ_SELECTION_ACTOR;
}

FVizSize fviz_selection_id(const FVizSelection* selection, FVizSize index)
{
    const FVizSelectionItem* item = selection != NULL
        ? (const FVizSelectionItem*)fviz_array_const_at(selection->items, index)
        : NULL;
    return item != NULL ? item->record.rendered_id : SIZE_MAX;
}
